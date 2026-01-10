#include <FastLED.h>
#include <RCSwitch.h>

// ================== DRV8871 MOTOR CONTROL — ESP32 (Arduino analogWrite) ==================
// IN1 = GPIO16
// IN2 = GPIO17
//
// PWM: 0–255 (classic Arduino style, our own convention)
// Default speed: 128 (≈50%)
// Step size: 26 (≈10%)
// Smooth ramp acceleration/deceleration
// =========================================================================================
#define IN1_PIN 16
#define IN2_PIN 17
#define LED_STATUS 19    // Onboard status LED (if any)
#define BUZZER_PIN 18


// 8 Button 433MHz remote code
#define ON_OFF              49592UL  
#define WLED_ON_OFF         49588UL
#define SPEED_UP            49596UL
#define WLED_EFFECT         49586UL
#define SPEED_DOWN          49594UL
#define WLED_RGB            49590UL
#define DIRECTION_REVERSE   49598UL
#define WLED_BRIGHTNESS     49585UL

bool systemOn=false;

// MOTOR DRIVE===================================
// -------- PWM limits --------
const int PWM_MAX = 255;
const int DEFAULT_SPEED = 128;   // 50%
const int SPEED_STEP = 26;       // about 10%
// -------- ramp profile --------
const int RAMP_STEP = 2;         // smaller = smoother
const int RAMP_DELAY = 50;        // ms per small change
// -------- current state --------
int currentSpeed = 0;            // 0–255
int currentDir = 0;              // 1 forward, -1 reverse, 0 stop

// LED STRIP DRIVE=============================================
// ---------- LED STRIP SETTINGS ----------
#define LED_PIN   14     // WLED data pin 3V3
#define NUM_LEDS 60
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];
// ---------- BRIGHTNESS ----------
uint8_t brightness50 = 128;   // 50%
uint8_t brightness90 = 230;   // ~90%
bool brightHigh = false;      // default 50%
// ---------- POWER ----------
bool stripOn = false;         // OFF at system start
// ---------- COLOR ----------
uint8_t redVal = 255;
uint8_t greenVal = 0;
uint8_t blueVal = 0;
uint8_t colorMode = 0;   // 0=R, 1=G, 2=B
// ---------- EFFECT STATE ----------
uint8_t currentEffect = 0;
const uint8_t totalEffects = 4;  // update if you add more

// RF 433MHZ RECEIVER ==================================
#define RF_PIN    12
unsigned long lastCode = 0;
unsigned long lastTime = 0;
RCSwitch rf = RCSwitch();


// Prototype functions
void system_on_off();
void writePWM(int duty);
void startMotor();
void stopMotor();
void increaseSpeed();
void decreaseSpeed();
void reverseMotor();

void wledSetup();
void togglePower();
void updateStrip();
void nextEffect();
void toggleBrightness();
void setColorRGB(uint8_t r, uint8_t g, uint8_t b);
void nextColor();

void rf433MhzSetup();


// LED STRIP EFFECTS
void solidColor();
void rainbowEffect();
void threaterChase();
void colorWipe();
void updateStripEffect();



void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  // Setup pins
  pinMode(LED_STATUS, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  // Setup FastLED
  wledSetup();
  // RF receiver on interrupt 0 (GPIO12 on ESP32)
  rf433MhzSetup();
}

void loop() {
  // Check for incoming RF code
  if (rf.available()) {
    // unsigned long code = rf.getReceivedValue();
    // unsigned int bitlen = rf.getReceivedBitlength();
    // unsigned int proto = rf.getReceivedProtocol();
    //  if (code == 0) {
    //   Serial.println("Unknown code");
    // } else {
    //   // simple debounce: ignore repeats within 200 ms
    //   if (millis() - lastTime > 200 || code != lastCode) {

    //     Serial.print("Code: ");
    //     Serial.print(code);
    //     Serial.print("  Bits: ");
    //     Serial.print(bitlen);
    //     Serial.print("  Protocol: ");
    //     Serial.println(proto);

    //     lastCode = code;
    //     lastTime = millis();
    //   }
    // }
    unsigned long code = rf.getReceivedValue();
    // Simple beep to acknowledge 
    tone(BUZZER_PIN, 262, 100); // Middle C piano
    
    switch(code) {
      case ON_OFF:  // Toggle system ON/OFF
        system_on_off();
        break;
      case WLED_ON_OFF:  // Toggle LED strip ON/OFF
        togglePower();
        break;
      case SPEED_UP:  // Increase speed
        increaseSpeed();
        break;
      case WLED_EFFECT:  
        nextEffect();
        break;
      case SPEED_DOWN:  // Decrease speed
        decreaseSpeed();
        break;
      case WLED_RGB:  // Set color R/G/B (cycle among colors)
        nextColor();
        setColorRGB(redVal, greenVal, blueVal);
        break;
      case DIRECTION_REVERSE:  
        reverseMotor();
        break;
      case WLED_BRIGHTNESS:  // Change wled's brightness
        toggleBrightness();
        break;
      default:
        Serial.println("Unknown Code!!!");
        break;
    }
    rf.resetAvailable();
  }
  
  // If system and LEDs are on, update effects
  if (systemOn && stripOn) 
  {
    updateStripEffect();
  }
}

void system_on_off()
{
  systemOn = !systemOn;
  if(systemOn)
  { 
    digitalWrite(LED_STATUS, HIGH);
    startMotor();
    tone(BUZZER_PIN, 1000, 1000);  // Long beep
  }
  else
  {
    digitalWrite(LED_STATUS, LOW);
    stopMotor();
    tone(BUZZER_PIN, 1000, 1000);  // Long beep
  }
  
}
void wled_on_off()
{
  stripOn = !stripOn;
  if (!stripOn) {
    FastLED.clear(); 
    FastLED.show();
  }
  else {
    FastLED.setBrightness(brightness50);
    // restore last effect/color if needed
  }
}

// Analog value is in range 0-255.
void writePWM(int duty)
{
  duty = constrain(duty, 0, PWM_MAX);
  if (currentDir == 1) {               // forward
    analogWrite(IN1_PIN, 0);           // IN1 LOW
    analogWrite(IN2_PIN, duty);        // PWM on IN2
  }
  else if (currentDir == -1) {         // reverse
    analogWrite(IN2_PIN, 0);           // IN2 LOW
    analogWrite(IN1_PIN, duty);        // PWM on IN1
  }
}

// ================== START FORWARD ==================
void startMotor()
{
  currentDir = 1;
  int target = DEFAULT_SPEED;
  for (int v = 0; v <= target; v += RAMP_STEP) {
    writePWM(v);
    delay(RAMP_DELAY);
  }

  currentSpeed = target;
}

// ================== SMOOTH STOP ==================
void stopMotor()
{
  for (int v = currentSpeed; v >= 0; v -= RAMP_STEP) {
    writePWM(v);
    delay(RAMP_DELAY);
  }

  analogWrite(IN1_PIN, 0);
  analogWrite(IN2_PIN, 0);

  currentSpeed = 0;
  currentDir = 0;
}


// ================== INCREASE SPEED 10% ==================
void increaseSpeed()
{
  if (currentDir == 0) return; // Motor stop. 
  int target = constrain(currentSpeed + SPEED_STEP, 0, PWM_MAX);
  for (int v = currentSpeed; v <= target; v += RAMP_STEP) {
    writePWM(v);
    delay(RAMP_DELAY);
  }
  currentSpeed = target;
}


// ================== DECREASE SPEED 10% ==================
void decreaseSpeed()
{
  if (currentDir == 0) return;    // Motor stop
  int target = currentSpeed - SPEED_STEP;
  if (target <= 0) {              // Stop the motor.
    stopMotor();
    return;
  }
  for (int v = currentSpeed; v >= target; v -= RAMP_STEP) {
    writePWM(v);
    delay(RAMP_DELAY);
  }
  currentSpeed = target;
}


// ================== SAFE REVERSE ==================
void reverseMotor()
{
  if (currentDir == 0) return;
  int previousSpeed = currentSpeed;
  int previousDir = currentDir;       // save CURRENT direction BEFORE stopping => Fixed
  stopMotor();                 // smooth decel → stop
  currentDir = -previousDir;    // flip direction
  for (int v = 0; v <= previousSpeed; v += RAMP_STEP) {
    writePWM(v);
    delay(RAMP_DELAY);
  }

  currentSpeed = previousSpeed;
}

// Setup for led strip
void wledSetup()
{
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.clear(true);
  FastLED.setBrightness(brightness50);
}

// Turn the wled on and off
void togglePower() {
  stripOn = !stripOn;

  if (!stripOn) {
    FastLED.clear(true);
  }
}
// Change effect
void nextEffect() {
  currentEffect++;
  if (currentEffect >= totalEffects) currentEffect = 0;
}
// Change R/G/B

// Toogle the brightness
void toggleBrightness() {
  brightHigh = !brightHigh;

  if (brightHigh)
    FastLED.setBrightness(brightness90);
  else
    FastLED.setBrightness(brightness50);
}


void nextColor()
{
  colorMode++;
  if(colorMode>2) colorMode=0;
   if (colorMode == 0) {        // Red
    redVal = 255; greenVal = 0; blueVal = 0;
  }
  else if (colorMode == 1) {   // Green
    redVal = 0; greenVal = 255; blueVal = 0;
  }
  else if (colorMode == 2) {   // Blue
    redVal = 0; greenVal = 0; blueVal = 255;
  }
}
void setColorRGB(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(r, g, b);
  }
  FastLED.show();
}
void updateStripEffect() {
  if (!stripOn) {
    FastLED.clear(true);
    return;
  }

  switch (currentEffect) {
    case 0: solidColor(); break;
    case 1: rainbowEffect(); break;
    case 2: theaterChase(); break;
    case 3: colorWipe(); break;
  }

  FastLED.show();
}

// WLED EFFECTS DEFINE==============
void solidColor() {
  fill_solid(leds, NUM_LEDS, CRGB(redVal, greenVal, blueVal));
}
void rainbowEffect() {
  static uint8_t hue = 0;
  hue++;

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(hue + i * 3, 255, 255);
  }
}
void theaterChase() {
  static uint8_t offset = 0;
  offset++;

  for (int i = 0; i < NUM_LEDS; i++) {
    if ((i + offset) % 3 == 0)
      leds[i] = CRGB(redVal, greenVal, blueVal);
    else
      leds[i] = CRGB::Black;
  }
}
void colorWipe() {
  static int pos = 0;
  leds[pos] = CRGB(redVal, greenVal, blueVal);
  pos++;
  if (pos >= NUM_LEDS) pos = 0;
}


// Rf433Mhz receiver 
void rf433MhzSetup()
{
  rf.enableReceive(digitalPinToInterrupt(RF_PIN));  // ESP32/ESP8266 compatible
  Serial.println("Listening for 433 MHz remote codes...");
}

  
