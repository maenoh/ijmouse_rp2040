```markdown
# RP2040-Zero USB Mouse to UART Converter (ijmouse_digit_mode)

RP2040-Zero（または RP2040 マイコンボード）の Pico-PIO-USB 機能を活用し、一般的な USB HID マウスの操作信号を 115200 bps の TTL UART シリアル信号へリアルタイム変換するファームウェアです。

こどもパソコン **IchigoJam** 等のシリアル入力（`INKEY()`）で直接受け取れる `ijmouse_digit_mode` 仕様に準拠しています。

---

## 特徴 (Features)

* **安価な構成:** RP2040-Zero（数百円）と USB Type-A メスコネクタのみで製作可能
* **プロトコル変換:** USB HID マウスのボタン・移動・ホイール操作を IchigoJam 用コントロールコードへ即時変換
* **簡単書き込み:** 開発環境なしでも、ビルド済み `.uf2` ファイルをドラッグ＆ドロップするだけで書き込み完了

---

## 接続ピン配置 (Hardware Wiring)

RP2040-Zero に USB Type-A メスコネクタおよび接続先機器（IchigoJam等）を以下のように接続します。

```text
 RP2040-Zero                        USB Type-A Connector (Host)
┌────────────┐                     ┌──────────────────────────┐
│  GP4 (DP)  ├──────[ 27Ω ]────────┤ Pin 3 (D+)               │
│  GP5 (DM)  ├──────[ 27Ω ]────────┤ Pin 2 (D-)               │
│  5V        ├─────────────────────┤ Pin 1 (VBUS 5V)          │
│  GND       ├─────────────────────┤ Pin 4 (GND)              │
└────────────┘                     └──────────────────────────┘

 RP2040-Zero                        Target Microcontroller (e.g. IchigoJam)
┌────────────┐                     ┌──────────────────────────┐
│  GP0 (TX)  ├─────────────────────┤ RXD (Serial Input)       │
│  GND       ├─────────────────────┤ GND                      │
└────────────┘                     └──────────────────────────┘

```

> **注記:** USB D+/D- 信号線にはインピーダンス整合用に **27Ω** のダンピング抵抗を直列に挟むことを推奨します。

---

## 出力データ仕様 (Serial Protocol Specification)

* **通信速度:** `115200 bps`
* **データ形式:** 8 bit, Stop bit 1, Parity None

### 1. ボタン状態（変化時のみ出力）

ボタンのクリック状態が変化した際、ボタン状態の合成値を ASCII 文字 `'0'` 〜 `'7'` (1バイト) で出力します。

* Bit 0 (1): 左ボタン
* Bit 1 (2): 右ボタン
* Bit 2 (4): 中ボタン (ホイール押し込み)

| 送信文字 | Hex | ボタン状態 |
| --- | --- | --- |
| `'0'` | `0x30` | 全ボタン離されている |
| `'1'` | `0x31` | 左ボタンのみ押下 |
| `'2'` | `0x32` | 右ボタンのみ押下 |
| `'4'` | `0x34` | 中ボタンのみ押下 |
| `'5'` | `0x35` | 左 ＋ 中ボタン押下 |
| `'6'` | `0x36` | 右 ＋ 中ボタン押下 |
| `'7'` | `0x37` | 左 ＋ 右 ＋ 中ボタン押下 |

### 2. X/Y 移動量

マウス移動量の蓄積値が `MOVE_THRESHOLD` (32) に達するごとに、IchigoJam のカーソルキーバイナリコード（1バイト）を出力します。

| 操作 | 送信バイナリ | 10進数 | IchigoJam キーコード |
| --- | --- | --- | --- |
| **左移動** | `0x1C` | `28` | `KEY_LEFT` |
| **右移動** | `0x1D` | `29` | `KEY_RIGHT` |
| **上移動** | `0x1E` | `30` | `KEY_UP` |
| **下移動** | `0x1F` | `31` | `KEY_DOWN` |

### 3. ホイールスクロール

ホイール回転検知時にバイナリコード（1バイト）を出力します。

| 操作 | 送信バイナリ | 10進数 | IchigoJam キーコード |
| --- | --- | --- | --- |
| **上スクロール** | `0x13` | `19` | `PAGE_UP` |
| **下スクロール** | `0x14` | `20` | `PAGE_DOWN` |

---

## IchigoJam 側での使用例 (Usage Example)

IchigoJam と本機を接続し、シリアル受信（`INKEY()`）でマウスの操作データ（ボタン・カーソルキー・スクロール）を取得する BASIC サンプルプログラムです。

```basic
100 ' USB Mouse Test for IchigoJam
110 K=INKEY()
120 IF K=0 GOTO 110
130 IF K>=48 AND K<=55 ? "Button: ";CHR$(K)
140 IF K>=28 AND K<=31 ? "Dir: ";K
150 IF K=19 OR K=20 ? "Wheel: ";K
160 GOTO 110

```

* **ボタン操作:** ボタン状態の変化時に `'0'` 〜 `'7'` (ASCII 48〜55) の文字が表示されます。
* **マウス移動:** 画面上のカーソル移動コード (`28`〜`31`) が取得できます。
* **スクロール:** ホイールの上下操作で `19` (PageUp) / `20` (PageDown) が取得できます。

---

## クイックスタート (開発環境なしで書き込む場合)

1. 本リポジトリの [Releases](https://github.com/maenoh/ijmouse_rp2040/releases/tag/v1.0.0) ページから最新の `ijmouse_rp2040.uf2` をダウンロードします。
2. RP2040-Zero の `BOOTSEL` ボタンを押しながら PC の USB ポートに接続します。
3. PC 上で `RPI-RP2` という名前のドライブとして認識されます。
4. ダウンロードした `ijmouse_rp2040.uf2` ファイルを `RPI-RP2` ドライブへドラッグ＆ドロップします。
5. 書き込み完了後、自動的に再起動して動作を開始します。

---

## ソースコードからのビルド手順 (Build Instructions)

### 開発環境・ライブラリ

* **Arduino IDE 2.x**
* **ボードコア:** [Raspberry Pi Pico/RP2040 Core](https://www.google.com/search?q=https://github.com/earlephilhower/arduino-pico&utm_source=gemini) (by Earle F. Philhower, III)
* 追加のボードマネージャ URL: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`


* **ライブラリ:**
* `Pico-PIO-USB` (by sekigon-gonnoc)
* `Adafruit TinyUSB Library`



### Arduino IDE 設定

* **ボード:** `Raspberry Pi Pico` (または `Generic RP2040`)
* **USB Stack:** **`Adafruit TinyUSB`** （※必須設定）
* **CPU Speed:** **`120 MHz`** (または `240 MHz`)

---

## 作者・ライセンス (Author & License)

* **Author:** Hideshi Maeno ([@maenoh](https://www.google.com/search?q=https://github.com/maenoh&utm_source=gemini))
* **License:** [MIT License](https://www.google.com/search?q=LICENSE&utm_source=gemini)

```

```
