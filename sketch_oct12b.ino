
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_BMP280.h>
#include "FirebaseESP32.h"

// =====================================================
// WIFI CONFIGURATION
// =====================================================

#define WIFI_SSID       "YOUR_WIFI_NAME"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// =====================================================
// FIREBASE CONFIGURATION
// Use the same credentials as your working Firebase code.
// FIREBASE_HOST must NOT include https://
// =====================================================

#define FIREBASE_HOST   "YOUR_FIREBASE_HOST"
#define FIREBASE_AUTH   "YOUR_FIREBASE_AUTH_TOKEN"

FirebaseData firebaseData;

// =====================================================
// PIN CONFIGURATION
// =====================================================

#define smPin       34
#define relayPin    13
#define magSwitch   16

// =====================================================
// BMP280 CONFIGURATION
// =====================================================

#define SEALEVELPRESSURE_HPA 1013.25

Adafruit_BMP280 bmp;

// =====================================================
// SENSOR VARIABLES
// =====================================================

int smValue;
int smLimit = 1000;

float tempC;
float tempF;
float atPressure;
float altitude;

int swStatus;
int lastSw;

bool pumpOn = false;

int delayTime = 2000;

// =====================================================
// FIREBASE DATABASE PATHS
// =====================================================

String basePath = "IOTLAB/My_App/SmartPlant";

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  // Soil moisture sensor
  pinMode(smPin, INPUT);
  analogReadResolution(12);

  // Relay: pump OFF initially
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  // Magnetic switch
  pinMode(magSwitch, INPUT_PULLUP);

  // ESP32 I2C: SDA = 21, SCL = 22
  Wire.begin(21, 22);

  Serial.println();
  Serial.println("==================================");
  Serial.println(" SMART PLANT MONITORING SYSTEM");
  Serial.println("==================================");

  // ---------------------------------------------------
  // INITIALIZE BMP280
  // ---------------------------------------------------

  if (!bmp.begin(0x76))
  {
    if (!bmp.begin(0x77))
    {
      Serial.println("ERROR: BMP280 not detected!");
      Serial.println("Check SDA, SCL, VCC, GND and address.");

      digitalWrite(relayPin, LOW);

      while (true)
      {
        delay(1000);
      }
    }
  }

  bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,
    Adafruit_BMP280::SAMPLING_X2,
    Adafruit_BMP280::SAMPLING_X16,
    Adafruit_BMP280::FILTER_X16,
    Adafruit_BMP280::STANDBY_MS_500
  );

  lastSw = digitalRead(magSwitch);

  Serial.println("BMP280 detected.");

  // ---------------------------------------------------
  // CONNECT TO WIFI
  // ---------------------------------------------------

  pinMode(2, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(2, !digitalRead(2));
    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // ---------------------------------------------------
  // INITIALIZE FIREBASE - YOUR WORKING API
  // ---------------------------------------------------

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase initialized.");
  Serial.println("System ready.");
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop()
{
  // ---------------------------------------------------
  // 1. READ SOIL MOISTURE
  // ---------------------------------------------------

  smValue = 4095 - analogRead(smPin);

  Serial.println();
  Serial.println("---------- SENSOR READINGS ----------");

  Serial.print("Soil Moisture: ");
  Serial.println(smValue);

  // ---------------------------------------------------
  // 2. AUTOMATIC WATERING
  // ---------------------------------------------------

  pumpOn = (smValue < smLimit);

  if (pumpOn)
  {
    digitalWrite(relayPin, HIGH);
    Serial.println("Soil Status: DRY");
    Serial.println("Pump: ON");
  }
  else
  {
    digitalWrite(relayPin, LOW);
    Serial.println("Soil Status: WET ENOUGH");
    Serial.println("Pump: OFF");
  }

  // ---------------------------------------------------
  // 3. READ BMP280
  // ---------------------------------------------------

  tempC = bmp.readTemperature();
  tempF = (1.8 * tempC) + 32.0;

  atPressure = bmp.readPressure() / 100.0F;
  altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);

  Serial.println();
  Serial.println("BMP280 ENVIRONMENT DATA");

  Serial.print("Temperature: ");
  Serial.print(tempC);
  Serial.println(" deg C");

  Serial.print("Temperature: ");
  Serial.print(tempF);
  Serial.println(" deg F");

  Serial.print("Pressure: ");
  Serial.print(atPressure);
  Serial.println(" hPa");

  Serial.print("Estimated Altitude: ");
  Serial.print(altitude);
  Serial.println(" m");

  // ---------------------------------------------------
  // 4. READ MAGNETIC WINDOW SWITCH
  // ---------------------------------------------------

  swStatus = digitalRead(magSwitch);
  lastSw = swStatus;

  bool windowOpen = (swStatus == HIGH);

  String soilStatus = pumpOn ? "DRY" : "WET ENOUGH";
  String pumpStatus = pumpOn ? "ON" : "OFF";
  String windowStatus = windowOpen ? "OPEN" : "CLOSED";

  Serial.println();
  Serial.println("WINDOW STATUS");
  Serial.print("Window: ");
  Serial.println(windowStatus);

  // ---------------------------------------------------
  // 5. UPLOAD DATA TO FIREBASE
  // Uses your existing FirebaseESP32 API.
  // Each value is stored at its own database path.
  // ---------------------------------------------------

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println();
    Serial.println("---------- FIREBASE UPLOAD ----------");

    bool uploadOK = true;

    if (!Firebase.setInt(
          firebaseData,
          basePath + "/Soil_Moisture",
          smValue))
    {
      uploadOK = false;
      Serial.print("Soil moisture error: ");
      Serial.println(firebaseData.errorReason());
    }

    if (!Firebase.setInt(
          firebaseData,
          basePath + "/Soil_Limit",
          smLimit))
    {
      uploadOK = false;
      Serial.print("Soil limit error: ");
      Serial.println(firebaseData.errorReason());
    }

    if (!Firebase.setString(
          firebaseData,
          basePath + "/Soil_Status",
          soilStatus))
    {
      uploadOK = false;
      Serial.print("Soil status error: ");
      Serial.println(firebaseData.errorReason());
    }

    if (!Firebase.setString(
          firebaseData,
          basePath + "/Pump_Status",
          pumpStatus))
    {
      uploadOK = false;
      Serial.print("Pump status error: ");
      Serial.println(firebaseData.errorReason());
    }

    if (!Firebase.setFloat(
          firebaseData,
          basePath + "/Temperature_C",
          tempC))
    {
      uploadOK = false;
      Serial.print("Temperature C error: ");
      Serial.println(firebaseData.errorReason());
    }

    if (!Firebase.setFloat(
          firebaseData,
          basePath + "/Temperature_F",
          tempF))
    {
      uploadOK = false;
      Serial.print("Temperature F error: ");
      Serial.println(firebaseData.errorReason());
    }

    if (!Firebase.setFloat(
          firebaseData,
          basePath + "/Pressure_hPa",
          atPressure))
    {
      uploadOK = false;
      Serial.print("Pressure error: ");
      Serial.println(firebaseData.errorReason());
    }

    if (!Firebase.setFloat(
          firebaseData,
          basePath + "/Altitude_m",
          altitude))
    {
      uploadOK = false;
      Serial.print("Altitude error: ");
      Serial.println(firebaseData.errorReason());
    }

    if (!Firebase.setString(
          firebaseData,
          basePath + "/Window_Status",
          windowStatus))
    {
      uploadOK = false;
      Serial.print("Window status error: ");
      Serial.println(firebaseData.errorReason());
    }

    if (uploadOK)
    {
      Serial.println("All sensor values uploaded successfully!");
      Serial.print("Database path: ");
      Serial.println(basePath);
    }
    else
    {
      Serial.println("Some Firebase uploads failed.");
    }
  }
  else
  {
    Serial.println("Wi-Fi disconnected. Upload skipped.");
    WiFi.reconnect();
  }

  // ---------------------------------------------------
  // 6. NEXT READING
  // ---------------------------------------------------

  Serial.println("-------------------------------------");
  Serial.println("Next reading in 2 seconds...");

  delay(delayTime);
}
