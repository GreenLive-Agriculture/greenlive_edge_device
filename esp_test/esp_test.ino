#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "Secret.h"
 
#define TIME_ZONE +1

unsigned long lastMillis = 0;
unsigned long previousMillis = 0;
const long interval = 5000;
 
#define AWS_IOT_SUBSCRIBE_TOPIC "client/esp_test"
#define AWS_IOT_PUBLISH_TOPIC "client/esp_publish"
 
WiFiClientSecure net;
 
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);
 
PubSubClient client(net);
 
time_t now;
time_t nowish = 1510592825;

#include "DHT.h"
DHT dht(2, DHT11);

float h = 0;
float t = 0;

char received_msg[10] ;
 
void NTPConnect(void)
{
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}
 
void messageReceived(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++)
  {
    // Append the payload into string
    Serial.print((char)payload[i]);
    received_msg[i] = (char)payload[i] ;
  }
}
 
void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println(String("Attempting to connect to SSID: ") + String(WIFI_SSID));
 
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  Serial.println("WiFI Connected");
  delay(1000);
  
  NTPConnect();
 
  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);
 
  client.setServer(AWS_IOT_ENDPOINT, 8883);
  client.setCallback(messageReceived);
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(1000);
  }
  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
}

void setup()
{
  Serial.begin(19200);

  dht.begin();
 
  delay(3000);
 
  connectAWS();
}

void publishMessage()
{
  StaticJsonDocument<200> doc;
  doc["timestamp"] = time(nullptr);
  doc["kit_id"] = 7102023;
  doc["humidity"] = int(h);
  doc["temperature"] = int(t);
  doc["capture"] = received_msg;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);

  //vider le buffer
  memset(received_msg, 0, sizeof(received_msg));
}
 
void loop()
{
  now = time(nullptr);
 
  if (!client.connected())
  {
    connectAWS();
  }
  else
  {
    
    client.loop();
    if (millis() - lastMillis > 5000)
    {
       h = dht.readHumidity();
       t = dht.readTemperature();

      Serial.print("Humidity: ");
      Serial.println(h);
      Serial.print("Temperature: ");
      Serial.println(t);

      lastMillis = millis();
      publishMessage();
    }
  }
}