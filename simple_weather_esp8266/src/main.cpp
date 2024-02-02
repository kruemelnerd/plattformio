#include <Arduino.h>
#include <secrets.h>
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define DEVICE "ESP8266"
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "DHT.h"

// Time zone info
#define TZ_INFO "UTC1"

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Declare Data point
Point sensor("weather_room_work");

#define DHTTYPE DHT22 // DHT 22 (AM2302)
#define DHTPIN D1     // Digital pin connected to the DHT sensor

DHT dht(DHTPIN, DHTTYPE);

struct weatherData
{
  float humidity;
  float temperature;
  float heat_index;
};

void setupWifi();
void setupDHT();
void setupInflux();
weatherData readDataFromSensor();
void sendDataToInflux(weatherData data);
void startDeepSleep();

void setup()
{
  Serial.begin(9600);
  setupWifi();
  setupInflux();
  setupDHT();
}

void loop()
{

  weatherData data = readDataFromSensor();
  sendDataToInflux(data);
  startDeepSleep();
}


/////////////////////////////////////////////
/// Setup ///////////////////////////////////
/////////////////////////////////////////////

void setupDHT()
{
  Serial.println(F("DHTxx test!"));

  dht.begin();
}

void setupWifi()
{
  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
}

void setupInflux()
{
  // Accurate time is necessary for certificate validation and writing in batches
  // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection())
  {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  }
  else
  {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

/////////////////////////////////////////////
/// LOOP  ///////////////////////////////////
/////////////////////////////////////////////


weatherData readDataFromSensor()
{
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(hic))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    startDeepSleep();
  }

  weatherData data = {h, t, hic};

  Serial.print(F("Humidity: "));
  Serial.print(data.humidity);
  Serial.print(F("%  Temperature: "));
  Serial.print(data.temperature);
  Serial.print(F("°C "));
  Serial.print(F(" Heat index: "));
  Serial.print(data.heat_index);
  Serial.print(F("°C "));
  Serial.println();

  return data;
}

void sendDataToInflux(weatherData data)
{
  // Clear fields for reusing the point. Tags will remain the same as set above.
  sensor.clearFields();
  sensor.addField("humidity", data.humidity);
  sensor.addField("temperature", data.temperature);
  sensor.addField("heat_index", data.heat_index);

  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(sensor.toLineProtocol());

  // Check WiFi connection and reconnect if needed
  if (wifiMulti.run() != WL_CONNECTED)
  {
    Serial.println("Wifi connection lost");
  }
  // Write point
  if (!client.writePoint(sensor))
  {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

/**
 * DeepSleep 
 * Basis: 5 * 1000000
 * 5000000 = 5 Sekunden
 * 60000000 = 1 Minute
 * 600000000 = 10 Minuten
 * 900000000 = 15 Minuten
 * 1800000000 = 30 Minuten
 */
void startDeepSleep()
{
  Serial.println("Going to deep sleep...");
  ESP.deepSleep(600000000);
  yield();
}
