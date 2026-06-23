# -*- coding: utf-8 -*-
"""
tonelib.py  ── 音階・波形テスト用の中核ライブラリ

依存:
    - numpy            (必須)
    - wave             (標準ライブラリ / WAV 入出力)
    - matplotlib       (任意 / 波形・スペクトル描画。無ければ描画関連だけスキップ)

設計方針:
    * 音の出力手段は WAV ファイル (16bit PCM)。
    * 平均律 (A4 = 440 Hz) で音名 <-> 周波数 を相互変換。
    * 生成した WAV を FFT で解析し、狙った周波数で鳴っているかを検証できる。
"""

from __future__ import annotations

import math
import wave
from dataclasses import dataclass, field
from typing import Dict, List, Tuple

import numpy as np

# ─────────────────────────────────────────────────────────────
# 既定パラメータ
# ─────────────────────────────────────────────────────────────
SAMPLE_RATE = 44100        # サンプリング周波数 [Hz]
SAMPLE_WIDTH = 2           # バイト数 (2 = 16bit PCM)
A4_FREQ = 440.0            # 基準音 A4 の周波数 [Hz]
DEFAULT_AMP = 0.8          # 既定振幅 (フルスケール 1.0 に対する比)

# 音名 -> 1オクターブ内の半音番号 (C を 0 とする)
_NOTE_BASE: Dict[str, int] = {
    "C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11,
}
_ACCIDENTAL: Dict[str, int] = {"#": 1, "♯": 1, "b": -1, "♭": -1, "": 0}

# 半音番号 (0..11) -> 代表的な音名表記 (♯系)
_SEMITONE_NAME: List[str] = [
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
]


# ─────────────────────────────────────────────────────────────
# 音名 <-> 周波数
# ─────────────────────────────────────────────────────────────
def note_to_midi(note: str) -> int:
    """音名 (例 'A4', 'C#5', 'Bb3') を MIDI ノート番号に変換する。"""
    note = note.strip()
    if not note:
        raise ValueError("空の音名です")

    letter = note[0].upper()
    if letter not in _NOTE_BASE:
        raise ValueError(f"不正な音名: {note!r}")

    idx = 1
    accidental = ""
    if idx < len(note) and note[idx] in _ACCIDENTAL:
        accidental = note[idx]
        idx += 1

    octave_str = note[idx:]
    try:
        octave = int(octave_str)
    except ValueError as exc:
        raise ValueError(f"オクターブ番号が読めません: {note!r}") from exc

    semitone = _NOTE_BASE[letter] + _ACCIDENTAL[accidental]
    # MIDI: C-1 = 0, A4 = 69
    return (octave + 1) * 12 + semitone


def midi_to_freq(midi: float) -> float:
    """MIDI ノート番号 -> 周波数 [Hz] (平均律, A4=69=440Hz)。"""
    return A4_FREQ * (2.0 ** ((midi - 69) / 12.0))


def note_to_freq(note: str) -> float:
    """音名 -> 周波数 [Hz]。"""
    return midi_to_freq(note_to_midi(note))


def freq_to_note(freq: float) -> Tuple[str, float]:
    """
    周波数 -> (最も近い音名, セント誤差)。
    セント誤差 > 0 は音名より高い、< 0 は低い。
    """
    if freq <= 0:
        return ("---", 0.0)
    midi_float = 69 + 12 * math.log2(freq / A4_FREQ)
    midi = int(round(midi_float))
    cents = (midi_float - midi) * 100.0
    name = _SEMITONE_NAME[midi % 12] + str(midi // 12 - 1)
    return (name, cents)


# ─────────────────────────────────────────────────────────────
# 波形生成
# ─────────────────────────────────────────────────────────────
def _phase(freq: float, dur: float, sr: int) -> np.ndarray:
    """連続位相 2πft を返す。"""
    n = int(round(dur * sr))
    t = np.arange(n, dtype=np.float64) / sr
    return 2.0 * np.pi * freq * t


def sine_wave(freq: float, dur: float, sr: int = SAMPLE_RATE) -> np.ndarray:
    """正弦波。"""
    return np.sin(_phase(freq, dur, sr))


def square_wave(freq: float, dur: float, sr: int = SAMPLE_RATE,
                harmonics: int = 0) -> np.ndarray:
    """
    矩形波。
    harmonics == 0 : 理想矩形波 (sign)。エイリアスを含む。
    harmonics >  0 : 奇数次倍音の加算合成 (バンドリミット)。
    """
    if harmonics <= 0:
        return np.sign(np.sin(_phase(freq, dur, sr)))
    ph = _phase(freq, dur, sr)
    out = np.zeros_like(ph)
    k = 1
    while k <= harmonics and freq * k < sr / 2:
        out += np.sin(k * ph) / k
        k += 2
    return (4.0 / np.pi) * out


def sawtooth_wave(freq: float, dur: float, sr: int = SAMPLE_RATE,
                  harmonics: int = 0) -> np.ndarray:
    """
    ノコギリ波。
    harmonics == 0 : 理想ノコギリ波。エイリアスを含む。
    harmonics >  0 : 全倍音の加算合成 (バンドリミット)。
    """
    if harmonics <= 0:
        n = int(round(dur * sr))
        t = np.arange(n, dtype=np.float64) * freq / sr
        return 2.0 * (t - np.floor(t + 0.5))
    ph = _phase(freq, dur, sr)
    out = np.zeros_like(ph)
    k = 1
    while k <= harmonics and freq * k < sr / 2:
        out += ((-1.0) ** (k + 1)) * np.sin(k * ph) / k
        k += 1
    return (2.0 / np.pi) * out


def triangle_wave(freq: float, dur: float, sr: int = SAMPLE_RATE,
                  harmonics: int = 0) -> np.ndarray:
    """
    三角波。
    harmonics == 0 : 理想三角波。
    harmonics >  0 : 奇数次倍音の加算合成。
    """
    if harmonics <= 0:
        n = int(round(dur * sr))
        t = np.arange(n, dtype=np.float64) * freq / sr
        return 2.0 * np.abs(2.0 * (t - np.floor(t + 0.5))) - 1.0
    ph = _phase(freq, dur, sr)
    out = np.zeros_like(ph)
    k = 1
    sign = 1.0
    while k <= harmonics and freq * k < sr / 2:
        out += sign * np.sin(k * ph) / (k * k)
        sign = -sign
        k += 2
    return (8.0 / (np.pi ** 2)) * out


# 波形名 -> 生成関数
WAVEFORMS = {
    "sine": sine_wave,
    "square": square_wave,
    "sawtooth": sawtooth_wave,
    "triangle": triangle_wave,
}


# ─────────────────────────────────────────────────────────────
# エンベロープ (ADSR)
# ─────────────────────────────────────────────────────────────
@dataclass
class ADSR:
    """音量エンベロープ。各値は秒、sustain は 0..1 のレベル。"""
    attack: float = 0.01
    decay: float = 0.05
    sustain: float = 0.7
    release: float = 0.08

    def apply(self, samples: np.ndarray, sr: int = SAMPLE_RATE) -> np.ndarray:
        n = len(samples)
        env = np.ones(n, dtype=np.float64)

        a = min(int(self.attack * sr), n)
        d = min(int(self.decay * sr), max(n - a, 0))
        r = min(int(self.release * sr), max(n - a - d, 0))
        s = max(n - a - d - r, 0)

        i = 0
        if a > 0:
            env[i:i + a] = np.linspace(0.0, 1.0, a, endpoint=False)
            i += a
        if d > 0:
            env[i:i + d] = np.linspace(1.0, self.sustain, d, endpoint=False)
            i += d
        if s > 0:
            env[i:i + s] = self.sustain
            i += s
        if r > 0:
            env[i:i + r] = np.linspace(self.sustain, 0.0, r, endpoint=True)
            i += r
        return samples * env


# ─────────────────────────────────────────────────────────────
# 音符 -> サンプル列
# ─────────────────────────────────────────────────────────────
def render_note(freq: float, dur: float, *,
                waveform: str = "sine",
                amp: float = DEFAULT_AMP,
                sr: int = SAMPLE_RATE,
                envelope: ADSR | None = None,
                harmonics: int = 0) -> np.ndarray:
    """1音を生成して float サンプル列 ([-1,1] 目安) を返す。"""
    if waveform not in WAVEFORMS:
        raise ValueError(f"未知の波形: {waveform!r} (選択肢: {list(WAVEFORMS)})")
    gen = WAVEFORMS[waveform]
    if waveform == "sine":
        wave_arr = gen(freq, dur, sr)
    else:
        wave_arr = gen(freq, dur, sr, harmonics=harmonics)
    wave_arr = wave_arr * amp
    if envelope is not None:
        wave_arr = envelope.apply(wave_arr, sr)
    return wave_arr


def render_silence(dur: float, sr: int = SAMPLE_RATE) -> np.ndarray:
    """無音 (休符) を返す。"""
    return np.zeros(int(round(dur * sr)), dtype=np.float64)


# ─────────────────────────────────────────────────────────────
# WAV 入出力
# ─────────────────────────────────────────────────────────────
def _float_to_int16(samples: np.ndarray) -> np.ndarray:
    """[-1,1] の float をクリップして 16bit PCM に変換。"""
    clipped = np.clip(samples, -1.0, 1.0)
    return (clipped * 32767.0).astype("<i2")


def write_wav(path: str, samples: np.ndarray, sr: int = SAMPLE_RATE) -> None:
    """モノラル 16bit PCM の WAV を書き出す。"""
    pcm = _float_to_int16(np.asarray(samples, dtype=np.float64))
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(SAMPLE_WIDTH)
        wf.setframerate(sr)
        wf.writeframes(pcm.tobytes())


def read_wav(path: str) -> Tuple[np.ndarray, int]:
    """WAV を読み込み (float サンプル列 [-1,1], サンプリング周波数) を返す。"""
    with wave.open(path, "rb") as wf:
        sr = wf.getframerate()
        nch = wf.getnchannels()
        width = wf.getsampwidth()
        nframes = wf.getnframes()
        raw = wf.readframes(nframes)

    if width != 2:
        raise ValueError(f"16bit PCM のみ対応 (この WAV は {width * 8}bit)")
    data = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    if nch > 1:
        data = data.reshape(-1, nch).mean(axis=1)  # ステレオ -> モノラル
    return data, sr


# ─────────────────────────────────────────────────────────────
# 解析: FFT ピッチ検出 / スペクトル / THD
# ─────────────────────────────────────────────────────────────
def detect_pitch(samples: np.ndarray, sr: int = SAMPLE_RATE) -> float:
    """
    FFT のピークを放物線補間してサブビン精度で基本周波数を推定する。
    無音なら 0.0 を返す。
    """
    x = np.asarray(samples, dtype=np.float64)
    if x.size == 0 or np.max(np.abs(x)) < 1e-9:
        return 0.0

    win = np.hanning(x.size)
    spec = np.abs(np.fft.rfft(x * win))
    if spec.size < 3:
        return 0.0

    k = int(np.argmax(spec[1:]) + 1)  # DC を避ける
    # 放物線補間 (対数振幅)
    if 1 <= k < spec.size - 1:
        a, b, c = (math.log(spec[k - 1] + 1e-12),
                   math.log(spec[k] + 1e-12),
                   math.log(spec[k + 1] + 1e-12))
        denom = (a - 2 * b + c)
        delta = 0.5 * (a - c) / denom if abs(denom) > 1e-12 else 0.0
    else:
        delta = 0.0
    return (k + delta) * sr / x.size


def spectrum(samples: np.ndarray, sr: int = SAMPLE_RATE
             ) -> Tuple[np.ndarray, np.ndarray]:
    """(周波数軸 [Hz], 正規化振幅) を返す。"""
    x = np.asarray(samples, dtype=np.float64)
    win = np.hanning(x.size)
    mag = np.abs(np.fft.rfft(x * win))
    freqs = np.fft.rfftfreq(x.size, d=1.0 / sr)
    peak = mag.max() if mag.size and mag.max() > 0 else 1.0
    return freqs, mag / peak


def total_harmonic_distortion(samples: np.ndarray, fundamental: float,
                              sr: int = SAMPLE_RATE,
                              n_harmonics: int = 10) -> float:
    """
    全高調波歪み率 THD = sqrt(Σ V_k^2) / V_1 を百分率で返す。
    fundamental は基本周波数 [Hz]。
    """
    x = np.asarray(samples, dtype=np.float64)
    win = np.hanning(x.size)
    mag = np.abs(np.fft.rfft(x * win))
    freqs = np.fft.rfftfreq(x.size, d=1.0 / sr)
    if mag.size == 0:
        return 0.0

    def amp_at(f: float) -> float:
        # ±半音幅ほどの窓内のピークを倍音振幅とみなす
        lo, hi = f * 0.97, f * 1.03
        sel = (freqs >= lo) & (freqs <= hi)
        return float(mag[sel].max()) if np.any(sel) else 0.0

    v1 = amp_at(fundamental)
    if v1 <= 0:
        return 0.0
    harm_sq = sum(amp_at(fundamental * k) ** 2 for k in range(2, n_harmonics + 1))
    return 100.0 * math.sqrt(harm_sq) / v1


def rms(samples: np.ndarray) -> float:
    """実効値 (RMS)。"""
    x = np.asarray(samples, dtype=np.float64)
    return float(np.sqrt(np.mean(x ** 2))) if x.size else 0.0


# ─────────────────────────────────────────────────────────────
# 楽譜 (音名+長さ) -> 連結サンプル列
# ─────────────────────────────────────────────────────────────
@dataclass
class Track:
    """音符列をまとめて 1 本の WAV に書き出すためのヘルパ。"""
    sr: int = SAMPLE_RATE
    waveform: str = "sine"
    amp: float = DEFAULT_AMP
    envelope: ADSR = field(default_factory=ADSR)
    gap: float = 0.0  # 音符間に挟む無音 [秒]
    harmonics: int = 0
    _parts: List[np.ndarray] = field(default_factory=list, repr=False)

    def add_note(self, note: str, dur: float) -> "Track":
        freq = note_to_freq(note)
        self._parts.append(render_note(
            freq, dur, waveform=self.waveform, amp=self.amp,
            sr=self.sr, envelope=self.envelope, harmonics=self.harmonics))
        if self.gap > 0:
            self._parts.append(render_silence(self.gap, self.sr))
        return self

    def add_rest(self, dur: float) -> "Track":
        self._parts.append(render_silence(dur, self.sr))
        return self

    def render(self) -> np.ndarray:
        if not self._parts:
            return np.zeros(0, dtype=np.float64)
        return np.concatenate(self._parts)

    def save(self, path: str) -> None:
        write_wav(path, self.render(), self.sr)
