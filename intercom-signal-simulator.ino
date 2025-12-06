#include "driver/ledc.h" 

// --- PIN DEFINITIONS ---
#define PWM_PIN_BUTTON 5    // Main pin (controlled by button) - Output to Intercom
#define PWM_PIN_TEST   6    // Test pin (ALWAYS ON) - Connect probe here to verify PWM generation
#define BUTTON_PIN     4    // Button pin

// --- PWM SETTINGS ---
#define PWM_TIMER      LEDC_TIMER_0      // Timer 0 for both channels
#define PWM_FREQ       800               // Frequency: 800 Hz
#define PWM_RESOLUTION LEDC_TIMER_14_BIT // Resolution: 14 bits

// Duty cycle 50% (2^14 / 2 = 8192)
const uint32_t DUTY_50_PERCENT = (1 << 14) / 2; 

void setup() {
  Serial.begin(115200);
  Serial.println("DEBUG MODE: Starting PWM...");

  // Setup button with internal pull-up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 1. TIMER CONFIGURATION (One timer shared by both channels)
  // We use explicit struct assignment to avoid compilation errors on some ESP32 cores
  ledc_timer_config_t ledc_timer;
  ledc_timer.speed_mode       = LEDC_LOW_SPEED_MODE;
  ledc_timer.timer_num        = PWM_TIMER;
  ledc_timer.duty_resolution  = PWM_RESOLUTION;
  ledc_timer.freq_hz          = PWM_FREQ;
  ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
  ledc_timer_config(&ledc_timer);

  // 2. CHANNEL 0 CONFIGURATION (For BUTTON -> GPIO 5)
  ledc_channel_config_t channel_button;
  channel_button.gpio_num   = PWM_PIN_BUTTON;
  channel_button.speed_mode = LEDC_LOW_SPEED_MODE;
  channel_button.channel    = LEDC_CHANNEL_0;     // Channel 0
  channel_button.intr_type  = LEDC_INTR_DISABLE;
  channel_button.timer_sel  = PWM_TIMER;
  channel_button.duty       = 0;                  // Initially OFF
  channel_button.hpoint     = 0;
  ledc_channel_config(&channel_button);

  // 3. CHANNEL 1 CONFIGURATION (For TEST -> GPIO 6)
  ledc_channel_config_t channel_test;
  channel_test.gpio_num   = PWM_PIN_TEST;
  channel_test.speed_mode = LEDC_LOW_SPEED_MODE;
  channel_test.channel    = LEDC_CHANNEL_1;       // Channel 1 (Different channel!)
  channel_test.intr_type  = LEDC_INTR_DISABLE;
  channel_test.timer_sel  = PWM_TIMER;
  channel_test.duty       = DUTY_50_PERCENT;      // Immediately ON (50%)
  channel_test.hpoint     = 0;
  ledc_channel_config(&channel_test);

  // IMPORTANT: Explicitly apply the duty cycle for the test channel to start it immediately
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
  
  Serial.println("Test PWM active on GPIO 6 (Always ON).");
}

void loop() {
  // Read button state
  // LOW means pressed (because of INPUT_PULLUP connection to GND)
  if (digitalRead(BUTTON_PIN) == LOW) {
    // Button pressed: Turn ON main channel (GPIO 5)
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, DUTY_50_PERCENT);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  } else {
    // Button released: Turn OFF main channel (GPIO 5)
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  }
  
  delay(50); // Small delay for debouncing
}
