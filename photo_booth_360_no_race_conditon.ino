#include <FastLED.h>
#include <RCSwitch.h>

// ================= PIN MAP =================
#define IN1_PIN 16
#define IN2_PIN 17
#define RF_PIN  12
#define LED_PIN 14
#define BUZZER  18
#define STATUS  19

// ================= REMOTE CODES =================
#define ON_OFF            49592UL  
#define WLED_ON_OFF       49588UL
#define SPEED_UP          49596UL
#define WLED_EFFECT       49586UL
#define SPEED_DOWN        49594UL
#define WLED_RGB          49590UL
#define REVERSE           49598UL
#define WLED_BRIGHTNESS   49585UL

// ================= MOTOR =================
const int PWM_MAX = 255;
const int SPEED_STEP = 32;
const int DEFAULT_SPEED = 128;
const int RAMP_STEP = 3;
const int RAMP_DELAY = 15;

int currentSpeed = 0;
int targetSpeed = 0;
int currentDir = 0;     // 1 = forward, -1 = reverse
int targetDir = 1;

unsigned long lastRamp = 0;

// ================= LED =================
#define NUM_LEDS 60
CRGB leds[NUM_LEDS];

bool stripOn = false;
bool brightHigh = false;
uint8_t effect = 0;
uint8_t r=255, g=0, b=0;

// ================= RF =================
RCSwitch rf = RCSwitch();
unsigned long lastRF = 0;

// ================= PWM =================
void writePWM(int duty)
{
  duty = constrain(duty,0,PWM_MAX);

  if(currentDir==1){
    analogWrite(IN1_PIN,0);
    analogWrite(IN2_PIN,duty);
  }
  else if(currentDir==-1){
    analogWrite(IN2_PIN,0);
    analogWrite(IN1_PIN,duty);
  }
  else{
    analogWrite(IN1_PIN,0);
    analogWrite(IN2_PIN,0);
  }
}

// ================= MOTOR STATE MACHINE =================
void updateMotor()
{
  if(millis()-lastRamp < RAMP_DELAY) return;
  lastRamp = millis();

  // change direction only when stopped
  if(currentSpeed==0 && currentDir!=targetDir){
    currentDir = targetDir;
  }

  // ramp speed
  if(currentSpeed < targetSpeed){
    currentSpeed += RAMP_STEP;
    if(currentSpeed > targetSpeed) currentSpeed = targetSpeed;
  }
  else if(currentSpeed > targetSpeed){
    currentSpeed -= RAMP_STEP;
    if(currentSpeed < targetSpeed) currentSpeed = targetSpeed;
  }

  writePWM(currentSpeed);
}

// ================= COMMANDS =================
void motorStart(){
  targetDir = 1;
  targetSpeed = DEFAULT_SPEED;
}

void motorStop(){
  targetSpeed = 0;
}

void motorReverse(){
  targetSpeed = 0;
  targetDir = -targetDir;
}

void speedUp(){
  targetSpeed = min(targetSpeed+SPEED_STEP, PWM_MAX);
}

void speedDown(){
  targetSpeed = max(targetSpeed-SPEED_STEP, 0);
}

// ================= LED =================
void updateLED()
{
  if(!stripOn){
    FastLED.clear();
    FastLED.show();
    return;
  }

  if(effect==0) fill_solid(leds,NUM_LEDS,CRGB(r,g,b));
  if(effect==1) fill_rainbow(leds,NUM_LEDS,millis()/10);
  if(effect==2) for(int i=0;i<NUM_LEDS;i++) leds[i]=(i+millis()/100)%3?CRGB::Black:CRGB(r,g,b);

  FastLED.show();
}

// ================= SETUP =================
void setup()
{
  Serial.begin(115200);

  pinMode(IN1_PIN,OUTPUT);
  pinMode(IN2_PIN,OUTPUT);
  pinMode(BUZZER,OUTPUT);
  pinMode(STATUS,OUTPUT);

  rf.enableReceive(digitalPinToInterrupt(RF_PIN));

  FastLED.addLeds<WS2812B,LED_PIN,GRB>(leds,NUM_LEDS);
  FastLED.setBrightness(128);
}

// ================= LOOP =================
void loop()
{
  updateMotor();
  updateLED();

  if(!rf.available()) return;
  if(millis()-lastRF < 200){ rf.resetAvailable(); return; }
  lastRF = millis();

  unsigned long code = rf.getReceivedValue();
  tone(BUZZER,2000,80);

  switch(code)
  {
    case ON_OFF: 
      if(targetSpeed==0) motorStart(); else motorStop();
      break;

    case SPEED_UP: speedUp(); break;
    case SPEED_DOWN: speedDown(); break;
    case REVERSE: motorReverse(); break;

    case WLED_ON_OFF: stripOn = !stripOn; break;
    case WLED_EFFECT: effect = (effect+1)%3; break;

    case WLED_RGB:
      if(r){ r=0; g=255; }
      else if(g){ g=0; b=255; }
      else{ b=0; r=255; }
      break;

    case WLED_BRIGHTNESS:
      brightHigh=!brightHigh;
      FastLED.setBrightness(brightHigh?240:120);
      break;
  }

  rf.resetAvailable();
}
