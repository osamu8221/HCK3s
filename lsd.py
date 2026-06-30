#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
lsd.py ── 音のLSD検査 / 対数スペクトル距離 (ガントチャート 233 用、任意の音に流用可)

各 wav と「理想音」のスペクトル形状の Log-Spectral Distance(LSD, dB)を計算し、
波形(音色)が理想からどれだけ離れているか=どれだけ濁り(ノイズ/非調和成分)が
あるかを定量化する。小さいほど理想に近い(クリーン)。6音専用ではなく任意の wav に使える。

  LSD(dB) = mean_frames sqrt( mean_band ( S_test_dB - S_ref_dB )^2 )   (mel帯域・ピーク基準)

参照(理想音)の作り方は2通り:
  fit (既定): その録音自身の倍音の強さを測って「同じ倍音バランスのクリーンな純音和」を合成。
              録音の音量エンベロープも合わせる。→ LSDは「理想倍音音からの濁り(ノイズ・
              非調和成分)」を測る。きれいな単音なら小さく出る=合否判定が意味を持つ。
  saw      : 1/n^rolloff の汎用倍音列(ノコギリ波的)を参照にする。絶対的な音色基準。

合否しきい値:
  ・--calibrate を付けると、対象ファイル群の統計(中央値+k×MAD)からしきい値を自動決定。
    「良品だけが入ったフォルダ」を渡せば、外れ値(濁った音)だけを FAIL にできる。
  ・付けない場合は --threshold(既定10dB)で判定。

使い方:
    cd /Users/hayakawakazuki/3s/HCK && source .venv/bin/activate
    python lsd.py                       # このフォルダの *.wav を fit 参照で検査
    python lsd.py --calibrate           # 群の統計でしきい値を自動決定
    python lsd.py ド.wav 440Hz.wav       # ファイル指定
    python lsd.py --ideal saw --rolloff 1.0   # 汎用倍音列を参照にする
    python lsd.py --threshold 8         # しきい値を手動指定

依存: numpy, soundfile, librosa, note_utils.py(同じフォルダ)
"""

import argparse
import os
import sys

import numpy as np
import librosa

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import note_utils as nu

N_FFT = 2048
HOP = 512
TOP_DB = 60.0  # 各信号のピークから下に見るダイナミックレンジ(dB)


def amp_envelope(x, sr, win_ms=20.0):
    """振幅エンベロープ(移動RMS, 最大1に正規化)。理想音を録音の音量変化に合わせる用。"""
    w = max(1, int(sr * win_ms / 1000.0))
    env = np.sqrt(np.convolve(x ** 2, np.ones(w) / w, mode="same"))
    m = env.max()
    return env / m if m > 0 else env


def harmonic_amplitudes(x, sr, f0, harmonics, fmax):
    """録音の平均スペクトルから倍音 n*f0 の強さ A_n を測る(±3%窓でビブラート吸収)。"""
    mag = np.abs(librosa.stft(x.astype(np.float32), n_fft=N_FFT, hop_length=HOP))
    energy = mag.sum(axis=0)
    if energy.max() > 0:
        active = energy >= energy.max() * (10 ** (-30 / 20))
        avg = mag[:, active].mean(axis=1) if active.any() else mag.mean(axis=1)
    else:
        avg = mag.mean(axis=1)
    freqs = np.fft.rfftfreq(N_FFT, d=1.0 / sr)
    amps = np.zeros(harmonics)
    for n in range(1, harmonics + 1):
        fn = n * f0
        if fn >= min(fmax, sr / 2.0):
            break
        band = (freqs >= fn * 0.97) & (freqs <= fn * 1.03)
        if np.any(band):
            amps[n - 1] = float(avg[band].max())
    return amps


def synth_ideal(f0, n_samples, sr, harmonics, amps=None, rolloff=1.0, envelope=None):
    """基本周波数 f0 の理想音(純音の倍音和)を合成。
    amps が与えられればその倍音振幅、無ければ 1/n^rolloff を使う。"""
    t = np.arange(n_samples) / sr
    y = np.zeros(n_samples)
    for n in range(1, harmonics + 1):
        fn = n * f0
        if fn >= sr / 2.0:
            break
        a = amps[n - 1] if amps is not None else (1.0 / (n ** rolloff))
        y += a * np.sin(2.0 * np.pi * fn * t)
    if envelope is not None and len(envelope) == n_samples:
        y *= envelope                      # 録音の音量変化に合わせる
    else:
        fade = max(1, int(0.01 * sr))      # 端のクリック対策
        if 2 * fade < n_samples:
            ramp = np.linspace(0.0, 1.0, fade)
            y[:fade] *= ramp
            y[-fade:] *= ramp[::-1]
    peak = np.max(np.abs(y))
    if peak > 0:
        y *= 0.9 / peak
    return y


def mel_db(y, sr, n_mels, fmax):
    """mel パワースペクトログラムを「ピーク基準の dB(下限 -TOP_DB)」で返す。"""
    S = librosa.feature.melspectrogram(y=y.astype(np.float32), sr=sr,
                                       n_fft=N_FFT, hop_length=HOP,
                                       n_mels=n_mels, fmax=fmax, power=2.0)
    return librosa.power_to_db(S, ref=np.max, top_db=TOP_DB)


def compute_lsd(test, ref, sr, n_mels, fmax, active_db):
    """被検 test と参照 ref(同じ長さ)の mel-LSD(dB) と対象フレーム数を返す。"""
    St = mel_db(test, sr, n_mels, fmax)
    Sr = mel_db(ref, sr, n_mels, fmax)
    f = min(St.shape[1], Sr.shape[1])
    St, Sr = St[:, :f], Sr[:, :f]

    active = St.max(axis=0) >= (St.max() - active_db)   # 発音区間のみ
    if not np.any(active):
        active = np.ones(f, dtype=bool)
    St, Sr = St[:, active], Sr[:, active]

    per_frame = np.sqrt(np.mean((St - Sr) ** 2, axis=0))
    return float(np.mean(per_frame)), int(St.shape[1])


def build_ref(x, sr, f0, args):
    if args.ideal == "fit":
        amps = harmonic_amplitudes(x, sr, f0, args.harmonics, args.fmax)
        env = amp_envelope(x, sr)
        return synth_ideal(f0, len(x), sr, args.harmonics, amps=amps, envelope=env)
    return synth_ideal(f0, len(x), sr, args.harmonics, rolloff=args.rolloff)


def main():
    ap = argparse.ArgumentParser(description="音のLSD検査(対数スペクトル距離)")
    ap.add_argument("files", nargs="*", help="検査する wav(省略時は --dir かこのフォルダを走査)")
    ap.add_argument("--dir", default=None, help="wav のあるフォルダ")
    ap.add_argument("--ideal", choices=["fit", "saw"], default="fit",
                    help="参照の作り方。fit=録音の倍音から合成(既定) / saw=1/n^rolloff汎用")
    ap.add_argument("--harmonics", type=int, default=20, help="倍音数。既定20")
    ap.add_argument("--rolloff", type=float, default=1.0,
                    help="saw 時の倍音減衰 1/n^rolloff。既定1.0")
    ap.add_argument("--n-mels", type=int, default=48, help="mel帯域数。既定48")
    ap.add_argument("--fmax", type=float, default=6000.0, help="解析上限周波数Hz。既定6000")
    ap.add_argument("--threshold", type=float, default=10.0,
                    help="合否しきい値(dB)。既定10.0(fit参照向け)")
    ap.add_argument("--calibrate", action="store_true",
                    help="対象群の統計(中央値+k×MAD)でしきい値を自動決定する")
    ap.add_argument("--calib-k", type=float, default=3.0,
                    help="--calibrate の係数 k。既定3.0")
    ap.add_argument("--active-db", type=float, default=30.0,
                    help="発音区間とみなすレベル幅(ピークから-N dB)。既定30")
    ap.add_argument("--octave", type=int, default=5,
                    help="--target-pitch 時、名前にオクターブが無い音名の既定オクターブ")
    ap.add_argument("--target-pitch", action="store_true",
                    help="参照を実測ピッチでなくファイル名の音(C5等)で合成する")
    args = ap.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    wavs = nu.discover_wavs(args.files, args.dir, script_dir)

    # --- 先に全ファイルの LSD を計算する(しきい値の自動校正のため) ---
    results = []  # (f0, name, label, f0disp, lsd, nframes, err)
    for path in wavs:
        name = os.path.basename(path)
        try:
            x, sr = nu.load_mono(path)
        except Exception as e:
            results.append((1e18, name, "-", None, None, None, f"読込失敗:{e}"))
            continue
        if args.target_pitch:
            tf, label = nu.parse_note(name, default_octave=args.octave)
            f0 = tf if tf else nu.estimate_f0(x, sr)
            if not tf:
                label = "(実測)"
        else:
            f0 = nu.estimate_f0(x, sr)
            label = "実測"
        if not f0 or f0 <= 0:
            results.append((1e18, name, label, None, None, None, "f0推定不可"))
            continue
        ref = build_ref(x, sr, f0, args)
        lsd, nframes = compute_lsd(x, ref, sr, args.n_mels, args.fmax, args.active_db)
        results.append((f0, name, label, f0, lsd, nframes, None))

    valid = [r[4] for r in results if r[4] is not None]

    # しきい値の決定(自動校正 or 手動)
    if args.calibrate and valid:
        med = float(np.median(valid))
        mad = float(np.median(np.abs(np.array(valid) - med))) or 1e-6
        threshold = med + args.calib_k * 1.4826 * mad  # MAD→標準偏差換算
        thr_src = f"自動(中央値{med:.2f}+{args.calib_k}×MAD)"
    else:
        threshold = args.threshold
        thr_src = "手動"

    results.sort(key=lambda r: r[0])

    print("=" * 74)
    print(" 音のLSD検査 (lsd) ── 各 wav と理想音のスペクトル距離")
    print(f"   対象 : {len(wavs)} ファイル"
          f"{' (' + (args.dir or script_dir) + ')' if not args.files else ''}")
    print(f"   参照 : {'録音の倍音から合成(fit)' if args.ideal == 'fit' else f'1/n^{args.rolloff} (saw)'}"
          f" / 倍音{args.harmonics}本 / "
          f"{'ファイル名の音に固定' if args.target_pitch else '実測ピッチ整合'}")
    print(f"   解析 : mel{args.n_mels}帯域 / 上限{args.fmax:.0f}Hz / レンジ{TOP_DB:.0f}dB")
    print(f"   合否 : しきい値 {threshold:.2f} dB 以下で PASS [{thr_src}]")
    print("=" * 74)
    print(f"{'ファイル':<14}{'参照':>7}{'参照f0Hz':>9}{'LSD(dB)':>10}{'frames':>8}  判定")
    print("-" * 74)

    lsds = []
    found = 0
    all_pass = True
    for f0key, name, label, f0, lsd, nframes, err in results:
        if lsd is None:
            print(f"{name:<14}{label:>7}{'':>9}{'':>10}{'':>8}  {err}")
            all_pass = False
            continue
        found += 1
        lsds.append(lsd)
        ok = lsd <= threshold
        all_pass = all_pass and ok
        print(f"{name:<14}{label:>7}{f0:>9.1f}{lsd:>10.2f}{nframes:>8}  "
              f"{'PASS' if ok else 'FAIL'}")

    print("-" * 74)
    if found == 0:
        print("判定: 検査できる wav がありませんでした。引数か --dir を確認してください。")
        return 2
    print(f"平均LSD: {np.mean(lsds):.2f} dB   "
          f"(最小 {np.min(lsds):.2f} / 最大 {np.max(lsds):.2f})")
    print("判定: " + ("全音 PASS ✓" if all_pass else "FAIL あり ✗"))
    if not args.calibrate:
        print("※ しきい値は --calibrate で群の統計から自動決定もできます。")
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
