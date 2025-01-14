#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

#include <WiFi.h>
#include <PubSubClient.h>
#include "symbols.h"

#define USE_DUMMY_CONFIG 1

#if USE_DUMMY_CONFIG
#include "dummy_config.h"
#else
#include "config.h" // Include the configuration file with real security credentials
#endif

#define LED_PIN 13 // Pin where the NeoPixel strip is connected
#define LED_COUNT 64 // Number of LEDs in the strip

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
static uint32_t green_color = strip.Color(0, 255, 0);
static uint32_t red_color = strip.Color(255, 0, 0);
static uint32_t white_color = strip.Color(255, 255, 255);

// Define color gradients from green to red in 8 steps
uint32_t colorGradient[8] = {
    strip.Color(255, 0, 0),   // Red
    strip.Color(219, 36, 0),  // Step 6
    strip.Color(183, 72, 0),  // Step 5
    strip.Color(146, 109, 0), // Step 4
    strip.Color(109, 146, 0), // Step 3
    strip.Color(72, 183, 0),  // Step 2
    strip.Color(36, 219, 0),  // Step 1
    strip.Color(0, 255, 0)    // Green
};
int8_t displayMatrix[8][8] = {
    {-1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1}};

void drawMatrix(const uint8_t matrix[][8], int rows, int cols, uint32_t color);
void drawMappedMatrix(const int8_t matrix[][8], int rows, int cols);
void shiftMatrixLeft(int8_t matrix[][8], int rows, const int8_t currentDataRow[8]);

uint16_t maxPixels = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setupWifi()
{
  drawMatrix(wifiSymbol, 8, 8, white_color);
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
// Create a new row with the current data
int8_t *createCurrentDataRow(int value)
{
  int8_t *row = new int8_t[8]{-1, -1, -1, -1, -1, -1, -1, -1};
  int mapped = -1;
  switch (value)
  {
  case 2000 ... INT_MAX:
    mapped = 0;
    break;
  case 1000 ... 1999:
    mapped = 1;
    break;
  case 500 ... 999:
    mapped = 2;
    break;
  case 100 ... 499:
    mapped = 3;
    break;
  case -99 ... 99:
    mapped = 4;
    break;
  case -300 ... - 100:
    mapped = 5;
    break;
  case -500 ... - 301:
    mapped = 6;
    break;
  case INT_MIN ... - 501:
    mapped = 7;
    break;
  default:
    // If value is not in any of the ranges, set mapped to -1
    mapped = -1;
    break;
  }
  row[mapped] = mapped;
  return row;
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String payloadStr = String((char *)payload).substring(0, length);
  Serial.print("Payload as String: ");
  Serial.println(payloadStr);

  // Convert payload to float
  float floatValue = atof(payloadStr.c_str());

  // Round float value to int
  int intValue = round(floatValue);

  int8_t *currentDataRow = createCurrentDataRow(round(intValue));
  shiftMatrixLeft(displayMatrix, 8, currentDataRow);
  drawMappedMatrix(displayMatrix, 8, 8);
  Serial.println();
}

void reconnectMQTT()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), loginMQTT, passwordMQTT))
    {
      Serial.println("connected");
      // subscribe to the current power consumption topic
      client.subscribe(MQTT_CurrentPowerConsumptionTopic);
      // Draw check mark
      drawMatrix(checkMark, 8, 8, green_color);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      drawMatrix(questionMark, 8, 8, red_color);
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void shiftMatrixLeft(int8_t matrix[][8], int rows, const int8_t currentDataRow[8])
{
  for (int y = 0; y < rows; y++)
  {
    for (int x = 0; x < 7; x++)
    {
      matrix[y][x] = matrix[y][x + 1]; // Shift each column to the left
    }
    matrix[y][7] = currentDataRow[y]; // Insert new row at the rightmost column
  }
}
void drawMatrix(const uint8_t matrix[][8], int rows, int cols, uint32_t color)
{
  for (int y = 0; y < rows; y++)
  {
    for (int x = 0; x < cols; x++)
    {
      if (matrix[y][x] == 1)
      {
        strip.setPixelColor(y * cols + x, color); // Set pixel to white
      }
      else
      {
        strip.setPixelColor(y * cols + x, strip.Color(0, 0, 0)); // Set pixel to off
      }
    }
  }
  strip.show();
}

uint32_t getColorWithBrightness(uint32_t color, uint8_t brightness)
{
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  r = r * brightness / 255;
  g = g * brightness / 255;
  b = b * brightness / 255;
  return strip.Color(r, g, b);
}

void drawMappedMatrix(const int8_t matrix[][8], int rows, int cols)
{
  for (int y = 0; y < rows; y++)
  {
    for (int x = 0; x < cols; x++)
    {
      int step = matrix[y][x];

      if (step == -1)
      {
        // If step is -1, set pixel to off
        strip.setPixelColor(y * cols + x, strip.Color(0, 0, 0));
      }
      else
      {
        // Set brightness to a minimum of 16 to avoid black pixels
        uint8_t brightness = max(16, x * 32);

        // Color the pixel with the color from the colorGradient array with fade out of the oldest data points
        strip.setPixelColor(y * cols + x, getColorWithBrightness(colorGradient[step], brightness));
      }
    }
  }
  strip.show();
}

void setup()
{
  Serial.begin(115200);

  // Initialize the NeoPixel library
  strip.begin();
  maxPixels = strip.numPixels();
  strip.setBrightness(255);
  strip.show();

  // Initialize WiFi and MQTT 
  setupWifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop()
{
  if (!client.connected())
  {
    // If WiFi client is not connected, try to reconnect WiFi
    setupWifi();

    reconnectMQTT();
  }
  client.loop();
}