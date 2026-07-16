/*
 * ESP32-C3 SuperMini + встроенный OLED 0.42" (72×40)
 * Смесь 50 Гц + 850 Гц → RC ФНЧ → XH-A902 (×5) → 9 Vpp
 *
 * Схемотехника ФНЧ:
 *   GPIO1 ──[10 кОм]──┬──► INL + INR (XH-A902)
 *                    │
 *                 [4.7 нФ]
 *                    │
 *                   GND
 *
 * Встроенный OLED (I2C, адрес 0x3C):
 *   SDA = GPIO5,  SCL = GPIO6,  видимая область 72×40 px
 *
 * Зависимости (Arduino Library Manager):
 *   - U8g2  (olikraus)
 *
 * Плата: ESP32C3 Dev Module
 * Board: ESP32 Arduino Core 3.x
 * Включить: USB CDC On Boot → Enabled
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <math.h>

// ── Пины ──────────────────────────────────────────────────────────────────
const int PWM_PIN = 1;   // ШИМ → R 10 кОм → преамп (пин «1» на плате)

// Встроенный OLED SuperMini: GPIO5=SDA, GPIO6=SCL
#define OLED_SDA 5
#define OLED_SCL 6

// Нативный конструктор 72×40 — без ручных offset
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

// ── Параметры сигнала ──────────────────────────────────────────────────────
const float F1          = 50.0f;
const float F2          = 850.0f;
const float FS          = 40000.0f;
const float TS_US       = 1000000.0f / FS;
const float MIX_F1      = 0.7f;
const float MIX_F2      = 0.3f;
const float SCALE       = 0.545f;
const float TARGET_VPP  = 9.0f;

const int PWM_CARRIER   = 100000;
const int PWM_BITS      = 8;

// ── Состояние ─────────────────────────────────────────────────────────────
volatile bool signal_running = false;

bool display_ok         = false;
bool blink_state        = true;
unsigned long last_blink_ms   = 0;
unsigned long last_refresh_ms = 0;
unsigned long boot_ms         = 0;

const unsigned long BLINK_MS   = 500;
const unsigned long REFRESH_MS = 400;
const unsigned long BOOT_MS    = 1500;

// ── Задача генерации сигнала ──────────────────────────────────────────────
void signalTask(void *param) {
  unsigned long next_us = micros();
  unsigned long n = 0;

  for (;;) {
    unsigned long now = micros();

    if (now >= next_us) {
      next_us = now + (unsigned long)TS_US;

      float t = (float)n / FS;
      float y = SCALE * (MIX_F1 * sinf(2.0f * PI * F1 * t)
                       + MIX_F2 * sinf(2.0f * PI * F2 * t));

      int pwm_val = (int)(127.5f * (1.0f + y));
      pwm_val = constrain(pwm_val, 0, 255);
      ledcWrite(PWM_PIN, pwm_val);

      n++;
      if (n >= (unsigned long)FS) {
        n = 0;
      }
    }

    taskYIELD();
  }
}

// ── Дисплей ───────────────────────────────────────────────────────────────
static String formatUptime(unsigned long ms) {
  unsigned long sec = ms / 1000;
  unsigned int m = sec / 60;
  unsigned int s = sec % 60;
  char buf[8];
  snprintf(buf, sizeof(buf), "%02u:%02u", m, s);
  return String(buf);
}

void showBootScreen() {
  if (!display_ok) return;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(4, 14, "SIGNAL GEN");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(10, 28, "ESP32-C3");
  u8g2.drawStr(6, 36, "Starting...");
  u8g2.sendBuffer();
}

void showRunningScreen() {
  if (!display_ok) return;

  unsigned long uptime = millis() - boot_ms;
  char line[24];

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);

  // Строка 1: статус + мигающая точка
  u8g2.drawStr(0, 6, signal_running ? "RUN" : "STOP");
  if (blink_state) {
    u8g2.drawDisc(66, 3, 2);
  } else {
    u8g2.drawCircle(66, 3, 2);
  }

  // Строка 2: частоты
  snprintf(line, sizeof(line), "%.0f+%.0f Hz", F1, F2);
  u8g2.drawStr(0, 16, line);

  // Строка 3: соотношение и Vpp
  snprintf(line, sizeof(line), "%.0f/%.0f%% %.1fVpp",
           MIX_F1 * 100, MIX_F2 * 100, TARGET_VPP);
  u8g2.drawStr(0, 26, line);

  // Строка 4: аптайм
  snprintf(line, sizeof(line), "T:%s", formatUptime(uptime).c_str());
  u8g2.drawStr(0, 36, line);

  u8g2.sendBuffer();
}

bool initDisplay() {
  u8g2.begin();
  u8g2.setContrast(255);
  u8g2.setBusClock(400000);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  return true;
}

void updateDisplay() {
  if (!display_ok) return;

  unsigned long now = millis();

  if (now - last_blink_ms >= BLINK_MS) {
    last_blink_ms = now;
    blink_state = !blink_state;
  }

  if (now - boot_ms < BOOT_MS) {
    return;
  }

  if (now - last_refresh_ms >= REFRESH_MS) {
    last_refresh_ms = now;
    showRunningScreen();
  }
}

// ── Setup / Loop ──────────────────────────────────────────────────────────
void setup() {
  boot_ms = millis();

  Serial.begin(115200);
  delay(500);

  ledcAttach(PWM_PIN, PWM_CARRIER, PWM_BITS);

  display_ok = initDisplay();
  if (display_ok) {
    showBootScreen();
  }

  signal_running = true;
  xTaskCreate(signalTask, "signal", 4096, NULL, 2, NULL);

  Serial.println("Signal generator started (SuperMini 72x40 OLED)");
}

void loop() {
  updateDisplay();
  delay(10);
}
