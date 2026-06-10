#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h> // Thêm thư viện quản lý file trên Flash

// ==========================================
// 1. CẤU HÌNH WIFI, THINGSBOARD & GOOGLE SHEETS
// ==========================================
#define WIFI_SSID           "Wokwi-GUEST"
#define WIFI_PASSWORD       ""
#define THINGSBOARD_SERVER  "eu.thingsboard.cloud" 
#define TOKEN               "OzpJ5mxL6G4R9p6wrRB1" 

String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbzqEx3l3VkCM-GJ4URKrvKmbEjdec3ijt4ArrN5VehKPvoa4wdJhsrSD1Snm_wVNCfzDQ/exec"; 

#define OFFLINE_FILE        "/offline_data.txt"
#define MAX_OFFLINE_LINES   100 // Giới hạn số lượng bản ghi lưu trữ tối đa

// ==========================================
// 2. ĐỊNH NGHĨA CÁC CHÂN
// ==========================================
#define DHTPIN        4     
#define LDR_PIN       33    
#define PRESSURE_PIN  36    
#define RAIN_PIN      34    
#define WIND_PIN      35    
#define GAS_PIN       32    
#define RELAY_PIN     23    

// ==========================================
// 3. KHỞI TẠO ĐỐI TƯỢNG
// ==========================================
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
unsigned long lastReconnectAttempt = 0;

// ==========================================
// 4. HÀM TRỢ NĂNG & XỬ LÝ OFFLINE
// ==========================================
float calculateDewPoint(float temp, float humid) {
    if (humid <= 0.0 || humid > 100.0 || isnan(humid) || isnan(temp)) return 0.0; 
    float a = 17.27, b = 237.7;
    float alpha = ((a * temp) / (b + temp)) + log(humid / 100.0);
    return (b * alpha) / (a - alpha);
}

void setup_wifi() {
    delay(10);
    Serial.println("\nĐang kết nối WiFi: " + String(WIFI_SSID));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// Kiểm tra và kết nối lại MQTT không chặn luồng (Non-blocking)
bool reconnect() {
    if (client.connected()) return true;
    
    if (WiFi.status() != WL_CONNECTED) return false;

    Serial.print("Đang kết nối đến ThingsBoard...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), TOKEN, "")) {
        Serial.println(" [THÀNH CÔNG]");
        return true;
    } else {
        Serial.print(" [THẤT BẠI] mã lỗi: ");
        Serial.println(client.state());
        return false;
    }
}

// Hàm lưu dữ liệu vào bộ nhớ Flash khi offline
void saveOfflineData(String payload) {
    // Kiểm tra số dòng hiện tại để tránh đầy bộ nhớ
    int lineCount = 0;
    if (LittleFS.exists(OFFLINE_FILE)) {
        File readFile = LittleFS.open(OFFLINE_FILE, FILE_READ);
        while (readFile.available()) {
            readFile.readStringUntil('\n');
            lineCount++;
        }
        readFile.close();
    }

    if (lineCount >= MAX_OFFLINE_LINES) {
        Serial.println("=> [OFFLINE] Bộ nhớ đầy! Bỏ qua bản ghi cũ để bảo vệ Flash.");
        return;
    }

    File file = LittleFS.open(OFFLINE_FILE, FILE_APPEND);
    if (!file) {
        Serial.println("=> [OFFLINE] Không thể mở file để ghi!");
        return;
    }
    file.println(payload);
    file.close();
    Serial.println("=> [OFFLINE] Đã lưu thành công 1 bản ghi vào Flash.");
}

// Hàm gửi gói tin lên Google Sheets (trả về true nếu thành công)
bool sendToGoogleSheets(String jsonPayload) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure secureClient;
    secureClient.setInsecure(); 

    HTTPClient http;
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.begin(secureClient, GOOGLE_SCRIPT_URL);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(jsonPayload);
    http.end();

    return (httpResponseCode == 200 || httpResponseCode == 302);
}

// Hàm đồng bộ dữ liệu từ Flash lên Google Sheets khi có mạng trở lại
void syncOfflineData() {
    if (!LittleFS.exists(OFFLINE_FILE) || WiFi.status() != WL_CONNECTED) return;

    Serial.println("=> [ĐỒNG BỘ] Phát hiện dữ liệu ngoại tuyến. Đang tiến hành đồng bộ...");
    
    File file = LittleFS.open(OFFLINE_FILE, FILE_READ);
    if (!file) return;

    // Tạo một file tạm để giữ lại các bản ghi đồng bộ thất bại (nếu mạng chập chờn)
    File tempFile = LittleFS.open("/temp.txt", FILE_WRITE);
    
    int syncedCount = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        // Chỉ đồng bộ tối đa 20 bản ghi mỗi chu kỳ để tránh nghẽn
        if (syncedCount < 20 && sendToGoogleSheets(line)) {
            syncedCount++;
            delay(200); // Khoảng nghỉ nhỏ giữa các lượt request
        } else {
            if (tempFile) tempFile.println(line); // Giữ lại bản ghi chưa gửi được
        }
    }
    file.close();
    if (tempFile) tempFile.close();

    LittleFS.remove(OFFLINE_FILE);
    if (LittleFS.exists("/temp.txt")) {
        LittleFS.rename("/temp.txt", OFFLINE_FILE);
    }

    Serial.printf("=> [ĐỒNG BỘ] Hoàn tất. Đã đồng bộ thành công %d bản ghi.\n", syncedCount);
}

// ==========================================
// 5. SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    // Khởi tạo LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("-> [LỖI] Khởi tạo LittleFS thất bại! Sẽ format lại...");
    } else {
        Serial.println("-> [OK] Khởi tạo LittleFS thành công.");
    }
    
    setup_wifi();
    client.setServer(THINGSBOARD_SERVER, 1883);

    Serial.println("-> Khởi tạo DHT22...");
    dht.begin();

    pinMode(LDR_PIN, INPUT);
    pinMode(PRESSURE_PIN, INPUT);
    pinMode(RAIN_PIN, INPUT);
    pinMode(WIND_PIN, INPUT);
    pinMode(GAS_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    Serial.println("-> Hệ thống khởi động hoàn tất!\n");
}

// ==========================================
// 6. LOOP
// ==========================================
void loop() {
    // Thử kết nối lại MQTT mỗi 10 giây nếu mất kết nối (Không chặn luồng)
    unsigned long now = millis();
    if (!client.connected()) {
        if (now - lastReconnectAttempt > 10000) {
            lastReconnectAttempt = now;
            if (reconnect()) {
                lastReconnectAttempt = 0;
            }
        }
    } else {
        client.loop();
    }

    // Đọc và xử lý dữ liệu mỗi 5 giây (Không dùng delay() lớn ở cuối loop)
    if (now - lastMsg > 5000) {
        lastMsg = now;

        // Đọc cảm biến
        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();
        if (isnan(temperature) || isnan(humidity)) { 
            temperature = 0.0; 
            humidity = 0.0; 
        }

        int rawPressure = analogRead(PRESSURE_PIN);
        float pressure = map(rawPressure, 0, 4095, 90000, 110000) / 100.0F; 

        int rawLux = analogRead(LDR_PIN);
        float lux = map(rawLux, 0, 4095, 0, 10000); 

        int rawRain = analogRead(RAIN_PIN);
        float rainIntensity = map(rawRain, 0, 4095, 0, 100);

        int rawWind = analogRead(WIND_PIN);
        float windSpeed = map(rawWind, 0, 4095, 0, 5000) / 100.0F;

        int rawGas = analogRead(GAS_PIN);
        int gasPPM = map(rawGas, 0, 4095, 0, 1000);

        float dewPoint = calculateDewPoint(temperature, humidity);

        // Xử lý Relay Cảnh báo
        bool isWarning = false;
        if (rainIntensity > 30.0 || windSpeed > 20.0 || gasPPM > 500) {
            digitalWrite(RELAY_PIN, HIGH);
            isWarning = true;
        } else {
            digitalWrite(RELAY_PIN, LOW);
        }

        // Tạo cấu trúc chuỗi Payload chung
        String payload = "{";
        payload += "\"temp\":" + String(temperature, 2) + ",";
        payload += "\"hum\":" + String(humidity, 1) + ",";
        payload += "\"pres\":" + String(pressure, 2) + ",";
        payload += "\"lux\":" + String(lux, 1) + ",";
        payload += "\"rain\":" + String(rainIntensity, 1) + ",";
        payload += "\"wind\":" + String(windSpeed, 2) + ",";
        payload += "\"gas\":" + String(gasPPM) + ",";
        payload += "\"dew\":" + String(dewPoint, 2) + ",";
        payload += "\"relay\":" + String(isWarning ? 1 : 0);
        payload += "}";

        // --- KIỂM TRA TRẠNG THÁI MẠNG ĐỂ XỬ LÝ ---
        if (WiFi.status() == WL_CONNECTED) {
            
            // 1. Gửi lên Thingsboard (chuyển đổi Key cho khớp cấu hình cũ của bạn)
            if (client.connected()) {
                String tb_payload = "{";
                tb_payload += "\"Nhiệt độ\":" + String(temperature, 2) + ",";
                tb_payload += "\"Độ ẩm\":" + String(humidity, 1) + ",";
                tb_payload += "\"Áp suất\":" + String(pressure, 2) + ",";
                tb_payload += "\"Ánh sáng\":" + String(lux, 1) + ",";
                tb_payload += "\"Lượng mưa\":" + String(rainIntensity, 1) + ",";
                tb_payload += "\"Tốc độ gió\":" + String(windSpeed, 2) + ",";
                tb_payload += "\"Khí gas\":" + String(gasPPM) + ",";
                tb_payload += "\"Điểm sương\":" + String(dewPoint, 2) + ",";
                tb_payload += "\"Trạng thái Relay\":" + String(isWarning ? 1 : 0);
                tb_payload += "}";
                
                client.publish("v1/devices/me/telemetry", tb_payload.c_str());
                Serial.println("=> [THINGSBOARD] Đã gửi dữ liệu trực tiếp.");
            }

            // 2. Gửi lên Google Sheets trực tiếp
            Serial.print("=> [GOOGLE SHEETS] Gửi trực tiếp... ");
            if (sendToGoogleSheets(payload)) {
                Serial.println("Thành công!");
            } else {
                Serial.println("Thất bại! Chuyển lưu offline.");
                saveOfflineData(payload);
            }

            // 3. Thực hiện kiểm tra và đồng bộ dữ liệu cũ (nếu có)
            syncOfflineData();

        } else {
            // Mất WiFi -> Lưu thẳng vào bộ nhớ Flash ngoại tuyến
            Serial.println("=> [MẤT KẾT NỐI] Hệ thống tự động chuyển sang lưu trữ ngoại tuyến.");
            saveOfflineData(payload);
        }
        
        Serial.println("-------------------------------");
    }
}