 


// thanks to https://github.com/joeSeggiola/arduino-eurorack-projects/blob/master/clock-divider/clock-divider.ino

// transistor S8050  https://www.el-component.com/images/bipolar-transistor/s8050-pinout.jpg


// blue led circuit 3V3:
//   short leg (-) -> GND
//   long leg (+) ->  resistor 4.7 K  -> PIN 8 + 9


// add supress active indicator resistor 530K
// add/supress gate indicator resistor 4.7K

// ===========================================================================


const int CLOCK_INPUT_PIN = 2; // Input signal pin, must be usable for interrupts
const int GATE_OUTPUT_PIN = 3; // Gate output
const int LED_SUPRESS_GATE_PIN = 5;  // indicates a suppressed gate, must support PWM
const int LED_ADD_GATE_PIN = 6;      // indicates an added gate, must support PWM

const int LED_GATE_IN_PIN = 8;
const int LED_GATE_OUT_PIN = 9;

const int GATE_LENGTH_POT_PIN = A2;
const int GATE_MOD_POT_PIN = A3;

const int potentiometerMaxValue = 680; // maximum retrieved value of analogRead() with a 10K trimpot

// ===========================================================================


bool ledAddGateGlowing = false;
bool ledSupressGateGlowing = false;

unsigned long currentMilliSecond = 0;

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



unsigned int maxGateModPerMinute = 100; // maximum of added or supressed gates per minute
unsigned long lastAdditionalGateMilliSecond = 0;
unsigned long lastSupressedGateMilliSecond = 0;
bool nowAddingGate = false;
bool nowSupressingGate = false;


const int minimumGapLength = 2; // milliseconds between LOW and HIGH; so maximum gate length will be gate interval minus this value [ms]

void gateOutHigh() {
  digitalWrite(GATE_OUTPUT_PIN, LOW); // inverted
  digitalWrite(LED_GATE_OUT_PIN, HIGH);
  outgoingGateState = HIGH;
  outgoingGateLastOpen = currentMilliSecond;
  //Serial.println("gateOutHigh");
  //Serial.println(analogRead(GATE_MOD_POT_PIN));
  //Serial.print("add gates per second: ");
  //Serial.println(getAddGateInterval());
  //Serial.print("supress gates per second: ");
  //Serial.println(getSupressGateInterval());
  
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

  if(getSupressGateInterval() > 0) {
    if(currentMilliSecond - lastSupressedGateMilliSecond > getSupressGateInterval()) {
      lastSupressedGateMilliSecond = currentMilliSecond;
      nowSupressingGate = true;
      ////digitalWrite(LED_SUPRESS_GATE_PIN, HIGH);
      gateSupressLedIndicator(HIGH);
      gateAddLedIndicator(LOW);
      Serial.print("suppressing gate...  interval is ");
      Serial.println(getSupressGateInterval());
      return;
    }
  }
  
  if (nowSupressingGate == true) {
    return;
  }

  if(getAddGateInterval() > 0) {
    
    
    //if (ledAddGateGlowing == false) {
    //  digitalWrite(LED_ADD_GATE_ACTIVE_PIN, HIGH);
    //  ledAddGateGlowing = true;
    //  analogWrite( LED_ADD_GATE_PIN, 30);
    //}
    if(currentMilliSecond - lastAdditionalGateMilliSecond > getAddGateInterval()) {
      lastAdditionalGateMilliSecond = currentMilliSecond;
      nowAddingGate = true;
      ////digitalWrite(LED_ADD_GATE_PIN, HIGH);
      gateAddLedIndicator(HIGH);
      gateSupressLedIndicator(LOW);
      Serial.print("adding gate...  interval is ");
      Serial.println(getAddGateInterval());
      gateOutHigh();
      gateOutLow();
    }
  }

  if (nowAddingGate == true && currentMilliSecond - lastAdditionalGateMilliSecond > 120) {
      nowAddingGate = false;
      ////digitalWrite(LED_ADD_GATE_PIN, LOW);
      gateAddLedIndicator(LOW);
  }
  

  if(incomingGateState != outgoingGateState) {
    gateOutHigh();
  }
}

void gateSupressLedIndicator(bool highOrLow) {
  if (highOrLow == HIGH) {
    digitalWrite(LED_SUPRESS_GATE_PIN, HIGH);
    analogWrite(LED_SUPRESS_GATE_PIN, 150);
    return;
  }

  if(getSupressGateInterval() > 0) {
    analogWrite(LED_SUPRESS_GATE_PIN, 10);
    return;
  }
  digitalWrite(LED_SUPRESS_GATE_PIN, LOW);
}

void gateAddLedIndicator(bool highOrLow) {
  if (highOrLow == HIGH) {
    digitalWrite(LED_ADD_GATE_PIN, HIGH);
    analogWrite(LED_ADD_GATE_PIN, 150);
    return;
  }

  if(getAddGateInterval() > 0) {
    analogWrite(LED_ADD_GATE_PIN, 10);
    return;
  }
  digitalWrite(LED_ADD_GATE_PIN, LOW);
}


// returns value between zero and maxGateModPerMinute
// in case the potentiometer is between 60% and 100%;
unsigned int getAddGateInterval() {
  int gatesPerMinute = 0;
  if (analogRead(GATE_MOD_POT_PIN) < potentiometerMaxValue*0.6) {
    return gatesPerMinute;
  }
  gatesPerMinute = map(analogRead(GATE_MOD_POT_PIN), potentiometerMaxValue*0.6, potentiometerMaxValue, 1, maxGateModPerMinute);
  if(gatesPerMinute < 1) {
    return 0;  
  }
  return 60000/gatesPerMinute;
}

// returns value between zero and maxGateModPerMinute
// in case the potentiometer is between 0% and 40%;
unsigned int getSupressGateInterval() {
  int gatesPerMinute = 0;
  if (analogRead(GATE_MOD_POT_PIN) > potentiometerMaxValue*0.4) {
    return gatesPerMinute;
  }
  gatesPerMinute = map(analogRead(GATE_MOD_POT_PIN), 0, potentiometerMaxValue*0.4, maxGateModPerMinute, 1);
  if(gatesPerMinute < 1) {
    return 0;  
  }
  return 60000/gatesPerMinute;
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
  unsigned int requestedGateLength = map(analogRead(GATE_LENGTH_POT_PIN), 0, potentiometerMaxValue, incomingGateLength, incomingGateInterval-minimumGapLength);

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
  nowSupressingGate = false;
  //digitalWrite(LED_SUPRESS_GATE_PIN, LOW);
  gateSupressLedIndicator(LOW);
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
  pinMode(LED_SUPRESS_GATE_PIN, OUTPUT);
  pinMode(LED_ADD_GATE_PIN, OUTPUT);
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
