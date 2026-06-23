# -*- coding: utf-8 -*-
"""
waveform_test.py ── 音の波形テスト

やること:
    1. 指定音 (既定 A4=440Hz) について sine / square / sawtooth / triangle
       の 4 波形を WAV 生成。
    2. 各 WAV を解析:
         - 基本周波数 (FFT ピッチ検出) と狙い値との誤差
         - RMS / ピーク値
         - THD (全高調波歪み率)
    3. matplotlib があれば、時間波形 (先頭数周期) と振幅スペクトルを
       PNG に保存。無ければ数値解析のみ。

使い方:
    python3 waveform_test.py                 # A4, 全波形
    python3 waveform_test.py --note C4 --harmonics 20
    python3 waveform_test.py --waveform square
出力:
    output/waveform/  に WAV / PNG / 解析CSV を保存。
"""

from __future__ import annotations

import argparse
import csv
import math
import os
from typing import List

import numpy as np

import tonelib as tl

try:
    import matplotlib
    matplotlib.use("Agg")  # 画面なしでも PNG 出力できるように
    import matplotlib.pyplot as plt
    HAVE_MPL = True
except Exception:  # pragma: no cover - 環境依存
    HAVE_MPL = False

TOLERANCE_CENTS = 10.0


def analyze_one(name: str, samples: np.ndarray, target: float,
                sr: int) -> dict:
    """1 波形を解析して結果 dict を返す。"""
    detected = tl.detect_pitch(samples, sr)
    cents = 1200.0 * math.log2(detected / target) if detected > 0 else float("nan")
    return {
        "waveform": name,
        "target_hz": target,
        "detected_hz": detected,
        "cents": cents,
        "pass": abs(cents) <= TOLERANCE_CENTS if detected > 0 else False,
        "rms": tl.rms(samples),
        "peak": float(np.max(np.abs(samples))) if samples.size else 0.0,
        "thd_percent": tl.total_harmonic_distortion(samples, target, sr),
    }


def plot_waveform(name: str, samples: np.ndarray, target: float,
                  sr: int, dest: str) -> str:
    """時間波形 + スペクトルを PNG 保存。matplotlib 必須。"""
    cycles = 3
    n_show = min(len(samples), int(round(cycles * sr / target)))
    t_ms = np.arange(n_show) / sr * 1000.0
    freqs, mag = tl.spectrum(samples, sr)
    fmax = target * 12  # 表示上限

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(9, 6))
    ax1.plot(t_ms, samples[:n_show], color="#1f77b4")
    ax1.set_title(f"{name} waveform  (target {target:.1f} Hz)")
    ax1.set_xlabel("time [ms]")
    ax1.set_ylabel("amplitude")
    ax1.grid(True, alpha=0.3)

    sel = freqs <= fmax
    ax2.plot(freqs[sel], mag[sel], color="#d62728")
    ax2.set_title("amplitude spectrum")
    ax2.set_xlabel("frequency [Hz]")
    ax2.set_ylabel("normalized magnitude")
    ax2.grid(True, alpha=0.3)

    fig.tight_layout()
    png_path = os.path.join(dest, f"{name}.png")
    fig.savefig(png_path, dpi=110)
    plt.close(fig)
    return png_path


def run(note: str, waveforms: List[str], harmonics: int,
        dur: float, outdir: str) -> int:
    sr = tl.SAMPLE_RATE
    os.makedirs(outdir, exist_ok=True)
    target = tl.note_to_freq(note)
    env = None  # 波形そのものを見たいのでエンベロープなし

    print(f"\n=== 波形テスト: note={note} ({target:.2f} Hz), "
          f"harmonics={harmonics} ===")
    print(f"{'波形':<9} {'検出Hz':>9} {'誤差cent':>8} "
          f"{'RMS':>6} {'Peak':>6} {'THD%':>7} {'判定':>5}")

    results = []
    for wf in waveforms:
        samples = tl.render_note(target, dur, waveform=wf, amp=tl.DEFAULT_AMP,
                                 sr=sr, envelope=env, harmonics=harmonics)
        wav_path = os.path.join(outdir, f"{note}_{wf}.wav")
        tl.write_wav(wav_path, samples, sr)

        loaded, _ = tl.read_wav(wav_path)
        res = analyze_one(wf, loaded, target, sr)
        results.append(res)

        if HAVE_MPL:
            plot_waveform(wf, loaded, target, sr, outdir)

        print(f"{wf:<9} {res['detected_hz']:>9.2f} {res['cents']:>+8.2f} "
              f"{res['rms']:>6.3f} {res['peak']:>6.3f} "
              f"{res['thd_percent']:>7.2f} "
              f"{'PASS' if res['pass'] else 'FAIL':>5}")

    # CSV 保存
    csv_path = os.path.join(outdir, "waveform_result.csv")
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(results[0].keys()))
        w.writeheader()
        w.writerows(results)

    passed = sum(1 for r in results if r["pass"])
    print(f"\n結果: {passed}/{len(results)} PASS  (ピッチ ±{TOLERANCE_CENTS:.0f} cent)")
    print(f"出力先 : {os.path.abspath(outdir)}")
    if not HAVE_MPL:
        print("※ matplotlib が無いため PNG 描画はスキップしました。")
    return 0 if passed == len(results) else 1


def main() -> None:
    p = argparse.ArgumentParser(description="波形テスト (WAV生成 + スペクトル解析)")
    p.add_argument("--note", default="A4", help="基準音 (例 A4, C4)")
    p.add_argument("--waveform", default="all",
                   choices=list(tl.WAVEFORMS) + ["all"])
    p.add_argument("--harmonics", type=int, default=0,
                   help="加算合成の倍音数 (0=理想波形)")
    p.add_argument("--dur", type=float, default=1.0, help="長さ [秒]")
    p.add_argument("--outdir", default="output/waveform")
    args = p.parse_args()

    wfs = list(tl.WAVEFORMS) if args.waveform == "all" else [args.waveform]
    raise SystemExit(run(args.note, wfs, args.harmonics, args.dur, args.outdir))


if __name__ == "__main__":
    main()
