# 音階テスト / 波形テスト (Python, WAV)

音を **WAV ファイル**として生成し、FFT で「狙った音程・波形になっているか」を検証する
Python テストツール群です。

## 構成

| ファイル | 役割 |
|---|---|
| `tonelib.py` | 中核ライブラリ。音名↔周波数変換、波形生成、ADSRエンベロープ、WAV入出力、FFTピッチ検出・THD解析 |
| `scale_test.py` | **音階テスト**。音階を WAV 生成し、各音が音名どおりの周波数で鳴るかをセント単位で検証 |
| `waveform_test.py` | **波形テスト**。sine/square/sawtooth/triangle を生成し、ピッチ・RMS・THD を解析、波形/スペクトルを PNG 出力 |
| `audio_test.ipynb` | 上記をまとめた **Jupyter Notebook 版**。音をその場で再生 (`IPython.display.Audio`)、波形/スペクトルをインライン描画。`IPython`/`pandas` が無い環境では WAV 保存・print にフォールバック |

## 必要環境
- Python 3.10+
- numpy （必須）
- matplotlib （任意。無くても数値解析は動き、PNG描画だけスキップ）
- `wave`, `scipy` は不要（`wave` は標準ライブラリ、`scipy` は未使用）

## 使い方

### 音階テスト
```bash
python3 scale_test.py                         # Cメジャー(C4起点), 正弦波
python3 scale_test.py --scale chromatic --root A3
python3 scale_test.py --scale frog            # カエルの歌
python3 scale_test.py --waveform square --harmonics 15
```
選べる音階: `chromatic` / `major` / `minor` / `pentatonic` / `frog`

出力（`output/scale/<名前>/`）:
- `NN_<音名>.wav` … 個別音
- `_sequence_*.wav` … 音階全体を連結（試聴用）
- `result.csv` … 目標Hz・検出Hz・セント誤差・PASS/FAIL

### 波形テスト
```bash
python3 waveform_test.py                       # A4で4波形すべて
python3 waveform_test.py --note C4 --harmonics 20
python3 waveform_test.py --waveform sawtooth
```
出力（`output/waveform/`）:
- `<音名>_<波形>.wav` … 各波形
- `<波形>.png` … 時間波形＋振幅スペクトル
- `waveform_result.csv` … 検出Hz・誤差cent・RMS・Peak・THD%

## 判定基準
- ピッチ誤差が **±10 cent 以内** で PASS（`TOLERANCE_CENTS` で変更可）

## ライブラリとして使う例
```python
import tonelib as tl

print(tl.note_to_freq("A4"))      # 440.0
tl.write_wav("a.wav", tl.render_note(tl.note_to_freq("A4"), 1.0, waveform="square"))
data, sr = tl.read_wav("a.wav")
print(tl.detect_pitch(data, sr))  # 検出された基本周波数
```

## メモ
- 平均律 (A4 = 440 Hz) 基準。`tonelib.A4_FREQ` で変更可。
- `harmonics > 0` は奇数/全倍音の加算合成（バンドリミット）でエイリアスを抑制。
  `harmonics = 0` は理想波形（高域にエイリアスを含む）。
