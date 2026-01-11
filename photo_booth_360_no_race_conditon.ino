#include <FastLED.h>
#include <RCSwitch.h>
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

// ========== PINS ==========
#define IN1 16
#define IN2 17
#define RF_PIN 12
#define LED_PIN 14
#define BUZZER 18
#define STATUS 19

// ========== RF CODES ==========
#define B1 49592UL  
#define B2 49588UL
#define B3 49596UL
#define B4 49586UL
#define B5 49594UL
#define B6 49590UL
#define B7 49598UL
#define B8 49585UL

RCSwitch rf;

// RF debounce
unsigned long lastRFTime = 0;
const unsigned long RF_DEBOUNCE = 200;   // ms

// ========== LED ==========
#define NUM_LEDS 60
CRGB leds[NUM_LEDS];
bool ledOn=false;
bool brightHigh=false;
uint8_t effect=0;
uint8_t r=255,g=0,b=0;

// ========== MOTOR ==========
int currentSpeed=0;
int targetSpeed=0;
int currentDir=1;   // 1 = forward, -1 = reverse
int targetDir=1;

const int DEFAULT_SPEED=128;
const int SPEED_STEP=32;
const int RAMP_STEP=1;
const int RAMP_DELAY=5;
unsigned long lastRamp=0;

// ========== PWM ==========
void writePWM(int v){
  if(currentDir==1){ analogWrite(IN1,0); analogWrite(IN2,v); }
  else if(currentDir==-1){ analogWrite(IN2,0); analogWrite(IN1,v); }
  else{ analogWrite(IN1,0); analogWrite(IN2,0); }
}

// ========== MOTOR FSM ==========
void updateMotor(){
  if(millis()-lastRamp<RAMP_DELAY) return;

  // If reverse, make sure that current speed is greater than zero!!
  if(currentSpeed==0 && currentDir!=targetDir) 
  {
    currentDir=targetDir;
    targetSpeed= DEFAULT_SPEED;
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

  // CLAMP SPEED
  currentSpeed = constrain(currentSpeed, 0, 255);
  writePWM(currentSpeed);

  

}

// ========== LED ==========
void updateLED(){
  if(!ledOn){ FastLED.clear(); FastLED.show(); return; }

  if(effect==0) fill_solid(leds,NUM_LEDS,CRGB(r,g,b));
  if(effect==1) fill_rainbow(leds,NUM_LEDS,millis()/10);
  if(effect==2) for(int i=0;i<NUM_LEDS;i++) leds[i]=(i+millis()/120)%3?CRGB::Black:CRGB(r,g,b);

  FastLED.show();
}

// ========== 8 BUTTON LOGIC ==========
void handleButton(uint8_t b){
  switch(b){
    case 1:  if(targetSpeed==0){ targetSpeed=DEFAULT_SPEED; targetDir=1; } else targetSpeed=0; break;
    case 2:  ledOn=!ledOn; break;
    case 3:  targetSpeed=min(targetSpeed+SPEED_STEP,255); break;
    case 4:  effect=(effect+1)%3; break;
    case 5:  targetSpeed=max(targetSpeed-SPEED_STEP,0); break;
    case 6:  if(r){r=0;g=255;} else if(g){g=0;b=255;} else{b=0;r=255;} break;
    case 7:  targetSpeed=0; targetDir=-targetDir; break;
    case 8:  brightHigh=!brightHigh; FastLED.setBrightness(brightHigh?240:120); break;
  }
  tone(BUZZER,2000,50);
}

// ========== RF ==========
void handleRF(){
  // If no signal, no problem, just skip
  if(!rf.available()) return;
  // Debounce time to prevent duplicate.
  unsigned long now = millis();
  if(now - lastRFTime<RF_DEBOUNCE)
  {
    rf.resetAvailable();
    return;
  }
  lastRFTime=now; // Once 

  unsigned long c=rf.getReceivedValue();
  rf.resetAvailable();

  if(c==B1)handleButton(1);
  if(c==B2)handleButton(2);
  if(c==B3)handleButton(3);
  if(c==B4)handleButton(4);
  if(c==B5)handleButton(5);
  if(c==B6)handleButton(6);
  if(c==B7)handleButton(7);
  if(c==B8)handleButton(8);
}

//  ==========================BLUETOOTH===========================
void handleBluetooth()
{
  if(!SerialBT.available()) return;

  char c = SerialBT.read();
  if(c < '1' || c > '8') return;   // ignore CR, LF, etc

  uint8_t b = c - '0';
  handleButton(b);
}


// ========== SETUP ==========
void setup(){
  pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT);
  pinMode(BUZZER,OUTPUT); pinMode(STATUS,OUTPUT);

  rf.enableReceive(digitalPinToInterrupt(RF_PIN));
  SerialBT.begin("ESP32-360Booth");


  FastLED.addLeds<WS2812B,LED_PIN,GRB>(leds,NUM_LEDS);
  FastLED.setBrightness(120);
}

// ========== LOOP ==========
void loop(){
  handleRF();
  handleBluetooth();
  updateMotor();
  updateLED();
}
