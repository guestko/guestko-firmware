// Matter Manager
#include <Arduino.h>
#include <Matter.h>
#include <Preferences.h>

// Libraries for 0.42" screen via I2C
#include <U8g2lib.h>
#include <Wire.h>

// Initialize screen with special profile 72x40 (SSD1306)
// Using hardware I2C. Pins will be specified in setup via Wire
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// Initialize Matter endpoint for the switch
MatterOnOffLight OnOffLight;

// Variables for non-volatile storage (NVS)
Preferences matterPref;
const char *onOffPrefKey = "OnOff";
const char *countPrefKey = "TurnOnCount"; // Key for storing the counter

// Activation counter variable
uint32_t turnOnCount = 0;

// Hardware configuration specifically for the board from the image
const uint8_t ledPin = 8;    // Built-in blue LED (Active-LOW)
const uint8_t buttonPin = 9; // BOOT button on the board

// Variables for button handling
uint32_t button_time_stamp = 0;
bool button_state = false;
const uint32_t debouceTime = 250;
const uint32_t decommissioningTimeout = 5000;

// FUNCTION TO UPDATE INFORMATION ON THE SCREEN (72x40 pixels)
void updateDisplay(bool is_on) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf); // Compact and clear font (height ~10 pixels)

  // 1st row (y = 10): Current device status
  u8g2.drawStr(0, 10, "STATUS:");
  u8g2.drawStr(46, 10, is_on ? "ON" : "OFF");

  // 2nd row (y = 22): Decorative separator
  u8g2.drawStr(0, 21, "-------");

  // 3rd row (y = 35): Counter output
  u8g2.drawStr(0, 35, "COUNT:");

  // Convert turnOnCount number to string for the screen
  char countStr[10];
  itoa(turnOnCount, countStr, 10);
  u8g2.drawStr(42, 35, countStr);

  u8g2.sendBuffer(); // Send buffer to the display
}

// Callback function: triggered on ecosystem/app action
bool setLightOnOff(bool state) {
  // Inverted logic of the built-in LED (LOW = ON)
  if (state) {
    digitalWrite(ledPin, LOW);

    // METRIC: Increment counter and write to flash on every activation
    turnOnCount++;
    matterPref.putUInt(countPrefKey, turnOnCount);
    Serial.printf("[+] Device turned ON! Total activations: %d\r\n",
                  turnOnCount);
  } else {
    digitalWrite(ledPin, HIGH);
  }

  // Instantly update the mini-screen
  updateDisplay(state);

  matterPref.putBool(onOffPrefKey, state);
  return true;
}

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);

  // HARDWARE I2C SETUP FOR YOUR BOARD: SDA = 5, SCL = 6
  Wire.begin(5, 6);
  u8g2.begin();

  // Open data storage (NVS)
  matterPref.begin("MatterPrefs", false);

  // Read old state and counter from memory (returns 0 if first run)
  bool lastOnOffState = matterPref.getBool(onOffPrefKey, true);
  turnOnCount = matterPref.getUInt(countPrefKey, 0);

  // Apply LED settings on board boot
  digitalWrite(ledPin, lastOnOffState ? LOW : HIGH);

  // Render the main screen on power-up
  updateDisplay(lastOnOffState);

  OnOffLight.begin(lastOnOffState);
  OnOffLight.onChange(setLightOnOff);

  // Start Matter
  Matter.begin();

  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node registered and ready for use.");
    OnOffLight.updateAccessory();
  }
}

void loop() {
  // If the device is not commissioned to Apple Home / Smart Life yet
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("\n[!] Waiting for commissioning...");

    // Display commissioning status directly on the board screen!
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 12, "PAIRING...");
    u8g2.drawStr(0, 25, "Scan QR");
    u8g2.drawStr(0, 38, "in Serial");
    u8g2.sendBuffer();

    while (!Matter.isDeviceCommissioned()) {
      delay(100);
    }
    OnOffLight.updateAccessory();
    Serial.println("Device successfully commissioned!");
    updateDisplay(
        OnOffLight.getOnOff()); // Draw the working screen after commissioning
  }

  // Poll physical BOOT button (GPIO 9) for manual testing
  if (digitalRead(buttonPin) == LOW && !button_state) {
    button_time_stamp = millis();
    button_state = true;
  }

  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > debouceTime &&
      digitalRead(buttonPin) == HIGH) {
    button_state = false;
    Serial.println("Button released. Toggling state!");
    OnOffLight
        .toggle(); // This will automatically call the setLightOnOff function
  }

  // If the BOOT button is held for 5 seconds — reset network and clear counter
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning Matter Node. Clearing counter.");
    OnOffLight.setOnOff(false);
    digitalWrite(ledPin, HIGH);

    // Reset activation counter on device decommissioning
    turnOnCount = 0;
    matterPref.putUInt(countPrefKey, 0);
    matterPref.clear();
    Matter.decommission();
  }
}