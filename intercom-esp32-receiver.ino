#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFiManager.h> 
#include <Ticker.h>      

// ==========================================
// --- HARDCODED CONFIG (SECURE) ---
// ==========================================
const String ROOT_ADMIN_ID = "ROOT_ADMIN_ID";
const String BOT_TOKEN     = "BOT_TOKEN";

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
UniversalTelegramBot* bot;
Preferences preferences; 
Preferences namePrefs;   
Ticker ticker;
WiFiManager wm;

// --- STORAGE ---
String listUsers = ""; 

// Invite System
String inviteCode = "";
unsigned long inviteTimestamp = 0;
const unsigned long INVITE_TIMEOUT = 300000; 

// Relay
const int RELAY_ACTIVE_LEVEL = HIGH; 
const int RELAY_INACTIVE_LEVEL = LOW;
const unsigned long RELAY_DURATION = 4000; 
unsigned long relayOnTime = 0;
bool isRelayActive = false;

// Logic
String secretCode = "";    
String inputBuffer = "";   
String tempCode = "";      
int settingGap = 200;       
int settingLongPress = 1000;

// State Machine
enum BotState { 
  STATE_NORMAL, 
  STATE_SETTING_CODE, 
  STATE_SETTING_GAP, 
  STATE_SETTING_LONG,
  STATE_REMOVING_USER 
};
BotState currentBotState = STATE_NORMAL;
unsigned long lastInteractionTime = 0; 
const unsigned long MENU_TIMEOUT = 300000; 

// Signal
const int SIGNAL_THRESHOLD = 200; 
unsigned long currentPressStartTime = 0;
unsigned long lastSignalSeenTime = 0;
bool isPressing = false; 
const int CODE_TIMEOUT = 2000; 
const int MAX_PRESS_LIMIT = 5000;

// LED & Telegram
int fadeValue = 0;
int fadeStep = 2; 
unsigned long lastFadeUpdate = 0;
int currentLedBrightness = -1;
unsigned long lastBotRun = 0;
const int BOT_INTERVAL = 300; 

// Reset Logic
unsigned long lastToggleTime = 0;
int toggleCounter = 0;
bool lastButtonState = false;
const int RESET_WINDOW = 3000;
const int TOGGLE_COUNT_REQ = 5;

// --- KEYBOARDS ---
String kbdMain = "[[\"Open 🔓\", \"Status 📊\"], [\"⚙️ Setup Code\", \"🛠 Settings\"], [\"👥 Users Management\"]]";
String kbdUsers = "[[\"➕ Invite User\", \"❌ Remove User\"], [\"👥 List All\", \"🔙 Back\"]]";
String kbdSetup = "[[\"⚫ Short\", \"➖ Long\"], [\"💾 Save\", \"❌ Cancel\"]]";
String kbdSettings = "[[\"⏳ Set Gap\", \"⏱ Set Long Time\"], [\"🔙 Back\"]]";
String kbdCancelOnly = "[[\"❌ Cancel\"]]";

// ==========================================
// --- USER SYSTEM (FIXED NAMES) ---
// ==========================================

void loadUsers() {
  preferences.begin("rbac", false);
  listUsers = preferences.getString("users", "");
  preferences.end();
}

bool isAuthorized(String id) {
  if (id == ROOT_ADMIN_ID) return true;
  if (listUsers.indexOf(id) != -1) return true;
  return false;
}

// Helper to save name ONLY if changed (save flash cycles)
void updateNameIfChanged(String id, String newName) {
  namePrefs.begin("names", false);
  String currentName = namePrefs.getString(id.c_str(), "");
  
  if (currentName != newName) {
    namePrefs.putString(id.c_str(), newName);
    Serial.println("Name updated for " + id + ": " + newName);
  }
  namePrefs.end();
}

void addUser(String id, String name) {
  if (id == ROOT_ADMIN_ID) return;
  
  if (listUsers.indexOf(id) == -1) {
    if (listUsers.length() > 0) listUsers += ",";
    listUsers += id;
    preferences.begin("rbac", false);
    preferences.putString("users", listUsers);
    preferences.end();
  }
  updateNameIfChanged(id, name);
}

String getName(String id) {
  namePrefs.begin("names", true); 
  String name = namePrefs.getString(id.c_str(), "Unknown");
  namePrefs.end();
  
  // Fallback if name is missing but it is Root
  if (name == "Unknown" && id == ROOT_ADMIN_ID) return "ROOT (Send msg to update)";
  
  return name;
}

String removeUser(String id) {
  if (id == ROOT_ADMIN_ID) return "⛔ CRITICAL: Cannot delete Root Admin!";
  
  if (listUsers.indexOf(id) != -1) {
    String newList = "";
    int start = 0;
    while (start < listUsers.length()) {
      int comma = listUsers.indexOf(',', start);
      if (comma == -1) comma = listUsers.length();
      String token = listUsers.substring(start, comma);
      if (token != id) {
        if (newList.length() > 0) newList += ",";
        newList += token;
      }
      start = comma + 1;
    }
    listUsers = newList;
    preferences.begin("rbac", false);
    preferences.putString("users", listUsers);
    preferences.end();

    namePrefs.begin("names", false);
    namePrefs.remove(id.c_str());
    namePrefs.end();

    return "✅ User removed.";
  }
  return "⚠️ ID not found.";
}

// ==========================================
// --- HARDWARE ---
// ==========================================

void setLedBrightness(int val) {
  if (val != currentLedBrightness) {
    analogWrite(PIN_LED_BREATHE, val);
    currentLedBrightness = val;
  }
}

void tick() {
  int state = digitalRead(PIN_LED_BREATHE);
  digitalWrite(PIN_LED_BREATHE, !state);
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

void checkResetSequence() {
  bool currentState = (digitalRead(PIN_BUTTON) == LOW);
  unsigned long now = millis();
  if (currentState != lastButtonState) {
    if (now - lastToggleTime < RESET_WINDOW) toggleCounter++;
    else toggleCounter = 1; 
    lastToggleTime = now;
    lastButtonState = currentState;
    if (toggleCounter >= TOGGLE_COUNT_REQ) {
      Serial.println("!!! FACTORY RESET !!!");
      setLedBrightness(255); delay(2000);
      wm.resetSettings(); ESP.restart();
    }
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {
  ticker.attach(0.1, tick);
}

// ==========================================
// --- TELEGRAM ---
// ==========================================
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    yield(); 
    String chat_id = String(bot->messages[i].chat_id);
    String text = bot->messages[i].text;
    String from_name = bot->messages[i].from_name;

    // --- AUTO-UPDATE NAME (For Root & Users) ---
    // If user is authorized, update their name in DB if it changed
    if (isAuthorized(chat_id)) {
      updateNameIfChanged(chat_id, from_name);
    }

    // --- 1. INVITE CHECK ---
    if (!isAuthorized(chat_id)) {
      if (inviteCode.length() > 0 && text == inviteCode) {
        if (millis() - inviteTimestamp < INVITE_TIMEOUT) {
          addUser(chat_id, from_name);
          inviteCode = ""; 
          
          bot->sendMessage(chat_id, "✅ **Access Granted!**\nWelcome, " + from_name, "Markdown");
          bot->sendMessageWithReplyKeyboard(chat_id, "Menu loaded.", "", kbdMain, true);
          
          bot->sendMessage(ROOT_ADMIN_ID, "ℹ️ New User: " + from_name + " (" + chat_id + ")", "");
        } else {
          bot->sendMessage(chat_id, "⛔ Invite expired.", "");
        }
      } else {
        bot->sendMessage(chat_id, "⛔ **Access Denied**\nEnter invite code.", "Markdown");
      }
      continue;
    }

    // --- 2. AUTHORIZED ---
    lastInteractionTime = millis();

    if (text == "❌ Cancel" || text == "/cancel" || text == "🔙 Back") {
      currentBotState = STATE_NORMAL;
      bot->sendMessageWithReplyKeyboard(chat_id, "Main Menu 🏠", "", kbdMain, true);
      continue;
    }
    if (text == "/start") {
      currentBotState = STATE_NORMAL;
      bot->sendMessageWithReplyKeyboard(chat_id, "🏠 **Main Menu**", "Markdown", kbdMain, true);
      continue;
    }

    switch (currentBotState) {
      
      // A. MAIN MENU
      case STATE_NORMAL:
        if (text == "Open 🔓" || text == "/open") {
          bot->sendMessage(chat_id, "Unlocking... ⏳", "");
          openDoor();
        }
        else if (text == "Status 📊" || text == "/status") {
          int val = analogRead(PIN_SIGNAL_INPUT);
          String msg = "✅ **System Online**\n";
          msg += "🔑 Code: " + visualizeCode(secretCode) + "\n";
          msg += "📈 ADC: " + String(val) + "\n";
          if (digitalRead(PIN_BUTTON) == LOW) msg += "🔘 Switch: ON (Listening)";
          else msg += "🔘 Switch: OFF (Safe Mode)";
          bot->sendMessage(chat_id, msg, "Markdown");
        }
        else if (text == "⚙️ Setup Code") {
          currentBotState = STATE_SETTING_CODE;
          tempCode = ""; 
          bot->sendMessageWithReplyKeyboard(chat_id, "✍️ **SETUP CODE**\nMax 10 clicks.", "Markdown", kbdSetup, true);
        }
        else if (text == "🛠 Settings") {
          bot->sendMessageWithReplyKeyboard(chat_id, "🔧 **SETTINGS**", "Markdown", kbdSettings, true);
        }
        else if (text == "👥 Users Management") {
          bot->sendMessageWithReplyKeyboard(chat_id, "👥 **USERS**", "Markdown", kbdUsers, true);
        }
        
        else if (text == "⏳ Set Gap") {
          currentBotState = STATE_SETTING_GAP;
          bot->sendMessageWithReplyKeyboard(chat_id, "Enter Gap (50-500 ms):", "", kbdCancelOnly, true);
        }
        else if (text == "⏱ Set Long Time") {
          currentBotState = STATE_SETTING_LONG;
          bot->sendMessageWithReplyKeyboard(chat_id, "Enter Long Press (500-3000 ms):", "", kbdCancelOnly, true);
        }
        
        else if (text == "➕ Invite User") {
          int code = random(1000, 9999);
          inviteCode = String(code);
          inviteTimestamp = millis();
          bot->sendMessage(chat_id, "🎟 **INVITE CODE**\n`" + inviteCode + "`\nValid for 5 min.", "Markdown");
        }
        else if (text == "👥 List All") {
          String msg = "👑 **ROOT:**\n" + getName(ROOT_ADMIN_ID) + "\n\n";
          msg += "👤 **USERS:**\n";
          
          if (listUsers.length() == 0) {
            msg += "(none)";
          } else {
            int start = 0;
            while (start < listUsers.length()) {
              int comma = listUsers.indexOf(',', start);
              if (comma == -1) comma = listUsers.length();
              String uid = listUsers.substring(start, comma);
              String uname = getName(uid);
              msg += "• " + uname + " (" + uid + ")\n";
              start = comma + 1;
            }
          }
          bot->sendMessage(chat_id, msg, "Markdown");
        }
        else if (text == "❌ Remove User") {
          currentBotState = STATE_REMOVING_USER;
          bot->sendMessageWithReplyKeyboard(chat_id, "🆔 **Send User ID to delete:**\n(Copy from List)", "Markdown", kbdCancelOnly, true);
        }
        else {
          bot->sendMessageWithReplyKeyboard(chat_id, "Use menu 👇", "", kbdMain, true);
        }
        break;

      // B. SETUP CODE
      case STATE_SETTING_CODE:
        if (text == "⚫ Short") {
          if (tempCode.length() < 10) tempCode += "S";
          bot->sendMessage(chat_id, "Seq: " + visualizeCode(tempCode), "");
        }
        else if (text == "➖ Long") {
          if (tempCode.length() < 10) tempCode += "L";
          bot->sendMessage(chat_id, "Seq: " + visualizeCode(tempCode), "");
        }
        else if (text == "💾 Save") {
          if (tempCode.length() > 0) {
            secretCode = tempCode;
            preferences.begin("intercom", false);
            preferences.putString("code", secretCode);
            preferences.end();
            bot->sendMessageWithReplyKeyboard(chat_id, "✅ Saved!", "", kbdMain, true);
            currentBotState = STATE_NORMAL;
          } else bot->sendMessage(chat_id, "⚠️ Empty!", "");
        }
        break;

      // C. SETTINGS
      case STATE_SETTING_GAP:
        {
          int val = text.toInt();
          if (val >= 50 && val <= 500) {
            settingGap = val;
            preferences.begin("intercom", false);
            preferences.putInt("gap", settingGap);
            preferences.end();
            bot->sendMessageWithReplyKeyboard(chat_id, "✅ Gap: " + String(val), "", kbdMain, true);
            currentBotState = STATE_NORMAL;
          } else bot->sendMessage(chat_id, "⚠️ Invalid range.", "");
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
            bot->sendMessageWithReplyKeyboard(chat_id, "✅ Long Press: " + String(val), "", kbdMain, true);
            currentBotState = STATE_NORMAL;
          } else bot->sendMessage(chat_id, "⚠️ Invalid range.", "");
        }
        break;

      // D. REMOVE USER
      case STATE_REMOVING_USER:
        String result = removeUser(text);
        bot->sendMessageWithReplyKeyboard(chat_id, result, "", kbdMain, true);
        currentBotState = STATE_NORMAL;
        break;
    }
  }
}

void telegramTask(void *pvParameters) {
  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      unsigned long now = millis();
      if (now - lastBotRun > BOT_INTERVAL) {
        int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
        while (numNewMessages) {
          handleNewMessages(numNewMessages);
          numNewMessages = bot->getUpdates(bot->last_message_received + 1);
        }
        lastBotRun = now;
      }
      if (currentBotState != STATE_NORMAL && (millis() - lastInteractionTime > MENU_TIMEOUT)) {
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

  preferences.begin("intercom", false);
  secretCode = preferences.getString("code", "SSS"); 
  settingGap = preferences.getInt("gap", 200); 
  settingLongPress = preferences.getInt("long", 1000);
  preferences.end();

  loadUsers();

  wm.setAPCallback(configModeCallback);
  if (!wm.autoConnect("Intercom-Setup")) ESP.restart();

  ticker.detach();

  client.setInsecure();
  bot = new UniversalTelegramBot(BOT_TOKEN, client);
  bot->sendMessage(ROOT_ADMIN_ID, "🚀 **System Online** (v31.1)", "Markdown");
  
  lastButtonState = (digitalRead(PIN_BUTTON) == LOW); 
  xTaskCreate(telegramTask, "TelegramTask", 10240, NULL, 1, NULL);
}

// ==========================================
// --- MAIN LOOP ---
// ==========================================
void loop() {
  updateRelay(); 
  checkResetSequence(); 

  bool isSystemArmed = (digitalRead(PIN_BUTTON) == LOW);

  if (isSystemArmed) {
    setLedBrightness(255);
    fadeValue = 255; 

    int val = analogRead(PIN_SIGNAL_INPUT);
    bool isSignalPresentNow = (val > SIGNAL_THRESHOLD); 
    unsigned long now = millis();

    if (isSignalPresentNow) {
      if (!isPressing) {
        isPressing = true;
        currentPressStartTime = now;
        Serial.println(">> SIGNAL START");
      }
      if (now - currentPressStartTime > MAX_PRESS_LIMIT) {
         isPressing = false; inputBuffer = "";
      }
      lastSignalSeenTime = now; 
    } 
    else {
      if (isPressing) {
        if (now - lastSignalSeenTime > settingGap) {
          isPressing = false;
          unsigned long duration = lastSignalSeenTime - currentPressStartTime;
          if (duration > 50) { 
             char pressType = (duration > settingLongPress) ? 'L' : 'S';
             inputBuffer += pressType;
             Serial.printf("Press: %ld ms -> %c\n", duration, pressType);
          }
        }
      }
      else {
        if (inputBuffer.length() > 0 && (now - lastSignalSeenTime > CODE_TIMEOUT)) {
          Serial.print("Checking: "); Serial.println(inputBuffer);
          if (inputBuffer == secretCode) {
            Serial.println("ACCESS GRANTED");
            openDoor(); 
          } else { Serial.println("DENIED"); }
          inputBuffer = "";
        }
      }
    }
  } else {
    runBreathingAnimation();
    if (inputBuffer.length() > 0) inputBuffer = "";
    isPressing = false;
  }
}
