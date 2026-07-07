/**
 * Intercom Signal Generator (Bench Calibrator)
 *
 * Always-on: 800 Hz PWM with 100 Hz gap (9 ms ON, 1 ms OFF).
 * No button — starts on power-up.
 *
 * See: matter/generator technical specification
 */

#include <Arduino.h>
#include <U8g2lib.h>

#define SIGNAL_OUTPUT_PIN 1

#define OLED_SDA 5
#define OLED_SCL 6

const uint32_t CARRIER_HZ = 800;

// 100 Hz gate: 10 ms period, 1 ms gap
const uint32_t MOD_PERIOD_US = 10000;
const uint32_t GAP_US = 1000;
const uint32_t CARRIER_ON_US = MOD_PERIOD_US - GAP_US;

U8G2_SSD1306_72X40_ER_F_SW_I2C u8g2(U8G2_R0, /* clock=*/OLED_SCL, /* data=*/OLED_SDA,
                                     /* reset=*/U8X8_PIN_NONE);

const int X_OFFSET = 30;
const int Y_OFFSET = 12;
const int SCREEN_W = 72;
const int SCREEN_H = 40;

unsigned long last_update_time = 0;
const unsigned long FPS_DELAY = 33;
int wave_offset = 0;

bool gateOpen = false;

bool isGateOpenAtPhase(uint32_t phaseUs) { return phaseUs < CARRIER_ON_US; }

void applyGate(bool open) {
  if (open == gateOpen) {
    return;
  }
  gateOpen = open;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(SIGNAL_OUTPUT_PIN, open ? 128 : 0);
#else
  ledcWrite(0, open ? 128 : 0);
#endif
}

void startGenerator() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(SIGNAL_OUTPUT_PIN, CARRIER_HZ, 8);
#else
  ledcSetup(0, CARRIER_HZ, 8);
  ledcAttachPin(SIGNAL_OUTPUT_PIN, 0);
#endif
  gateOpen = false;
  applyGate(isGateOpenAtPhase(micros() % MOD_PERIOD_US));
}

void updateSignalGate() {
  uint32_t phase = micros() % MOD_PERIOD_US;
  applyGate(isGateOpenAtPhase(phase));
}

void drawModulatedWaveform() {
  const int env_y = Y_OFFSET + 22;
  const int env_h = 8;
  const int car_y = Y_OFFSET + 32;
  const int car_amp = 4;

  const int env_period_px = 24;
  const int gap_px = 2;

  for (int x = 0; x < SCREEN_W - 2; x++) {
    int phase_x = (x + wave_offset) % env_period_px;
    if (phase_x < env_period_px - gap_px) {
      u8g2.drawVLine(X_OFFSET + 1 + x, env_y, env_h);
    }
  }
  u8g2.drawHLine(X_OFFSET, env_y + env_h, SCREEN_W);

  const int car_period = 5;
  const int half = car_period / 2;

  for (int x = 0; x < SCREEN_W - 2; x++) {
    int phase_x = (x + wave_offset) % env_period_px;
    if (phase_x >= env_period_px - gap_px) {
      continue;
    }
    int car_phase = (x + wave_offset * 2) % car_period;
    int y = car_y + ((car_phase < half) ? -car_amp : car_amp);
    u8g2.drawPixel(X_OFFSET + 1 + x, y);
  }
}

void drawDisplay() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawFrame(X_OFFSET, Y_OFFSET, SCREEN_W, SCREEN_H);

  u8g2.setCursor(X_OFFSET + 2, Y_OFFSET + 7);
  u8g2.print("800Hz");
  u8g2.setCursor(X_OFFSET + 34, Y_OFFSET + 7);
  u8g2.print("-100");

  u8g2.drawHLine(X_OFFSET, Y_OFFSET + 10, SCREEN_W);
  drawModulatedWaveform();

  u8g2.sendBuffer();
  wave_offset++;
}

void setup() {
  Serial.begin(115200);

  {
    U8G2_SSD1306_128X64_NONAME_F_SW_I2C clearer(U8G2_R0, OLED_SCL, OLED_SDA,
                                                  U8X8_PIN_NONE);
    clearer.begin();
    clearer.clearBuffer();
    clearer.sendBuffer();
  }

  u8g2.begin();
  u8g2.setContrast(255);

  startGenerator();
  drawDisplay();

  Serial.println(F("Generator: 800 Hz, 100 Hz gap — always ON"));
}

void loop() {
  updateSignalGate();

  unsigned long now = millis();
  if (now - last_update_time >= FPS_DELAY) {
    last_update_time = now;
    drawDisplay();
  }
}
