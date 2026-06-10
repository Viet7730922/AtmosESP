#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ==========================================
// 1. CẤU HÌNH WIFI & THINGSBOARD
// ==========================================
#define WIFI_SSID           "Wokwi-GUEST"
#define WIFI_PASSWORD       ""
#define THINGSBOARD_SERVER  "eu.thingsboard.cloud" 
#define TOKEN               "OzpJ5mxL6G4R9p6wrRB1" 

// ==========================================
// 2. CẤU HÌNH TELEGRAM BOT (THAY THÔNG TIN CỦA BẠN)
// ==========================================
#define BOT_TOKEN   "8919276387:AAGc52bGK03gVELBDYwKd6t6iQgMMZ-ObTE"  // THAY BẰNG TOKEN CỦA BẠN
#define CHAT_ID     "5413418959"  // THAY BẰNG CHAT ID CỦA BẠN

// ==========================================
// 3. ĐỊNH NGHĨA CÁC CHÂN CẢM BIẾN
// ==========================================
#define DHTPIN        4     
#define LDR_PIN       33    
#define PRESSURE_PIN  36    
#define RAIN_PIN      34    
#define WIND_PIN      35    
#define GAS_PIN       32    
#define RELAY_PIN     23    

// ==========================================
// 4. KHỞI TẠO ĐỐI TƯỢNG
// ==========================================
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

// Biến quản lý cảnh báo Telegram
bool lastWarningState = false;
unsigned long lastTelegramSend = 0;
const unsigned long TELEGRAM_COOLDOWN = 60000;  // 60 giây giữa các lần gửi nhắc lại

// ==========================================
// 5. HÀM TRỢ NĂNG
// ==========================================

// Tính điểm sương
float calculateDewPoint(float temp, float humid) {
    if (humid <= 0.0 || humid > 100.0 || isnan(humid) || isnan(temp)) return 0.0; 
    float a = 17.27, b = 237.7;
    float alpha = ((a * temp) / (b + temp)) + log(humid / 100.0);
    return (b * alpha) / (a - alpha);
}

// Mã hóa URL cho tiếng Việt và ký tự đặc biệt
String urlEncode(String str) {
    String encoded = "";
    char c;
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            encoded += "+";
        } else {
            encoded += '%';
            char hex[3];
            sprintf(hex, "%02X", (unsigned char)c);
            encoded += hex;
        }
    }
    return encoded;
}

// Gửi tin nhắn đến Telegram Bot
bool sendToTelegram(String message) {
    WiFiClientSecure secureClient;
    secureClient.setInsecure();  // Bỏ qua kiểm tra SSL
    
    HTTPClient https;
    
    String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + 
                 "/sendMessage?chat_id=" + String(CHAT_ID) + 
                 "&text=" + urlEncode(message) +
                 "&parse_mode=Markdown";
    
    https.begin(secureClient, url);
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    Serial.print("=> [TELEGRAM] Đang gửi... ");
    
    int httpCode = https.GET();
    bool success = false;
    
    if (httpCode == 200) {
        Serial.println("THÀNH CÔNG!");
        success = true;
    } else {
        Serial.print("THẤT BẠI! Mã lỗi: ");
        Serial.println(httpCode);
        
        if (httpCode > 0) {
            String response = https.getString();
            Serial.println("Phản hồi: " + response);
        }
    }
    
    https.end();
    return success;
}

// Gửi cảnh báo khi phát hiện sự cố
void sendWarningAlert(float temp, float hum, float rain, float wind, int gas) {
    String message = "🚨 *CẢNH BÁO KHẨN CẤP!* 🚨\n\n";
    message += "🌡️ *Nhiệt độ:* " + String(temp, 1) + "°C\n";
    message += "💧 *Độ ẩm:* " + String(hum, 1) + "%\n";
    message += "🌧️ *Lượng mưa:* " + String(rain, 1) + " mm\n";
    message += "💨 *Tốc độ gió:* " + String(wind, 1) + " m/s\n";
    message += "🔥 *Khí gas:* " + String(gas) + " ppm\n\n";
    message += "⚠️ *Nguy cơ phát hiện:*\n";
    
    if (rain > 30) message += "• 🌧️ Mưa lớn (>30mm)\n";
    if (wind > 20) message += "• 💨 Gió mạnh (>20m/s)\n";
    if (gas > 500) message += "• 🔥 Rò rỉ khí gas (>500ppm)\n";
    
    message += "\n🔔 *Hệ thống đã kích hoạt relay cảnh báo!*";
    
    sendToTelegram(message);
}

// Gửi thông báo khi hết cảnh báo
void sendNormalAlert() {
    String message = "✅ *HỆ THỐNG ĐÃ TRỞ LẠI BÌNH THƯỜNG* ✅\n\n";
    message += "Các chỉ số đã an toàn. Relay đã tắt.\n";
    message += "🕐 Hệ thống tiếp tục giám sát...";
    
    sendToTelegram(message);
}

// Kết nối WiFi
void setup_wifi() {
    delay(10);
    Serial.println("\n📡 Đang kết nối WiFi: " + String(WIFI_SSID));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\n✅ Đã kết nối WiFi. IP: " + WiFi.localIP().toString());
}

// Kết nối lại ThingsBoard nếu mất
void reconnect() {
    while (!client.connected()) {
        Serial.print("🔌 Đang kết nối đến ThingsBoard...");
        String clientId = "ESP32Client-" + String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str(), TOKEN, "")) {
            Serial.println(" ✅ THÀNH CÔNG");
        } else {
            Serial.print(" ❌ THẤT BẠI (lỗi ");
            Serial.print(client.state());
            Serial.println(") -> Thử lại sau 5 giây");
            delay(5000);
        }
    }
}

// ==========================================
// 6. SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n========================================");
    Serial.println("   ESP32 WEATHER MONITOR SYSTEM");
    Serial.println("========================================\n");
    
    // Kết nối WiFi
    setup_wifi();
    
    // Cấu hình ThingsBoard
    client.setServer(THINGSBOARD_SERVER, 1883);
    
    // Khởi tạo cảm biến
    Serial.println("🌡️ Khởi tạo DHT22...");
    dht.begin();
    
    // Cấu hình chân
    pinMode(LDR_PIN, INPUT);
    pinMode(PRESSURE_PIN, INPUT);
    pinMode(RAIN_PIN, INPUT);
    pinMode(WIND_PIN, INPUT);
    pinMode(GAS_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    
    Serial.println("✅ Hệ thống khởi động hoàn tất!\n");
    
    // Gửi tin nhắn chào mừng qua Telegram
    delay(3000);
    sendToTelegram("🤖 *ESP32 WEATHER MONITOR KHỞI ĐỘNG* 🤖\n\n✅ Hệ thống giám sát thời tiết và cảnh báo đã sẵn sàng!\n\n📊 Đang theo dõi:\n• Nhiệt độ & Độ ẩm\n• Áp suất & Ánh sáng\n• Lượng mưa & Gió\n• Khí gas");
}

// ==========================================
// 7. LOOP
// ==========================================
void loop() {
    // Đảm bảo kết nối ThingsBoard
    if (!client.connected()) reconnect();
    client.loop(); 
    
    // ==========================================
    // ĐỌC DỮ LIỆU CẢM BIẾN
    // ==========================================
    
    // DHT22 (Nhiệt độ, độ ẩm)
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    if (isnan(temperature) || isnan(humidity)) { 
        temperature = 0.0; 
        humidity = 0.0; 
    }
    
    // Áp suất (mô phỏng từ cảm biến áp suất analog)
    int rawPressure = analogRead(PRESSURE_PIN);
    float pressure = map(rawPressure, 0, 4095, 90000, 110000) / 100.0F; 
    
    // Ánh sáng
    int rawLux = analogRead(LDR_PIN);
    float lux = map(rawLux, 0, 4095, 0, 10000); 
    
    // Lượng mưa
    int rawRain = analogRead(RAIN_PIN);
    float rainIntensity = map(rawRain, 0, 4095, 0, 100);
    
    // Tốc độ gió
    int rawWind = analogRead(WIND_PIN);
    float windSpeed = map(rawWind, 0, 4095, 0, 5000) / 100.0F;
    
    // Khí gas
    int rawGas = analogRead(GAS_PIN);
    int gasPPM = map(rawGas, 0, 4095, 0, 1000);
    
    // Điểm sương
    float dewPoint = calculateDewPoint(temperature, humidity);
    
    // ==========================================
    // XỬ LÝ CẢNH BÁO & RELAY
    // ==========================================
    bool currentWarning = (rainIntensity > 30.0 || windSpeed > 20.0 || gasPPM > 500);
    
    // Điều khiển relay
    if (currentWarning) {
        digitalWrite(RELAY_PIN, HIGH);
    } else {
        digitalWrite(RELAY_PIN, LOW);
    }
    
    // Gửi cảnh báo Telegram (chống gửi trùng)
    unsigned long now = millis();
    
    if (currentWarning && !lastWarningState) {
        // Chuyển từ an toàn → cảnh báo
        Serial.println("⚠️ PHÁT HIỆN CẢNH BÁO! ⚠️");
        sendWarningAlert(temperature, humidity, rainIntensity, windSpeed, gasPPM);
        lastTelegramSend = now;
    } 
    else if (!currentWarning && lastWarningState) {
        // Chuyển từ cảnh báo → an toàn
        Serial.println("✅✅✅ HẾT CẢNH BÁO! ✅✅✅");
        sendNormalAlert();
        lastTelegramSend = now;
    }
    else if (currentWarning && lastWarningState && (now - lastTelegramSend > TELEGRAM_COOLDOWN)) {
        // Cảnh báo kéo dài, nhắc lại sau mỗi 60 giây
        Serial.println("⚠️ CẢNH BÁO KÉO DÀI - GỬI NHẮC LẠI");
        sendWarningAlert(temperature, humidity, rainIntensity, windSpeed, gasPPM);
        lastTelegramSend = now;
    }
    
    lastWarningState = currentWarning;
    
    // ==========================================
    // GỬI DỮ LIỆU LÊN THINGSBOARD
    // ==========================================
    
    // Payload hiển thị (có dấu - để in ra Serial)
    String tb_payload_display = "{";
    tb_payload_display += "\"Nhiệt độ\":" + String(temperature, 2) + ",";
    tb_payload_display += "\"Độ ẩm\":" + String(humidity, 1) + ",";
    tb_payload_display += "\"Áp suất\":" + String(pressure, 2) + ",";
    tb_payload_display += "\"Ánh sáng\":" + String(lux, 1) + ",";
    tb_payload_display += "\"Lượng mưa\":" + String(rainIntensity, 1) + ",";
    tb_payload_display += "\"Tốc độ gió\":" + String(windSpeed, 2) + ",";
    tb_payload_display += "\"Khí gas\":" + String(gasPPM) + ",";
    tb_payload_display += "\"Điểm sương\":" + String(dewPoint, 2) + ",";
    tb_payload_display += "\"Trạng thái Relay\":" + String(currentWarning ? 1 : 0);
    tb_payload_display += "}";
    
    // Payload gửi lên ThingsBoard (không dấu)
    String payload = "{";
    payload += "\"temperature\":" + String(temperature, 2) + ",";
    payload += "\"humidity\":" + String(humidity, 1) + ",";
    payload += "\"pressure\":" + String(pressure, 2) + ",";
    payload += "\"light\":" + String(lux, 1) + ",";
    payload += "\"rain\":" + String(rainIntensity, 1) + ",";
    payload += "\"wind_speed\":" + String(windSpeed, 2) + ",";
    payload += "\"gas_ppm\":" + String(gasPPM) + ",";
    payload += "\"dew_point\":" + String(dewPoint, 2) + ",";
    payload += "\"relay_status\":" + String(currentWarning ? 1 : 0);
    payload += "}";
    
    // In ra Serial để theo dõi
    Serial.println("\n📊 DỮ LIỆU CẢM BIẾN:");
    Serial.println(tb_payload_display);
    
    // Gửi lên ThingsBoard
    if (client.publish("v1/devices/me/telemetry", payload.c_str())) {
        Serial.println("✅ [THINGSBOARD] Gửi dữ liệu thành công");
    } else {
        Serial.println("❌ [THINGSBOARD] Gửi thất bại");
    }
    
    Serial.println("========================================\n");
    
    delay(5000);  // Gửi dữ liệu mỗi 5 giây
}