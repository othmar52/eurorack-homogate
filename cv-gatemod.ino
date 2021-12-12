 


// thanks to https://github.com/joeSeggiola/arduino-eurorack-projects/blob/master/clock-divider/clock-divider.ino

// transistor S8050  https://www.el-component.com/images/bipolar-transistor/s8050-pinout.jpg


// blue led circuit 3V3:
//   short leg (-) -> GND
//   long leg (+) ->  resistor 4,7 K  -> PIN 8 + 9

// ===========================================================================


const int CLOCK_INPUT_PIN = 2; // Input signal pin, must be usable for interrupts
const int GATE_OUTPUT_PIN = 4; // Gate output

const int LED_GATE_IN_PIN = 8;
const int LED_GATE_OUT_PIN = 9;

const int GATE_LENGTH_POT_PIN = A2;
const int GATE_MOD_POT_PIN = A3;

const int gateLengthPotMax = 680; // maximum retrieved value of analogRead() with a 10K trimpot

// ===========================================================================


unsigned long currentMilliSecond = 0;
volatile bool clockTickSignal = false; // Clock signal digital reading, set in the clock ISR

bool incomingGateState = LOW;
unsigned long incomingGateLastOpen = 0;
unsigned long incomingGateLastClose = 0;
unsigned long incomingGateInterval = 0;
unsigned long incomingGateLength = 0;


bool outgoingGateState = LOW;
unsigned long outgoingGateLastOpen = 0;
unsigned long outgoingGateLastClose = 0;
unsigned long outgoingGateInterval = 0;
unsigned long outgoingGateLength = 0;

const int minimumGapLength = 2;

void gateOutHigh() {
  digitalWrite(GATE_OUTPUT_PIN, LOW); // inverted
  digitalWrite(LED_GATE_OUT_PIN, HIGH);
  outgoingGateState = HIGH;
  outgoingGateLastOpen = currentMilliSecond;
  //Serial.println("gateOutHigh");
  Serial.println(analogRead(GATE_MOD_POT_PIN));
  
}

void gateOutLow() {
  digitalWrite(GATE_OUTPUT_PIN, HIGH); // inverted
  digitalWrite(LED_GATE_OUT_PIN, LOW);
  outgoingGateState = LOW;
  outgoingGateLastClose = currentMilliSecond;
  //Serial.println("gateOutLow");
  //Serial.println(analogRead(GATE_LENGTH_POT_PIN));
}

void checkGateOutHigh() {
  if (outgoingGateState == HIGH) {
    return;  
  }

  if(incomingGateState != outgoingGateState) {
    gateOutHigh();
  }
}
void checkGateOutLow() {
  if (outgoingGateState == LOW) {
    return;  
  }

  if(analogRead(GATE_LENGTH_POT_PIN) == 0 && incomingGateState != outgoingGateState) {
    gateOutLow();
    return;
  }
  // the current gate length is bigger than incoming gate length map(value, fromLow, fromHigh, toLow, toHigh)
  unsigned int requestedGateLength = map(analogRead(GATE_LENGTH_POT_PIN), 0, gateLengthPotMax, incomingGateLength, incomingGateInterval-minimumGapLength);

  if(currentMilliSecond >= outgoingGateLastOpen + requestedGateLength) {
    gateOutLow();
  }
  
  
}

void handleIncomingGateChangeToHigh() {
  incomingGateInterval = currentMilliSecond - incomingGateLastOpen;
  incomingGateLastOpen = currentMilliSecond;
  digitalWrite(LED_GATE_IN_PIN, HIGH);
  incomingGateState = HIGH;
}

void handleIncomingGateChangeToLow() {
  incomingGateLength = currentMilliSecond - incomingGateLastOpen;
  incomingGateLastClose = currentMilliSecond;
  digitalWrite(LED_GATE_IN_PIN, LOW);
  incomingGateState = LOW;
}

void setup()
{
  Serial.begin(115200);
  setupCvClocksAndLeds();
}

void loop()
{
  currentMilliSecond = millis();
  checkGateOutLow();
  checkGateOutHigh();
}



void setupCvClocksAndLeds() {
  // Interrupts
  pinMode(CLOCK_INPUT_PIN, INPUT);
  pinMode(GATE_OUTPUT_PIN, OUTPUT);
  pinMode(LED_GATE_IN_PIN, OUTPUT);
  pinMode(LED_GATE_OUT_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(CLOCK_INPUT_PIN), isrClock, CHANGE);
}


void isrClock() {
  if(digitalRead(CLOCK_INPUT_PIN) == HIGH) {
    handleIncomingGateChangeToLow();
  } else {
    handleIncomingGateChangeToHigh();
  }
  //Serial.print("interval ");
  //Serial.println( incomingGateInterval);
  //Serial.print("length ");
  //Serial.println(incomingGateLength);
  
}
