#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// ==========================================
// --- USER CONFIGURATION (FILL THIS!) ---
// ==========================================
const char* ssid       = "SSID";      // Put your WiFi SSID here
const char* password   = "PASSWORD";  // Put your WiFi Password here
const char* botToken   = "BOT_TOKEN";      // From @BotFather
const char* chatID     = "CHAT_ID";        // From @IDBot (Your numeric ID)

// ==========================================
// --- PINS ---
// ==========================================
const int PIN_SIGNAL_INPUT  = 3;  // ADC Input (Connected to RC Filter)
const int PIN_BUTTON        = 4;  // Physical Switch (Arm/Disarm)
const int PIN_LED_BREATHE   = 5;  // LED
const int PIN_RELAY         = 10; // Relay (HIGH = OPEN)

// ==========================================
// --- OBJECTS ---
// ==========================================
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);
Preferences preferences;

// Relay Logic
const int RELAY_ACTIVE_LEVEL = HIGH; 
const int RELAY_INACTIVE_LEVEL = LOW;
const unsigned long RELAY_DURATION = 4000; 
unsigned long relayOnTime = 0;
bool isRelayActive = false;

// Code Logic
String secretCode = "";    
String inputBuffer = "";   
String tempCode = "";      

// --- SETTINGS ---
int settingGap = 200;       // Gap/Debounce (ms)
int settingLongPress = 1000;// Long Press Threshold (ms)

// --- SIGNAL THRESHOLD (DC LOGIC) ---
// Active signal is ~0.38V (ADC ~470). Idle is 0V.
// We set threshold at ~0.16V (ADC 200) to be safe.
const int SIGNAL_THRESHOLD = 200; 

// Debounce / Logic Variables
unsigned long currentPressStartTime = 0;
unsigned long lastSignalSeenTime = 0;
bool isPressing = false; 
const int CODE_TIMEOUT = 2000;    // Time to wait after sequence ends
const int MAX_PRESS_LIMIT = 5000; // 5s limit for stuck button

// Bot State Machine
enum BotState { STATE_NORMAL, STATE_SETTING_CODE, STATE_SETTING_GAP, STATE_SETTING_LONG };
BotState currentBotState = STATE_NORMAL;
unsigned long lastInteractionTime = 0; 
const unsigned long MENU_TIMEOUT = 300000; 

// Breathing LED
int fadeValue = 0;
int fadeStep = 2; 
unsigned long lastFadeUpdate = 0;
int currentLedBrightness = -1;

// Telegram Timing
unsigned long lastBotRun = 0;
const int BOT_INTERVAL = 300; 

// Keyboards
String kbdMain = "[[\"Open 🔓\", \"Status 📊\"], [\"⚙️ Setup Code\", \"🛠 Settings\"]]";
String kbdSetup = "[[\"⚫ Short\", \"➖ Long\"], [\"💾 Save\", \"❌ Cancel\"]]";
String kbdSettings = "[[\"⏳ Set Gap (Bounce)\"], [\"⏱ Set Long Time\"], [\"🔙 Back\"]]";
String kbdCancelOnly = "[[\"❌ Cancel\"]]";

// ==========================================
// --- FUNCTIONS ---
// ==========================================

void setLedBrightness(int val) {
  if (val != currentLedBrightness) {
    analogWrite(PIN_LED_BREATHE, val);
    currentLedBrightness = val;
  }
}

void runBreathingAnimation() {
  unsigned long now = millis();
  if (now - lastFadeUpdate > 10) { 
    lastFadeUpdate = now;
    fadeValue += fadeStep;
    if (fadeValue >= 255) { fadeValue = 255; fadeStep = -fadeStep; }
    else if (fadeValue <= 0) { fadeValue = 0; fadeStep = -fadeStep; }
    setLedBrightness(fadeValue);
  }
}

void openDoor() {
  digitalWrite(PIN_RELAY, RELAY_ACTIVE_LEVEL);
  isRelayActive = true;
  relayOnTime = millis();
  Serial.println("DOOR OPENING");
}

void updateRelay() {
  if (isRelayActive) {
    if (millis() - relayOnTime > RELAY_DURATION) {
      digitalWrite(PIN_RELAY, RELAY_INACTIVE_LEVEL);
      isRelayActive = false;
      Serial.println("DOOR CLOSED");
    }
  }
}

String visualizeCode(String rawCode) {
  String visual = "";
  if (rawCode.length() == 0) return "(empty)";
  for (int i = 0; i < rawCode.length(); i++) {
    if (rawCode[i] == 'S') visual += "⚫ "; 
    if (rawCode[i] == 'L') visual += "➖ "; 
  }
  return visual;
}

// ==========================================
// --- TELEGRAM LOGIC ---
// ==========================================
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    yield(); 
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    if (chat_id != chatID) continue;
    lastInteractionTime = millis();

    if (text == "❌ Cancel" || text == "/cancel") {
      currentBotState = STATE_NORMAL;
      bot.sendMessageWithReplyKeyboard(chat_id, "Cancelled. 🏠", "", kbdMain, true);
      continue;
    }
    if (text == "/start" || text == "🔙 Back") {
      currentBotState = STATE_NORMAL;
      bot.sendMessageWithReplyKeyboard(chat_id, "🏠 **Main Menu**", "Markdown", kbdMain, true);
      continue;
    }

    switch (currentBotState) {
      case STATE_NORMAL:
        if (text == "Open 🔓" || text == "/open") {
          bot.sendMessage(chat_id, "Unlocking... ⏳", "");
          openDoor();
        }
        else if (text == "Status 📊" || text == "/status") {
          int val = analogRead(PIN_SIGNAL_INPUT);
          String msg = "✅ **System Online**\n";
          msg += "🔑 Code: " + visualizeCode(secretCode) + "\n";
          msg += "------------------\n";
          msg += "⚙️ Gap: " + String(settingGap) + " ms\n";
          msg += "⚙️ Long: " + String(settingLongPress) + " ms\n";
          msg += "📈 ADC: " + String(val) + " (Threshold > " + String(SIGNAL_THRESHOLD) + ")\n";
          
          if (digitalRead(PIN_BUTTON) == LOW) msg += "🔘 Switch: ON (Listening)";
          else msg += "🔘 Switch: OFF (Safe Mode)";
          
          bot.sendMessage(chat_id, msg, "Markdown");
        }
        else if (text == "⚙️ Setup Code") {
          currentBotState = STATE_SETTING_CODE;
          tempCode = ""; 
          bot.sendMessageWithReplyKeyboard(chat_id, "✍️ **SETUP CODE**\nMax 10 clicks.", "Markdown", kbdSetup, true);
        }
        else if (text == "🛠 Settings") {
          bot.sendMessageWithReplyKeyboard(chat_id, "🔧 **SETTINGS**\nSelect parameter:", "Markdown", kbdSettings, true);
        }
        else if (text == "⏳ Set Gap (Bounce)") {
          currentBotState = STATE_SETTING_GAP;
          bot.sendMessageWithReplyKeyboard(chat_id, "Enter Gap (50 - 500 ms):", "", kbdCancelOnly, true);
        }
        else if (text == "⏱ Set Long Time") {
          currentBotState = STATE_SETTING_LONG;
          bot.sendMessageWithReplyKeyboard(chat_id, "Enter Long Press (500 - 3000 ms):", "", kbdCancelOnly, true);
        }
        break;

      case STATE_SETTING_CODE:
        if (text == "⚫ Short") {
          if (tempCode.length() < 10) tempCode += "S";
          bot.sendMessage(chat_id, "Seq: " + visualizeCode(tempCode), "");
        }
        else if (text == "➖ Long") {
          if (tempCode.length() < 10) tempCode += "L";
          bot.sendMessage(chat_id, "Seq: " + visualizeCode(tempCode), "");
        }
        else if (text == "💾 Save") {
          if (tempCode.length() > 0) {
            secretCode = tempCode;
            preferences.begin("intercom", false);
            preferences.putString("code", secretCode);
            preferences.end();
            bot.sendMessageWithReplyKeyboard(chat_id, "✅ **SAVED!**", "Markdown", kbdMain, true);
            currentBotState = STATE_NORMAL;
          } else {
            bot.sendMessage(chat_id, "⚠️ Empty!", "");
          }
        }
        break;

      case STATE_SETTING_GAP:
        {
          int val = text.toInt(); 
          if (val >= 50 && val <= 500) {
            settingGap = val;
            preferences.begin("intercom", false);
            preferences.putInt("gap", settingGap);
            preferences.end();
            currentBotState = STATE_NORMAL;
            bot.sendMessageWithReplyKeyboard(chat_id, "✅ Gap saved: " + String(val) + " ms", "", kbdMain, true);
          } else bot.sendMessage(chat_id, "⚠️ Invalid range (50-500).", "");
        }
        break;

      case STATE_SETTING_LONG:
        {
          int val = text.toInt();
          if (val >= 500 && val <= 3000) {
            settingLongPress = val;
            preferences.begin("intercom", false);
            preferences.putInt("long", settingLongPress);
            preferences.end();
            currentBotState = STATE_NORMAL;
            bot.sendMessageWithReplyKeyboard(chat_id, "✅ Long Press saved: " + String(val) + " ms", "", kbdMain, true);
          } else bot.sendMessage(chat_id, "⚠️ Invalid range (500-3000).", "");
        }
        break;
    }
  }
}

void telegramTask(void *pvParameters) {
  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      unsigned long now = millis();
      if (now - lastBotRun > BOT_INTERVAL) {
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        while (numNewMessages) {
          handleNewMessages(numNewMessages);
          numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        }
        lastBotRun = now;
      }
      if (currentBotState != STATE_NORMAL && (millis() - lastInteractionTime > MENU_TIMEOUT)) {
        currentBotState = STATE_NORMAL;
        bot.sendMessageWithReplyKeyboard(chatID, "⏳ **Menu timed out.**", "Markdown", kbdMain, true);
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS); 
  }
}

// ==========================================
// --- SETUP ---
// ==========================================
void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_SIGNAL_INPUT, INPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED_BREATHE, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT); 
  digitalWrite(PIN_RELAY, RELAY_INACTIVE_LEVEL); 

  preferences.begin("intercom", true);
  secretCode = preferences.getString("code", "SSS"); 
  settingGap = preferences.getInt("gap", 200); 
  settingLongPress = preferences.getInt("long", 1000);
  preferences.end();
  
  Serial.println("System Started (DC Logic Mode)");

  WiFi.begin(ssid, password);
  client.setInsecure();
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  bot.sendMessage(chatID, "🚀 **Online!** (DC Mode)", "Markdown");
  xTaskCreate(telegramTask, "TelegramTask", 10240, NULL, 1, NULL);
}

// ==========================================
// --- MAIN LOOP (DC LOGIC) ---
// ==========================================
void loop() {
  updateRelay(); 

  bool isSystemArmed = (digitalRead(PIN_BUTTON) == LOW);

  if (isSystemArmed) {
    // --- ARMED MODE ---
    setLedBrightness(255);
    fadeValue = 255; 

    // 1. INSTANT READ (RC Filtered)
    int val = analogRead(PIN_SIGNAL_INPUT);
    
    // Logic: Signal is ACTIVE if voltage > 0.16V (ADC > 200)
    bool isSignalPresentNow = (val > SIGNAL_THRESHOLD);
    unsigned long now = millis();

    // 2. STATE MACHINE
    if (isSignalPresentNow) {
      if (!isPressing) {
        // New Press Started
        isPressing = true;
        currentPressStartTime = now;
        Serial.println(">> SIGNAL START");
      }
      // Safety: Stuck Button Check
      if (now - currentPressStartTime > MAX_PRESS_LIMIT) {
        Serial.println("Error: Button Stuck (Reset)");
        isPressing = false; 
        inputBuffer = "";
      }
      
      lastSignalSeenTime = now; 
    } 
    else {
      // No Signal
      if (isPressing) {
        // Gap Check
        if (now - lastSignalSeenTime > settingGap) {
          isPressing = false;
          unsigned long duration = lastSignalSeenTime - currentPressStartTime;
          
          // Noise Filter (> 50ms)
          if (duration > 50) { 
             char pressType = (duration > settingLongPress) ? 'L' : 'S';
             inputBuffer += pressType;
             Serial.printf(">> PRESS END: %ld ms -> %c. Buffer: %s\n", duration, pressType, visualizeCode(inputBuffer).c_str());
          }
        }
      }
      else {
        // Code Timeout Check
        if (inputBuffer.length() > 0 && (now - lastSignalSeenTime > CODE_TIMEOUT)) {
          Serial.print("Checking Code: "); Serial.println(inputBuffer);
          
          if (inputBuffer == secretCode) {
            Serial.println(">>> ACCESS GRANTED <<<");
            openDoor(); 
          } else {
            Serial.println("--- DENIED ---");
          }
          inputBuffer = "";
        }
      }
    }

  } else {
    // --- SAFE MODE ---
    runBreathingAnimation();
    if (inputBuffer.length() > 0) inputBuffer = "";
    isPressing = false;
  }
}
