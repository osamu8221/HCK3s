# Codes 統合説明

この `Codes` フォルダは、センサから最初に届く `START` をきっかけに、親機が決められた順番と拍間隔で各楽器を開始する構成です。

- `SensClient`: `START`、`STOP`、速度レベルを親機へTCP送信する
- `SyncMain`: 演奏順と拍数を管理し、`START`をunicast、`STOP`をbroadcastする
- `InstClient`: `START`で演奏を開始し、`STOP`で停止する

## 1. 演奏開始の流れ

1. 全子機が親機へTCP接続し、名前を登録する
2. センサが最初のダウンビートで親機へ `START` を1回送る
3. 親機が最初の楽器またはグループへ直ちに `START` をunicastする
4. 親機が現在のテンポで拍数を数え、8拍ごとに次のグループへ `START` をunicastする
5. `START` を受信した楽器だけが演奏を開始する
6. センサから `STOP` が届くと、親機が全子機へ `STOP` をbroadcastして順番と拍数を初期化する

演奏中の追加 `START` は無視します。STOP後に届いたSTARTは新しい演奏として受理し、最初のグループから同じ順番で開始します。

## 2. センサ制御担当

使用するコードは `Codes/SensClient` です。

- 親機への接続設定: `Codes/SensClient/config.h`
- 親機への送信処理: `Codes/SensClient/Sensfunc.cpp`

送信内容は次のとおりです。

- 演奏シーケンスを開始: `START`（最初の1回のみ）
- 演奏を停止: `STOP`
- 速度変更: `1`、`2`、`3`

輪唱開始用の `ROUND` は使用しません。演奏開始後は `sendStart()` を再度呼ばず、停止時に `sendStop()` を呼び出してください。STOP後は `sendStart()` で再開始できます。

シリアル確認では `s` がSTART、`x` がSTOP、`1`～`3`がLEVELです。

## 3. 親機担当

演奏開始順は `Codes/SyncMain/config.h` の `PLAYBACK_TARGET_GROUPS` で設定します。

```cpp
static const char* const PLAYBACK_TARGET_GROUPS[] = {
    "inst1",
    "inst2"
};
```

この設定では、センサ `START` の直後に `inst1`、8拍後に `inst2` を開始します。

同時に複数台を開始する場合は、1要素へカンマ区切りで指定します。

```cpp
static const char* const PLAYBACK_TARGET_GROUPS[] = {
    "inst1,inst2",
    "inst3"
};
```

この場合、センサ `START` の直後に `inst1` と `inst2`、8拍後に `inst3` を開始します。同じグループの各楽器にもUDPパケットは個別にunicastされます。

開始間隔は `START_INTERVAL_BEATS` で設定します。現在は8拍、つまり4拍子の2小節です。各グループは初回STARTから0拍、8拍、16拍……の位置で開始します。

親機はLEVELごとの代表BPMを使い、`millis()`の経過時間から累積拍数を計算します。

- LEVEL 1: 80 BPM
- LEVEL 2: 100 BPM
- LEVEL 3: 120 BPM
- START前にLEVEL未受信の場合: LEVEL 2

次の楽器の開始前にLEVELが変わった場合、変更前までの拍数を旧BPMで確定し、その後は新BPMで拍数を進めます。

STOPを受信すると、親機は拍数と送信対象を先頭へ戻し、LEVELを2へ戻します。TCP接続とREADY状態は維持するため、子機を再接続せずに次のSTARTを受け付けます。

親機は、楽器がTCP登録した接続のIPアドレスをunicast先として使用します。対象楽器が未接続の場合は送信できません。

## 4. 楽器担当

使用するコードは `Codes/InstClient` です。

- 楽器名: `Codes/InstClient/config.h` の `myname`
- 指示の受信処理: `Codes/InstClient/InstClient.ino`

各Arduinoで重複しない名前を設定してください。

```cpp
#define myname "inst1"
```

楽器側が受信する指示は次のとおりです。

- `START`: 演奏を開始する
- `STOP`: 演奏を停止し、START待機状態とLEVEL 2へ戻る
- `LEVEL:1`～`LEVEL:3`: 速度レベルを更新する

`START:inst1` のような対象名付き指示と `ROUND` は使用しません。送信先は親機側のunicast先IPで区別します。

## 5. 接続台数と通信設定

`Codes/SyncMain/config.h` の `numchild` は、親機がTCP登録を待つ子機の総数です。

- センサ1台 + 楽器1台: `2`
- センサ1台 + 楽器2台: `3`
- センサ1台 + 楽器3台: `4`

`PLAYBACK_TARGET_GROUPS` に書いた楽器をすべて含む値にしてください。現在の `numchild` は `2` のため、現在のまま実機接続できる楽器は `inst1` までです。

3種類のコードでSSID、パスワード、TCPポート、UDPポート、親機IPを一致させてください。現在はTCP `9000`、UDP `9001` です。

## 6. ログと確認項目

本番用イベントログは次の固定形式です。

- 親機: 開始グループへのunicast処理成功後に `RELAY:START`
- 楽器: `START` 受信時に `START`
- 親機: STOPのbroadcast処理後に `RELAY:STOP`
- 楽器: STOP受信時に `STOP`
- 親機: 速度中継後に `RELAY:LEVEL:1`～`RELAY:LEVEL:3`
- 楽器: 速度受信時に `LEVEL:1`～`LEVEL:3`

`RELAY` はUDP送信処理の成功を表し、楽器への到着保証ではありません。

統合時は、センサから1回だけ `START` を送り、設定順の楽器が8拍間隔で1回ずつ開始することを確認してください。STOP後は後続STARTが止まり、次のSTARTで先頭から再開することも確認してください。LEVEL 1では8拍が6秒、LEVEL 2では4.8秒、LEVEL 3では4秒です。
