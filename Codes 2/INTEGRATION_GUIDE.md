# Codes 統合説明

この `Codes` フォルダは、以下の 3 つを前提にしています。

- `SensClient`: センサ側。親機に `START` `ROUND` `1` `2` `3` を TCP 送信する
- `SyncMain`: 親機側。センサから受けた指示を楽器側へ UDP 中継する
- `InstClient`: 楽器側。親機から受けた指示のうち、自分宛てのものだけ反応する

## 1. 役割分担

### センサ制御担当

センサ担当が使うのは `Codes/SensClient` です。

- 親機への接続設定は `Codes/SensClient/config.h`
- 親機へ送る指示は `Codes/SensClient/Sensfunc.cpp`

今の送信内容は次です。

- 演奏開始: `START`
- 輪唱追加: `ROUND`
- 速度変更: `1` `2` `3`

重要なのは、センサ側は `inst1` や `inst2` を直接指定しないことです。どの楽器を鳴らすかは親機側で決めます。

### 楽器担当

楽器担当が使うのは `Codes/InstClient` です。

- 楽器名の設定は `Codes/InstClient/config.h`
- 受信処理は `Codes/InstClient/InstClient.ino`

楽器側が受ける UDP 文字列は次です。

- `START`
- `START:inst1`
- `START:inst1,inst2`
- `ROUND`
- `ROUND:inst2`
- `ROUND:inst2,inst3`
- `LEVEL:1`
- `LEVEL:2`
- `LEVEL:3`

今の実装では、`START` と `ROUND` は次のように動きます。

- `START` だけなら全楽器が反応
- `START:inst1` なら `myname` が `inst1` の楽器だけ反応
- `ROUND:inst2,inst3` なら `inst2` と `inst3` だけ反応
- `LEVEL:n` は全楽器が受け取って更新

楽器を増やす場合は、各 Arduino の `Codes/InstClient/config.h` の `myname` を重複しない名前に変えてください。

例:

- 1 台目: `#define myname "inst1"`
- 2 台目: `#define myname "inst2"`

## 2. 親機側で決めること

親機側の調整場所は `Codes/SyncMain/config.h` です。

```cpp
static const char* const PLAYBACK_TARGET_GROUPS[] = {
    "inst1",
    "inst2"
};
```

この配列の意味は次です。

- 1 要素目: 最初の `START` で鳴らす対象
- 2 要素目以降: `ROUND` が来るたびに順番に鳴らす対象

現在の `START:inst1` や `ROUND:inst2` の指定方法は、統合を進めるための暫定ルールです。輪唱対象の分け方や、途中参加時のタイミング合わせの仕様が明確になったら、この文書とコード内コメントを更新してください。

今の設定では次の動きになります。

1. センサが `START` を送る
2. 親機が `START:inst1` を送る
3. センサが `ROUND` を送る
4. 親機が `ROUND:inst2` を送る

同時に複数台を鳴らしたい場合は 1 要素にカンマ区切りで書けます。

```cpp
static const char* const PLAYBACK_TARGET_GROUPS[] = {
    "inst1,inst2",
    "inst3"
};
```

この場合は、最初の `START` で `inst1` と `inst2` が同時に反応します。

## 3. 統合時の注意

### 接続台数

`Codes/SyncMain/config.h` の `numchild` は、親機が TCP 登録を待つ台数です。

- `sens` 1 台 + `inst1` 1 台なら `2`
- `sens` 1 台 + `inst1` 1 台 + `inst2` 1 台なら `3`

`PLAYBACK_TARGET_GROUPS` に `inst2` を書いても、`numchild` が足りないと `inst2` は接続完了できません。

### Wi-Fi とポート

3 つのコードで次をそろえてください。

- SSID
- パスワード
- `TCP_PORT`
- `UDP_PORT`
- 親機 IP

今の本番用設定は、親機 AP に接続し、TCP `9000`、UDP `9001` を使う構成です。

### ログ

本番用ログは外部処理で判定しやすいように単純化してあります。

- 楽器側: `START` `ROUND` `LEVEL:1` `LEVEL:2` `LEVEL:3`
- 親機側: `RELAY:START` `RELAY:ROUND` `RELAY:LEVEL:1` から `RELAY:LEVEL:3`

`RELAY` は UDP 中継を実行したことを表します。子機への到達保証ではありません。

## 4. 担当ごとの最小確認項目

### センサ担当

- `SensClient` から `START` `ROUND` `1` `2` `3` を送れること
- 親機に TCP 接続できること

### 楽器担当

- `InstClient` で自分の `myname` が正しいこと
- 自分宛ての `START:...` と `ROUND:...` だけ反応すること
- `LEVEL:1` から `LEVEL:3` を受けて状態更新できること

### 統合担当

- 全子機接続後に親機から `READY` が返ること
- `START` 後に親機で `RELAY:START`、対象楽器で `START` が 1 回ずつ出ること
- `ROUND` 後に親機で `RELAY:ROUND`、対象楽器で `ROUND` が 1 回出ること
