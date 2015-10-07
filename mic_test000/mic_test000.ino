#define PIN_ENVELOPE   A5
#define PIN_GATE       A4

typedef struct Timer {
  uint32_t last;
  uint32_t sleep;
  uint32_t now;
} Timer;
Timer s_gTimer;

#define NUM_SAMPLES   16
struct EnvelopeSample {
  uint8_t i;
  int value[NUM_SAMPLES];
  int average;
};
struct EnvelopeSample s_gEnvelope;

void readEnvelope()
{
  int i;

  // next sample?
  s_gEnvelope.i++;
  if (s_gEnvelope.i > NUM_SAMPLES) s_gEnvelope.i = 0;

  // Maybe not initialized yet?
  for (i=0; i<NUM_SAMPLES; i++) {
    if (s_gEnvelope.value[i] == 0) {
      s_gEnvelope.value[i] = analogRead(PIN_ENVELOPE); 
    }
  }

  // Read another sample
  s_gEnvelope.value[s_gEnvelope.i] = analogRead(PIN_ENVELOPE);

  // Average calculation
  for (i=0; i<NUM_SAMPLES; i++) {
    s_gEnvelope.average += s_gEnvelope.value[i]; 
  }
  s_gEnvelope.average = s_gEnvelope.average / NUM_SAMPLES;  
}

void printEnvelope()
{
  // Generic sleep-until routine...
  s_gTimer.now = millis();
  if (s_gTimer.now - s_gTimer.last >= 200) {
    s_gTimer.last = s_gTimer.now;
  }
  else return;

  // We have slept long enough...
  Serial.print("this='");
  Serial.print(s_gEnvelope.value[s_gEnvelope.i]);
  Serial.print("', average='");
  Serial.print(s_gEnvelope.average);
  Serial.println("'");
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_ENVELOPE, INPUT);
  pinMode(PIN_GATE, INPUT);
}

void loop() {
  readEnvelope();
  printEnvelope();
}
