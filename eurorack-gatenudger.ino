 

/**
 * https://github.com/othmar52/eurorack-gatenudger
 * arduino based eurorack module to add or remove single gate pulses
 * very useful in combination with the TuringMachine (https://github.com/TomWhitwell/TuringMachine)
 * this module gives us the possibility to nudge a locked sequence left and right on the timeline
 * by adding or supressing sigle gate pulses
 * further it is possible to increase the gate length with a potentiometer
 *
 * thanks to https://github.com/TomWhitwell/TuringMachine/issues/166
 * thanks to https://fuzzysynth.blogspot.com/2016/07/interfacing-modular-and-microcontrollers.html
 * thanks to https://github.com/joeSeggiola/arduino-eurorack-projects/blob/master/clock-divider/clock-divider.ino
 *
 * TODO: fast gate length change bug
 * on very fast decreasing gate length from max to min we get some unwanted shift
 */
#define NUM_INSTANCES 2

const int CLOCK_INPUT_PIN[NUM_INSTANCES] = {2, 3};  // Input signal pin, must be usable for interrupts
const int GATE_OUTPUT_PIN[NUM_INSTANCES] = {4, 5};  // Gate output

const int NUDGE_AHEAD_PIN[NUM_INSTANCES] = {6, 7};  // pushbutton
const int NUDGE_BEHIND_PIN[NUM_INSTANCES] = {8, 9}; // pushbutton

const int LED_GATE_IN_PIN[NUM_INSTANCES] = {10, 11};  // led indicating incoming clock pulses
const int LED_GATE_OUT_PIN[NUM_INSTANCES] = {12, 13}; // led indicating outgoing clock pulses

const int GATE_LENGTH_POT_PIN[NUM_INSTANCES] = {A0, A1}; // potentiometer to increase gate length
const int OFFSET_POT_PIN[NUM_INSTANCES] = {A2, A3}; // potentiometer for time offset
const int potentiometerMaxValue = 680; // maximum retrieved value of analogRead() with the 10K trimpot i am using. shouldn't this be 1024?

unsigned long currentMilliSecond = 0;

bool incomingGateState[NUM_INSTANCES] = {LOW, LOW};
unsigned long incomingGateLastOpen[NUM_INSTANCES] = {0, 0};
unsigned long incomingGateInterval[NUM_INSTANCES] = {0, 0};
unsigned long incomingGateLength[NUM_INSTANCES] = {0, 0};

bool outgoingGateState[NUM_INSTANCES] = {LOW, LOW};
unsigned long outgoingGateLastOpen[NUM_INSTANCES] = {0, 0};

bool nudgeAheadButtonState[NUM_INSTANCES] = {LOW, LOW};
bool nudgeBehindButtonState[NUM_INSTANCES] = {LOW, LOW};

bool nowSupressingGate[NUM_INSTANCES] = {false, false};

unsigned int addGatesQue[NUM_INSTANCES] = {0, 0};
unsigned int supressGatesQue[NUM_INSTANCES] = {0, 0};

bool currentlySupressingIncomingGate[NUM_INSTANCES] = {false, false};

const int minimumGapLength = 2; // milliseconds between LOW and HIGH; so maximum gate length will be gate interval minus this value [milliseconds]

void setup()
{
  for (uint8_t i=0; i<NUM_INSTANCES; i++) {
    pinMode(CLOCK_INPUT_PIN[i], INPUT);
  
    pinMode(NUDGE_AHEAD_PIN[i], INPUT_PULLUP);
    pinMode(NUDGE_BEHIND_PIN[i], INPUT_PULLUP);
  
    pinMode(GATE_OUTPUT_PIN[i], OUTPUT);
    pinMode(LED_GATE_IN_PIN[i], OUTPUT);
    pinMode(LED_GATE_OUT_PIN[i], OUTPUT);
  }
  // Interrupts
  // arguments seems to be impossible for interrupts!?
  attachInterrupt(
    digitalPinToInterrupt(CLOCK_INPUT_PIN[0]),
    handleIncomingGateChange1,
    CHANGE
  );
  attachInterrupt(
    digitalPinToInterrupt(CLOCK_INPUT_PIN[1]),
    handleIncomingGateChange2,
    CHANGE
  );
  Serial.begin(115200);
  Serial.print("attaching interrupts");
}

void loop()
{
  currentMilliSecond = millis();
  loopPushButtons();
  checkGateOutLow();
  checkGateOutHigh();
}

/**
 * send gate out high to TRS jack with led indication
 */
void gateOutHigh(uint8_t i) {
  if (outgoingGateState[i] == HIGH) {
    return;
  }
  outgoingGateState[i] = HIGH;
  outgoingGateLastOpen[i] = currentMilliSecond;
  digitalWrite(GATE_OUTPUT_PIN[i], LOW);   // inverted
  digitalWrite(LED_GATE_OUT_PIN[i], HIGH);

}

/**
 * send gate out low to TRS jack with led indication
 */
void gateOutLow(uint8_t i) {
  if (outgoingGateState[i] == LOW) {
    return;
  }
  outgoingGateState[i] = LOW;
  digitalWrite(GATE_OUTPUT_PIN[i], HIGH); // inverted
  digitalWrite(LED_GATE_OUT_PIN[i], LOW);

}

/**
 * check if the outgoing gate state is same as incoming gate state
 * but avoid to send a new gate in case we have to supress the current gate pulse
 */
void checkGateOutHigh() {
  for (uint8_t i=0; i<NUM_INSTANCES; i++) {
    if (outgoingGateState[i] == HIGH) {
      // already high - no need for further checks
      continue;  
    }
  
    if (supressGatesQue[i] > 0) {
      nowSupressingGate[i] = true;
      supressGatesQue[i]--;
      Serial.println("suppressing gate");
      currentlySupressingIncomingGate[i] = true;
      continue;
    }
  
    if (addGatesQue[i] > 0) {
      Serial.println("adding gate");
      gateOutHigh(i);
      gateOutLow(i);
      addGatesQue[i]--;
    }
  
    if (incomingGateState[i] == HIGH && currentlySupressingIncomingGate[i] == false) {
      gateOutHigh(i);
    }
  }
}

/*
 * check if the current millisecond requires out gate set to low
 * but take the increase gate length pot value into account
 */
void checkGateOutLow() {
  for (uint8_t i=0; i<NUM_INSTANCES; i++) {
    if (outgoingGateState[i] == LOW) {
      // already low - no need for further checks
      continue;  
    }
  
    // check if we have an increased gate length
    int valueGateLengthPot = analogRead(GATE_LENGTH_POT_PIN[i]);
  
    if (valueGateLengthPot < 2) {
      // potentiometer is fully turned counter clockwise
      // no gate length modification - just pass through the incoming gate pulses
      if (incomingGateState[i] == LOW) {
        gateOutLow(i);
      }
      continue;
    }
  
    // we do have an increased gate length due to potentiometer position
    int modifiedGateLength = calculateModifiedGateLengthMilliSeconds(
      valueGateLengthPot,
      i
    );
  
    // check if it's time to close the open gate
    if (currentMilliSecond < outgoingGateLastOpen[i] + modifiedGateLength) {
      continue;
    }
    gateOutLow(i);
  }
}

/*
 * the current gate length is longer than incoming gate length
 * with the potentiometer fully turned clockwise the gate length will
 * be as long as the incoming gate interval minus 2 milliseconds
 */
int calculateModifiedGateLengthMilliSeconds(int potentiometerValue, uint8_t i) {
  int requestedGateLength = map(
    potentiometerValue,                    // value
    0,                                     // fromLow
    potentiometerMaxValue,                 // fromHigh
    incomingGateLength[i],                    // toLow
    incomingGateInterval[i]-minimumGapLength  // toHigh
  );
  
  // just to make sure we have no quirks
  if (requestedGateLength < incomingGateLength[i]) {
    requestedGateLength = incomingGateLength[i];
  }
  if (requestedGateLength < minimumGapLength) {
    requestedGateLength = minimumGapLength;
  }
  return requestedGateLength;
}

void handleIncomingGateChange1() {
  //Serial.println("handleIncomingGateChange1");
  handleIncomingGateChange(0);
}
void handleIncomingGateChange2() {
  //Serial.println("handleIncomingGateChange2");
  handleIncomingGateChange(1);
}

/*
 * incoming gate has changed its state
 */
void handleIncomingGateChange(uint8_t i) {
  if (digitalRead(CLOCK_INPUT_PIN[i]) == HIGH) {
    handleIncomingGateChangeToLow(i);
    return;
  }
  handleIncomingGateChangeToHigh(i);
}

/*
 * incoming gate has changed its state to HIGH
 * set a few variables and make sure the LED is on
 */
void handleIncomingGateChangeToHigh(uint8_t i) {
  incomingGateInterval[i] = currentMilliSecond - incomingGateLastOpen[i];
  incomingGateLastOpen[i] = currentMilliSecond;
  digitalWrite(LED_GATE_IN_PIN[i], HIGH);
  incomingGateState[i] = HIGH;
  //Serial.println("handleIncomingGateChangeToHigh");
}

/*
 * incoming gate has changed its state to LOW
 * set a few variables and make sure the LED is off
 */
void handleIncomingGateChangeToLow(uint8_t i) {
  currentlySupressingIncomingGate[i] = false;
  incomingGateLength[i] = currentMilliSecond - incomingGateLastOpen[i];
  digitalWrite(LED_GATE_IN_PIN[i], LOW);
  incomingGateState[i] = LOW;
  nowSupressingGate[i] = false;
}

/*
 * check if push button states has changed and add changes to a que
 */
void loopPushButtons() {
  for (uint8_t i=0; i<NUM_INSTANCES; i++) {
    if (digitalRead(NUDGE_AHEAD_PIN[i]) != nudgeAheadButtonState[i]) {
      nudgeAheadButtonState[i] = digitalRead(NUDGE_AHEAD_PIN[i]);
      if (nudgeAheadButtonState[i] == HIGH) {
        // shouldn't it be adding instead of supressing? anyway, it works...
        supressGatesQue[i]++;
        Serial.println("got button +");
      }
    }
    if (digitalRead(NUDGE_BEHIND_PIN[i]) != nudgeBehindButtonState[i]) {
      nudgeBehindButtonState[i] = digitalRead(NUDGE_BEHIND_PIN[i]);
      if (nudgeBehindButtonState[i] == HIGH) {
        // shouldn't it be supressing instead of adding? anyway, it works...
        addGatesQue[i]++;
        Serial.println("got button -");
      }
    }
  }
}
