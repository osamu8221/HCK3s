#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
onkai.py ── 音階のテスト / ピッチ検査 (ガントチャート 231 用、任意の音に流用可)

wav の基本周波数(ピッチ)を推定し、期待する音と合っているかを判定する客観テスト。
6音(ド〜ラ)専用ではなく、任意の wav・任意の音名に流用できる:
  ・期待周波数は【ファイル名から自動認識】する(note_utils.parse_note):
        日本語  : ド レ ミ ファ ソ ラ シ (＃やオクターブ数字も可: 例 ラ4, ド＃5)
        科学表記: C5  A4  F#3  Bb2
        明示    : 440Hz  523.2hz
    名前から読めない場合は「最も近い平均律の音」を期待値として“調律ズレ”を見る。
  ・判定は音名(ドレミ)の一致で行い、オクターブのズレは情報表示(--strict-octave で必須化)。

使い方:
    cd /Users/hayakawakazuki/3s/HCK && source .venv/bin/activate
    python onkai.py                      # このフォルダの *.wav を全部検査
    python onkai.py ド.wav ラ.wav         # ファイルを指定して検査
    python onkai.py --dir /path/to/wav   # 別フォルダの *.wav を検査
    python onkai.py --tol 30             # 合否しきい値(セント)。既定 50
    python onkai.py --octave 4           # 名前にオクターブが無い音名の既定オクターブ
    python onkai.py --strict-octave      # オクターブ一致も合否条件にする

依存: numpy, soundfile, librosa, note_utils.py(同じフォルダ)
"""

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import note_utils as nu


def main():
    ap = argparse.ArgumentParser(description="音階のテスト(ピッチ検査)")
    ap.add_argument("files", nargs="*", help="検査する wav(省略時は --dir かこのフォルダを走査)")
    ap.add_argument("--dir", default=None, help="wav のあるフォルダ")
    ap.add_argument("--tol", type=float, default=50.0, help="合否しきい値(セント)。既定 50")
    ap.add_argument("--octave", type=int, default=5,
                    help="名前にオクターブが無い音名の既定オクターブ。既定 5(C5〜)")
    ap.add_argument("--strict-octave", action="store_true",
                    help="期待オクターブとの一致も合否条件にする")
    args = ap.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    wavs = nu.discover_wavs(args.files, args.dir, script_dir)

    print("=" * 74)
    print(" 音階のテスト (onkai) ── 各 wav のピッチを期待する音と照合")
    print(f"   対象 : {len(wavs)} ファイル"
          f"{' (' + (args.dir or script_dir) + ')' if not args.files else ''}")
    print(f"   許容 : ±{args.tol:.0f} セント / 既定オクターブ {args.octave} / "
          f"オクターブ一致: {'必須' if args.strict_octave else '情報のみ'}")
    print("=" * 74)
    print(f"{'ファイル':<14}{'期待':>7}{'期待Hz':>9}{'実測Hz':>9}{'近い音':>7}"
          f"{'誤差cent':>9}{'oct差':>6}  判定")
    print("-" * 74)

    rows = []
    for path in wavs:
        name = os.path.basename(path)
        try:
            x, sr = nu.load_mono(path)
        except Exception as e:
            rows.append((1e18, name, "-", None, None, None, None, f"読込失敗:{e}"))
            continue
        f0 = nu.estimate_f0(x, sr)
        if not f0 or f0 <= 0:
            rows.append((1e18, name, "-", None, None, None, None, "推定不可"))
            continue

        target, label = nu.parse_note(name, default_octave=args.octave)
        auto = target is None
        if auto:                     # 名前から読めない → 最も近い平均律音を期待値に
            target = nu.midi_to_freq(round(69 + 12 * np.log2(f0 / 440.0)))
            label = "(自動)"
        oct_off, err = nu.fold_octave(f0, target)
        class_ok = abs(err) <= args.tol
        ok = class_ok and (oct_off == 0 if args.strict_octave else True)
        mark = "PASS" if ok else ("FAIL" if not class_ok else "FAIL(oct)")
        rows.append((f0, name, label, target, f0, nu.freq_to_name(f0),
                     (oct_off, err), mark))

    rows.sort(key=lambda r: r[0])    # 周波数の低い順に並べる(音階が見やすい)
    all_pass = True
    found = 0
    oct_offsets = []
    for _, name, label, target, f0, near, oe, mark in rows:
        if f0 is None:
            print(f"{name:<14}{label:>7}{'':>9}{'':>9}{'':>7}{'':>9}{'':>6}  {mark}")
            all_pass = False
            continue
        found += 1
        oct_off, err = oe
        oct_offsets.append(oct_off)
        all_pass = all_pass and mark == "PASS"
        print(f"{name:<14}{label:>7}{target:>9.1f}{f0:>9.1f}{near:>7}"
              f"{err:>+9.1f}{oct_off:>+6d}  {mark}")

    print("-" * 74)
    if found == 0:
        print("判定: 検査できる wav がありませんでした。引数か --dir を確認してください。")
        return 2
    print("判定: " + ("音階OK ✓" if all_pass else "FAIL あり ✗"))
    named = [o for o in oct_offsets]
    if named and all(o == named[0] for o in named) and named[0] != 0:
        d = named[0]
        print(f"※ 全音そろって期待より {abs(d)} オクターブ{'低い' if d < 0 else '高い'}。"
              f"音階の並びは正しいので、期待オクターブ(--octave)か音源のどちらを正とするか確認を。")
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
