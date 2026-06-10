# AtmosESP 🌡️

**Hệ thống giám sát thời tiết IoT sử dụng ESP32**

Hệ thống theo dõi các thông số thời tiết thời gian thực: nhiệt độ, độ ẩm, áp suất, ánh sáng, lượng mưa, tốc độ gió, khí gas và điểm sương. Dữ liệu được gửi lên **ThingsBoard** (dashboard) và **Google Sheets** (lưu trữ lâu dài). Có chức năng cảnh báo qua relay khi điều kiện thời tiết bất thường.

## ✨ Tính năng chính

- 📡 Giám sát 8 thông số thời tiết + điểm sương
- 📊 Hiển thị realtime trên **ThingsBoard Dashboard**
- 📝 Lưu trữ dữ liệu dài hạn trên **Google Sheets**
- ⚠️ Cảnh báo tự động qua Relay (khi mưa lớn >30%, gió mạnh >20m/s, khí gas >500ppm)
- 🌐 Kết nối WiFi + MQTT (ThingsBoard)
- 🔒 Hỗ trợ gửi dữ liệu qua HTTPS tới Google Sheets (bỏ qua kiểm tra SSL)
- 🔄 Gửi dữ liệu mỗi 5 giây

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
#define THINGSBOARD_SERVER  "eu.thingsboard.cloud"
#define TOKEN               "ThingsBoard Device Token"
String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/.../exec";
```

### 2. Thư viện Arduino cần cài
WiFi
PubSubClient (MQTT)
DHT sensor library
HTTPClient
WiFiClientSecure

### 3. ThingsBoard
- Tạo thiết bị (Device) trên ThingsBoard
- Lấy Access Token gán vào TOKEN
- Dashboard tự động nhận telemetry với các key: Nhiệt độ, Độ ẩm, Áp suất, Ánh sáng, Lượng mưa, Tốc độ gió, Khí gas, Điểm sương, Trạng thái Relay

### 4. Google Sheets
- Tạo Google Apps Script Web App trả về doPost() nhận JSON
- Triển khai dưới dạng Web App (quyền Anyone hoặc Anyone with link)
- Lấy URL và gán vào GOOGLE_SCRIPT_URL

### 📤 Dữ liệu gửi lên Google Sheets (JSON)

```json
{
  "temp": 26.5,
  "hum": 65.0,
  "pres": 1012.34,
  "lux": 850.0,
  "rain": 12.3,
  "wind": 5.67,
  "gas": 120,
  "dew": 19.45,
  "relay": 0
}

### 🧪 Mô phỏng với Wokwi
Có thể chạy mô phỏng trực tiếp trên Wokwi với file diagram.json đi kèm. Các cảm biến dạng potentiometer dùng để mô phỏng:
Pressure (hPa)
Rain (mm/h)
Wind (m/s)
Gas (ppm)

### ⚠️ Cảnh báo Relay
Relay tự động BẬT khi:
Mưa > 30%
Gió > 20 m/s
Khí gas > 500 ppm

### 🔧 Xử lý lỗi thường gặp
Lỗi	Nguyên nhân	Cách khắc phục
Lỗi HTTP -1	Không kết nối được Google Script	Kiểm tra URL, WiFi, hoặc tăng timeout
[THẤT BẠI] mã lỗi -2	MQTT không kết nối	Kiểm tra token, server, cổng 1883
DHT22 trả về NaN	Chân sai hoặc chưa khởi tạo	Kiểm tra DHTPIN, thêm dht.begin()
### 📄 Giấy phép
MIT - Dùng cho mục đích học tập và nghiên cứu.

👨‍💻 Tác giả
Võ Quốc Việt NTU - MSSV: 65134318
