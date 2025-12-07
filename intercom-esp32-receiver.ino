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
const int PIN_SIGNAL_INPUT  = 3;  
const int PIN_BUTTON        = 4;  
const int PIN_LED_BREATHE   = 5;  
const int PIN_RELAY         = 10; 

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

// Flags for Multitasking (FIX FOR CRASHES)
volatile bool pendingNotification = false; // Flag to send TG message

// Bot State
enum BotState {
  STATE_NORMAL,
  STATE_SETTING_CODE
};
BotState currentBotState = STATE_NORMAL;
unsigned long lastInteractionTime = 0; 
const unsigned long MENU_TIMEOUT = 60000; 

// Signal Detection
const int SIGNAL_THRESHOLD = 300; 
const int SAMPLE_WINDOW = 50;     
const int MIN_PULSES = 25;        
unsigned long samplingStartTime = 0; 
int currentPulseCount = 0;           
bool currentSignalState = false;     

// Logic Timing
unsigned long signalDuration = 0;
unsigned long lastSignalEndTime = 0;
const int LONG_PRESS_THRESHOLD = 1000; 
const int CODE_TIMEOUT = 2000;         

// Breathing LED
int fadeValue = 0;
int fadeStep = 2; 
unsigned long lastFadeUpdate = 0;
int currentLedBrightness = -1;

// Telegram Timing
unsigned long lastBotRun = 0;
const int BOT_INTERVAL = 300; 

// Keyboards
String kbdMain = "[[\"Open 🔓\", \"Status 📊\"], [\"⚙️ Setup Code\"]]";
String kbdSetup = "[[\"⚫ Short\", \"➖ Long\"], [\"💾 Save\", \"❌ Cancel\"]]";

// ==========================================
// --- FUNCTIONS ---
// ==========================================

void setLedBrightness(int val) {
  if (val != currentLedBrightness) {
    analogWrite(PIN_LED_BREATHE, val);
    currentLedBrightness = val;
  }
}

void updateBreathingSystem() {
  if (digitalRead(PIN_BUTTON) == LOW) {
    setLedBrightness(255);
    fadeValue = 255; 
  } else {
    unsigned long now = millis();
    if (now - lastFadeUpdate > 10) { 
      lastFadeUpdate = now;
      fadeValue += fadeStep;
      if (fadeValue >= 255) { fadeValue = 255; fadeStep = -fadeStep; }
      else if (fadeValue <= 0) { fadeValue = 0; fadeStep = -fadeStep; }
      setLedBrightness(fadeValue);
    }
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
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    if (chat_id != chatID) {
      bot.sendMessage(chat_id, "⛔ Access Denied", "");
      continue;
    }
    lastInteractionTime = millis();

    if (text == "❌ Cancel" || text == "/cancel") {
      currentBotState = STATE_NORMAL;
      bot.sendMessageWithReplyKeyboard(chat_id, "Cancelled. 🏠", "", kbdMain, true);
      continue;
    }

    if (currentBotState == STATE_NORMAL) {
      if (text == "/start") {
        bot.sendMessageWithReplyKeyboard(chat_id, "🏠 **Main Menu**", "Markdown", kbdMain, true);
      }
      else if (text == "Open 🔓" || text == "/open") {
        bot.sendMessage(chat_id, "Unlocking... ⏳", "");
        openDoor();
      }
      else if (text == "Status 📊" || text == "/status") {
        String msg = "✅ **System Online**\n";
        msg += "🔑 Active Code: " + visualizeCode(secretCode);
        bot.sendMessage(chat_id, msg, "Markdown");
      }
      else if (text == "⚙️ Setup Code") {
        currentBotState = STATE_SETTING_CODE;
        tempCode = ""; 
        String msg = "✍️ **SETUP MODE**\nSequence limit: 10.";
        bot.sendMessageWithReplyKeyboard(chat_id, msg, "Markdown", kbdSetup, true);
      }
    }
    else if (currentBotState == STATE_SETTING_CODE) {
      if (text == "⚫ Short") {
        if (tempCode.length() < 10) {
          tempCode += "S";
          bot.sendMessage(chat_id, "Sequence: " + visualizeCode(tempCode), "");
        }
      }
      else if (text == "➖ Long") {
        if (tempCode.length() < 10) {
          tempCode += "L";
          bot.sendMessage(chat_id, "Sequence: " + visualizeCode(tempCode), "");
        }
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
    }
  }
}

// TASK: Handles WiFi & Telegram (Background)
void telegramTask(void *pvParameters) {
  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      
      // 1. Check if we need to send a notification (SAFE METHOD)
      if (pendingNotification) {
        bot.sendMessage(chatID, "🔔 **Access Granted!** Door Opened.", "Markdown");
        pendingNotification = false; // Reset flag
      }

      // 2. Check for incoming messages
      unsigned long now = millis();
      if (now - lastBotRun > BOT_INTERVAL) {
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        while (numNewMessages) {
          handleNewMessages(numNewMessages);
          numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        }
        lastBotRun = now;
      }
      
      if (currentBotState == STATE_SETTING_CODE && (now - lastInteractionTime > MENU_TIMEOUT)) {
        currentBotState = STATE_NORMAL;
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
  preferences.end();
  Serial.println("Loaded Code: " + secretCode);

  WiFi.begin(ssid, password);
  client.setInsecure();
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  bot.sendMessage(chatID, "🚀 **Online!**\nCode: " + visualizeCode(secretCode), "Markdown");
  
  xTaskCreate(telegramTask, "TelegramTask", 10240, NULL, 1, NULL);
}

// ==========================================
// --- LOOP (MAIN LOGIC) ---
// ==========================================
void loop() {
  updateBreathingSystem();
  updateRelay();

  int val = analogRead(PIN_SIGNAL_INPUT);
  
  if (!currentSignalState && val > SIGNAL_THRESHOLD) { 
    currentSignalState = true; 
    currentPulseCount++; 
  } 
  else if (currentSignalState && val < (SIGNAL_THRESHOLD - 50)) { 
    currentSignalState = false; 
  }

  unsigned long now = millis();
  
  if (now - samplingStartTime >= SAMPLE_WINDOW) {
    bool isSignalPresent = (currentPulseCount >= MIN_PULSES);

    if (isSignalPresent) {
      signalDuration += SAMPLE_WINDOW;
      lastSignalEndTime = now;
    } else {
      if (signalDuration > 0) {
        char pressType = (signalDuration > LONG_PRESS_THRESHOLD) ? 'L' : 'S';
        inputBuffer += pressType;
        Serial.print("Input: "); Serial.println(visualizeCode(inputBuffer));
        signalDuration = 0;
      }

      if (inputBuffer.length() > 0 && (now - lastSignalEndTime > CODE_TIMEOUT)) {
        Serial.print("Checking: "); Serial.println(inputBuffer);
        
        if (inputBuffer == secretCode) {
          Serial.println("ACCESS GRANTED");
          
          openDoor();
          
          // [FIX]: Don't send Telegram message here directly!
          // Just set the flag, and let the background task handle it safely.
          pendingNotification = true; 
          
        } else {
          Serial.println("DENIED");
        }
        inputBuffer = "";
      }
    }

    currentPulseCount = 0;
    samplingStartTime = now;
  }
}
