#include <ESP8266WiFi.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// 이전 약속한 사용자 규칙에 따른 OLED 핀 맵
#define OLED_SDA 14 // D5 (GPIO 14)
#define OLED_SCL 12 // D6 (GPIO 12)

// 패시브 부저 연결 핀 (D7 = GPIO 13)
#define BUZZER_PIN 13

// 와이파이 상세 정보
const char* ssid = "JUNSEOKIM";
const char* password = "91879717";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 부저 알림음 패턴 정의
void playConnectSound() {
  tone(BUZZER_PIN, 523, 100); delay(120); // 도
  tone(BUZZER_PIN, 659, 100); delay(120); // 미
  tone(BUZZER_PIN, 784, 150); delay(180); // 솔
  noTone(BUZZER_PIN);
}

void playSyncSound() {
  tone(BUZZER_PIN, 880, 80); delay(100);  // 라
  tone(BUZZER_PIN, 1047, 120); delay(150); // 높은 도
  noTone(BUZZER_PIN);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);

  // I2C 디스플레이 핀 설정 및 초기화
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // 1. 와이파이 연결 시도
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  
  int dotCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
    // OLED 대기 화면 렌더링
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("Connecting to WiFi:"));
    display.println(ssid);
    display.println();
    
    // 점이 늘어나는 프로그레스 바 형태 표현
    display.print(F("Connecting"));
    for (int i = 0; i < dotCount; i++) {
      display.print(F("."));
    }
    display.println();
    display.display();
    
    dotCount = (dotCount + 1) % 6;
  }

  // 와이파이 연결 성공
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // OLED에 IP 표시
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("WiFi Connected!"));
  display.println(F("---------------------"));
  display.print(F("IP: "));
  display.println(WiFi.localIP().toString());
  display.println(F("---------------------"));
  display.println(F("NTP Time Syncing..."));
  display.display();

  playConnectSound();
  delay(1000);

  // 2. NTP 서버 환경설정 (한국 표준시 UTC+9 = 9 * 3600초 적용, DST 적용 안함)
  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov", "time.windows.com");

  // NTP 서버로부터 정상 시간(1970년 기본값이 아닌 실제 타임스탬프)을 받아올 때까지 대기
  Serial.println("Waiting for NTP time sync...");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) { // 약 1970년 초기 값이 유지 중인 경우 대기
    delay(500);
    Serial.print("*");
    now = time(nullptr);
  }
  Serial.println("");
  Serial.println("Time synchronized!");

  playSyncSound();
}

void loop() {
  // 현재 시간 획득
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  // 시리얼 모니터 시간 출력
  Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo->tm_year + 1900,
                timeinfo->tm_mon + 1,
                timeinfo->tm_mday,
                timeinfo->tm_hour,
                timeinfo->tm_min,
                timeinfo->tm_sec);

  // 3. OLED 실시간 시계 화면 렌더링
  display.clearDisplay();

  // 상단 바 (바 스타일의 가로선과 Wi-Fi 신호 상태 강도 텍스트)
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("WiFi: "));
  display.print(ssid);
  
  // RSSI(수신 신호 강도)에 따른 막대바 또는 텍스트 표현
  long rssi = WiFi.RSSI();
  display.setCursor(95, 0);
  display.printf("%ddBm", rssi);
  
  display.drawLine(0, 10, SCREEN_WIDTH - 1, 10, SSD1306_WHITE);

  // 날짜 출력 (중앙상단)
  display.setTextSize(1);
  char dateBuffer[32];
  const char* wday_name[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  sprintf(dateBuffer, "%04d-%02d-%02d (%s)", 
          timeinfo->tm_year + 1900, 
          timeinfo->tm_mon + 1, 
          timeinfo->tm_mday,
          wday_name[timeinfo->tm_wday]);
  
  // 가로 방향 가운데 정렬 연산
  int dateWidth = strlen(dateBuffer) * 6; // 1사이즈 폰트 가로 6픽셀
  display.setCursor((SCREEN_WIDTH - dateWidth) / 2, 17);
  display.print(dateBuffer);

  // 시간 출력 (중앙, 큼직하게 2사이즈 폰트)
  display.setTextSize(2);
  char timeBuffer[16];
  sprintf(timeBuffer, "%02d:%02d:%02d", 
          timeinfo->tm_hour, 
          timeinfo->tm_min, 
          timeinfo->tm_sec);
  
  int timeWidth = strlen(timeBuffer) * 12; // 2사이즈 폰트 가로 12픽셀
  display.setCursor((SCREEN_WIDTH - timeWidth) / 2, 33);
  display.print(timeBuffer);

  // 하단 바 장식 (디자인 완성도)
  display.drawLine(0, 53, SCREEN_WIDTH - 1, 53, SSD1306_WHITE);
  display.setTextSize(1);
  
  char ipBuffer[32];
  sprintf(ipBuffer, "IP: %s", WiFi.localIP().toString().c_str());
  int ipWidth = strlen(ipBuffer) * 6;
  display.setCursor((SCREEN_WIDTH - ipWidth) / 2, 56);
  display.print(ipBuffer);

  display.display();
  
  // 1초 단위로 화면 업데이트 (NTP 서버에 부하를 주지 않고 로컬 RTC 기준 카운팅을 매초 렌더링)
  delay(1000);
}
