# ボタンリアクションゲーム（Button Reaction Game）

## 概要
Raspberry PiとAQM0802A LCDディスプレイ、タクトスイッチを用いた反応速度ゲームです。LCDに表示された「＊」の位置に対応するボタンを素早く押すことでスコアを競います。

## 開発・動作環境

- **開発環境**：Windows（Git for Windows, VSCode など）
- **ターゲット環境**：Raspberry Pi OS（Linux）
- **言語**：C
- **ビルドツール**：gcc（Pi上でコンパイル）
- **通信方式**：I2C（AQM0802A制御）およびGPIO制御
- **依存ライブラリ**：なし（`wiringPi` 等の追加ライブラリは未使用）

## 使用ハードウェア
- Raspberry Pi（Linux）
- AQM0802A LCDモジュール（I2C接続）
- タクトスイッチ × 4（GPIO22〜25に接続）

## 実行方法（Windowsで作成 → Raspberry Piで実行）
1. このリポジトリのファイルを Raspberry Pi に転送（例：SCP、USB）
2. Pi上で以下のコマンドを実行：

   ```bash
   gcc main.c -o reaction_game
   sudo ./reaction_game
   ```

## ゲームの流れ
1. **コントラスト設定画面**
   - LCDに「contrast」「L:b, H:r」が表示されます
   - GPIO22（左ボタン）で暗めコントラスト（0x15）  
   - GPIO23（右ボタン）で明るめコントラスト（0x22）

2. **カウントダウン（3 → 2 → 1 → Start!）**

3. **ゲーム本編**
   - ランダムな位置に「＊」が表示されるので、対応するボタンを押下
   - 正解：スコア+1、不正解：スコア-1（押し続けても減点されません）

4. **スコア表示**
   - ゲーム終了後、2行目にスコアが表示されます

## 表示位置とGPIOピン対応

| LCD表示位置  | GPIOピン  | ボタン位置       |
|-------------|-----------|-----------------|
| 1行目 左端   | GPIO22    | (茶)ボタン      |
| 2行目 左端   | GPIO23    | (赤)ボタン      |
| 2行目 右端   | GPIO24    | (橙)ボタン      |
| 1行目 右端   | GPIO25    | (黄)ボタン      |

## 注意点
- Piの`i2c`機能を有効にしてください（`sudo raspi-config` → Interface Options → I2C → Enable）
- 実行時は `sudo` を付けて起動してください（GPIO/I2Cにアクセスするため）
