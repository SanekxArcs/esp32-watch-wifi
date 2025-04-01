#include <WiFi.h>
#include <FastLED.h>
#include <WebServer.h>
#include <ezTime.h>
#include <Preferences.h> // Add Preferences library for settings persistence

// ===== LED Settings =====
#define NUM_LEDS    7       // Number of segments per digit
#define LED_TYPE    WS2812B // LED type
#define COLOR_ORDER GRB     // Change to RGB or BRG if needed

// Pins for digits and colon
#define DIGIT1_PIN 1   // First digit (tens of hours)
#define DIGIT2_PIN 2   // Second digit (ones of hours)
#define COLON_PIN  3   // Colon (2 LEDs)
#define DIGIT3_PIN 4   // Third digit (tens of minutes)
#define DIGIT4_PIN 5   // Fourth digit (ones of minutes)
#define COLON_COUNT 2  // Colon consists of 2 LEDs

// LED arrays for each digit and colon
CRGB leds1[NUM_LEDS], leds2[NUM_LEDS], leds3[NUM_LEDS], leds4[NUM_LEDS];
CRGB colon[COLON_COUNT];

// Digit mapping for 7-segment display (0-9)
const PROGMEM bool digits[10][NUM_LEDS] = {
  {0, 1, 1, 1, 1, 1, 1}, // 0
  {0, 0, 1, 1, 0, 0, 0}, // 1
  {1, 1, 0, 1, 1, 0, 1}, // 2
  {1, 1, 1, 1, 1, 0, 0}, // 3
  {1, 0, 1, 1, 0, 1, 0}, // 4
  {1, 1, 1, 0, 1, 1, 0}, // 5
  {1, 1, 1, 0, 1, 1, 1}, // 6
  {0, 0, 1, 1, 1, 0, 0}, // 7
  {1, 1, 1, 1, 1, 1, 1}, // 8
  {1, 1, 1, 1, 1, 1, 0}  // 9
};

// Character mapping for IP display
const PROGMEM bool letter_period[NUM_LEDS] = {0, 0, 0, 1, 0, 0, 0}; // Just the bottom segment
const PROGMEM bool letter_space[NUM_LEDS] = {0, 0, 0, 0, 0, 0, 0};  // Blank display

// ===== Wi‑Fi & Time Settings =====
const char* ssid     = "SANDS_WiFi";
const char* password = "Ytp@29!144";

// Time zone for Poland (auto-adjusts for DST)
Timezone Poland;

// ===== Global variables =====
uint8_t userBrightness = 25;     // Brightness (0-255). Default ~10%
uint8_t mode = 0;                // 0 - Rainbow (color transition), 1 - Static (fixed color)
CRGB staticColor = CRGB::Red;    // Default static color (red)
uint8_t globalHue = 0;           // For rainbow mode
uint8_t rainbowSpeed = 1;        // Speed of rainbow color change (1-10)
WebServer server(80);            // Web server on port 80
bool autoBrightnessEnabled = true; // Enable/disable automatic brightness adjustment
Preferences preferences;         // For saving settings to flash

// Auto brightness settings (percentages)
uint8_t dayBrightness = 100;     // Brightness percentage for day (9:00-18:00)
uint8_t nightBrightness = 10;    // Brightness percentage for night (22:00-6:00)
uint8_t transitionBrightness = 50; // Brightness percentage for transition periods

// Timing variables for non-blocking operation
unsigned long lastColonUpdate = 0;
bool colonState = false;

// ===== Display Functions =====

// Display a digit (0-9) on a given LED array with specified color
void displayDigit(uint8_t digit, CRGB ledsArray[], CRGB color) {
  if (digit > 9) digit = 0; // Prevent invalid indexes
  
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    ledsArray[i] = pgm_read_byte(&digits[digit][i]) ? color : CRGB::Black;
  }
}

// Display a character on a given LED array with specified color
void displayChar(char c, CRGB ledsArray[], CRGB color) {
  const bool* mapping;
  
  if (c >= '0' && c <= '9') {
    // It's a digit
    displayDigit(c - '0', ledsArray, color);
    return;
  } else if (c == '.') {
    mapping = letter_period;
  } else {
    mapping = letter_space;
  }
  
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    ledsArray[i] = pgm_read_byte(&mapping[i]) ? color : CRGB::Black;
  }
}

// Display a status (same digit on all displays); used for showing "0" or "2"
void displayStatus(uint8_t status, CRGB color) {
  displayDigit(status, leds1, color);
  displayDigit(status, leds2, color);
  displayDigit(status, leds3, color);
  displayDigit(status, leds4, color);
  
  // Turn off colon
  for (uint8_t i = 0; i < COLON_COUNT; i++) {
    colon[i] = CRGB::Black;
  }
  FastLED.show();
}

// Display the time (hours and minutes)
void displayTime(CRGB color) {
  uint8_t hours = Poland.hour();
  uint8_t minutes = Poland.minute();
  
  displayDigit(hours / 10, leds1, color);
  displayDigit(hours % 10, leds2, color);
  displayDigit(minutes / 10, leds3, color);
  displayDigit(minutes % 10, leds4, color);
  
  // Check if it's time to update the colon state (every 500ms)
  unsigned long currentMillis = millis();
  if (currentMillis - lastColonUpdate >= 500) {
    lastColonUpdate = currentMillis;
    colonState = !colonState; // Toggle colon state
    
    // Set colon based on state
    for (uint8_t i = 0; i < COLON_COUNT; i++) {
      colon[i] = colonState ? color : CRGB::Black;
    }
  }
  
  FastLED.show();
}

// Displays the IP address by scrolling it on the 4-digit display
void showIPAddress(CRGB color) {
  String ipString = WiFi.localIP().toString();
  uint8_t len = ipString.length();
  unsigned long startTime = millis();
  
  // Display IP for 10 seconds
  while (millis() - startTime < 10000) {
    for (uint8_t pos = 0; pos <= len - 4; pos++) {
      // Get 4 characters of the IP address
      displayChar((pos < len) ? ipString.charAt(pos) : ' ', leds1, color);
      displayChar(((pos + 1) < len) ? ipString.charAt(pos + 1) : ' ', leds2, color);
      displayChar(((pos + 2) < len) ? ipString.charAt(pos + 2) : ' ', leds3, color);
      displayChar(((pos + 3) < len) ? ipString.charAt(pos + 3) : ' ', leds4, color);
      
      // Turn off colon during IP display
      for (uint8_t i = 0; i < COLON_COUNT; i++) {
        colon[i] = CRGB::Black;
      }
      
      FastLED.show();
      delay(500); // Slightly slower scroll for readability
    }
    
    // Pause at the end before restarting the scroll
    delay(1000);
  }
}

// Calculate brightness based on time of day
uint8_t getTimeBrightness() {
  if (!autoBrightnessEnabled) {
    return userBrightness; // Return user setting if auto brightness is disabled
  }
  
  uint8_t hour = Poland.hour();
  uint8_t brightnessPercent;
  
  // Night time: 22:00-06:00
  if (hour >= 22 || hour < 6) {
    brightnessPercent = nightBrightness;
  }
  // Full day: 09:00-18:00
  else if (hour >= 9 && hour < 18) {
    brightnessPercent = dayBrightness;
  }
  // Transition periods: 06:00-09:00 and 18:00-22:00
  else {
    brightnessPercent = transitionBrightness;
  }
  
  // Calculate the actual brightness value based on user setting as maximum
  uint8_t maxBrightness = userBrightness;
  return (maxBrightness * brightnessPercent) / 100;
}

// Save settings to non-volatile storage
void saveSettings() {
  preferences.begin("clockSettings", false); // "clockSettings" is the namespace
  
  preferences.putUChar("brightness", userBrightness);
  preferences.putUChar("mode", mode);
  preferences.putUChar("red", staticColor.red);
  preferences.putUChar("green", staticColor.green);
  preferences.putUChar("blue", staticColor.blue);
  preferences.putUChar("rainbowSpeed", rainbowSpeed);
  preferences.putBool("autoBrightness", autoBrightnessEnabled);
  preferences.putUChar("dayBrightness", dayBrightness);
  preferences.putUChar("nightBrightness", nightBrightness);
  preferences.putUChar("transitionBrightness", transitionBrightness);
  
  preferences.end();
  Serial.println("Settings saved to flash");
}

// Load settings from non-volatile storage
void loadSettings() {
  preferences.begin("clockSettings", true); // true = read-only mode
  
  if (preferences.isKey("brightness")) {
    userBrightness = preferences.getUChar("brightness", 25);
    mode = preferences.getUChar("mode", 0);
    staticColor.red = preferences.getUChar("red", 255);
    staticColor.green = preferences.getUChar("green", 0);
    staticColor.blue = preferences.getUChar("blue", 0);
    rainbowSpeed = preferences.getUChar("rainbowSpeed", 1);
    autoBrightnessEnabled = preferences.getBool("autoBrightness", true);
    dayBrightness = preferences.getUChar("dayBrightness", 100);
    nightBrightness = preferences.getUChar("nightBrightness", 10);
    transitionBrightness = preferences.getUChar("transitionBrightness", 50);
    
    Serial.println("Settings loaded from flash");
  } else {
    Serial.println("No saved settings found, using defaults");
  }
  
  preferences.end();
}

// ===== Web Server Handlers =====

// Main page with Tailwind CSS for styling
void handleRoot() {
  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Clock Settings</title>";
  html += "<script src='https://cdn.tailwindcss.com'></script>";
  html += "<script src='https://cdn.jsdelivr.net/npm/@jaames/iro@5'></script>"; // Add color picker library
  html += "</head><body class='bg-gray-100 flex items-center justify-center min-h-screen'>";
  html += "<div class='bg-white shadow-lg rounded-lg p-8 max-w-md w-full'>";
  html += "<h1 class='text-2xl font-bold mb-6 text-center'>Clock Settings</h1>";
  html += "<form action='/set' method='GET' class='space-y-4'>";
  
  // Brightness field (in %)
  html += "<div>";
  html += "<label class='block text-gray-700'>Max Brightness (0-100%):</label>";
  html += "<input type='number' name='brightness' min='0' max='100' value='" + String(userBrightness * 100 / 255) + "' class='mt-1 block w-full px-3 py-2 border rounded-md' required>";
  html += "</div>";
  
  // Auto brightness toggle
  html += "<div>";
  html += "<label class='inline-flex items-center'>";
  html += "<input type='checkbox' name='autoBrightness' value='1'" + String(autoBrightnessEnabled ? " checked" : "") + " class='form-checkbox h-4 w-4 text-blue-600'>";
  html += "<span class='ml-2'>Auto adjust brightness by time of day</span>";
  html += "</label>";
  html += "</div>";
  
  // Auto brightness settings - only visible when auto brightness is enabled using javascript
  html += "<div id='autoBrightnessSettings' " + String(autoBrightnessEnabled ? "" : "class='hidden'") + ">";
  html += "<div class='mt-2 p-3 bg-gray-50 rounded-md'>";
  
  // Day brightness (9:00-18:00)
  html += "<div class='mb-2'>";
  html += "<label class='block text-gray-700 text-sm'>Day Brightness (9:00-18:00):</label>";
  html += "<input type='number' name='dayBrightness' min='0' max='100' value='" + String(dayBrightness) + "' class='mt-1 block w-full px-3 py-2 border rounded-md text-sm'>";
  html += "</div>";
  
  // Night brightness (22:00-6:00)
  html += "<div class='mb-2'>";
  html += "<label class='block text-gray-700 text-sm'>Night Brightness (22:00-6:00):</label>";
  html += "<input type='number' name='nightBrightness' min='0' max='100' value='" + String(nightBrightness) + "' class='mt-1 block w-full px-3 py-2 border rounded-md text-sm'>";
  html += "</div>";
  
  // Transition brightness (6:00-9:00 & 18:00-22:00)
  html += "<div>";
  html += "<label class='block text-gray-700 text-sm'>Transition Brightness (6:00-9:00 & 18:00-22:00):</label>";
  html += "<input type='number' name='transitionBrightness' min='0' max='100' value='" + String(transitionBrightness) + "' class='mt-1 block w-full px-3 py-2 border rounded-md text-sm'>";
  html += "</div>";
  
  html += "</div>"; // End of auto brightness settings panel
  html += "</div>";
  
  // Mode field
  html += "<div>";
  html += "<span class='block text-gray-700'>Mode:</span>";
  html += "<label class='inline-flex items-center mt-1'><input type='radio' name='mode' value='0'" + String(mode==0?" checked":"") + " class='form-radio h-4 w-4 text-blue-600' id='rainbow-mode'><span class='ml-2'>Rainbow (Color Transition)</span></label><br>";
  html += "<label class='inline-flex items-center mt-1'><input type='radio' name='mode' value='1'" + String(mode==1?" checked":"") + " class='form-radio h-4 w-4 text-blue-600' id='static-mode'><span class='ml-2'>Static (Fixed Color)</span></label>";
  html += "</div>";
  
  // Rainbow speed control - only visible when rainbow mode is selected
  html += "<div id='rainbowSettings' " + String(mode==0 ? "" : "class='hidden'") + ">";
  html += "<div class='mt-2 p-3 bg-gray-50 rounded-md'>";
  html += "<label class='block text-gray-700 text-sm'>Rainbow Speed (1-10):</label>";
  html += "<div class='flex items-center space-x-2'>";
  html += "<span class='text-xs'>Slow</span>";
  html += "<input type='range' name='rainbowSpeed' min='1' max='10' value='" + String(rainbowSpeed) + "' class='flex-grow'>";
  html += "<span class='text-xs'>Fast</span>";
  html += "<span class='ml-2 text-sm font-bold' id='speedValue'>" + String(rainbowSpeed) + "</span>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  // Advanced color picker (only shown when static mode is selected)
  html += "<div id='colorSettings' " + String(mode==1 ? "" : "class='hidden'") + ">";
  html += "<div class='mt-2'>";
  html += "<label class='block text-gray-700'>Color:</label>";
  html += "<div id='color-picker-container' class='mt-2 flex justify-center'></div>";
  
  // Hidden input field to store the color value
  char buf[8];
  sprintf(buf, "#%06X", staticColor.red << 16 | staticColor.green << 8 | staticColor.blue);
  html += "<input type='hidden' name='color' id='color-value' value='" + String(buf) + "'>";
  
  // Color preview
  html += "<div class='mt-2 flex justify-between items-center'>";
  html += "<span class='text-gray-700'>Preview:</span>";
  html += "<div id='color-preview' class='w-16 h-8 border' style='background-color: " + String(buf) + ";'></div>";
  html += "<span id='color-hex' class='text-sm font-mono'>" + String(buf) + "</span>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='text-center mt-6'>";
  html += "<input type='submit' value='Update Settings' class='bg-blue-500 text-white px-4 py-2 rounded hover:bg-blue-600'>";
  html += "</div>";
  html += "</form>";
  
  // JavaScript for interactive elements
  html += "<script>";
  
  // Auto brightness toggle
  html += "document.querySelector('input[name=\"autoBrightness\"]').addEventListener('change', function() {";
  html += "  document.getElementById('autoBrightnessSettings').classList.toggle('hidden', !this.checked);";
  html += "});";
  
  // Mode selection toggle for rainbow/static specific settings
  html += "document.getElementById('rainbow-mode').addEventListener('change', function() {";
  html += "  if(this.checked) {";
  html += "    document.getElementById('rainbowSettings').classList.remove('hidden');";
  html += "    document.getElementById('colorSettings').classList.add('hidden');";
  html += "  }";
  html += "});";
  
  html += "document.getElementById('static-mode').addEventListener('change', function() {";
  html += "  if(this.checked) {";
  html += "    document.getElementById('colorSettings').classList.remove('hidden');";
  html += "    document.getElementById('rainbowSettings').classList.add('hidden');";
  html += "  }";
  html += "});";
  
  // Rainbow speed slider
  html += "document.querySelector('input[name=\"rainbowSpeed\"]').addEventListener('input', function() {";
  html += "  document.getElementById('speedValue').textContent = this.value;";
  html += "});";
  
  // Initialize color picker
  html += "var colorPicker = new iro.ColorPicker('#color-picker-container', {";
  html += "  width: 180,";
  html += "  color: '" + String(buf) + "',";
  html += "  borderWidth: 1,";
  html += "  borderColor: '#ccc',";
  html += "  layout: [";
  html += "    {";
  html += "      component: iro.ui.Wheel,";
  html += "      options: {}";
  html += "    },";
  html += "    {";
  html += "      component: iro.ui.Slider,";
  html += "      options: {";
  html += "        sliderType: 'value'";
  html += "      }";
  html += "    }";
  html += "  ]";
  html += "});";
  
  // Update hidden input and preview when color changes
  html += "colorPicker.on('color:change', function(color) {";
  html += "  var hexColor = color.hexString;";
  html += "  document.getElementById('color-value').value = hexColor;";
  html += "  document.getElementById('color-preview').style.backgroundColor = hexColor;";
  html += "  document.getElementById('color-hex').textContent = hexColor;";
  html += "});";
  
  html += "</script>";
  
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

// Handler for settings update
void handleSet() {
  if (server.hasArg("brightness")) {
    int percent = server.arg("brightness").toInt();
    percent = constrain(percent, 0, 100);
    userBrightness = (percent * 255) / 100;
  }
  
  // Handle auto brightness setting
  autoBrightnessEnabled = server.hasArg("autoBrightness");
  
  // Process brightness settings for different times of day
  if (server.hasArg("dayBrightness")) {
    dayBrightness = constrain(server.arg("dayBrightness").toInt(), 0, 100);
  }
  
  if (server.hasArg("nightBrightness")) {
    nightBrightness = constrain(server.arg("nightBrightness").toInt(), 0, 100);
  }
  
  if (server.hasArg("transitionBrightness")) {
    transitionBrightness = constrain(server.arg("transitionBrightness").toInt(), 0, 100);
  }
  
  if (server.hasArg("mode")) {
    mode = server.arg("mode").toInt();
  }
  
  if (server.hasArg("color")) {
    String colorStr = server.arg("color");
    if (colorStr.charAt(0) == '#') {
      colorStr = colorStr.substring(1);
    }
    long colorVal = strtol(colorStr.c_str(), NULL, 16);
    staticColor = CRGB((colorVal >> 16) & 0xFF, (colorVal >> 8) & 0xFF, colorVal & 0xFF);
  }
  
  // Handle rainbow speed
  if (server.hasArg("rainbowSpeed")) {
    rainbowSpeed = constrain(server.arg("rainbowSpeed").toInt(), 1, 10);
  }
  
  // Save settings to flash after update
  saveSettings();
  
  String response = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>";
  response += "<meta http-equiv='refresh' content='2; url=/'><title>Settings Updated</title>";
  response += "<script src='https://cdn.tailwindcss.com'></script></head><body class='bg-gray-100 flex items-center justify-center min-h-screen'>";
  response += "<div class='bg-white shadow-lg rounded-lg p-8 max-w-md w-full text-center'>";
  response += "<h1 class='text-2xl font-bold mb-4'>Settings Updated!</h1>";
  response += "<p class='mb-4'>Your settings have been updated and saved.</p>";
  response += "<a href='/' class='text-blue-500 hover:underline'>Return to Settings</a>";
  response += "</div></body></html>";
  server.send(200, "text/html", response);
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  
  // Load saved settings
  loadSettings();
  
  // Initialize LED displays and colon
  FastLED.addLeds<LED_TYPE, DIGIT1_PIN, COLOR_ORDER>(leds1, NUM_LEDS);
  FastLED.addLeds<LED_TYPE, DIGIT2_PIN, COLOR_ORDER>(leds2, NUM_LEDS);
  FastLED.addLeds<LED_TYPE, DIGIT3_PIN, COLOR_ORDER>(leds3, NUM_LEDS);
  FastLED.addLeds<LED_TYPE, DIGIT4_PIN, COLOR_ORDER>(leds4, NUM_LEDS);
  FastLED.addLeds<LED_TYPE, COLON_PIN, COLOR_ORDER>(colon, COLON_COUNT);
  
  // Set initial brightness
  FastLED.setBrightness(userBrightness);
  FastLED.clear();
  FastLED.show();
  
  Serial.println("Connecting to Wi‑Fi...");
  WiFi.begin(ssid, password);
  
  // Wait for Wi‑Fi connection; display status "0" meanwhile
  while (WiFi.status() != WL_CONNECTED) {
    CRGB tempColor = CHSV(globalHue, 255, 255);
    displayStatus(0, tempColor);
    delay(1000);
    globalHue++;
  }
  Serial.println("Wi‑Fi Connected!");
  
  // Initialize and set the time using ezTime
  waitForSync(2);  // 30 seconds timeout
  
  // Set timezone to Poland (Europe/Warsaw)
  Poland.setLocation("Europe/Warsaw");
  Serial.print("Poland time: ");
  Serial.println(Poland.dateTime());
  
  // Start web server
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();
  Serial.println("Web server started!");
  
  // Show IP address on startup
  CRGB displayColor = (mode == 0) ? CHSV(globalHue, 255, 255) : staticColor;
  showIPAddress(displayColor);
}

// ===== Main Loop =====
void loop() {
  // Update brightness based on time of day
  FastLED.setBrightness(getTimeBrightness());
  
  // Handle web server requests
  server.handleClient();
  
  // Update time with ezTime (non-blocking)
  events();
  
  // Determine current color based on mode
  CRGB currentColor = (mode == 0) ? CHSV(globalHue, 255, 255) : staticColor;
  
  // Display clock if time obtained; if not, show status "2"
  if (timeStatus() == timeSet) {
    displayTime(currentColor);
  } else {
    if (WiFi.status() == WL_CONNECTED) {
      displayStatus(2, currentColor);
    } else {
      // If Wi‑Fi is lost after time was obtained, continue displaying last time
      displayTime(currentColor);
    }
  }
  
  // Shorter delay to allow more frequent updates
  delay(50);
  
  // Update rainbow color with configurable speed
  if (mode == 0) {
    globalHue += rainbowSpeed; // Increment by speed value
  }
}
