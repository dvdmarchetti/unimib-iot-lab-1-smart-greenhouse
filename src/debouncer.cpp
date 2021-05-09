#include <Arduino.h>
#include "../includes/debouncer.h"

Debouncer::Debouncer(
  uint8_t pin,
  button_callback callback,
  uint8_t mode,
  ulong delay
) : _pin(pin), _callback(callback), _mode(mode), _delay(delay) {
  pinMode(_pin, INPUT_PULLUP);

  _last_state = _state = digitalRead(_pin);
}

void Debouncer::loop() {
  if (digitalRead(_pin) == _state) {
    return;
  }

  ulong start = millis();
  while (millis() - start > _delay) {
    delay(1);
  }

  if (digitalRead(_pin) != _state) {
    _last_state = _state;
    _state = !_state;

    if (_mode == TRIGGER_BOTH
     || _mode == TRIGGER_FALLING && _state < _last_state
     || _mode == TRIGGER_RAISING && _state > _last_state) {
      this->_callback(_pin);
    }
  }
}
