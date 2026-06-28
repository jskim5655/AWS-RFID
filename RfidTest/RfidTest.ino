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

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_PN532 nfc((uint8_t)PN532_IRQ, (uint8_t)PN532_RESET, &Wire);

// 경쾌한 시작 멜로디 (도-미-솔-도)
void playBootSound() {
  tone(BUZZER_PIN, 262, 80);  delay(100);
  tone(BUZZER_PIN, 330, 80);  delay(100);
  tone(BUZZER_PIN, 392, 80);  delay(100);
  tone(BUZZER_PIN, 523, 150); delay(180);
  noTone(BUZZER_PIN);
}

// 명확한 카드 인식 알림음 (하이톤 비프)
void playCardSound() {
  tone(BUZZER_PIN, 1000, 120); delay(150);
  noTone(BUZZER_PIN);
}

void setup() {
  Serial.begin(115200);
  
  // 부저 핀 출력 모드 설정
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println(F("\n=== RFID Reader, OLED & Buzzer Test ==="));

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
  display.println(F("OLED Initialized."));
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
    
    // 에러 발생 경고음 (삐-삐-삐- 낮은 소리)
    for (int i = 0; i < 3; i++) {
      tone(BUZZER_PIN, 150, 300);
      delay(400);
    }
    while (1); // 멈춤
  }
  
  // 펌웨어 버전 정보 추출
  Serial.print(F("Found chip PN5")); 
  Serial.println((versiondata>>24) & 0xFF, HEX); 
  
  // 리더기 SAM 설정 (보안 인증 해제 및 일반 카드 읽기 모드 세팅)
  nfc.SAMConfig();
  
  Serial.println(F("Waiting for an ISO14443A Card ..."));

  // OLED에 대기 안내 출력
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("System Init OK!"));
  display.println(F("---------------------"));
  display.println(F("Place your RFID Tag"));
  display.println(F("near the reader..."));
  display.display();

  // 모든 장치가 성공적으로 초기화된 후 부팅 사운드 재생
  playBootSound();
}

void loop() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // UID를 저장할 버퍼
  uint8_t uidLength;                        // UID 길이 (4바이트 또는 7바이트)
    
  // 카드가 감지될 때까지 최대 500ms 동안 대기 (논블로킹 구현)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 500);
  
  if (success) {
    // 1. 카드 인식음 출력 (비프음)
    playCardSound();

    // 2. 시리얼 모니터 출력
    Serial.println(F("Found an ISO14443A card"));
    Serial.print(F("  UID Length: ")); Serial.print(uidLength, DEC); Serial.println(F(" bytes"));
    Serial.print(F("  UID Value: "));
    nfc.PrintHex(uid, uidLength);
    Serial.println(F(""));

    // 3. OLED 화면 표시
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F(">> CARD DETECTED <<"));
    display.println(F("---------------------"));
    
    display.print(F("Type: ISO14443A"));
    display.print(F("\nLength: ")); display.print(uidLength); display.println(F(" bytes"));
    
    display.println(F("UID Hex:"));
    display.setTextSize(2); // UID는 큼직하게 표기
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) display.print(F("0"));
      display.print(uid[i], HEX);
      if (i < uidLength - 1) display.print(F(" "));
    }
    
    display.display();
    
    // 카드 인식 완료 후 화면 유지 시간 대기
    delay(2000);

    // 다시 대기 화면으로 복귀
    display.setTextSize(1);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("PN532 Active"));
    display.println(F("---------------------"));
    display.println(F("Place your RFID Tag"));
    display.println(F("near the reader..."));
    display.display();
  }
}
