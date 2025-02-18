#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <FastLED.h>
#include <WebServer.h>

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

// Digit mapping for numbers (existing 7-seg style)
const bool digits[10][NUM_LEDS] = {
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

// ===== Letter Mapping for Greeting =====
// Order: {Middle, Top_Right, Bottom_Right, Bottom, Bottom_Left, Top_Left, Top}
const bool letter_H[NUM_LEDS]     = {1, 0, 1, 1, 0, 1, 1};
const bool letter_E[NUM_LEDS]     = {1, 1, 0, 0, 1, 1, 1};
const bool letter_L[NUM_LEDS]     = {0, 1, 0, 0, 0, 1, 1};
const bool letter_O[NUM_LEDS]     = {0, 1, 1, 1, 1, 1, 1}; // Same as digit 0
const bool letter_C[NUM_LEDS]     = {0, 1, 0, 0, 1, 1, 1};
const bool letter_A[NUM_LEDS]     = {1, 0, 1, 1, 1, 1, 1};
const bool letter_space[NUM_LEDS] = {0, 0, 0, 0, 0, 0, 0};

// Returns pointer to mapping array for given character
const bool* getCharMapping(char c) {
  switch(toupper(c)) {
    case 'H': return letter_H;
    case 'E': return letter_E;
    case 'L': return letter_L;
    case 'O': return letter_O;
    case 'C': return letter_C;
    case 'A': return letter_A;
    default:  return letter_space;
  }
}

// Display a character on a given LED array with specified color
void displayChar(char c, CRGB ledsArray[], CRGB color) {
  const bool* mapping = getCharMapping(c);
  for (int i = 0; i < NUM_LEDS; i++) {
    ledsArray[i] = mapping[i] ? color : CRGB::Black;
  }
}

// Scrolls the greeting message "HELO CAHA" on the 4-digit display for 10 seconds
void showGreeting(CRGB color) {
  String greeting = "HELO CAHA";
  int len = greeting.length();
  unsigned long startTime = millis();
  while (millis() - startTime < 10000) { // scroll for 10 seconds
    for (int pos = 0; pos <= len; pos++) {
      char ch1 = (pos < len) ? greeting.charAt(pos) : ' ';
      char ch2 = ((pos + 1) < len) ? greeting.charAt(pos + 1) : ' ';
      char ch3 = ((pos + 2) < len) ? greeting.charAt(pos + 2) : ' ';
      char ch4 = ((pos + 3) < len) ? greeting.charAt(pos + 3) : ' ';
      displayChar(ch1, leds1, color);
      displayChar(ch2, leds2, color);
      displayChar(ch3, leds3, color);
      displayChar(ch4, leds4, color);
      // Turn off colon during greeting
      for (int i = 0; i < COLON_COUNT; i++) {
        colon[i] = CRGB::Black;
      }
      FastLED.show();
      delay(300);
    }
  }
}

// ===== Wi‑Fi & NTP Settings =====
// Replace with your Wi‑Fi credentials:
const char* ssid     = "SANDS_WiFi";
const char* password = "Ytp@29!144";

// NTP: offset +3600 for GMT+1 (Poland)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);

// ===== Global variables for mode & brightness =====
int userBrightness = 25;         // Brightness (0-255). Default ~10%
int mode = 0;                    // 0 - Rainbow (color transition), 1 - Static (fixed color)
CRGB staticColor = CRGB::Red;    // Default static color (red)
uint8_t globalHue = 0;           // For rainbow mode

// ===== Web Server on port 80 =====
WebServer server(80);

// ===== Display Functions for Digits =====

// Display a digit (0-9) on a given LED array with specified color
void displayDigit(int digit, CRGB ledsArray[], CRGB color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    ledsArray[i] = digits[digit][i] ? color : CRGB::Black;
  }
}

// Display a status (same digit on all displays); used for showing "0" or "2"
void displayStatus(int status, CRGB color) {
  displayDigit(status, leds1, color);
  displayDigit(status, leds2, color);
  displayDigit(status, leds3, color);
  displayDigit(status, leds4, color);
  // Turn off colon
  for (int i = 0; i < COLON_COUNT; i++) {
    colon[i] = CRGB::Black;
  }
  FastLED.show();
}

// Display the time (hours and minutes)
void displayTime(CRGB color) {
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();
  int h1 = hours / 10;
  int h2 = hours % 10;
  int m1 = minutes / 10;
  int m2 = minutes % 10;
  
  displayDigit(h1, leds1, color);
  displayDigit(h2, leds2, color);
  displayDigit(m1, leds3, color);
  displayDigit(m2, leds4, color);
  
  // Blink colon: on if even second, off if odd
  int seconds = timeClient.getSeconds();
  if (seconds % 2 == 0) {
    for (int i = 0; i < COLON_COUNT; i++) {
      colon[i] = color;
    }
  } else {
    for (int i = 0; i < COLON_COUNT; i++) {
      colon[i] = CRGB::Black;
    }
  }
  FastLED.show();
}

// ===== Web Server Handlers =====

// Main page with Tailwind CSS for styling
void handleRoot() {
  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Clock Settings</title>";
  html += "<script src='https://cdn.tailwindcss.com'></script>";
  html += "</head><body class='bg-gray-100 flex items-center justify-center min-h-screen'>";
  html += "<div class='bg-white shadow-lg rounded-lg p-8 max-w-md w-full'>";
  html += "<h1 class='text-2xl font-bold mb-6 text-center'>Clock Settings</h1>";
  html += "<form action='/set' method='GET' class='space-y-4'>";
  // Brightness field (in %)
  html += "<div>";
  html += "<label class='block text-gray-700'>Brightness (0-100%):</label>";
  html += "<input type='number' name='brightness' min='0' max='100' value='" + String(userBrightness * 100 / 255) + "' class='mt-1 block w-full px-3 py-2 border rounded-md' required>";
  html += "</div>";
  // Mode field
  html += "<div>";
  html += "<span class='block text-gray-700'>Mode:</span>";
  html += "<label class='inline-flex items-center mt-1'><input type='radio' name='mode' value='0'" + String(mode==0?" checked":"") + " class='form-radio h-4 w-4 text-blue-600'><span class='ml-2'>Rainbow (Color Transition)</span></label><br>";
  html += "<label class='inline-flex items-center mt-1'><input type='radio' name='mode' value='1'" + String(mode==1?" checked":"") + " class='form-radio h-4 w-4 text-blue-600'><span class='ml-2'>Static (Fixed Color)</span></label>";
  html += "</div>";
  // Color field (only used if Static mode is selected)
  html += "<div>";
  html += "<label class='block text-gray-700'>Color:</label>";
  char buf[8];
  sprintf(buf, "#%06X", staticColor.red << 16 | staticColor.green << 8 | staticColor.blue);
  html += "<input type='color' name='color' value='" + String(buf) + "' class='mt-1 block w-full h-10'>";
  html += "</div>";
  html += "<div class='text-center'>";
  html += "<input type='submit' value='Update Settings' class='bg-blue-500 text-white px-4 py-2 rounded hover:bg-blue-600'>";
  html += "</div>";
  html += "</form>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

// Handler for settings update
void handleSet() {
  if (server.hasArg("brightness")) {
    int percent = server.arg("brightness").toInt();
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    userBrightness = (percent * 255) / 100;
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
  String response = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>";
  response += "<meta http-equiv='refresh' content='2; url=/'><title>Settings Updated</title>";
  response += "<script src='https://cdn.tailwindcss.com'></script></head><body class='bg-gray-100 flex items-center justify-center min-h-screen'>";
  response += "<div class='bg-white shadow-lg rounded-lg p-8 max-w-md w-full text-center'>";
  response += "<h1 class='text-2xl font-bold mb-4'>Settings Updated!</h1>";
  response += "<p class='mb-4'>Your settings have been updated.</p>";
  response += "<a href='/' class='text-blue-500 hover:underline'>Return to Settings</a>";
  response += "</div></body></html>";
  server.send(200, "text/html", response);
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  
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
  
  // Start NTP client
  timeClient.begin();
  unsigned long startWait = millis();
  while (timeClient.getEpochTime() == 0 && millis() - startWait < 30000) {
    timeClient.update();
    Serial.println("Waiting for NTP time...");
    delay(500);
  }
  if (timeClient.getEpochTime() == 0) {
    Serial.println("Failed to obtain NTP time.");
  } else {
    Serial.print("Time obtained: ");
    Serial.println(timeClient.getFormattedTime());
  }
  
  // Start web server
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();
  Serial.println("Web server started!");
  
  // Show greeting message for 10 seconds before starting the clock
  CRGB greetColor = (mode == 0) ? CHSV(globalHue, 255, 255) : staticColor;
  showGreeting(greetColor);
}

// ===== Main Loop =====
void loop() {
  // Update brightness
  FastLED.setBrightness(userBrightness);
  
  // Handle web server requests
  server.handleClient();
  
  // Determine current color based on mode
  CRGB currentColor;
  if (mode == 0) {
    currentColor = CHSV(globalHue, 255, 255);
  } else {
    currentColor = staticColor;
  }
  
  // Display clock if time obtained; if not, show status "2"
  if (timeClient.getEpochTime() != 0) {
    timeClient.update();
    displayTime(currentColor);
  } else {
    if (WiFi.status() == WL_CONNECTED) {
      displayStatus(2, currentColor);
    } else {
      // If Wi‑Fi is lost after time was obtained, continue displaying last time
      displayTime(currentColor);
    }
  }
  
  delay(1000);
  globalHue++; // Smooth color change for rainbow mode
}
