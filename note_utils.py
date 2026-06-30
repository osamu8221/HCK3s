# -*- coding: utf-8 -*-
"""
note_utils.py ── onkai.py / lsd.py 共通の音声・音名ユーティリティ。

任意の wav・任意の音を検査に流用できるよう、次をまとめてある:
  ・wav 読み込み(モノラル化)
  ・基本周波数(ピッチ)推定  estimate_f0
  ・周波数 ⇔ 音名            freq_to_name / note_to_freq
  ・ファイル名から期待周波数を自動認識  parse_note
        日本語(ド レ ミ ファ ソ ラ シ / ＃) ・科学表記(C5, A4, F#3, Bb2) ・"440Hz"
  ・オクターブ畳み込み      fold_octave
  ・検査対象 wav の収集      discover_wavs

※ onkai.py / lsd.py と同じフォルダに置いて import する。
"""

import math
import os
import re
import glob

import numpy as np
import soundfile as sf
import librosa

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

# 固定ド: 日本語音名 -> Cからの半音
SOLFEGE = {"ド": 0, "レ": 2, "ミ": 4, "ファ": 5, "ソ": 7, "ラ": 9, "シ": 11}
# 科学表記の文字 -> Cからの半音
LETTER = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def load_mono(path):
    """wav を読み込み、(モノラル float64 配列, サンプリングレート) を返す。"""
    x, sr = sf.read(path)
    if x.ndim > 1:
        x = x.mean(axis=1)
    return x.astype(np.float64), sr


def estimate_f0(x, sr, fmin=120.0, fmax=2000.0):
    """基本周波数(Hz)を推定。librosa.pyin の有声中央値→失敗時はFFTピーク。"""
    fmax = min(fmax, sr / 2.0 - 1.0)
    try:
        f0, _, _ = librosa.pyin(x.astype(np.float32), sr=sr, fmin=fmin, fmax=fmax)
        vals = f0[~np.isnan(f0)]
        if len(vals):
            return float(np.median(vals))
    except Exception:
        pass
    return _f0_fft(x, sr, fmin, fmax)


def _f0_fft(x, sr, fmin, fmax):
    n = len(x)
    if n >= 2048:
        c = n // 2
        half = min(n, 1 << 15) // 2
        seg = x[max(0, c - half):c + half]
    else:
        seg = x
    seg = seg - np.mean(seg)
    win = np.hanning(len(seg))
    spec = np.abs(np.fft.rfft(seg * win))
    freqs = np.fft.rfftfreq(len(seg), d=1.0 / sr)
    band = (freqs >= fmin) & (freqs <= min(fmax, sr / 2.0))
    if not np.any(band):
        return None
    idx = int(np.argmax(np.where(band, spec, 0.0)))
    if 1 <= idx < len(spec) - 1:
        a, b, c2 = spec[idx - 1], spec[idx], spec[idx + 1]
        denom = (a - 2 * b + c2)
        delta = 0.5 * (a - c2) / denom if denom != 0 else 0.0
    else:
        delta = 0.0
    f = (idx + delta) * sr / len(seg)
    return float(f) if f > 0 else None


def midi_to_freq(midi):
    return 440.0 * (2.0 ** ((midi - 69) / 12.0))


def note_to_freq(semitone_from_c, octave):
    """Cからの半音 + オクターブ(科学表記, C4=MIDI60) -> 周波数Hz。"""
    midi = (octave + 1) * 12 + semitone_from_c
    return midi_to_freq(midi)


def freq_to_name(freq):
    """周波数に最も近い平均律の音名(例 C5)を返す。"""
    midi = int(round(69 + 12 * math.log2(freq / 440.0)))
    return f"{NOTE_NAMES[midi % 12]}{midi // 12 - 1}"


def fold_octave(f_meas, f_target):
    """f_meas を f_target に最も近いオクターブへ畳んだときの
    (オクターブオフセット, 音名内のセント誤差) を返す。"""
    k = round(math.log2(f_meas / f_target))
    f_shifted = f_target * (2.0 ** k)
    return k, 1200.0 * math.log2(f_meas / f_shifted)


def _accidental(s):
    if s in ("#", "＃", "♯"):
        return 1
    if s in ("b", "♭"):
        return -1
    return 0


def parse_note(filename, default_octave=5):
    """ファイル名から期待周波数を自動認識する。
    返り値: (周波数Hz or None, 表示ラベル)
      認識する書式(優先順):
        1. "440Hz" / "523.2hz" のような明示周波数
        2. 科学表記 C5 / A4 / F#3 / Bb2 (オクターブ省略時は default_octave)
        3. 日本語(固定ド) ド/レ/ミ/ファ/ソ/ラ/シ (+＃, +オクターブ数字)
      いずれも該当しなければ (None, ファイル名)。
    """
    base = os.path.splitext(os.path.basename(filename))[0].strip()

    # 1. 明示周波数 "...440Hz..."
    m = re.search(r'(\d+(?:\.\d+)?)\s*[Hh][Zz]', base)
    if m:
        return float(m.group(1)), f"{m.group(1)}Hz"

    # 2. 科学表記 (先頭が A〜G)。誤認防止のためオクターブ数字か全体が短い時のみ採用。
    m = re.match(r'^([A-Ga-g])([#＃♯b♭]?)(-?\d+)?$', base)
    if m:
        letter = m.group(1).upper()
        semis = LETTER[letter] + _accidental(m.group(2))
        octave = int(m.group(3)) if m.group(3) is not None else default_octave
        freq = note_to_freq(semis, octave)
        label = f"{letter}{m.group(2) or ''}{octave}"
        return freq, label

    # 3. 日本語(固定ド)。ファ を ド より先に試すため長い順で照合。
    m = re.match(r'^(ファ|ド|レ|ミ|ソ|ラ|シ)([＃#♯]?)(\d+)?', base)
    if m:
        semis = SOLFEGE[m.group(1)] + _accidental(m.group(2))
        octave = int(m.group(3)) if m.group(3) is not None else default_octave
        freq = note_to_freq(semis, octave)
        label = f"{m.group(1)}{m.group(2) or ''}{m.group(3) or ''}"
        return freq, label

    return None, base


def discover_wavs(files, directory, script_dir):
    """検査対象 wav のパス一覧を返す。
      files     : コマンドラインで明示指定された wav(優先)
      directory : --dir 指定フォルダ
      script_dir: 既定の探索先(スクリプトと同じ場所)
    """
    if files:
        return [os.path.abspath(f) for f in files]
    d = directory or script_dir
    return sorted(glob.glob(os.path.join(d, "*.wav")))
