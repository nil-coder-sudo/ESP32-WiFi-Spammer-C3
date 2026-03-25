# ESP32-WiFi-Spammer-C3

A simple yet powerful WiFi security testing tool for ESP32-C3.

## 🛠️ Requirements
1. **Hardware**: ESP32-C3 SuperMini (or any ESP32-C3 board).
2. **Display**: 0.96" OLED SSD1306 (I2C).
3. **Software**: [Arduino IDE](https://www.arduino.cc/en/software).

## 📦 Libraries Needed
Please install these libraries via Arduino Library Manager:
- **U8g2** by oliver

## 🔧 How to Compile
1. Open `ESP32_WiFi_Spammer.ino` in Arduino IDE.
2. Go to **Tools > Board**, select **ESP32C3 Dev Module**.
3. Go to **Tools > USB CDC On Boot**, set to **Enabled** (Important for C3).
4. Connect your board and click **Upload**.

## 📍 Pinout (Default)
- **SDA**: GPIO 8
- **SCL**: GPIO 9
- **Button**: GPIO 2

## ⚠️ Disclaimer
This tool is for educational and testing purposes only. Use it only on networks you own or have permission to test.
