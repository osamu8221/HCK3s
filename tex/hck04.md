```mermaid
flowchart TD
    %% 全体構成
    User((鑑賞者)) -->|指揮棒を振る| Parent[親機: 加速度センサー]
    Parent -->|無線送信: 音楽再生等の指示| Child[子機: 受信処理]

    %% 担当箇所：子機側の処理
    subgraph TargetScope ["担当箇所: 子機側の基本設計・詳細設計"]
        Child --> Arduino[Arduinoによるデータ解析]
        
        %% データ解析アルゴリズム
        subgraph DataAnalysis ["データ解析アルゴリズム"]
            Arduino --> ProcessingTx[Processingへのデータ送信・バッファ格納]
            ProcessingTx --> Frame[フレーム構造の定義と指示の抽出]
            Frame --> BitPattern[特定のビットパターン識別<br>内容とパターンの決定]
        end

        %% タイミング制御
        subgraph TimingControl ["音楽再生と視覚演出のタイミング制御"]
            BitPattern --> TimerSet[タイマーの設定]
            TimerSet -->|タイマー発火| Sync[タイミングの同期]
            Sync --> Music[Processing: 音楽の再生]
            Sync --> LED[Arduino: LEDマトリクス演出]
        end
    end