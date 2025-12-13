/**
 * Smart Intercom v40.1 (English / UX Edition)
 * * Fully button-based interface.
 * * Persistent settings storage (EEPROM/Preferences).
 * * Smart noise peak diagnostics.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <HTTPUpdate.h>

// ==========================================
// --- USER SETTINGS (EDIT HERE) ---
// ==========================================
const char* ssid          = "ssid";
const char* password      = "password";
const char* BOT_TOKEN     = "BOT_TOKEN";
const char* ADMIN_ID      = "ADMIN_ID";

// ==========================================
// --- HARDWARE CONFIG ---
// ==========================================
#define LED_PIN 8       // Correct pin for your board
#define LED_ON  LOW     // LOW turns LED ON
#define LED_OFF HIGH    // HIGH turns LED OFF

const int PIN_SIGNAL_INPUT  = 3;   
const int PIN_RELAY         = 10;  

const int MIN_PRESS_TIME    = 500;  
const int MAX_PRESS_TIME    = 5000; 
const int RELAY_PULSE_MS    = 500;  
const int DEFAULT_THRESHOLD = 1000; 

// ==========================================
// --- OBJECTS ---
// ==========================================
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
Preferences preferences;

// Logic vars
int signalThreshold = DEFAULT_THRESHOLD;
unsigned long relayOnTime = 0;
bool isRelayActive = false;
bool isReceivingSignal = false;
unsigned long signalStartTime = 0;
int maxSignalLevel = 0;
int backgroundNoisePeak = 0;

// System State
bool debugMode = false;
bool shouldReboot = false;
unsigned long lastDebugSend = 0;
unsigned long lastBotCheck = 0;
unsigned long lastWifiCheck = 0;

// Keyboards
String KBD_MAIN = "[[\"🔓 Open\", \"📊 Status\"], [\"🔧 Sensitivity\", \"🛠 Diagnostics\"]]";
String KBD_DEBUG = "[[\"🟢 Stream ON\", \"🔴 Stream OFF\"], [\"⬅️ Back\"]]";
String KBD_SENS = "[[\"Set: 300\", \"Set: 500\", \"Set: 800\"], [\"Set: 1000\", \"Set: 1200\", \"Set: 1500\"], [\"⬅️ Back\"]]";

// ==========================================
// --- LED LOGIC ---
// ==========================================
void handleLedStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    // === CONNECTED: SOLID ON ===
    digitalWrite(LED_PIN, LED_ON);  
  } else {
    // === DISCONNECTED: OFF ===
    digitalWrite(LED_PIN, LED_OFF);
  }
}

// ==========================================
// --- OTA & HELPERS ---
// ==========================================
String fixGoogleDriveLink(String url) {
  if (url.indexOf("drive.google.com") != -1 && url.indexOf("/d/") != -1) {
    int start = url.indexOf("/d/") + 3; 
    int end = url.indexOf("/", start);  
    if (end == -1) end = url.indexOf("?", start);
    if (end == -1) end = url.length();
    return "https://drive.google.com/uc?export=download&id=" + url.substring(start, end);
  }
  return url; 
}

void performUpdate(String url) {
  String cleanUrl = fixGoogleDriveLink(url);
  
  if (debugMode) bot.sendMessage(ADMIN_ID, "⏳ OTA...", "");
  
  WiFiClientSecure updateClient;
  updateClient.setInsecure();
  updateClient.setTimeout(15000); 

  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(false); 

  t_httpUpdate_return ret = httpUpdate.update(updateClient, cleanUrl);
  
  switch (ret) {
    case HTTP_UPDATE_FAILED: 
      bot.sendMessage(ADMIN_ID, "❌ Failed:\n" + httpUpdate.getLastErrorString(), ""); 
      break;
    case HTTP_UPDATE_NO_UPDATES: 
      bot.sendMessage(ADMIN_ID, "⚠️ No updates", ""); 
      break;
    case HTTP_UPDATE_OK: 
      bot.sendMessage(ADMIN_ID, "✅ OK! Rebooting...", ""); 
      shouldReboot = true;
      break;
  }
}

// ==========================================
// --- HANDLERS ---
// ==========================================
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    if (chat_id != ADMIN_ID) continue;

    if (text == "/start" || text == "⬅️ Back") {
      bot.sendMessageWithReplyKeyboard(chat_id, "🏠 Menu", "", KBD_MAIN, true);
    }
    else if (text == "🔓 Open" || text == "/open") {
      digitalWrite(PIN_RELAY, HIGH);
      isRelayActive = true;
      relayOnTime = millis();
      bot.sendMessage(chat_id, "✅ Open", "");
    }
    else if (text == "📊 Status" || text == "/status") {
      int adc = analogRead(PIN_SIGNAL_INPUT);
      String msg = "📊 **STATUS**\n";
      msg += "Build: " + String(__DATE__) + " " + String(__TIME__); 
      bot.sendMessage(chat_id, msg, "Markdown");
    }
    else if (text == "🔧 Sensitivity") {
      bot.sendMessageWithReplyKeyboard(chat_id, "Threshold:", "", KBD_SENS, true);
    }
    else if (text.startsWith("Set: ")) {
      int newVal = text.substring(5).toInt();
      if (newVal > 0) {
        signalThreshold = newVal;
        preferences.begin("intercom", false);
        preferences.putInt("limit", signalThreshold);
        preferences.end();
        bot.sendMessageWithReplyKeyboard(chat_id, "💾 Saved: " + String(signalThreshold), "", KBD_MAIN, true);
      }
    }
    else if (text == "🛠 Diagnostics") {
       bot.sendMessageWithReplyKeyboard(chat_id, "Debug", "", KBD_DEBUG, true);
    }
    else if (text == "🟢 Stream ON") debugMode = true;
    else if (text == "🔴 Stream OFF") debugMode = false;
    
    else if (text.startsWith("/update ")) {
      performUpdate(text.substring(8));
    }
  }
}

void updateRelay() {
  if (isRelayActive && (millis() - relayOnTime > RELAY_PULSE_MS)) {
    digitalWrite(PIN_RELAY, LOW);
    isRelayActive = false;
  }
}

// ==========================================
// --- SETUP & LOOP ---
// ==========================================
void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_SIGNAL_INPUT, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  // === CONFIRMATION BLINKS ===
  // 5 rapid blinks at startup to show life
  for(int i=0; i<5; i++) {
    digitalWrite(LED_PIN, LED_ON);
    delay(100);
    digitalWrite(LED_PIN, LED_OFF);
    delay(100);
  }
  // ===========================
  
  digitalWrite(PIN_RELAY, LOW);
  digitalWrite(LED_PIN, LED_OFF); 

  preferences.begin("intercom", true); 
  int savedLimit = preferences.getInt("limit", 0);
  preferences.end();
  if (savedLimit > 100) signalThreshold = savedLimit;

  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 15) {
    delay(500);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    String msg = "✅ **SYSTEM READY**\n";
    msg += "📅 Build: " + String(__DATE__) + " " + String(__TIME__);
    bot.sendMessageWithReplyKeyboard(ADMIN_ID, msg, "Markdown", KBD_MAIN, true);
  }
}

void loop() {
  updateRelay();
  handleLedStatus(); 

  // Logic
  int adcVal = analogRead(PIN_SIGNAL_INPUT);
  if (adcVal > backgroundNoisePeak) backgroundNoisePeak = adcVal;
  bool signalActive = (adcVal > signalThreshold);

  if (signalActive) {
    if (!isReceivingSignal) {
      isReceivingSignal = true;
      signalStartTime = millis();
      maxSignalLevel = adcVal;
    }
    if (adcVal > maxSignalLevel) maxSignalLevel = adcVal;
  } else {
    if (isReceivingSignal) {
      unsigned long duration = millis() - signalStartTime;
      isReceivingSignal = false;
      if (duration >= MIN_PRESS_TIME && duration <= MAX_PRESS_TIME) {
        digitalWrite(PIN_RELAY, HIGH);
        isRelayActive = true;
        relayOnTime = millis();
        if (WiFi.status() == WL_CONNECTED) bot.sendMessage(ADMIN_ID, "🔔 Ding-Dong!", "");
      }
    }
  }

  // Diagnostics
  if (debugMode && !isReceivingSignal && WiFi.status() == WL_CONNECTED) {
    if (millis() - lastDebugSend > 3000) { 
        bot.sendMessage(ADMIN_ID, "👀 Noise: " + String(backgroundNoisePeak), "");
        backgroundNoisePeak = 0; 
        lastDebugSend = millis();
    }
  }

  // Telegram
  if (!isReceivingSignal && WiFi.status() == WL_CONNECTED && millis() - lastBotCheck > 1000) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1); 
    }
    lastBotCheck = millis();
  }
  
  // Reconnect
  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiCheck > 60000) {
    WiFi.disconnect();
    WiFi.reconnect();
    lastWifiCheck = millis();
  }

  if (shouldReboot) {
    delay(1000); 
    bot.getUpdates(bot.last_message_received + 1);
    delay(1000);
    ESP.restart();
  }
  
  delay(10);
}
