/**
 * @file ijmouse_rp2040_5btn.ino
 * @brief RP2040-Zero USB 5-Button Mouse to UART Converter for IchigoJam (Hardware Watchdog Protected)
 * @author Hideshi Maeno (maenoh)
 * @date 2026/11/7
 *
 * MIT License
 * Copyright (c) 2026 Hideshi Maeno
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <pio_usb.h>
#include "Adafruit_USBH_Host.h"
#include "hardware/watchdog.h" // RP2040内蔵ハードウェアウォッチドッグを使用

// USB Host インスタンス
Adafruit_USBH_Host USBHost;

// PIO-USB ピン設定構造体
static pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;

#define MOVE_THRESHOLD  32

#define KEY_LEFT        28
#define KEY_RIGHT       29
#define KEY_UP          30
#define KEY_DOWN        31
#define PAGE_UP         0x13
#define PAGE_DOWN       0x14

static uint8_t prev_buttons = 0xFF;
static int16_t acc_x = 0;
static int16_t acc_y = 0;

void setup() {
  Serial1.begin(115200);
  Serial.begin(115200);
  
  // スタンドアロン（PCなし）起動時の遅延を防ぐため、待機時間を1秒に短縮
  unsigned long startMillis = millis();
  while (!Serial && (millis() - startMillis < 1000)) {
    delay(10);
  }

  Serial.println("\n========================================");
  Serial.println(" RP2040 5-Btn Mouse Converter           ");
  Serial.println(" Mode: Hardware Watchdog Protected      ");
  Serial.println("========================================");

  pio_cfg.pin_dp = 4;
  USBHost.configure_pio_usb(1, &pio_cfg);
  USBHost.begin(1);

  // 【重要】ハードウェアウォッチドッグを有効化
  // loop()内の更新が2000ミリ秒(2秒)以上途絶えると、システムを自動的に強制再起動します
  watchdog_enable(2000, 1);
  
  Serial.println("[SETUP] Setup completed. Waiting for mouse...");
}

void loop() {
  // ウォッチドッグタイマーをリセット（システムが生存していることを通知）
  watchdog_update();

  USBHost.task();

  // PIO-USBのステートマシンに余裕を持たせ、タスク詰まりを防ぐ微小ウェイト
  delay(1);
}

// ====================================================================
// TinyUSB HID ホスト コールバック関数群
// ====================================================================
#ifdef __cplusplus
extern "C" {
#endif

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
  if (itf_protocol == HID_ITF_PROTOCOL_MOUSE || itf_protocol == HID_ITF_PROTOCOL_NONE) {
    if (!tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_REPORT)) {
      tuh_hid_receive_report(dev_addr, instance);
    }
  }
}

void tuh_hid_set_protocol_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t protocol) {
  tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  prev_buttons = 0xFF;
  acc_x = 0;
  acc_y = 0;
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  if ((itf_protocol == HID_ITF_PROTOCOL_MOUSE || itf_protocol == HID_ITF_PROTOCOL_NONE) && len >= 8) {
    uint8_t raw_buttons = report[1];
    int16_t dx = (int16_t)((report[3] << 8) | report[2]);
    int16_t dy = (int16_t)((report[5] << 8) | report[4]);
    int8_t  wheel = (int8_t)report[6];

    uint8_t current_buttons = 0;
    if (raw_buttons & 0x08) {
      current_buttons = 8;
    } else if (raw_buttons & 0x10) {
      current_buttons = 16;
    } else {
      if (raw_buttons & 0x01) current_buttons |= 0x01;
      if (raw_buttons & 0x02) current_buttons |= 0x02;
      if (raw_buttons & 0x04) current_buttons |= 0x04;
    }

    if (current_buttons != prev_buttons) {
      char send_char;
      if (current_buttons <= 7) send_char = '0' + current_buttons;
      else if (current_buttons == 8) send_char = '8';
      else if (current_buttons == 16) send_char = '9';
      else send_char = '0';
      
      Serial1.write(send_char);
      prev_buttons = current_buttons;
    }

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

    if (wheel > 0) {
      Serial1.write(PAGE_UP);
    } else if (wheel < 0) {
      Serial1.write(PAGE_DOWN);
    }
  }

  // ここにあった delay(2) はコールバックをブロックしてハングを誘発するため削除しました
  tuh_hid_receive_report(dev_addr, instance);
}

#ifdef __cplusplus
}
#endif
