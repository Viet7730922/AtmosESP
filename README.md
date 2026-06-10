# AtmosESP 🌡️

**Hệ thống giám sát thời tiết và khí quyển IoT sử dụng ESP32**

Hệ thống theo dõi các thông số thời tiết thời gian thực: nhiệt độ, độ ẩm, áp suất, ánh sáng, lượng mưa, tốc độ gió, khí gas và điểm sương. Dữ liệu được gửi lên **ThingsBoard** (dashboard giám sát). Hệ thống tự động gửi **cảnh báo qua Telegram Bot** và kích hoạt relay khi phát hiện điều kiện thời tiết nguy hiểm (mưa lớn, gió mạnh, khí gas cao).

## ✨ Tính năng chính

- 📡 Giám sát 8 thông số thời tiết + điểm sương
- 📊 Hiển thị realtime trên **ThingsBoard Dashboard** (9 telemetry keys)
- 🤖 **Cảnh báo qua Telegram Bot** khi có sự cố (kèm nhắc lại định kỳ 60s)
- ⚠️ Cảnh báo tự động qua Relay (khi mưa lớn >30 mm/h, gió mạnh >20 m/s, khí gas >500 ppm)
- 🔄 Cơ chế chống gửi trùng cảnh báo (cooldown 60 giây)
- 🌐 Kết nối WiFi + MQTT (ThingsBoard)
- 🔒 Hỗ trợ HTTPS tới Telegram API (bỏ qua kiểm tra SSL)

## 🛠️ Cấu hình phần cứng

### Linh kiện chính (mô phỏng trên Wokwi):
- ESP32 DevKit V4
- DHT22 (Nhiệt độ + Độ ẩm)
- Photoresistor (Ánh sáng)
- Potentiometer (mô phỏng Áp suất, Lượng mưa, Tốc độ gió, Khí gas)
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

### 1. Cấu hình WiFi & ThingsBoard

```cpp
#define WIFI_SSID           "Wokwi-GUEST"
#define WIFI_PASSWORD       ""
#define THINGSBOARD_SERVER  "eu.thingsboard.cloud"
#define TOKEN               "Your_Access_Token_Here"
```

2. Cấu hình Telegram Bot
```cpp
#define BOT_TOKEN   "1234567890:ABCdefGHIjklMNOpqrsTUVwxyz"
#define CHAT_ID     "123456789"
```

Cách lấy token và chat id:
Tìm @BotFather trên Telegram → /newbot → nhận BOT_TOKEN
Tìm @userinfobot → gửi tin nhắn bất kỳ → nhận CHAT_ID

### 3. Thư viện Arduino cần cài
- WiFi.h
- PubSubClient.h (MQTT)
- DHT sensor library
- HTTPClient.h
- WiFiClientSecure.h

### 4. ThingsBoard Dashboard
- Tạo thiết bị (Device) trên ThingsBoard
- Lấy Access Token gán vào TOKEN
- Import file esp32_dashboard.json để có giao diện đầy đủ các widget
- Các key telemetry tự động nhận: temperature, humidity, pressure, light, rain, wind_speed, gas_ppm, dew_point, relay_status

### 5. Telegram Bot (không cần cấu hình thêm)
Sau khi cập nhật đúng token và chat id, ESP32 sẽ tự động:
- Gửi tin nhắn chào mừng khi khởi động
- Gửi cảnh báo khẩn cấp khi phát hiện sự cố
- Nhắc lại mỗi 60 giây nếu cảnh báo kéo dài
- Gửi thông báo khi hết cảnh báo

### 🧪 Mô phỏng với Wokwi
Có thể chạy mô phỏng trực tiếp trên Wokwi với file diagram.json đi kèm. Các biến trở (potentiometer) dùng để mô phỏng:

## Biến trở	Giá trị mô phỏng	Dải giá trị
- Pressure (hPa)	Áp suất khí quyển	900–1100 hPa
- Rain (mm/h)	Lượng mưa	0–100 mm/h
- Wind (m/s)	Tốc độ gió	0–50 m/s
- Gas (ppm)	Nồng độ khí gas	0–1000 ppm
⚠️ Cảnh báo Relay & Telegram
Relay và Telegram tự động BẬT / GỬI CẢNH BÁO khi:

## Điều kiện Ngưỡng
- Lượng mưa (rain)	> 30 mm/h
- Tốc độ gió (wind)	> 20 m/s
- Khí gas (gas_ppm)	> 500 ppm
- Khi tất cả điều kiện trở về dưới ngưỡng → relay tắt + gửi thông báo "HẾT CẢNH BÁO".

📊 Cấu trúc dữ liệu gửi lên ThingsBoard
```json
{
  "temperature": 26.5,
  "humidity": 65.0,
  "pressure": 1012.34,
  "light": 850.0,
  "rain": 12.3,
  "wind_speed": 5.67,
  "gas_ppm": 120,
  "dew_point": 19.45,
  "relay_status": 0
}
```

### 🔧 Xử lý lỗi thường gặp
**Lỗi: Nguyên nhân ->	Cách khắc phục**
- ❌ THẤT BẠI (lỗi -2):	MQTT không kết nối ->	Kiểm tra token, server, cổng 1883
- [TELEGRAM] THẤT BẠI! Mã lỗi: -1 :	Không kết nối được Telegram API ->	Kiểm tra WiFi, token, chat id
- DHT22 trả về NaN : Chân sai hoặc chưa khởi tạo	-> Kiểm tra DHTPIN, thêm dht.begin()
- Gửi Telegram thành công nhưng không nhận tin nhắn :	Chat ID sai hoặc bot chưa start	-> Gửi /start cho bot, kiểm lại CHAT_ID

### 👨‍💻 Tác giả
Võ Quốc Việt NTU - MSSV: 65134318 - Nhóm 3: IoT và Ứng dụng </br>
GitHub: [Viet7730922/AtmosESP](https://github.com/Viet7730922/AtmosESP)<br>
File GG Drive hướng dẫn chi tiết: [AsmotphereReporter](https://drive.google.com/file/d/1NDPUi26vJS0BQoynyR75u60UcGblTChL/view?usp=drive_link)
