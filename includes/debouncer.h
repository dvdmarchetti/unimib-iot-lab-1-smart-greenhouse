#ifndef DEBOUNCER_HPP_
#define DEBOUNCER_HPP_

#define DEBOUNCER_CLICK_DELAY 50
#define DEBOUNCER_LONG_PRESS_DELAY 50

#define TRIGGER_BOTH 0
#define TRIGGER_RAISING 1
#define TRIGGER_FALLING 2

typedef void (*button_callback)(uint8_t pin);

class Debouncer {
public:
  Debouncer(uint8_t pin, button_callback callback, uint8_t mode = TRIGGER_BOTH, ulong delay = DEBOUNCER_CLICK_DELAY);
  void loop();

  boolean is_pressed();
  boolean is_released();

private:
  uint8_t _pin;
  ulong _last_state;
  ulong _state;
  uint8_t _mode;
  ulong _delay;

  button_callback _callback;
};

#endif // DEBOUNCER_HPP_
