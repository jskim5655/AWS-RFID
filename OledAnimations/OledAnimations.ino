#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// 이전 약속한 사용자 규칙에 따른 OLED 핀 맵
#define OLED_SDA 14 // D5 (GPIO 14)
#define OLED_SCL 12 // D6 (GPIO 12)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 애니메이션 관리를 위한 전역 변수
int currentAnim = 0;
unsigned long lastSwitchTime = 0;
const unsigned long ANIM_DURATION = 5000; // 각 애니메이션 지속 시간 (5초)

// 1. Starfield (3D 별자리 터널) 구조체 및 전역 변수
#define NUM_STARS 40
struct Star {
  float x, y, z;
};
Star stars[NUM_STARS];

// 2. Sine/Cosine Wave Interference 변수
float wavePhase1 = 0;
float wavePhase2 = 0;

// 3. Rotating 3D Cube 변수
struct Point3D {
  float x, y, z;
};
// 큐브 꼭짓점 8개 (로컬 좌표 -10 ~ +10)
Point3D cubeVertices[8] = {
  {-10, -10, -10}, {10, -10, -10}, {10, 10, -10}, {-10, 10, -10},
  {-10, -10, 10},  {10, -10, 10},  {10, 10, 10},  {-10, 10, 10}
};
// 큐브 모서리 연결 인덱스 쌍 12개
int cubeEdges[12][2] = {
  {0, 1}, {1, 2}, {2, 3}, {3, 0}, // 앞면
  {4, 5}, {5, 6}, {6, 7}, {7, 4}, // 뒷면
  {0, 4}, {1, 5}, {2, 6}, {3, 7}  // 연결 모서리
};
float angleX = 0;
float angleY = 0;
float angleZ = 0;

// 4. Expanding Geometric Web 변수
#define NUM_NODES 8
float nodeAngle[NUM_NODES];
float nodeSpeed[NUM_NODES];
float webRadius = 20.0;
bool expanding = true;

// 5. Digital Rain (Matrix) 변수
#define NUM_DROPS 15
struct RainDrop {
  int x;
  float y;
  float speed;
  int length;
};
RainDrop rainDrops[NUM_DROPS];

// 초기 설정 함수들
void initStarfield() {
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i].x = random(-64, 64);
    stars[i].y = random(-32, 32);
    stars[i].z = random(1, 100);
  }
}

void initGeometricWeb() {
  for (int i = 0; i < NUM_NODES; i++) {
    nodeAngle[i] = (i * 2 * M_PI) / NUM_NODES;
    nodeSpeed[i] = random(10, 30) / 1000.0; // 회전 속도
  }
}

void initDigitalRain() {
  for (int i = 0; i < NUM_DROPS; i++) {
    rainDrops[i].x = i * 9 + random(0, 4); // x좌표 분산 배치
    rainDrops[i].y = random(-60, 0);       // 화면 상단 밖에서 시작
    rainDrops[i].speed = random(15, 35) / 10.0;
    rainDrops[i].length = random(10, 25);  // 비줄기 길이
  }
}

void setup() {
  Serial.begin(115200);
  
  // 내장 I2C 핀 명시적 설정
  Wire.begin(OLED_SDA, OLED_SCL);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.display();

  // 각 애니메이션의 데이터 초기화
  randomSeed(analogRead(0));
  initStarfield();
  initGeometricWeb();
  initDigitalRain();

  lastSwitchTime = millis();
}

// -------------------------------------------------------------
// 1. Starfield (3D 우주 비행) 애니메이션
// -------------------------------------------------------------
void runStarfield() {
  display.clearDisplay();
  int cx = SCREEN_WIDTH / 2;
  int cy = SCREEN_HEIGHT / 2;

  for (int i = 0; i < NUM_STARS; i++) {
    // z 값을 감소시켜 화면 안쪽으로 깊어지게 전진
    stars[i].z -= 1.5;

    // 카메라 뒷부분으로 가거나 화면 가장자리를 벗어나면 초기화
    if (stars[i].z <= 0) {
      stars[i].x = random(-64, 64);
      stars[i].y = random(-32, 32);
      stars[i].z = 100;
    }

    // 3D 좌표를 2D 평면에 투영 (원근감 부여)
    int k = 40; // 투영 거리 상수
    int px = (int)((stars[i].x * k) / stars[i].z) + cx;
    int py = (int)((stars[i].y * k) / stars[i].z) + cy;

    // 화면 영역 안인 경우에만 픽셀을 그림 (가까워질수록 크게 표시)
    if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
      if (stars[i].z < 30) {
        // 가까운 별은 십자선이나 2x2 픽셀로 큼직하게 그리기
        display.fillRect(px, py, 2, 2, SSD1306_WHITE);
      } else {
        // 먼 별은 1픽셀 점으로 그리기
        display.drawPixel(px, py, SSD1306_WHITE);
      }
    }
  }
  display.display();
}

// -------------------------------------------------------------
// 2. Wave Interference (사인/코사인 간섭 파형) 애니메이션
// -------------------------------------------------------------
void runWaveInterference() {
  display.clearDisplay();
  
  int midY = SCREEN_HEIGHT / 2;

  // 두 파형을 x에 따라 조합하여 기하학적인 격자 물결 그리기
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    // 첫 번째 물결 계산 (sin)
    float y1 = sin(x * 0.05 + wavePhase1) * 15;
    // 두 번째 물결 계산 (cos)
    float y2 = cos(x * 0.08 + wavePhase2) * 10;
    
    // 두 파동의 덧셈/간섭
    int finalY1 = midY + (int)(y1 + y2);
    int finalY2 = midY - (int)(y1 - y2); // 상칭 파형

    display.drawPixel(x, finalY1, SSD1306_WHITE);
    display.drawPixel(x, finalY2, SSD1306_WHITE);
    
    // 점들을 가끔 선으로 이어 입체적인 기둥선 연출
    if (x % 4 == 0) {
      display.drawLine(x, finalY1, x, finalY2, SSD1306_WHITE);
    }
  }

  // 매 프레임 파동 위상 증가
  wavePhase1 += 0.05;
  wavePhase2 += 0.08;

  display.display();
}

// -------------------------------------------------------------
// 3. Rotating 3D Wireframe Cube (3D 큐브 회전) 애니메이션
// -------------------------------------------------------------
void run3DCube() {
  display.clearDisplay();
  int cx = SCREEN_WIDTH / 2;
  int cy = SCREEN_HEIGHT / 2;

  // 회전된 2D 꼭짓점 좌표를 저장할 배열
  int px[8];
  int py[8];

  // 3축 회전 연산
  for (int i = 0; i < 8; i++) {
    Point3D p = cubeVertices[i];

    // X축 회전
    float y1 = p.y * cos(angleX) - p.z * sin(angleX);
    float z1 = p.y * sin(angleX) + p.z * cos(angleX);

    // Y축 회전
    float x2 = p.x * cos(angleY) + z1 * sin(angleY);
    float z2 = -p.x * sin(angleY) + z1 * cos(angleY);

    // Z축 회전
    float x3 = x2 * cos(angleZ) - y1 * sin(angleZ);
    float y3 = x2 * sin(angleZ) + y1 * cos(angleZ);

    // 원근 투영 (Perspective projection)
    float zoom = 60.0;
    float distance = 40.0; // 카메라와의 거리
    px[i] = cx + (int)((x3 * zoom) / (z2 + distance));
    py[i] = cy + (int)((y3 * zoom) / (z2 + distance));
  }

  // 회전각 업데이트
  angleX += 0.03;
  angleY += 0.04;
  angleZ += 0.02;

  // 12개 모서리(선) 그리기
  for (int i = 0; i < 12; i++) {
    display.drawLine(px[cubeEdges[i][0]], py[cubeEdges[i][0]],
                     px[cubeEdges[i][1]], py[cubeEdges[i][1]], SSD1306_WHITE);
  }

  // 꼭짓점에 점(사각형) 찍기
  for (int i = 0; i < 8; i++) {
    display.fillRect(px[i] - 1, py[i] - 1, 3, 3, SSD1306_WHITE);
  }

  display.display();
}

// -------------------------------------------------------------
// 4. Expanding Geometric Web (기하학 그물망) 애니메이션
// -------------------------------------------------------------
void runGeometricWeb() {
  display.clearDisplay();
  int cx = SCREEN_WIDTH / 2;
  int cy = SCREEN_HEIGHT / 2;

  int x[NUM_NODES];
  int y[NUM_NODES];

  // 반경 팽창 / 수축 연산
  if (expanding) {
    webRadius += 0.4;
    if (webRadius >= 28.0) expanding = false;
  } else {
    webRadius -= 0.4;
    if (webRadius <= 12.0) expanding = true;
  }

  // 노드들의 좌표 계산
  for (int i = 0; i < NUM_NODES; i++) {
    nodeAngle[i] += nodeSpeed[i];
    x[i] = cx + (int)(cos(nodeAngle[i]) * webRadius);
    y[i] = cy + (int)(sin(nodeAngle[i]) * webRadius);

    // 꼭짓점에 작은 기하학 상자 그리기
    display.drawRect(x[i] - 2, y[i] - 2, 4, 4, SSD1306_WHITE);
  }

  // 노드들을 얽어 매는 선(웹) 그리기
  for (int i = 0; i < NUM_NODES; i++) {
    for (int j = i + 1; j < NUM_NODES; j++) {
      // 거리에 비례하여 선을 그리거나 건너뛰어 기하학적인 불규칙성 연출
      int distSq = (x[i] - x[j])*(x[i] - x[j]) + (y[i] - y[j])*(y[i] - y[j]);
      // 일정 거리 이내의 노드끼리만 그물 연결
      if (distSq < 1500) {
        display.drawLine(x[i], y[i], x[j], y[j], SSD1306_WHITE);
      }
    }
  }

  // 바깥 테두리 기하학 라인 추가
  for (int i = 0; i < NUM_NODES; i++) {
    int next = (i + 1) % NUM_NODES;
    display.drawLine(x[i], y[i], x[next], y[next], SSD1306_WHITE); // 독특한 엇갈린 교차 선 유도
  }

  display.display();
}

// -------------------------------------------------------------
// 5. Digital Rain (Matrix style) 애니메이션
// -------------------------------------------------------------
void runDigitalRain() {
  display.clearDisplay();

  for (int i = 0; i < NUM_DROPS; i++) {
    // 빗방울 아래로 이동
    rainDrops[i].y += rainDrops[i].speed;

    // 시작점 및 끝점 계산
    int startY = (int)rainDrops[i].y;
    int endY = startY - rainDrops[i].length;

    // 화면 범위 내에서 세로선(빗줄기) 그리기
    if (startY >= 0 && endY < SCREEN_HEIGHT) {
      display.drawLine(rainDrops[i].x, max(0, endY), rainDrops[i].x, min(SCREEN_HEIGHT - 1, startY), SSD1306_WHITE);
      // 가장 앞쪽 빗방울 머리 부분을 밝게(2x2 도트로 강조) 그리기
      if (startY < SCREEN_HEIGHT) {
        display.fillRect(rainDrops[i].x - 1, startY, 3, 2, SSD1306_WHITE);
      }
    }

    // 빗줄기가 화면 밑으로 완전히 넘어가면 재초기화
    if (endY > SCREEN_HEIGHT) {
      rainDrops[i].y = random(-40, 0);
      rainDrops[i].speed = random(15, 35) / 10.0;
      rainDrops[i].length = random(10, 25);
    }
  }

  display.display();
}

// -------------------------------------------------------------
// 메인 루프 (5초마다 순환 전환)
// -------------------------------------------------------------
void loop() {
  unsigned long now = millis();

  // 5초(5000ms) 경과 시 다음 애니메이션으로 변경
  if (now - lastSwitchTime >= ANIM_DURATION) {
    currentAnim = (currentAnim + 1) % 5;
    lastSwitchTime = now;
  }

  // 애니메이션 실행 분기
  switch (currentAnim) {
    case 0:
      runStarfield();
      break;
    case 1:
      runWaveInterference();
      break;
    case 2:
      run3DCube();
      break;
    case 3:
      runGeometricWeb();
      break;
    case 4:
      runDigitalRain();
      break;
  }

  // 프레임 제어 (약 30fps)
  delay(30);
}
