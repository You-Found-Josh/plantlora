//Arduino Raspberry Pi wireless Comunnication through LoRa - SX1278
//Sending Soil Moisture values from Arduino through Radio head LoRa without ACK

#include <Wire.h>
#include <SPI.h> //Import SPI library 
#include <RH_RF95.h> // RF95 from RadioHead Library 
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <avr/wdt.h> // WDT library (added Jan 30)

#define RFM95_CS 10 //CS if Lora connected to pin 10
#define RFM95_RST 9 //RST of Lora connected to pin 9
#define RFM95_INT 2 //INT of Lora connected to pin 2

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 434.0

//set the sea level pressure?
#define SEALEVELPRESSURE_HPA (airPressure + 1)

#define UP 0
#define DOWN 1

Adafruit_BME280 bme;

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);


const byte pwmLED = 3;
int relay = 4;
volatile byte relayState = LOW;

// constants for min and max PWM
const int minPWM = 0;
const int maxPWM = 255;
// State Variable for Fade Direction
byte fadeDirection = UP;
// Global Fade Value
// but be bigger than byte and signed, for rollover
int fadeValue = 0;
// How smooth to fade?
byte fadeIncrement = 5;


int moistureLimit = 40;
int soilMoistureValue; //realtime value from sensor (analog)
int soilMoisturePercent; //digital mapped percent of moisture
int airTemperature; // air temp
int airPressure; // air pressure
int altitude; //altitude
int airHumidity; //air humidity
int lightVal;
int lightPercent;


const int AirValue = 600;   //resistance value of the air
const int WaterValue = 310;  //resistance value of water
const int BrightSun = 1100;   //value of LightVal in a bright day
const int DarkNight = 25;  //value of LightVal in a dark room

unsigned long startTime = 0;
unsigned long fadeInterval = 50; //LED Fade Interval
//unsigned long sensorInterval = 15000; //Interval test
unsigned long sensorInterval = 600000; //Interval for sending data to Raspi over Lora
unsigned long pumpInterval = 1000; //Interval between checking soil moisture and turning pump on and off
bool timeFlag = false;

unsigned long previousFadeMillis;
unsigned long previousSensorMillis;
unsigned long previousPumpMillis;

void setup()
{

  //Initialize Serial Monitor
  Serial.begin(9600);

  //wdt reset initialize
  wdt_enable(WDTO_1S); //added Jan 30

  // Reset LoRa Module
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  analogWrite(pwmLED, fadeValue);

  // Pin for relay module set as output
  pinMode(relay, OUTPUT);
  digitalWrite(relay, HIGH);

  //Initialize LoRa Module
  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1);
  }


  //Set the default frequency 434.0MHz
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }

  //Initialize the BME280 temp/pressure/humidity sensor
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  rf95.setTxPower(22, false); //Transmission power of the Lora Module

  //call Sensor once at 0 seconds
  Values();
  Sensor();
}

void Values() {
  soilMoistureValue = analogRead(A0);  // Soil Moisture Value
  soilMoisturePercent = map(soilMoistureValue, AirValue, WaterValue, 0, 100); // Soil Moisture Percent
  airTemperature = bme.readTemperature(); // Air Temperature Value
  airPressure = bme.readPressure() / 100.0F; // Air Pressure Value
  altitude = bme.readAltitude(SEALEVELPRESSURE_HPA); // Approximate Altitude
  airHumidity = bme.readHumidity(); // Air Humidity Value
  lightVal = analogRead(A1); // Raw Light Sensor
  lightPercent = map(lightVal, DarkNight, BrightSun, 0, 100); // Soil Moisture Percent
}

void Fade(unsigned long thisMillis) {

  if (thisMillis - previousFadeMillis >= fadeInterval) {
    // yup, it's time!
    if (fadeDirection == UP) {
      fadeValue = fadeValue + fadeIncrement;
      if (fadeValue >= maxPWM) {
        // At max, limit and change direction
        fadeValue = maxPWM;
        fadeDirection = DOWN;
      }
    } else {
      //if we aren't going up, we're going down
      fadeValue = fadeValue - fadeIncrement;
      if (fadeValue <= minPWM) {
        // At min, limit and change direction
        fadeValue = minPWM;
        fadeDirection = UP;
      }
    }
    // Only need to update when it changes
    analogWrite(pwmLED, fadeValue);

    // reset millis for the next iteration (fade timer only)
    previousFadeMillis = thisMillis;
  }
}

void Sensor() {
  //debugging
  Serial.print("Soil Moisture Value: ");
  Serial.println(soilMoistureValue);

  if (soilMoisturePercent > 100)
  {
    Serial.print("Soil Moisture Percentage: ");
    Serial.println("100 %");
  }
  else if (soilMoisturePercent < 0)
  {
    Serial.print("Soil Moisture Percentage: ");
    Serial.println("0 %");
  }
  else if (soilMoisturePercent > 0 && soilMoisturePercent < 100)
  {
    Serial.print("Soil Moisture Percentage: ");
    Serial.print(soilMoisturePercent);
    Serial.println("%");
  }

  Serial.print("Temperature: ");
  Serial.print(airTemperature);
  Serial.println("*C");

  Serial.print("Pressure: ");
  Serial.print(airPressure);
  Serial.println("hPa");

  Serial.print("Approx. Altitude: ");
  Serial.print(altitude);
  Serial.println("m");

  Serial.print("Humidity: ");
  Serial.print(airHumidity);
  Serial.println("%");

  Serial.print("Light Percent: ");
  Serial.print(lightPercent);
  Serial.println("%");

  Serial.println();

  //end debugging

  char buf[80] = {0}; //80 byte message
  sprintf(buf, "%d|%d|%d|%d|%d|%d", soilMoisturePercent, airTemperature, airPressure, altitude, airHumidity, lightPercent); //can add in new variables here, but don't forget to add another|%d
  rf95.send((uint8_t *)buf, 80); // send integer measured value as C-string, six byte packet
}

void TimeSensor(unsigned long thisMillis) {

  if (thisMillis - previousSensorMillis >= sensorInterval) {
    //call sensor function here
    Sensor();
    // reset millis for the next iteration (sensor timer only)
    previousSensorMillis = thisMillis;
  }

}

void Pump(unsigned long thisMillis) {
  if (thisMillis - previousPumpMillis >= pumpInterval) {

    //debugging
    Serial.print("Soil Moisture Percentage: ");
    Serial.print(soilMoisturePercent);
    Serial.println("%");
    Serial.println(soilMoistureValue);
    //end debugging

    if (soilMoisturePercent <= moistureLimit) {
      startWatering();
    } else {
      stopWatering();
    }
    // reset millis for the next iteration (pump timer only)
    previousPumpMillis = thisMillis;
  }
}

void startWatering() {
  digitalWrite(relay, LOW);
  Serial.println("pumping water now");
  delay(100);
}

void stopWatering() {
  digitalWrite(relay, HIGH);
  Serial.println("stopping pumping water now");
  delay(100);
}

void loop()
{
  unsigned long currentMillis = millis();

  Values();
//  Fade(currentMillis);
//  Pump(currentMillis);
  TimeSensor(currentMillis);

  wdt_reset(); //wdt reset - added Jan 30

}
