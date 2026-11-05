/**
 * @file ijmouse_rp2040_5btn.ino
 * @brief RP2040-Zero USB 5-Button Mouse to UART Converter for IchigoJam (3-Button Combinations + Exclusive Back/Forward as '8'/'9'). 3-Button Mouse is not supported.
 * @author Hideshi Maeno (maenoh)
 * @date 2026/11/5
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
// 定数・定義
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
  // 1. IchigoJam用 UART1 (GP0: TX) の初期化 (115200 bps)
  Serial1.begin(115200);

  // 2. PCデバッグ用 USBシリアルの初期化
  Serial.begin(115200);
  
  unsigned long startMillis = millis();
  while (!Serial && (millis() - startMillis < 2000)) {
    delay(10);
  }

  Serial.println("========================================");
  Serial.println(" RP2040 USB 5-Button Mouse Converter    ");
  Serial.println(" Mode: 3-Btn Combo + Back('8') / Fwd('9') ");
  Serial.println("========================================");

  // 3. PIO-USB ピン指定 (GP4: D+, GP5: D-)
  pio_cfg.pin_dp = 4;

  // 4. USB Host (PIO) の開始
  USBHost.configure_pio_usb(1, &pio_cfg);
  USBHost.begin(1);
  
  Serial.println("USB Host initialized. Waiting for mouse...");
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
  Serial.println("[INFO] HID Device Mounted");
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  if (itf_protocol == HID_ITF_PROTOCOL_MOUSE || itf_protocol == HID_ITF_PROTOCOL_NONE) {
    if (!tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_REPORT)) {
      tuh_hid_receive_report(dev_addr, instance);
    }
  }
}

// プロトコル変更完了時に呼ばれるコールバック
void tuh_hid_set_protocol_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t protocol) {
  Serial.println("[INFO] Protocol set to REPORT complete.");
  tuh_hid_receive_report(dev_addr, instance);
}

// HIDデバイス切断時
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  Serial.println("[INFO] HID Device Unmounted");
  prev_buttons = 0xFF;
  acc_x = 0;
  acc_y = 0;
}

// HIDレポートデータ受信時 (16bit軸対応8バイトパケット)
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  if ((itf_protocol == HID_ITF_PROTOCOL_MOUSE || itf_protocol == HID_ITF_PROTOCOL_NONE) && len >= 8) {
    
    uint8_t raw_buttons = report[1];
    int16_t dx = (int16_t)((report[3] << 8) | report[2]);
    int16_t dy = (int16_t)((report[5] << 8) | report[4]);
    int8_t  wheel = (int8_t)report[6];

    // ----------------------------------------------------------------
    // 1. ボタン状態のデコード
    //    - 戻る / 進む が押されている場合はそれぞれ独立して 8 または 16 を設定
    //    - それ以外（左・右・中）は同時押しを考慮したビット組み合わせ（0 〜 7）
    // ----------------------------------------------------------------
    uint8_t current_buttons = 0;

    if (raw_buttons & 0x08) {
      current_buttons = 8;  // 戻るボタン（手前）
    } else if (raw_buttons & 0x10) {
      current_buttons = 16; // 進むボタン（先端）
    } else {
      // 3ボタン（左・右・中）は同時押しを許容
      if (raw_buttons & 0x01) current_buttons |= 0x01; // Left (bit0)
      if (raw_buttons & 0x02) current_buttons |= 0x02; // Right (bit1)
      if (raw_buttons & 0x04) current_buttons |= 0x04; // Middle (bit2)
    }

    // 状態が変化した時のみ文字コードを1バイト送信
    if (current_buttons != prev_buttons) {
      char send_char;
      if (current_buttons <= 7) {
        send_char = '0' + current_buttons;          // '0' 〜 '7' (3ボタンの組み合わせ)
      } else if (current_buttons == 8) {
        send_char = '8';                            // '8' (戻るボタン)
      } else if (current_buttons == 16) {
        send_char = '9';                            // '9' (進むボタン)
      } else {
        send_char = '0';
      }
      
      Serial1.write(send_char);
      prev_buttons = current_buttons;
    }

    // ----------------------------------------------------------------
    // 2. 移動量の蓄積とカーソルキー出力 (MOVE_THRESHOLD 判定)
    // ----------------------------------------------------------------
    acc_x += dx;
    acc_y += dy;

    while (acc_x >= MOVE_THRESHOLD) {
      Serial1.write(KEY_RIGHT);
      acc_x -= MOVE_THRESHOLD;
    }
    while (acc_x <= -MOVE_THRESHOLD) {
      Serial1.write(KEY_LEFT);
      acc_x += MOVE_THRESHOLD;
    }

    while (acc_y <= -MOVE_THRESHOLD) {
      Serial1.write(KEY_UP);
      acc_y += MOVE_THRESHOLD;
    }
    while (acc_y >= MOVE_THRESHOLD) {
      Serial1.write(KEY_DOWN);
      acc_y -= MOVE_THRESHOLD;
    }

    // ----------------------------------------------------------------
    // 3. ホイール操作判定 (PageUp / PageDown)
    // ----------------------------------------------------------------
    if (wheel > 0) {
      Serial1.write(PAGE_UP);
    } else if (wheel < 0) {
      Serial1.write(PAGE_DOWN);
    }
  }

  tuh_hid_receive_report(dev_addr, instance);
}

#ifdef __cplusplus
}
#endif
