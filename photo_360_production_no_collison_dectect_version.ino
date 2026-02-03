#include <FastLED.h>
#include <RCSwitch.h>
#include "esp_task_wdt.h"

// ===== PINS =====
#define IN1 16
#define IN2 17
#define RF_PIN 12
#define LED_PIN 14
#define BUZZER 18
#define STATUS 19 // Active LOW LED

// ===== RF CODES =====

// RF03
#define B1 15627496UL  
#define B2 15627492UL
#define B3 15627500UL
#define B4 15627490UL
#define B5 15627498UL
#define B6 15627494UL
#define B7 15627502UL
#define B8 15627489UL

//RF04
#define B12 2471144UL  
#define B22 2471140UL
#define B32 2471148UL
#define B42 2471138UL
#define B52 2471146UL
#define B62 2471142UL
#define B72 2471150UL
#define B82 2471137UL


RCSwitch rf;
//BluetoothSerial SerialBT;

// RF debounce
unsigned long lastRFTime = 0;
const unsigned long RF_DEBOUNCE = 300;

// ===== LED =====
#define NUM_LEDS 60
CRGB leds[NUM_LEDS];
bool ledOn=false;
bool brightHigh=false;
uint8_t effect=0;
const uint8_t EFFECT_COUNT=9;
uint8_t r=255,g=0,b=0;

// ===== MOTOR =====
bool coasting=false;
unsigned long coastStart=0;
#define COAST_TIME 1000   // ms
int currentSpeed=0;
int targetSpeed=0;
int currentDir=1;
int targetDir=1;

// 3-level speed
uint8_t speedLevel=0;
const uint8_t speedTable[4]={0,235,245,255};

const int RAMP_STEP=1;
const int RAMP_DELAY=9;
unsigned long lastRamp=0;


// =================================================== FUNCTIONS =====================================
// ===== PWM =====
void writePWM(int v){
  if(currentDir==1){
    analogWrite(IN1,0);
    analogWrite(IN2,v);
  }
  else if(currentDir==-1){
    analogWrite(IN2,0);
    analogWrite(IN1,v);
  }
  else{
    analogWrite(IN1,0);
    analogWrite(IN2,0);   // COAST
  }
}

// ===== MOTOR FSM =====
void updateMotor(){
  if(millis()-lastRamp<RAMP_DELAY) return;

  // Save the driver!!!!
  if(coasting){
    if(millis() - coastStart < COAST_TIME){
        writePWM(0);    // both pins LOW → coast
        return;
    }else{
        coasting=false;
        currentDir = targetDir;
        targetSpeed = speedTable[speedLevel];
    }
}

  if(currentSpeed==0 && currentDir!=targetDir){
    // enter coast before reversing
    coasting=true;
    coastStart=millis();
    currentDir=0;        // coast mode
    writePWM(0);
    lastRamp=millis();  
    return;
}

  lastRamp=millis();

  if(currentSpeed<targetSpeed){
    currentSpeed+=RAMP_STEP;
    if(currentSpeed>targetSpeed) currentSpeed=targetSpeed;
  }
  else if(currentSpeed>targetSpeed){
    currentSpeed-=RAMP_STEP;
    if(currentSpeed<targetSpeed) currentSpeed=targetSpeed;
  }

  currentSpeed=constrain(currentSpeed,0,255);
  writePWM(currentSpeed);
}
// ===== FEEDBACK SIGNALS =====
void feedback(uint16_t ms){
  digitalWrite(STATUS, HIGH);   // LED OFF
  tone(BUZZER, 2000);
  delay(ms);
  noTone(BUZZER);
  digitalWrite(STATUS, LOW);  // LED ON
}


// ===== RGB Cycle =====
void nextColor(){
  if(r==255){ r=0; g=255; b=0; }
  else if(g==255){ r=0; g=0; b=255; }
  else{ r=255; g=0; b=0; }
}

// ===== LED Effects =====
void updateLED(){
  if(!ledOn){ FastLED.clear(); FastLED.show(); return; }

  static uint8_t hue=0;
  static uint16_t pos=0;
  hue++;

  switch(effect){
    case 0: fill_solid(leds,NUM_LEDS,CRGB(r,g,b)); break;
    case 1: fill_rainbow(leds,NUM_LEDS,hue,5); break;
    case 2: for(int i=0;i<NUM_LEDS;i++) leds[i]=((i+pos)%3==0)?CRGB(r,g,b):CRGB::Black; pos++; break;
    case 3:{ uint8_t v=sin8(millis()/6); fill_solid(leds,NUM_LEDS,CRGB(r*v/255,g*v/255,b*v/255)); } break;
    case 4: fill_solid(leds,NUM_LEDS,CRGB(r/5,g/5,b/5)); leds[random(NUM_LEDS)]=CRGB(r,g,b); break;
    case 5: for(int i=0;i<NUM_LEDS;i++) leds[i]=CHSV(hue+i*5,255,255); break;
    case 6: fill_solid(leds,NUM_LEDS,CRGB::Black); leds[pos%NUM_LEDS]=CRGB(r,g,b); pos++; break;
    case 7: for(int i=0;i<NUM_LEDS;i++){ uint8_t f=random(150,255); leds[i]=CRGB((r*f)/255,(g*f)/255,(b*f)/255);} break;
    case 8: if((millis()/100)%2) fill_solid(leds,NUM_LEDS,CRGB(r,g,b)); else fill_solid(leds,NUM_LEDS,CRGB::Black); break;
  }

  FastLED.show();
}

// ===== Button Logic =====
void handleButton(uint8_t b){
  switch(b){
    // ===== ON / OFF =====
    case 1:
      if(speedLevel == 0){
        // Motor currently OFF → try to START

        if(targetSpeed == 0){
          // No speed selected yet → do nothing (just feedback)
          break;
        }

        // Use pre-selected speed
        speedLevel = constrain(speedLevel,1,3);
        targetSpeed = speedTable[speedLevel];
        targetDir = 1;

      }else{
        // Motor ON → STOP
        speedLevel = 0;
        targetSpeed = 0;
      }
      break;
    case 2: ledOn=!ledOn; break;
    case 3: if(speedLevel<3) speedLevel++; targetSpeed=speedTable[speedLevel]; break;
    case 4: effect=(effect+1)%EFFECT_COUNT; break;
    case 5: if(speedLevel>1) speedLevel--; targetSpeed=speedTable[speedLevel]; break;
    case 6: nextColor(); break;
    case 7: targetSpeed=0; targetDir=-targetDir; break;
    case 8: brightHigh=!brightHigh; FastLED.setBrightness(brightHigh?220:110); break;
  }
  feedback(40); // Indicating. 
}

// ===== RF =====
void handleRF(){
  if(!rf.available()) return;

  unsigned long now=millis();
  if(now-lastRFTime<RF_DEBOUNCE){ rf.resetAvailable(); return; }
  lastRFTime=now;

  unsigned long c=rf.getReceivedValue();
  rf.resetAvailable();

  if(c==B1||c==B12)handleButton(1);
  if(c==B2||c==B22)handleButton(2);
  if(c==B3||c==B32)handleButton(3);
  if(c==B4||c==B42)handleButton(4);
  if(c==B5||c==B52)handleButton(5);
  if(c==B6||c==B62)handleButton(6);
  if(c==B7||c==B72)handleButton(7);
  if(c==B8||c==B82)handleButton(8);
}

// ===== Bluetooth =====
// void handleBluetooth(){
//   if(!SerialBT.available()) return;
//   char c=SerialBT.read();
//   if(c<'1'||c>'8') return;
//   handleButton(c-'0');
// }



// ===== Setup =====
void setup(){
  pinMode(IN1,OUTPUT);
  pinMode(IN2,OUTPUT);
  pinMode(BUZZER,OUTPUT);
  pinMode(STATUS,OUTPUT);

  digitalWrite(STATUS, LOW);   // LED OFF (active-LOW)


  rf.enableReceive(digitalPinToInterrupt(RF_PIN));
  //SerialBT.begin("ESP32-360Booth");

  FastLED.addLeds<WS2812B,LED_PIN,GRB>(leds,NUM_LEDS);
  FastLED.setBrightness(110);
    
  // ====== WATCHDOG TIMER ============
  esp_task_wdt_config_t wdt_config = 
  {
    .timeout_ms = 6000,       // 6 seconds
    .idle_core_mask = (1 << 0) | (1 << 1),   // both cores
    .trigger_panic = true    // reset on timeout
  };

  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);    // add loop() task
}

// ===== Loop =====
void loop(){
  //handleBluetooth();
  handleRF();
  updateMotor();
  updateLED();

  esp_task_wdt_reset(); // keep system alive

}
