/**
 * Intercom Signal Generator (Bench Calibrator)
 *
 * Simulates analog intercom call signal (±4V AC) for testing receivers.
 * Hardware: ESP32-C3 0.42" OLED board, IRLB8743 MOSFET, SSD1306 72x40.
 *
 * See: matter/generator technical specification
 */

#include <Arduino.h>
#include <U8g2lib.h>

#define SIGNAL_OUTPUT_PIN 1
#define BUTTON_PIN 3

#define OLED_SDA 5
#define OLED_SCL 6

U8G2_SSD1306_72X40_ER_F_SW_I2C u8g2(U8G2_R0, /* clock=*/OLED_SCL, /* data=*/OLED_SDA,
                                     /* reset=*/U8X8_PIN_NONE);

const int X_OFFSET = 30;
const int Y_OFFSET = 12;
const int SCREEN_W = 72;
const int SCREEN_H = 40;

const uint32_t TEST_FREQUENCIES[] = {0, 400, 800, 1200, 1600, 2000};
const int FREQ_COUNT = sizeof(TEST_FREQUENCIES) / sizeof(TEST_FREQUENCIES[0]);
int current_freq_index = 2;

int last_button_state = HIGH;
int button_state = HIGH;
unsigned long last_debounce_time = 0;
const unsigned long DEBOUNCE_DELAY = 50;

unsigned long last_update_time = 0;
const unsigned long FPS_DELAY = 33;
int wave_offset = 0;

void setGeneratorFrequency(uint32_t freq) {
  if (freq == 0) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(SIGNAL_OUTPUT_PIN, 0);
#else
    ledcWrite(0, 0);
#endif
    return;
  }

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(SIGNAL_OUTPUT_PIN, freq, 8);
  ledcWrite(SIGNAL_OUTPUT_PIN, 128);
#else
  ledcSetup(0, freq, 8);
  ledcAttachPin(SIGNAL_OUTPUT_PIN, 0);
  ledcWrite(0, 128);
#endif
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  {
    U8G2_SSD1306_128X64_NONAME_F_SW_I2C clearer(U8G2_R0, OLED_SCL, OLED_SDA,
                                                  U8X8_PIN_NONE);
    clearer.begin();
    clearer.clearBuffer();
    clearer.sendBuffer();
  }

  u8g2.begin();
  u8g2.setContrast(255);
  u8g2.clearBuffer();

  setGeneratorFrequency(TEST_FREQUENCIES[current_freq_index]);
  Serial.println(F("Bench Generator Activated."));
}

void loop() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != last_button_state) {
    last_debounce_time = millis();
  }

  if ((millis() - last_debounce_time) > DEBOUNCE_DELAY) {
    if (reading != button_state) {
      button_state = reading;
      if (button_state == LOW) {
        current_freq_index = (current_freq_index + 1) % FREQ_COUNT;
        uint32_t new_freq = TEST_FREQUENCIES[current_freq_index];
        setGeneratorFrequency(new_freq);
        Serial.printf("[GEN] Active frequency: %d Hz\n", new_freq);
      }
    }
  }
  last_button_state = reading;

  unsigned long now = millis();
  if (now - last_update_time >= FPS_DELAY) {
    last_update_time = now;
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tr);

    u8g2.drawFrame(X_OFFSET, Y_OFFSET, SCREEN_W, SCREEN_H);

    u8g2.setCursor(X_OFFSET + 4, Y_OFFSET + 7);
    u8g2.print("P:1");
    u8g2.setCursor(X_OFFSET + 32, Y_OFFSET + 7);
    u8g2.print("+/-4.0V");

    u8g2.setCursor(X_OFFSET + 4, Y_OFFSET + 15);
    u8g2.print("F:");
    uint32_t active_freq = TEST_FREQUENCIES[current_freq_index];
    if (active_freq > 0) {
      u8g2.print(active_freq);
      u8g2.print("Hz");
    } else {
      u8g2.print("OFF");
    }

    u8g2.drawHLine(X_OFFSET, Y_OFFSET + 18, SCREEN_W);

    int wave_y_center = Y_OFFSET + 29;
    int wave_amplitude = 7;

    if (active_freq > 0) {
      int period = 4000 / active_freq;
      if (period < 4) period = 4;
      if (period > 24) period = 24;
      int half_period = period / 2;

      for (int x = 0; x < SCREEN_W - 2; x++) {
        int phase = (x + wave_offset) % period;
        int y_offset = (phase < half_period) ? -wave_amplitude : wave_amplitude;

        u8g2.drawPixel(X_OFFSET + 1 + x, wave_y_center + y_offset);

        if (phase == 0 || phase == half_period) {
          u8g2.drawVLine(X_OFFSET + 1 + x, wave_y_center - wave_amplitude,
                         wave_amplitude * 2);
        }
      }
    } else {
      u8g2.drawHLine(X_OFFSET + 1, wave_y_center, SCREEN_W - 2);
    }

    u8g2.sendBuffer();
    wave_offset++;
  }
}
