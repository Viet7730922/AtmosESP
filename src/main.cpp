#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ==========================================
// 1. CẤU HÌNH WIFI, THINGSBOARD & GOOGLE SHEETS
// ==========================================
#define WIFI_SSID           "Wokwi-GUEST"
#define WIFI_PASSWORD       ""
#define THINGSBOARD_SERVER  "eu.thingsboard.cloud" 
#define TOKEN               "OzpJ5mxL6G4R9p6wrRB1" 

// WEB APP URL CỦA GOOGLE SCRIPT (nên dùng production thay vì /dev)
String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbyEFNNzqOvaO1XTgBJ22ymy8vujSf0Bc4y5a7_L0PxItDxGsp8Yf0lpKvCXMPLb51RvDw/exec"; 

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

// ==========================================
// 4. HÀM TRỢ NĂNG
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
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[OK] Đã kết nối WiFi. IP: " + WiFi.localIP().toString());
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Đang kết nối đến ThingsBoard...");
        String clientId = "ESP32Client-" + String(random(0xffff), HEX);
        if (client.connect(clientId.c_str(), TOKEN, "")) {
            Serial.println(" [THÀNH CÔNG]");
        } else {
            Serial.print(" [THẤT BẠI] mã lỗi: ");
            Serial.print(client.state());
            Serial.println(" -> Thử lại sau 5 giây");
            delay(5000);
        }
    }
}

// ==========================================
// HÀM GỬI DỮ LIỆU LÊN GOOGLE SHEETS (ĐÃ SỬA LỖI SSL)
// ==========================================
void sendToGoogleSheets(float temp, float hum, float pres, float lux, 
                       float rain, float wind, int gas, float dew, int relay) {
    
    WiFiClientSecure secureClient;
    secureClient.setInsecure();                    // Bỏ qua kiểm tra chứng chỉ
    // secureClient.setTimeout(15000);             

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // Ngăn redirect gây lỗi

    http.begin(secureClient, GOOGLE_SCRIPT_URL);
    http.addHeader("Content-Type", "application/json");

    String gs_payload = "{";
    gs_payload += "\"temp\":" + String(temp, 2) + ",";
    gs_payload += "\"hum\":" + String(hum, 1) + ",";
    gs_payload += "\"pres\":" + String(pres, 2) + ",";
    gs_payload += "\"lux\":" + String(lux, 1) + ",";
    gs_payload += "\"rain\":" + String(rain, 1) + ",";
    gs_payload += "\"wind\":" + String(wind, 2) + ",";
    gs_payload += "\"gas\":" + String(gas) + ",";
    gs_payload += "\"dew\":" + String(dew, 2) + ",";
    gs_payload += "\"relay\":" + String(relay);
    gs_payload += "}";

    Serial.print("=> [GOOGLE SHEETS] Đang gửi dữ liệu... ");
    
    int httpResponseCode = http.POST(gs_payload);
    
    if (httpResponseCode > 0) {
        Serial.print("Thành công! Mã: ");
        Serial.println(httpResponseCode);
        String response = http.getString();
        if (response.length() > 0 && response.length() < 100) {
            Serial.print(" | Phản hồi: ");
            Serial.println(response);
        } else {
            Serial.println(" | OK");
        }
    } else {
        Serial.print("Lỗi HTTP: ");
        Serial.println(httpResponseCode);
        Serial.println("Chi tiết: " + http.errorToString(httpResponseCode));
    }
    
    http.end();
}

// ==========================================
// 5. SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(2000);
    
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
    if (!client.connected()) reconnect();
    client.loop(); 

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

    // Xử lý Relay
    bool isWarning = false;
    if (rainIntensity > 30.0 || windSpeed > 20.0 || gasPPM > 500) {
        digitalWrite(RELAY_PIN, HIGH);
        isWarning = true;
    } else {
        digitalWrite(RELAY_PIN, LOW);
    }

    // ==========================================
    // 7. GỬI DỮ LIỆU LÊN THINGSBOARD
    // ==========================================
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

    String payload = "{";
    payload += "\"temperature\":" + String(temperature, 2) + ",";
    payload += "\"humidity\":" + String(humidity, 1) + ",";
    payload += "\"pressure\":" + String(pressure, 2) + ",";
    payload += "\"light\":" + String(lux, 1) + ",";
    payload += "\"rain\":" + String(rainIntensity, 1) + ",";
    payload += "\"wind_speed\":" + String(windSpeed, 2) + ",";
    payload += "\"gas_ppm\":" + String(gasPPM) + ",";
    payload += "\"dew_point\":" + String(dewPoint, 2) + ",";
    payload += "\"relay_status\":" + String(isWarning ? 1 : 0);
    payload += "}";

    Serial.println("=> [THINGSBOARD] Payload: " + tb_payload);

    if (client.publish("v1/devices/me/telemetry", payload.c_str())) {
        Serial.println("=> [THINGSBOARD] Đã gửi dữ liệu thành công");
    } else {
        Serial.println("=> [THINGSBOARD] Gửi thất bại");
    }

    // ==========================================
    // 8. GỬI DỮ LIỆU LÊN GOOGLE SHEETS
    // ==========================================
    // sendToGoogleSheets(temperature, humidity, pressure, lux, 
    //                   rainIntensity, windSpeed, gasPPM, dewPoint, 
    //                   isWarning ? 1 : 0);

    Serial.println("-------------------------------");
    delay(5000); 
}