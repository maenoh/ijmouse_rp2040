/**
 * @file ijmouse_rp2040.ino
 * @brief RP2040-Zero USB Mouse to UART Converter for IchigoJam (ijmouse_digit_mode)
 * @author Hideshi Maeno (maenoh)
 * @date 2026
 *
 * MIT License
 *
 * Copyright (c) 2026 Hideshi Maeno
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <pio_usb.h>
#include "Adafruit_USBH_Host.h"

// USB Host インスタンス
Adafruit_USBH_Host USBHost;

// PIO-USB ピン設定構造体
static pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;

// ====================================================================
// ijmouse_digit_mode 定数・定義
// ====================================================================
#define MOVE_THRESHOLD  32    // 移動検出のしきい値（ジッタ除去）

// IchigoJam コントロールコード定義
#define KEY_LEFT        28    // 0x1C
#define KEY_RIGHT       29    // 0x1D
#define KEY_UP          30    // 0x1E
#define KEY_DOWN        31    // 0x1F
#define PAGE_UP         0x13  // 19 (上スクロール)
#define PAGE_DOWN       0x14  // 20 (下スクロール)

// 状態保持用変数
static uint8_t prev_buttons = 0xFF; // 初回判定用に初期値0xFF
static int16_t acc_x = 0;
static int16_t acc_y = 0;

void setup() {
  // 1. UART1 (GP0: TX) の初期化 (115200 bps)
  Serial1.begin(115200);

  // 2. PIO-USB ピン指定 (GP4: D+, GP5: D-)
  pio_cfg.pin_dp = 4;

  // 3. USB Host (PIO) の開始
  USBHost.configure_pio_usb(1, &pio_cfg);
  USBHost.begin(1);
}

void loop() {
  // USBホストタスクをループ処理
  USBHost.task();
}

// ====================================================================
// TinyUSB HID ホスト コールバック関数群 (Cリンケージの確保)
// ====================================================================
#ifdef __cplusplus
extern "C" {
#endif

// HIDデバイス接続時
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  if (itf_protocol == HID_ITF_PROTOCOL_MOUSE || itf_protocol == HID_ITF_PROTOCOL_NONE) {
    // ホイール（拡張機能）を有効にするため Report Protocol への切り替えを非同期要求
    if (!tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_REPORT)) {
      // 既にReport Protocolの場合など、要求が不要・失敗した場合は直接レポート受信を開始
      tuh_hid_receive_report(dev_addr, instance);
    }
  }
}

// tuh_hid_set_protocol によるプロトコル変更完了時に呼ばれるコールバック
void tuh_hid_set_protocol_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t protocol) {
  // プロトコル切替完了後に安全にレポート受信を開始
  tuh_hid_receive_report(dev_addr, instance);
}

// HIDデバイス切断時
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  prev_buttons = 0xFF;
  acc_x = 0;
  acc_y = 0;
}

// HIDレポートデータ受信時
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  // 接続デバイスのプロトコル種別を取得 (0: None/Generic, 1: Keyboard, 2: Mouse)
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  if ((itf_protocol == HID_ITF_PROTOCOL_MOUSE || itf_protocol == HID_ITF_PROTOCOL_NONE) && len >= 3) {
    
    uint8_t offset = 0;
    uint8_t raw_buttons = report[offset];
    int8_t  dx          = (int8_t)report[offset + 1];
    int8_t  dy          = (int8_t)report[offset + 2];
    int8_t  wheel       = (len >= (offset + 4)) ? (int8_t)report[offset + 3] : 0;

    // ----------------------------------------------------------------
    // 1. ボタン状態のデコードと出力 ('0' 〜 '7')
    // ----------------------------------------------------------------
    uint8_t current_buttons = 0;
    if (raw_buttons & 0x01) current_buttons |= 1; // Left (bit0)
    if (raw_buttons & 0x02) current_buttons |= 2; // Right (bit1)
    if (raw_buttons & 0x04) current_buttons |= 4; // Middle (bit2)

    // 状態が変化した時のみ ASCII 文字 ('0' 〜 '7') を1バイト出力
    if (current_buttons != prev_buttons) {
      Serial1.write('0' + current_buttons);
      prev_buttons = current_buttons;
    }

    // ----------------------------------------------------------------
    // 2. 移動量の蓄積とカーソルキー出力 (MOVE_THRESHOLD 判定)
    // ----------------------------------------------------------------
    acc_x += dx;
    acc_y += dy;

    // X軸方向判定 (右: 29, 左: 28)
    while (acc_x >= MOVE_THRESHOLD) {
      Serial1.write(KEY_RIGHT);
      acc_x -= MOVE_THRESHOLD;
    }
    while (acc_x <= -MOVE_THRESHOLD) {
      Serial1.write(KEY_LEFT);
      acc_x += MOVE_THRESHOLD;
    }

    // Y軸方向判定 (上: 30, 下: 31)
    while (acc_y <= -MOVE_THRESHOLD) {
      Serial1.write(KEY_UP);
      acc_y += MOVE_THRESHOLD;
    }
    while (acc_y >= MOVE_THRESHOLD) {
      Serial1.write(KEY_DOWN);
      acc_y -= MOVE_THRESHOLD;
    }

    // ----------------------------------------------------------------
    // 3. ホイール操作判定 (PageUp: 0x13, PageDown: 0x14)
    // ----------------------------------------------------------------
    if (wheel > 0) {
      Serial1.write(PAGE_UP);
    } else if (wheel < 0) {
      Serial1.write(PAGE_DOWN);
    }
  }

  // 次のレポート受信をリクエスト
  tuh_hid_receive_report(dev_addr, instance);
}

#ifdef __cplusplus
}
#endif
