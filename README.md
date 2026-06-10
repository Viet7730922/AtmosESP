# AtmosESP 🌡️

**Hệ thống giám sát thời tiết IoT sử dụng ESP32**

Hệ thống theo dõi các thông số thời tiết thời gian thực: nhiệt độ, độ ẩm, áp suất, ánh sáng, lượng mưa, tốc độ gió, khí gas và điểm sương. Dữ liệu được gửi lên **ThingsBoard** (dashboard) và **Google Sheets** (lưu trữ lâu dài). Có chức năng cảnh báo qua relay khi điều kiện thời tiết bất thường.

## ✨ Tính năng chính

- 📡 Giám sát 8 thông số thời tiết + điểm sương
- 📊 Hiển thị realtime trên **ThingsBoard Dashboard**
- 📝 Lưu trữ dữ liệu dài hạn trên **Google Sheets**
- ⚠️ Cảnh báo tự động qua Relay (khi mưa lớn, gió mạnh, gas cao)
- 🌐 Kết nối WiFi + MQTT
- 🔋 Phù hợp chạy trên ESP32 DevKit

## 🛠️ Cấu hình phần cứng

### Linh kiện chính:
- ESP32 DevKit V4
- DHT22 (Nhiệt độ + Độ ẩm)
- Photoresistor (Ánh sáng)
- Potentiometer (mô phỏng Áp suất, Mưa, Gió, Gas)
- Relay Module

### Kết nối chân (Pinout):
| Chức năng          | Chân ESP32 |
|--------------------|------------|
| DHT22              | GPIO 4     |
| LDR (Ánh sáng)     | GPIO 33    |
| Áp suất            | GPIO 36    |
| Lượng mưa          | GPIO 34    |
| Tốc độ gió         | GPIO 35    |
| Khí gas            | GPIO 32    |
| Relay              | GPIO 23    |

## 📥 Cài đặt & Chạy

### 1. Cấu hình code
```cpp
#define WIFI_SSID           "Tên WiFi"
#define WIFI_PASSWORD       "Mật khẩu WiFi"
#define TOKEN               "ThingsBoard Device Token"
String GOOGLE_SCRIPT_URL = "https://script.google.com/.../exec";