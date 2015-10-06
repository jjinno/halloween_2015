#define FAN_PIN    9
#define LED_PIN    13

void setup() {
  Serial.begin(115200);
  
  // put your setup code here, to run once:
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
}

bool fan = false;
void toggle() {
  if (fan == true) {
    Serial.println("<< FAN: OFF >>");
    digitalWrite(LED_PIN, LOW);
    digitalWrite(FAN_PIN, LOW);
    fan = false;
  }
  else {
    Serial.println("<< FAN: ON  >>");
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(FAN_PIN, HIGH);
    fan = true;
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(1000);
  toggle();
  
}
