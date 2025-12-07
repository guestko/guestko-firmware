#include "driver/ledc.h" 

// --- PIN DEFINITIONS ---
#define PWM_PIN_BUTTON 5    // Выход на Домофон (управляется кнопкой)
#define PWM_PIN_TEST   6    // Тестовый выход (Всегда работает)
#define BUTTON_PIN     4    // Кнопка (на GND)

// --- PWM SETTINGS ---
#define PWM_TIMER       LEDC_TIMER_0      
#define PWM_FREQ        800               
#define PWM_RESOLUTION  LEDC_TIMER_14_BIT 

// 50% скважность
const uint32_t DUTY_50_PERCENT = (1 << 14) / 2; 

void setup() {
  Serial.begin(115200);
  Serial.println("GENERATOR STARTING...");
  Serial.println("Mode: Idle = LOW (0V), Active = PWM");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 1. Настройка таймера
  ledc_timer_config_t ledc_timer = {
    .speed_mode       = LEDC_LOW_SPEED_MODE,
    .duty_resolution  = PWM_RESOLUTION,
    .timer_num        = PWM_TIMER,
    .freq_hz          = PWM_FREQ,
    .clk_cfg          = LEDC_AUTO_CLK
  };
  ledc_timer_config(&ledc_timer);

  // 2. Настройка канала КНОПКИ (GPIO 5)
  ledc_channel_config_t channel_button = {
    .gpio_num       = PWM_PIN_BUTTON,
    .speed_mode     = LEDC_LOW_SPEED_MODE,
    .channel        = LEDC_CHANNEL_0,
    .intr_type      = LEDC_INTR_DISABLE,
    .timer_sel      = PWM_TIMER,
    .duty           = 0,
    .hpoint         = 0
  };
  ledc_channel_config(&channel_button);
  
  // ВАЖНО: Сразу принудительно выключаем канал в 0 (LOW)
  ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0); 

  // 3. Настройка канала ТЕСТ (GPIO 6)
  ledc_channel_config_t channel_test = {
    .gpio_num       = PWM_PIN_TEST,
    .speed_mode     = LEDC_LOW_SPEED_MODE,
    .channel        = LEDC_CHANNEL_1,
    .intr_type      = LEDC_INTR_DISABLE,
    .timer_sel      = PWM_TIMER,
    .duty           = DUTY_50_PERCENT,
    .hpoint         = 0
  };
  ledc_channel_config(&channel_test);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1); // Запускаем тест сразу
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    // Кнопка нажата -> Генерируем ШИМ
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, DUTY_50_PERCENT);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  } else {
    // Кнопка отпущена -> СТОП (Уровень 0)
    // ledc_stop принудительно прижимает пин к указанному уровню (0)
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0); 
  }
  
  delay(50); 
}
