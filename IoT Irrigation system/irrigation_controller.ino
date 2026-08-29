/*
  IoT Smart Irrigation System - Firmware
  ---------------------------------------
  Reads soil moisture, temperature, light, water level, humidity, rain,
  pH, and flow. Drives irrigation, cooling/heating, and lighting based
  on threshold logic. Sends readings over serial to a Raspberry Pi for
  logging and MQTT publishing.

  Hardware notes (see connection map):
  - LDR and NTC thermistor are read through voltage dividers (10k).
  - Flyback diodes are required across the solenoid and relay coils.
  - DHT22 needs a 10k pull-up on its data line.
  - Serial connection to Raspberry Pi goes over USB, not GPIO, to avoid
    the 5V/3.3V logic mismatch.
  - Heating and cooling use two separate relay channels (PIN_COOLING,
    PIN_HEATING), not one shared pin - the original design could not
    actually distinguish "too hot" from "too cold" on a single relay.
*/

#include <DHT.h>

// ---- Pin assignments ----
const int PIN_SOIL_MOISTURE = A0;
const int PIN_TEMP_NTC      = A1;
const int PIN_LDR           = A2;
const int PIN_RAIN          = A3;
const int PIN_PH            = A4;

const int PIN_DHT22         = 11;
const int PIN_FLOW          = 3;   // interrupt pin

const int PIN_WATER_PROBE[4] = {4, 5, 6, 7}; // empty -> full, 4 stages
const int PIN_PUMP           = 8;   // via motor driver
const int PIN_SOLENOID       = 9;   // via relay
const int PIN_COOLING        = 10;  // via relay (fan) - separate channel from heating
const int PIN_HEATING        = 13;  // via relay (heater) - separate channel from cooling
const int PIN_LIGHT          = 12;  // via relay

#define DHTTYPE DHT22
DHT dht(PIN_DHT22, DHTTYPE);

// ---- Thresholds (tune during testing) ----
const int SOIL_DRY_THRESHOLD   = 400;   // 0-1023, lower = wetter
const float TEMP_HIGH_C        = 30.0;
const float TEMP_LOW_C         = 15.0;
const int LIGHT_LOW_THRESHOLD  = 300;   // 0-1023, lower = darker
const int RAIN_DETECTED_VALUE  = 500;   // below this = rain present
const float HUMIDITY_HIGH_PCT  = 80.0;

// ---- Flow sensor pulse counting ----
volatile unsigned long flowPulses = 0;
void flowISR() { flowPulses++; }
const float FLOW_PULSES_PER_LITRE = 450.0; // datasheet-dependent, calibrate

unsigned long lastFlowCheck = 0;
float totalLitres = 0.0;

void setup() {
  Serial.begin(9600);
  dht.begin();

  for (int i = 0; i < 4; i++) pinMode(PIN_WATER_PROBE[i], INPUT);
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_SOLENOID, OUTPUT);
  pinMode(PIN_COOLING, OUTPUT);
  pinMode(PIN_HEATING, OUTPUT);
  pinMode(PIN_LIGHT, OUTPUT);

  pinMode(PIN_FLOW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW), flowISR, FALLING);

  lastFlowCheck = millis();
}

void loop() {
  // ---- Read sensors ----
  int soilValue = analogRead(PIN_SOIL_MOISTURE);
  int tempRaw   = analogRead(PIN_TEMP_NTC);
  int lightRaw  = analogRead(PIN_LDR);
  int rainRaw   = analogRead(PIN_RAIN);
  int phRaw     = analogRead(PIN_PH);
  float humidity = dht.readHumidity();
  float tempC     = readTemperatureC(tempRaw);
  float phValue   = readPH(phRaw);

  bool rainDetected = rainRaw < RAIN_DETECTED_VALUE;
  int waterLevelStage = readWaterLevel();

  // ---- Irrigation logic ----
  // Rain overrides soil-moisture-triggered irrigation, even if soil reads dry.
  bool soilDry = soilValue > SOIL_DRY_THRESHOLD;
  bool humidityHigh = !isnan(humidity) && humidity > HUMIDITY_HIGH_PCT;

  bool shouldIrrigate = soilDry && !rainDetected && !humidityHigh;
  digitalWrite(PIN_SOLENOID, shouldIrrigate ? HIGH : LOW);

  // ---- Water tank pump logic ----
  // Stage 0 = empty, 3 = full (4-probe ladder from original 9-probe design,
  // simplified here; extend PIN_WATER_PROBE[] for finer resolution)
  digitalWrite(PIN_PUMP, waterLevelStage < 3 ? HIGH : LOW);

  // ---- Temperature logic ----
  // Two separate relay channels - only one is ever active at a time.
  // The original design shared one relay for both loads, which cannot
  // actually represent "too hot" and "too cold" as distinct states.
  bool tooHot  = tempC > TEMP_HIGH_C;
  bool tooCold = tempC < TEMP_LOW_C;

  digitalWrite(PIN_COOLING, tooHot ? HIGH : LOW);
  digitalWrite(PIN_HEATING, tooCold ? HIGH : LOW);

  // ---- Light logic ----
  digitalWrite(PIN_LIGHT, lightRaw < LIGHT_LOW_THRESHOLD ? HIGH : LOW);

  // ---- Flow tracking (once per second) ----
  if (millis() - lastFlowCheck >= 1000) {
    noInterrupts();
    unsigned long pulses = flowPulses;
    flowPulses = 0;
    interrupts();
    totalLitres += pulses / FLOW_PULSES_PER_LITRE;
    lastFlowCheck = millis();
  }

  // ---- Send readings to Raspberry Pi over serial (CSV line) ----
  Serial.print(soilValue);        Serial.print(",");
  Serial.print(tempC);            Serial.print(",");
  Serial.print(lightRaw);         Serial.print(",");
  Serial.print(humidity);         Serial.print(",");
  Serial.print(rainDetected);     Serial.print(",");
  Serial.print(phValue);          Serial.print(",");
  Serial.print(waterLevelStage);  Serial.print(",");
  Serial.print(totalLitres);      Serial.print(",");
  Serial.print(shouldIrrigate);   Serial.print(",");
  Serial.print(tooHot);           Serial.print(",");
  Serial.print(tooCold);
  Serial.println();

  delay(1000);
}

// Converts NTC voltage divider reading to Celsius using the beta-equation
// approximation. Matches the 5k NTC thermistor described in the original
// report (linear 10mV/°C formula does not apply to a resistive NTC device -
// that formula belongs to a different sensor type, like an LM35).
// BETA and NOMINAL_RESISTANCE come from the thermistor's datasheet -
// values below are typical for a 5k NTC and should be confirmed.
const float NOMINAL_RESISTANCE = 5000.0;  // resistance at 25C
const float NOMINAL_TEMP_C     = 25.0;
const float BETA               = 3950.0; // thermistor beta coefficient
const float SERIES_RESISTOR    = 10000.0; // fixed divider resistor

float readTemperatureC(int raw) {
  float resistance = SERIES_RESISTOR * (1023.0 / raw - 1.0);
  float steinhart = resistance / NOMINAL_RESISTANCE;
  steinhart = log(steinhart);
  steinhart /= BETA;
  steinhart += 1.0 / (NOMINAL_TEMP_C + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15; // convert Kelvin to Celsius
  return steinhart;
}

// Converts pH probe voltage to a pH value.
// Requires calibration against pH 4 and pH 7 buffer solutions -
// slope/offset below are placeholders until calibrated.
float readPH(int raw) {
  float voltage = raw * (5.0 / 1023.0);
  float slope = -5.70;
  float offset = 21.34;
  return slope * voltage + offset;
}

// Reads the probe ladder and returns how many probes are submerged.
int readWaterLevel() {
  int stage = 0;
  for (int i = 0; i < 4; i++) {
    if (digitalRead(PIN_WATER_PROBE[i]) == HIGH) stage++;
  }
  return stage;
}
