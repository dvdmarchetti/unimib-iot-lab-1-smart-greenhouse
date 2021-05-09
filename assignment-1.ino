// DHT11 Sensor
#include <DHT.h>

// WiFi
#include <ESP8266WiFi.h>
#include "includes/secrets.h"

// Web Server
#include <ESP8266WebServer.h>

// JSON
#include <ArduinoJson.h>

// Display
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// Button debouncer
#include "includes/debouncer.h"

// Multi analog sensor
#include "includes/multi_analog.h"

// Influx DB
#include <InfluxDbClient.h>

// Devices Configurations
//------------------------
// Button
#define BUTTON_PIN 10

// Led pin
#define LED_MOISTURE_PIN D0
#define LED_TEMPERATURE_PIN D4

// Analog pin
#define ANALOG_PIN A0

// Temperature/Humidity
#define DHT_PIN D3
#define DHTTYPE DHT11
#define TEMPERATURE_MAX_LIMIT 26
#define TEMPERATURE_MIN_LIMIT 22
#define HUMIDITY_MAX_LIMIT 50
#define HUMIDITY_MIN_LIMIT 30

// Moisture
#define MOISTURE_VCC D8
#define MOISTURE_GND D7
#define MOISTURE_MIN_LIMIT 300

// Photoresistor
#define PHOTORESISTOR_VCC D6
#define PHOTORESISTOR_GND D5
#define LIGHT_MAX_LIMIT 700
#define LIGHT_MIN_LIMIT 300

// I2C Bus Address
#define DISPLAY_ADDR 0x27
#define DISPLAY_CHARS 16
#define DISPLAY_LINES 2

// LCD Icons
#define ICON_DEGREE 0
#define ICON_DROP 1
#define ICON_MOON 2
#define ICON_PLANT 3

// HTML Pages
//------------
#include "pages/index.h"


// Type Definitions
//------------------
struct dht_read {
  float humidity, temperature, heat_index;
  dht_read() : humidity(0), temperature(0), heat_index(0) {}
};

// Global objects
//----------------
LiquidCrystal_I2C display(DISPLAY_ADDR, DISPLAY_CHARS, DISPLAY_LINES);

DHT dht = DHT(DHT_PIN, DHTTYPE);

const char* ssid = SECRET_SSID;
const char* pass = SECRET_PASS;
WiFiClient client;

void onButtonClick(uint8_t);
Debouncer button = Debouncer(BUTTON_PIN, onButtonClick, TRIGGER_FALLING);

ESP8266WebServer server(80);

MultiAnalog multi_analog_manager(ANALOG_PIN);
analog_sensor photoresistor_sensor(PHOTORESISTOR_VCC, PHOTORESISTOR_GND);
analog_sensor moisture_sensor(MOISTURE_VCC, MOISTURE_GND);

InfluxDBClient client_idb(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point device("device_status");

void setup() {
  Serial.begin(115200);

  setup_display();
  // Led
  setup_leds();
  // Wifi
  WiFi.mode(WIFI_STA);
  // Web Server
  setup_webserver();
  // Dht
  dht.begin();
  // Analog pins
  multi_analog_manager.add(photoresistor_sensor);
  multi_analog_manager.add(moisture_sensor);
  multi_analog_manager.disable_all();

  Serial.println(F("[OS] Setup completed."));
}

void setup_display() {
  Serial.println(F("[LCD] Checking display connection..."));

  Wire.begin();
  Wire.beginTransmission(DISPLAY_ADDR);

  uint8_t error = Wire.endTransmission();

  if (error != 0) {
    Serial.print(F("[LCD] Display not found. Error "));
    Serial.println(error);
    Serial.println(F("[LCD] Check connections and configuration. Reset to try again!"));
    while (true) delay(1);
  }

  Serial.println(F("[LCD] Display found."));
  display.begin(DISPLAY_CHARS, 2);

  byte degree_symbol[] = {0x02, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
  display.createChar(ICON_DEGREE, degree_symbol);
  byte drop_symbol[] = {0x00, 0x04, 0x0E, 0x0E, 0x1F, 0x1F, 0x0E, 0x00};
  display.createChar(ICON_DROP, drop_symbol);
  byte moon_symbol[] = {0x00, 0x0E, 0x1D, 0x18, 0x18, 0x1D, 0x0E, 0x00};
  display.createChar(ICON_MOON, moon_symbol);
  byte plant_symbol[] = {0x1C, 0x14, 0x07, 0x05, 0x04, 0x1F, 0x0E, 0x0E};
  display.createChar(ICON_PLANT, plant_symbol);
}

void setup_leds() {
  pinMode(LED_MOISTURE_PIN, OUTPUT);
  digitalWrite(LED_MOISTURE_PIN, HIGH);

  pinMode(LED_TEMPERATURE_PIN, OUTPUT);
  digitalWrite(LED_TEMPERATURE_PIN, HIGH);
}

void setup_webserver() {
  server.on("/", HTTP_GET, handle_root);
  server.on("/api/status", HTTP_GET, handle_api_status);
  server.on("/api/temperature/increase", HTTP_POST, handle_temperature_increase_command);
  server.on("/api/temperature/decrease", HTTP_POST, handle_temperature_decrease_command);
  server.on("/api/moisture", HTTP_POST, handle_adjust_moisture_command);
  server.onNotFound(handle_error);

  server.begin();
  Serial.println(F("[WEB] WebServer started."));
}

// Menu handling
enum MenuPages {HOME, TEMPERATURE, LIGHT, MOISTURE, LENGTH};
uint8_t current_section = MenuPages::HOME;

// State vars
ulong command_temperature_increase = 0;
ulong command_temperature_decrease = 0;
ulong command_adjust_moisture = 0;
ulong next_redraw = 0;
dht_read dht_values = dht_read();
uint terrain_moisture = 0;
uint light = 0;

void loop() {
  ensure_wifi_is_connected();
  ensure_influx_db_is_connected();

  server.handleClient();
  button.loop();

  read_sensors();
  write_data_to_influx();
  check_thresholds();
  draw_menu();
}

void ensure_wifi_is_connected() {
  static boolean has_logged_ip = false;

  if (WiFi.status() != WL_CONNECTED) {
    has_logged_ip = false;
    Serial.print(F("[WiFi] Connecting to SSID: "));
    Serial.println(SECRET_SSID);

    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(F("."));
      delay(150);
    }
    Serial.print(F("\n[WiFi] Connected! Device address: "));
    Serial.println(WiFi.localIP());
    has_logged_ip = true;
  }

  if (! has_logged_ip) {
    Serial.print(F("\n[WiFi] Connected! Device address: "));
    Serial.println(WiFi.localIP());
    has_logged_ip = true;
  }
}

void ensure_influx_db_is_connected() {
  static bool last_status = false;
  if (client_idb.validateConnection()) {
    if (last_status != true) {
      Serial.print(F("[INFLUX] Connected to InfluxDB: "));
      Serial.println(client_idb.getServerUrl());
    }
    last_status = true;
  } else {
    if (last_status != false) {
      Serial.print(F("[INFLUX] InfluxDB connection failed: "));
      Serial.println(client_idb.getLastErrorMessage());
    }
    last_status = false;
  }
}

void draw_menu() {
  static ulong last_redraw = millis();

  ulong current_time = millis();
  if (current_time - last_redraw < next_redraw) {
    return;
  }

  last_redraw = millis();
  display.clear();
  display.setBacklight(255);
  display.home();
  Serial.print(F("[LCD] Drawing "));

  if (current_section == MenuPages::HOME) {
    Serial.println(F("HOME"));

    display.print("Welcome to ESP");
    display.setCursor(0, 1);
    display.print("Greenhouse!");
    display.write(ICON_PLANT);

    next_redraw = 1000;
  } else if (current_section == MenuPages::TEMPERATURE) {
    Serial.println(F("TEMPERATURE"));

    display.print("Temp: ");
    display.print(dht_values.temperature);
    display.write(ICON_DEGREE);
    display.print("C");
    display.setCursor(0, 1);
    display.print("Humidity: ");
    display.print(dht_values.humidity);
    display.print("%");

    next_redraw = 2000;
  } else if (current_section == MenuPages::LIGHT) {
    Serial.println(F("LIGHT"));

    display.print("Light amount:");
    display.setCursor(0, 1);
    display.print(light);
    display.print(F(" ("));
    display.print(float(light)/1024*100);
    display.print(F("%)"));

    next_redraw = 1000;
  } else if (current_section == MenuPages::MOISTURE) {
    Serial.println(F("MOISTURE"));

    display.print("Soil moisture:");
    display.setCursor(0, 1);
    display.print(terrain_moisture);
    display.print(F(" ("));
    display.print(float(terrain_moisture)/1024*100);
    display.print(F("%)"));

    next_redraw = 1000;
  }

  if (current_section != MenuPages::HOME && terrain_moisture < MOISTURE_MIN_LIMIT) {
    display.setCursor(15, 0);
    display.write(ICON_DROP);
  }

  if (current_section != MenuPages::HOME && light < LIGHT_MIN_LIMIT) {
    display.setCursor(14, 0);
    display.write(ICON_MOON);
  }
}

void check_thresholds() {
  const ulong current_time = millis();

  if (current_time - command_temperature_increase < 5000 || dht_values.temperature < TEMPERATURE_MIN_LIMIT
   || current_time - command_temperature_decrease < 5000 || dht_values.temperature > TEMPERATURE_MAX_LIMIT) {
    digitalWrite(LED_TEMPERATURE_PIN, LOW);
  } else {
    digitalWrite(LED_TEMPERATURE_PIN, HIGH);
  }

  if (current_time - command_adjust_moisture < 3000 || terrain_moisture < MOISTURE_MIN_LIMIT) {
    digitalWrite(LED_MOISTURE_PIN, LOW);
  } else {
    digitalWrite(LED_MOISTURE_PIN, HIGH);
  }
}

void read_sensors() {
  dht_values = read_dht();
  terrain_moisture = read_terrain_moisture_value();
  light = read_light_value();
}

void force_redraw() {
  next_redraw = 1;
}

void write_data_to_influx() {
  static ulong last_read = 0;

  if (millis() - last_read < 2000) {
    return;
  }
  last_read = millis();

  device.clearFields();
  device.addField("temperature", dht_values.temperature);
  device.addField("humidity", dht_values.humidity);
  device.addField("heat_index", dht_values.heat_index);
  device.addField("light_amount", light);
  device.addField("terrain_moisture", terrain_moisture);

  if (! client_idb.writePoint(device)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
}

// Sensor reads
uint read_terrain_moisture_value() {
  static uint value = 0;
  static ulong last_read = 0;

  if (millis() - last_read < 10) {
    return value;
  }

  last_read = millis();
  // Invert moisture value
  value = 1024 - multi_analog_manager.read(moisture_sensor);

  return value;
}

uint read_light_value() {
  static uint value = 0;
  static ulong last_read = 0;

  if (millis() - last_read < 10) {
    return value;
  }

  last_read = millis();
  value = multi_analog_manager.read(photoresistor_sensor);

  return value;
}

dht_read read_dht() {
  static dht_read value;
  static ulong last_read = 0;
  if (millis() - last_read <= 2000) {
    return value;
  }

  last_read = millis();

  value.humidity = dht.readHumidity();
  value.temperature = dht.readTemperature();
  value.heat_index = dht.computeHeatIndex(value.temperature, value.humidity, false);

  return value;
}

// Button Handlers
void onButtonClick(uint8_t pin) {
  current_section = (current_section+1) % (MenuPages::LENGTH - 1) + 1;
  force_redraw();
}

// Web Server handlers
void handle_root() {
  server.send_P(200, "text/html", PAGE_ROOT);
}

void handle_api_status() {
  String output;
  DynamicJsonDocument json(512);

  json["temperature"] = dht_values.temperature;
  json["humidity"] = dht_values.humidity;
  json["lightness"] = light;
  json["moisture"] = terrain_moisture;

  serializeJson(json, output);
  server.send(200, F("application/json"), output);
}

void handle_temperature_increase_command() {
  command_temperature_increase = millis();
  server.send(204, F("application/json"), F(""));
}

void handle_temperature_decrease_command() {
  command_temperature_decrease = millis();
  server.send(204, F("application/json"), F(""));
}

void handle_adjust_moisture_command() {
  command_adjust_moisture = millis();
  server.send(204, F("application/json"), F(""));
}

void handle_error() {
  server.send(404, F("application/json"), F("Page not found"));
}
