#ifndef MULTI_ANALOG_HPP_
#define MULTI_ANALOG_HPP_

#include <Arduino.h>

struct analog_sensor {
  uint8_t vcc, gnd;
  analog_sensor(uint8_t e, uint8_t g) : vcc(e), gnd(g) {}
};

class MultiAnalog {
public:
  MultiAnalog(uint8_t analog_pin);

  void add(analog_sensor sensor);
  uint read(analog_sensor sensor);
  void disable_all();

private:
  uint8_t _analog_pin;
  std::vector<analog_sensor> _sensors;

  void enable(analog_sensor sensor);
};

#endif // MULTI_ANALOG_HPP_
