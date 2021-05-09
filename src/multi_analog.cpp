#include "../includes/multi_analog.h"

MultiAnalog::MultiAnalog(
  uint8_t analog_pin
) : _analog_pin(analog_pin) {
  //
}

void MultiAnalog::add(analog_sensor sensor) {
  _sensors.push_back(sensor);
}

uint MultiAnalog::read(analog_sensor sensor) {
  this->disable_all();

  this->enable(sensor);
  delay(5);
  uint value = analogRead(_analog_pin);

  this->disable_all();
  return value;
}

void MultiAnalog::disable_all() {
  for (analog_sensor sensor : _sensors) {
    pinMode(sensor.vcc, INPUT);
    pinMode(sensor.gnd, INPUT);
  }
}

void MultiAnalog::enable(analog_sensor sensor) {
  pinMode(sensor.vcc, OUTPUT);
  pinMode(sensor.gnd, OUTPUT);

  digitalWrite(sensor.vcc, HIGH);
  digitalWrite(sensor.gnd, LOW);
}
