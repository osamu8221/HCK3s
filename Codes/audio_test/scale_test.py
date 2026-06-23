# -*- coding: utf-8 -*-
"""
scale_test.py ── 音階テスト

やること:
    1. 指定した音階 (クロマチック / 長調 / 短調 / カエルの歌) の各音を WAV 生成。
    2. 生成した WAV を FFT で解析し、狙った周波数 (音名どおり) で
       鳴っているかをセント単位で検証 (PASS / FAIL)。
    3. 音階全体を 1 本の WAV に連結 (試聴用)。

使い方:
    python3 scale_test.py                  # 既定 (Cメジャー, C4起点)
    python3 scale_test.py --scale chromatic --root C4
    python3 scale_test.py --scale frog     # カエルの歌
    python3 scale_test.py --waveform square --harmonics 15
出力:
    output/scale/<name>/  に個別音 WAV と連結 WAV、結果表 (CSV) を保存。
"""

from __future__ import annotations

import argparse
import csv
import os
from typing import List, Tuple

import tonelib as tl

# 音階パターン (ルートからの半音間隔)
SCALE_PATTERNS = {
    "chromatic": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],
    "major":     [0, 2, 4, 5, 7, 9, 11, 12],          # 全全半全全全半
    "minor":     [0, 2, 3, 5, 7, 8, 10, 12],          # 自然短音階
    "pentatonic": [0, 2, 4, 7, 9, 12],                # 長ペンタトニック
}

# カエルの歌 (ドレミファミレド…) を (音名, 長さ[拍]) で定義
FROG_SONG: List[Tuple[str, float]] = [
    ("C4", 1), ("D4", 1), ("E4", 1), ("F4", 1), ("E4", 1), ("D4", 1), ("C4", 1), ("REST", 1),
    ("E4", 1), ("F4", 1), ("G4", 1), ("A4", 1), ("G4", 1), ("F4", 1), ("E4", 1), ("REST", 1),
    ("C4", 1), ("REST", 1), ("C4", 1), ("REST", 1), ("C4", 1), ("REST", 1), ("C4", 1), ("REST", 1),
    ("C4", 0.5), ("C4", 0.5), ("D4", 0.5), ("D4", 0.5), ("E4", 0.5), ("E4", 0.5), ("F4", 0.5), ("F4", 0.5),
    ("E4", 1), ("D4", 1), ("C4", 1),
]

TOLERANCE_CENTS = 10.0  # 合否のしきい値 (±10セント = 約0.6%)


def build_scale_notes(scale: str, root: str) -> List[Tuple[str, float]]:
    """音階名とルート音から (音名, 長さ[拍]) のリストを作る。"""
    if scale == "frog":
        return FROG_SONG
    if scale not in SCALE_PATTERNS:
        raise ValueError(f"未知の音階: {scale} (選択肢: {list(SCALE_PATTERNS)} + frog)")
    root_midi = tl.note_to_midi(root)
    notes = []
    for semi in SCALE_PATTERNS[scale]:
        name, cents = tl.freq_to_note(tl.midi_to_freq(root_midi + semi))
        notes.append((name, 1.0))
    return notes


def run(scale: str, root: str, waveform: str, harmonics: int,
        bpm: float, outdir: str) -> int:
    sr = tl.SAMPLE_RATE
    beat = 60.0 / bpm  # 1拍の秒数
    name = scale if scale == "frog" else f"{scale}_{root}"
    dest = os.path.join(outdir, name)
    os.makedirs(dest, exist_ok=True)

    notes = build_scale_notes(scale, root)
    env = tl.ADSR(attack=0.01, decay=0.04, sustain=0.75, release=0.06)
    track = tl.Track(sr=sr, waveform=waveform, envelope=env,
                     harmonics=harmonics, gap=0.0)

    results = []
    print(f"\n=== 音階テスト: {name}  (波形={waveform}, BPM={bpm}) ===")
    print(f"{'#':>2} {'音名':<5} {'目標Hz':>9} {'検出Hz':>9} "
          f"{'誤差cent':>8} {'判定':>5}")

    idx = 0
    for note, length in notes:
        dur = length * beat
        if note == "REST":
            track.add_rest(dur)
            continue
        idx += 1
        target = tl.note_to_freq(note)

        # 個別音を生成 -> WAV 保存 -> 読み戻して FFT 検証
        samples = tl.render_note(target, dur, waveform=waveform,
                                 sr=sr, envelope=env, harmonics=harmonics)
        note_path = os.path.join(dest, f"{idx:02d}_{note}.wav")
        tl.write_wav(note_path, samples, sr)

        loaded, _ = tl.read_wav(note_path)
        detected = tl.detect_pitch(loaded, sr)
        cents = 1200.0 * __import__("math").log2(detected / target) if detected > 0 else float("nan")
        ok = abs(cents) <= TOLERANCE_CENTS if detected > 0 else False
        verdict = "PASS" if ok else "FAIL"
        print(f"{idx:>2} {note:<5} {target:>9.2f} {detected:>9.2f} "
              f"{cents:>+8.2f} {verdict:>5}")
        results.append((idx, note, target, detected, cents, verdict))

        track.add_note(note, dur)

    # 連結 WAV (試聴用)
    seq_path = os.path.join(dest, f"_sequence_{name}.wav")
    track.save(seq_path)

    # 結果を CSV 保存
    csv_path = os.path.join(dest, "result.csv")
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["index", "note", "target_hz", "detected_hz", "cents", "verdict"])
        w.writerows(results)

    passed = sum(1 for r in results if r[5] == "PASS")
    print(f"\n結果: {passed}/{len(results)} PASS  "
          f"(しきい値 ±{TOLERANCE_CENTS:.0f} cent)")
    print(f"連結WAV : {seq_path}")
    print(f"結果CSV : {csv_path}")
    return 0 if passed == len(results) else 1


def main() -> None:
    p = argparse.ArgumentParser(description="音階テスト (WAV生成 + FFT検証)")
    p.add_argument("--scale", default="major",
                   choices=list(SCALE_PATTERNS) + ["frog"],
                   help="音階の種類 (frog=カエルの歌)")
    p.add_argument("--root", default="C4", help="ルート音 (例 C4, A3)")
    p.add_argument("--waveform", default="sine", choices=list(tl.WAVEFORMS))
    p.add_argument("--harmonics", type=int, default=0,
                   help="加算合成の倍音数 (0=理想波形)")
    p.add_argument("--bpm", type=float, default=120.0)
    p.add_argument("--outdir", default="output/scale")
    args = p.parse_args()
    raise SystemExit(run(args.scale, args.root, args.waveform,
                         args.harmonics, args.bpm, args.outdir))


if __name__ == "__main__":
    main()
