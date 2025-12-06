#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ==========================================
// --- USER CONFIGURATION (FILL THIS!) ---
// ==========================================
const char* ssid       = "SSID";      // Put your WiFi SSID here
const char* password   = "PASSWORD";  // Put your WiFi Password here
const char* botToken   = "BOT_TOKEN";      // From @BotFather
const char* chatID     = "CHAT_ID";        // From @IDBot (Your numeric ID)

// ==========================================
// --- PIN DEFINITIONS ---
// ==========================================
const int PIN_SIGNAL_INPUT  = 3;  // ADC Input for 800Hz signal
const int PIN_BUTTON        = 4;  // Latching button (to GND)
const int PIN_LED_BREATHE   = 5;  // External LED/MOSFET (Controlled by button)
const int PIN_LED_INDICATOR = 8;  // Built-in LED/Relay (Active LOW)

// ==========================================
// --- OBJECTS & VARIABLES ---
// ==========================================
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

// Settings for detection
const int SIGNAL_THRESHOLD = 300; 
const int SAMPLE_WINDOW = 50;     // 50ms window
const int MIN_PULSES = 25;
const int MAX_PULSES = 60;

// Breathing LED variables
int fadeValue = 0;
int fadeStep = 2; 
unsigned long lastFadeUpdate = 0;
const int FADE_INTERVAL = 10;

// Telegram variables
unsigned long lastBotRun = 0;
const int BOT_INTERVAL = 2000; // Check messages every 2 seconds

// ==========================================
// --- HELPER FUNCTIONS ---
// ==========================================

// 1. Function to handle the breathing LED (Non-blocking)
void updateBreathingSystem() {
  // Check button state: LOW = Closed (GND), HIGH = Open
  bool isSwitchClosed = (digitalRead(PIN_BUTTON) == LOW);

  if (isSwitchClosed) {
    // Mode: Solid ON
    analogWrite(PIN_LED_BREATHE, 255);
    fadeValue = 255; // Reset fade so it starts smoothly when opened
  } else {
    // Mode: Breathing
    unsigned long now = millis();
    if (now - lastFadeUpdate > FADE_INTERVAL) {
      lastFadeUpdate = now;
      fadeValue += fadeStep;

      // Reverse direction at limits
      if (fadeValue >= 255) {
        fadeValue = 255;
        fadeStep = -fadeStep;
      } else if (fadeValue <= 0) {
        fadeValue = 0;
        fadeStep = -fadeStep;
      }
      analogWrite(PIN_LED_BREATHE, fadeValue);
    }
  }
}

// 2. Function to handle new Telegram messages (Updated with Buttons)
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    // Security check
    if (chat_id != chatID) {
      bot.sendMessage(chat_id, "Access Denied ⛔", "");
      continue;
    }

    // --- KEYBOARD CONFIGURATION ---
    // We create a JSON string that defines the button layout.
    // Format: [["Row1_Btn1", "Row1_Btn2"], ["Row2_Btn1"]]
    // resize_keyboard: true (makes buttons smaller/neater)
    String keyboardJson = "[[\"Open Door 🔓\", \"Status 📊\"], [\"/start\"]]";

    // --- COMMANDS ---

    // 1. START (Show the menu)
    if (text == "/start") {
      String welcome = "👋 Control Panel Ready.\nSelect an option below:";
      
      // Send message WITH the keyboard
      bot.sendMessageWithReplyKeyboard(chat_id, welcome, "", keyboardJson, true);
    }
    
    // 2. OPEN DOOR (Check for command OR button text)
    else if (text == "/open" || text == "Open Door 🔓") {
      bot.sendMessage(chat_id, "Opening door... 🔓", "");
      
      digitalWrite(PIN_LED_INDICATOR, LOW); // Active LOW -> ON
      delay(2000); 
      digitalWrite(PIN_LED_INDICATOR, HIGH); // Active LOW -> OFF
      
      bot.sendMessage(chat_id, "Door closed. 🔒", "");
    }

    // 3. STATUS (Check for command OR button text)
    else if (text == "/status" || text == "Status 📊") {
      String stats = "System Online ✅\n";
      stats += "WiFi Signal: " + String(WiFi.RSSI()) + " dBm\n";
      stats += "Waiting for signals...";
      bot.sendMessage(chat_id, stats, "");
    }
    
    // 4. UNKNOWN COMMAND
    else {
      bot.sendMessage(chat_id, "Unknown command. Press /start to reset buttons.", "");
    }
  }
}

// 3. The Telegram Task (Runs in background via FreeRTOS)
void telegramTask(void *pvParameters) {
  while (true) {
    // Check WiFi connection
    if (WiFi.status() == WL_CONNECTED) {
      unsigned long now = millis();
      
      // Poll Telegram every few seconds
      if (now - lastBotRun > BOT_INTERVAL) {
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        while (numNewMessages) {
          handleNewMessages(numNewMessages);
          numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        }
        lastBotRun = now;
      }
    }
    
    // CRITICAL: Yield to other tasks (Signal detection). 
    // Wait 10ms before checking loop again to save CPU.
    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
}

// ==========================================
// --- SETUP ---
// ==========================================
void setup() {
  Serial.begin(115200);

  // Pin Setup
  pinMode(PIN_SIGNAL_INPUT, INPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED_BREATHE, OUTPUT);
  pinMode(PIN_LED_INDICATOR, OUTPUT);
  digitalWrite(PIN_LED_INDICATOR, HIGH); // Start OFF (assuming active LOW)

  // WiFi Setup
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  client.setInsecure(); // Allow connection without strict certificate check

  // Wait for WiFi (with timeout so we don't hang forever)
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    // Send a message on boot to confirm it works
    bot.sendMessage(chatID, "System Started! 🚀", "");
  } else {
    Serial.println("\nWiFi Failed. Continuing in offline mode.");
  }

  // Create Telegram Task
  // We pin it to the same core, but FreeRTOS handles time slicing.
  xTaskCreate(
    telegramTask,   // Function to run
    "TelegramTask", // Name
    8192,           // Stack size (Telegram needs a lot)
    NULL,           // Parameters
    1,              // Priority (Low priority)
    NULL            // Task handle
  );
}

// ==========================================
// --- MAIN LOOP (High Priority) ---
// ==========================================
void loop() {
  // This loop handles the 800Hz detection and breathing LED.
  // It runs constantly. The Telegram task runs in the "gaps".

  unsigned long startMillis = millis();
  int pulseCount = 0;
  bool signalWasHigh = false;

  // Sampling Window (50ms)
  while (millis() - startMillis < SAMPLE_WINDOW) {
    // 1. Keep LED breathing smooth
    updateBreathingSystem();

    // 2. Read Signal
    int analogValue = analogRead(PIN_SIGNAL_INPUT);

    // Schmitt trigger logic
    if (!signalWasHigh && analogValue > SIGNAL_THRESHOLD) {
      signalWasHigh = true;
      pulseCount++; 
    } 
    else if (signalWasHigh && analogValue < (SIGNAL_THRESHOLD - 50)) {
      signalWasHigh = false;
    }
  }

  // Analysis
  if (pulseCount >= MIN_PULSES && pulseCount <= MAX_PULSES) {
    // Signal Detected!
    // We can notify Telegram here later, or trigger relay
    Serial.printf("Signal DETECTED! Pulses: %d\n", pulseCount);
    
    // Activate Relay/LED (Active LOW)
    digitalWrite(PIN_LED_INDICATOR, LOW); 
  } else {
    // Deactivate Relay/LED
    digitalWrite(PIN_LED_INDICATOR, HIGH); 
  }
}
