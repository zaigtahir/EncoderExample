/*

Provided free of charge for educational and learning purposes
As is no guarantee is implied
11-27-2025
Z. Tahir
If you find this helpful please support/subscrib/and like my videos on my youtube channel: @MakerZig (https://www.youtube.com/@MakerZig)

This is a sketch to read a quadrature encoder

Quadrature Encoder (has a detent position at every 4th reading)
clk -> GPIO32
DT  -> GPIO33
SW  -> GPIO25
+   <- 3.3V from regulator
GND -> connect to ground

3.3 Regulator
vIN <- 5V pin from ESP232 board
GND -> connect to ground
OUT -> connect to 3.3V of the encoder
*/


class DebounceSwitch {
  //change to microseconds for quad inputs
  //add limits

  /*
  for now if its about to ignore skipped steps there is inconsistency in the count by 1 number: -22 -21 -20 -19 -18 -17 -18
  the last number should be -16
  -- figure that our sometime

  -35 -34 -33 -32 -31 -30 -29 -28 -27 -26 -25 -24 -23 -22 -21 -20 -19 -18 -17 -18 
  ignoring skipped steps
  -19 
  ignoring skipped steps
  -18 -17 -16 -15 -14 -13 -12 
  */




private:
  uint8_t pin;
  uint8_t lastValue;        //last input value
  uint16_t debounceMicros;  //time for debounce MS
  unsigned long highTimer;  //when input became high
  unsigned long lowTimer;   //when input became low

public:
  DebounceSwitch() {}

  DebounceSwitch(uint8_t pin, uint16_t debounceMicros) {
    this->pin = pin;
    this->debounceMicros = debounceMicros;
  }

  void setup() {
    pinMode(pin, INPUT);
    lastValue = digitalRead(pin);
    highTimer = micros();
    lowTimer = micros();
  }

  uint8_t getNoDebounce() {
    // returns level without debouncing
    return digitalRead(pin);
  }

  uint8_t getSwitchLevel() {
    // returns current debounced switch state

    uint8_t currentValue = digitalRead(pin);

    unsigned long highTime = micros() - highTimer;
    unsigned long lowTime = micros() - lowTimer;

    if ((highTime < debounceMicros) && (lowTime < debounceMicros)) {
      //still at start up and none of the timers have hit the delay yet
      return lastValue;
    }

    if (currentValue == 1) {
      lowTimer = micros();
      if (highTime >= debounceMicros) {
        lastValue = 1;
      }
    } else {
      highTimer = micros();
      if (lowTime >= debounceMicros) {
        lastValue = 0;
      }
    }

    return lastValue;
  }

  bool switchPressed() {
    //returns TRUE only one time when the switch is in LOW position
    //pressed state  = 0

    static bool reportedLow = false;

    uint8_t switchState = getSwitchLevel();

    if (switchState == 0) {
      if (reportedLow == false) {
        reportedLow = true;
        return true;
      } else {
        return false;
      }

    } else {
      //switch is 1
      reportedLow = false;
      return false;
    }
  }
};

class QuadratureDetentEncoder {
private:
  DebounceSwitch inputA;
  DebounceSwitch inputB;
  DebounceSwitch inputS;

  //store last states
  uint8_t lastInA;
  uint8_t lastInB;
  uint8_t lastInS;

  long position = 0;

  //limits
  bool enableLimits = false;
  bool enableLoop = false;
  long highLimit = 0;
  long lowLimit = 0;

  int8_t getChange(uint8_t inA, uint8_t inB) {
    // will return 0, +1 or -1 based on the previous and current encoder channel states
    // 01 00 10 11 01 00 10 11 01 00 10 11

    int8_t change = 0;

    if ((lastInA == inA) && (lastInB == inB)) {
      // there is no change in the encoder reading
      return 0;
    }

    if ((inA + inB) == 2) {
      if ((lastInA == 1) && (lastInB == 0)) {
        change = 1;
      }

      if ((lastInA == 0) && (lastInB == 1)) {
        change = -1;
      }
    }

    return change;
  }

  long enforceLimits(int8_t change) {
    // will use the postion, change values to enforce limits
    if (!enableLimits) {
      // limits not enabled, just return the postion
      return position + change;
    }

    long newPosition = position + change;

    if (newPosition > highLimit) {
      if (enableLoop) {
        newPosition = lowLimit;
      } else {
        newPosition = highLimit;
      }

      return newPosition;
    }

    if (newPosition < lowLimit) {
      if (enableLoop) {
        newPosition = highLimit;
      } else {
        newPosition = lowLimit;
      }

      return newPosition;
    }

    return newPosition;
  }


public:
  QuadratureDetentEncoder() {}

  QuadratureDetentEncoder(uint8_t pinA, uint8_t pinB, uint8_t pinS) {

    inputA = DebounceSwitch(pinA, 500);  //found 250 microsecond bounce good balance between skipping and bouncing
    inputB = DebounceSwitch(pinB, 500);
    inputS = DebounceSwitch(pinS, 10000);
  }

  void setup() {
    inputA.setup();
    inputB.setup();
    inputS.setup();

    // read initial values, assume steady state at reset
    lastInA = inputA.getNoDebounce();
    lastInB = inputB.getNoDebounce();
    lastInS = inputS.getNoDebounce();
  }

  long updatePosition() {
    // will return 0, +1 or -1 based on the previous and current encoder channel states
    // 01 00 10 11 01 00 10 11 01 00 10 11

    uint8_t inA = inputA.getSwitchLevel();
    uint8_t inB = inputB.getSwitchLevel();

    int8_t change = getChange(inA, inB);

    lastInA = inA;
    lastInB = inB;

    long newPosition = enforceLimits(change);

    position = newPosition;

    //position = position + change;

    return position;
  }

  void setPosition(long position){
    this->position = position;
  }

  bool switchPressed() {
    return inputS.switchPressed();
  }

  void setLimits (long highLimit, long lowLimit, bool loop) {
    if (lowLimit > highLimit) {
      //no change is made
      return;

      this->highLimit = highLimit;
      this->lowLimit = lowLimit;
      enableLimits = true;
      enableLoop = loop;

    }
  }

  void disableLimits() {
    enableLimits = false;
    enableLoop = false;
  }

};

QuadratureDetentEncoder encoder = QuadratureDetentEncoder(32, 33, 25);

void setup() {
  Serial.begin(115200);
  encoder.setup();
}

void loop() {
  // put your main code here, to run repeatedly:

  static int8_t i = 0;
  int8_t change = encoder.updatePosition();
  if (i != change) {
    Serial.print(change);
    Serial.print(" ");
    i = change;
  }

  if(encoder.switchPressed()){
    encoder.setPosition(0);
    Serial.println();
  }

}
