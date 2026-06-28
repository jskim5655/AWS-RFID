#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_PN532.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// 이전 약속한 사용자 규칙에 따른 OLED 및 I2C 공유 핀 맵
#define OLED_SDA 14 // D5 (GPIO 14)
#define OLED_SCL 12 // D6 (GPIO 12)

// PN532 제어를 위한 하드웨어 IRQ, RESET 더미 핀 설정 (물리적으로 연결하지 않아도 됨)
#define PN532_IRQ   2  // D4 (GPIO 2)
#define PN532_RESET 0  // D3 (GPIO 0)

// 패시브 부저 연결 핀 (D7 = GPIO 13)
#define BUZZER_PIN 13

// 와이파이 상세 정보
const char* ssid = "JUNSEOKIM";
const char* password = "91879717";

// AWS 서버 API 주소
const char* serverApiUrl = "http://54.116.172.123/api/access.php";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_PN532 nfc((uint8_t)PN532_IRQ, (uint8_t)PN532_RESET, &Wire);

// -------------------------------------------------------------
// 부저 효과음 재생 함수군
// -------------------------------------------------------------

// 1. 부팅 / 와이파이 성공 멜로디 (도-미-솔-도)
void playBootSound() {
  tone(BUZZER_PIN, 262, 80);  delay(100);
  tone(BUZZER_PIN, 330, 80);  delay(100);
  tone(BUZZER_PIN, 392, 80);  delay(100);
  tone(BUZZER_PIN, 523, 150); delay(180);
  noTone(BUZZER_PIN);
}

// 2. 카드 감지 순간 비프음 (짧은 삑-)
void playDetectSound() {
  tone(BUZZER_PIN, 2000, 80); delay(100);
  noTone(BUZZER_PIN);
}

// 3. 서버 승인 완료 멜로디 (솔-도 높은음)
void playApprovedSound() {
  tone(BUZZER_PIN, 784, 80);  delay(100);
  tone(BUZZER_PIN, 1047, 180); delay(200);
  noTone(BUZZER_PIN);
}

// 4. 서버 승인 거절 멜로디 (낮은음 삐-삐- 2회)
void playDeniedSound() {
  for (int i = 0; i < 2; i++) {
    tone(BUZZER_PIN, 180, 200);
    delay(250);
  }
  noTone(BUZZER_PIN);
}

// -------------------------------------------------------------
// 단순 문자열 파싱 헬퍼 (JSON 라이브러리 의존성 우회용)
// -------------------------------------------------------------
String parseJsonValue(const String& json, const String& key) {
  String searchKey = "\"" + key + "\":\"";
  int startIdx = json.indexOf(searchKey);
  if (startIdx == -1) {
    // 키 이름 뒤에 공백이 있거나 쌍따옴표가 없는 등 다른 형태 대응
    searchKey = "\"" + key + "\":";
    startIdx = json.indexOf(searchKey);
    if (startIdx == -1) return "";
    
    int valStart = startIdx + searchKey.length();
    // 숫자인 경우 파싱
    int endIdx1 = json.indexOf(",", valStart);
    int endIdx2 = json.indexOf("}", valStart);
    int endIdx = (endIdx1 == -1) ? endIdx2 : min(endIdx1, endIdx2);
    if (endIdx == -1) return "";
    
    String val = json.substring(valStart, endIdx);
    val.replace("\"", ""); // 쌍따옴표 있을 시 제거
    val.trim();
    return val;
  }
  
  int valStart = startIdx + searchKey.length();
  int endIdx = json.indexOf("\"", valStart);
  if (endIdx == -1) return "";
  
  return json.substring(valStart, endIdx);
}

// -------------------------------------------------------------
// 초기 설정 (Setup)
// -------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);

  // I2C 핀 공유 설정 (SDA=14, SCL=12)
  Wire.begin(OLED_SDA, OLED_SCL);

  // 1. OLED 디스플레이 초기화
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);             
  display.setTextColor(SSD1306_WHITE);        
  display.setCursor(0, 0);             
  display.println(F("Initializing System..."));
  display.println(F("Connecting PN532..."));
  display.display();
  delay(500);

  // 2. PN532 NFC 리더기 초기화
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.print(F("Didn't find PN53x board"));
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("[ERROR]"));
    display.println(F("PN532 Not Found!"));
    display.println(F("Check I2C Wiring &"));
    display.println(F("DIP Switch (ON-OFF)"));
    display.display();
    
    playDeniedSound();
    while (1);
  }
  nfc.SAMConfig();

  // 3. 와이파이 연결 시도
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("Connecting to WiFi..."));
  display.println(ssid);
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // 대기 모드 안내
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("  Aegis NFC Active  "));
  display.println(F("---------------------"));
  display.println(F(" Ready for Scanning "));
  display.println(F(" Place your RFID Tag "));
  display.println(F("---------------------"));
  display.print(F("IP: "));
  display.println(WiFi.localIP().toString());
  display.display();

  // 시작 성공 알림 멜로디 재생
  playBootSound();
}

// -------------------------------------------------------------
// 메인 루프 (Loop)
// -------------------------------------------------------------
void loop() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // UID 저장 버퍼
  uint8_t uidLength;                        // UID 길이
  
  // 카드가 감지될 때까지 최대 400ms 동안 대기 (논블로킹 루프)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 400);
  
  if (success) {
    // 1. 카드 접촉 비프음 재생 (인식 피드백 제공)
    playDetectSound();

    // 2. UID를 Hex String 포맷으로 변환 (예: "84 AC A8 A0")
    String uidStr = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) uidStr += "0";
      uidStr += String(uid[i], HEX);
      if (i < uidLength - 1) uidStr += " ";
    }
    uidStr.toUpperCase();

    Serial.print("Detected Card UID: ");
    Serial.println(uidStr);

    // OLED 화면에 전송 중 상태 표시
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F(">> CARD DETECTED <<"));
    display.println(F("---------------------"));
    display.print(F("UID: ")); display.println(uidStr);
    display.println(F("Verifying with AWS..."));
    display.display();

    // 3. AWS EC2 웹 서버로 HTTP POST 요청 날리기
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;

      if (http.begin(client, serverApiUrl)) {
        http.addHeader("Content-Type", "application/json");
        
        // JSON Body 데이터 생성
        String postBody = "{\"uid\":\"" + uidStr + "\"}";
        
        Serial.print("Sending POST request to AWS: ");
        Serial.println(postBody);

        int httpCode = http.POST(postBody);
        
        if (httpCode > 0) {
          String payload = http.getString();
          Serial.print("Response Payload: ");
          Serial.println(payload);

          // JSON 응답 수동 파싱
          String status = parseJsonValue(payload, "status");
          String name = parseJsonValue(payload, "name");
          String dept = parseJsonValue(payload, "dept");

          display.clearDisplay();
          display.setCursor(0, 0);

          if (status == "APPROVED") {
            // [승인 완료]
            display.println(F("   >>> ACCESS OK <<<   "));
            display.println(F("-----------------------"));
            display.print(F("Name : ")); display.println(name);
            display.print(F("Dept : ")); display.println(dept);
            display.println(F("-----------------------"));
            display.println(F("      Welcome!         "));
            display.display();
            
            // 승인 성공음 재생
            playApprovedSound();
          } else {
            // [승인 거부 - 미등록 또는 차단]
            display.println(F("  >>> ACCESS DENIED <<< "));
            display.println(F("-----------------------"));
            display.println(F("   Unregistered Tag    "));
            display.print(F("UID  : ")); display.println(uidStr);
            display.println(F("-----------------------"));
            display.println(F("  Register on Portal   "));
            display.display();
            
            // 거절 경고음 재생
            playDeniedSound();
          }
        } else {
          // HTTP 통신 코드 에러
          Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
          display.clearDisplay();
          display.setCursor(0, 0);
          display.println(F("[SERVER ERROR]"));
          display.println(F("Connection Failed!"));
          display.print(F("Err: ")); display.println(http.errorToString(httpCode));
          display.display();
          playDeniedSound();
        }
        http.end();
      }
    } else {
      // 와이파이 단절 상태
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(F("[NETWORK ERROR]"));
      display.println(F("WiFi Disconnected!"));
      display.display();
      playDeniedSound();
    }

    // 3초 동안 결과를 화면에 유지
    delay(3000);

    // 대기 화면으로 자동 복구
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("  Aegis NFC Active  "));
    display.println(F("---------------------"));
    display.println(F(" Ready for Scanning "));
    display.println(F(" Place your RFID Tag "));
    display.println(F("---------------------"));
    display.print(F("IP: "));
    display.println(WiFi.localIP().toString());
    display.display();
  }
}
