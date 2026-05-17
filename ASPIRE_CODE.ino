#define BLYNK_PRINT Serial

// ---------------- BLYNK ----------------
#define BLYNK_TEMPLATE_ID "TMPL6sXIgtzlE"
#define BLYNK_TEMPLATE_NAME "Automatic Solar Pump"
#define BLYNK_AUTH_TOKEN "iijONJ5nyznpCUGtDFzkBoXNAoFGm4p0"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------------- WIFI ----------------
char ssid[] = "Electronics";
char pass[] = "programming224";

// ---------------- PIN CONFIG ----------------
#define TRIG_PIN 2
#define ECHO_PIN 15

#define FLOW_SENSOR 4
#define RELAY_PIN 16

#define RED_LED 17
#define YELLOW_LED 5
#define GREEN_LED 18

#define TEMP_PIN 19
#define VOLTAGE_PIN 21

// ---------------- RELAY LOGIC ----------------
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// ---------------- OBJECTS ----------------
OneWire oneWire(TEMP_PIN);  
DallasTemperature tempSensor(&oneWire);
BlynkTimer timer;

// ---------------- VARIABLES ----------------
volatile int pulseCount = 0;
float flowRate = 0;

float tankHeight = 100.0;

bool pumpState = false;

// 🔴 NEW DRY RUN VARIABLES
unsigned long dryRunStartTime = 0;
bool dryRunDetected = false;

// ---------------- INTERRUPT ----------------
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// ---------------- BLYNK CONTROL ----------------
BLYNK_WRITE(V0) {
  pumpState = param.asInt();
  digitalWrite(RELAY_PIN, pumpState ? RELAY_ON : RELAY_OFF);
}

// ---------------- WATER LEVEL ----------------
float getWaterLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 0;

  float distance = duration * 0.034 / 2;
  float level = tankHeight - distance;

  float percent = (level / tankHeight) * 100.0;

  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  return percent;
}

// ---------------- FLOW RATE ----------------
void computeFlow() {
  flowRate = pulseCount / 15.0;
  pulseCount = 0;
}

// ---------------- VOLTAGE ----------------
float getVoltage() {
  int adc = analogRead(VOLTAGE_PIN);

  float measuredVoltage = (adc / 4095.0) * 3.3;
  float realVoltage = measuredVoltage * 11.0;

  if (realVoltage < 0) realVoltage = 0;
  if (realVoltage > 30) realVoltage = 30;

  return realVoltage;
}

// ---------------- TEMPERATURE ----------------
float getTemperature() {
  tempSensor.requestTemperatures();
  float temp = tempSensor.getTempCByIndex(0);

  if (temp < 0) temp = 0;
  if (temp > 60) temp = 60;

  return temp;
}

// ---------------- MAIN FUNCTION ----------------
void sendData() {

  float waterLevel = getWaterLevel();
  computeFlow();
  float voltage = getVoltage();
  float temperature = getTemperature();

  // -------- DRY RUN PROTECTION (5s DELAY) --------
  if (pumpState && flowRate < 0.5) {

    if (!dryRunDetected) {
      dryRunDetected = true;
      dryRunStartTime = millis(); // start timer
    }

    if (millis() - dryRunStartTime >= 5000) {
      Serial.println("⚠️ DRY RUN DETECTED! Pump OFF after 5s");

      digitalWrite(RELAY_PIN, RELAY_OFF);
      pumpState = false;

      if (Blynk.connected()) {
        Blynk.virtualWrite(V0, 0);
      }
    }

  } else {
    // reset if flow returns
    dryRunDetected = false;
  }

  // -------- LED INDICATORS --------
  if (pumpState && dryRunDetected && (millis() - dryRunStartTime >= 5000)) {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
  }
  else if (waterLevel < 30) {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
  }
  else if (waterLevel < 70) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
  }
  else {
    digitalWrite(RED_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
  }

  // -------- BLYNK SEND --------
  if (Blynk.connected()) {
    Blynk.virtualWrite(V1, waterLevel);
    Blynk.virtualWrite(V2, flowRate);
    Blynk.virtualWrite(V3, voltage);
    Blynk.virtualWrite(V4, temperature);
  }

  // -------- SERIAL DEBUG --------
  Serial.println("------ SYSTEM DATA ------");
  Serial.print("Water Level: "); Serial.print(waterLevel); Serial.println("%");
  Serial.print("Flow Rate: "); Serial.println(flowRate);
  Serial.print("Voltage: "); Serial.print(voltage); Serial.println("V");
  Serial.print("Temperature: "); Serial.print(temperature); Serial.println("C");
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  pinMode(FLOW_SENSOR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR), pulseCounter, FALLING);

  tempSensor.begin();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(2000L, sendData);
}

// ---------------- LOOP ----------------
void loop() {
  Blynk.run();
  timer.run();
}