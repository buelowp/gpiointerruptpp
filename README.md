# C++ library for doing GPIO interrupt activities

The goal here is to create a general purpose C++ class that can handle GPIO in Linux. It's mostly a dumb wrapper on C functions for handling GPIO open/close/read/write but also does threaded polling for edge triggered events with C++ callbacks to either standard functions, or class member functions using std::bind.

The design makes use of the singleton design pattern to allow for a single instance which can handle a set of GPIO pins independently. Each pin can create a unique setup, and then a single thread will watch for events on all the pins in that setup. The thread runs epoll in an edge triggered setup, and each pin will default to a rising edge even if not designated.

Using it is relatively simple. Create a new pin in the instance, and then you can read/write the pin, or set it up to report changes on rising/falling/both edges. 

## Usage

Create a new pin
```
GpioInterrupt::instance()->addPin(pin);
```

The addPin() method sets defaults for the following values

* Interrupt type, RISING, FALLING, BOTH, or NONE, default is RISING
* Direction, either IN or OUT, default is IN
* Pin state, either HIGH or LOW, default is HIGH
* Debounce time, default is 100ms

These are C++ method defaults, so you must set them all explicitly if you want to set the debounce. However, you can always set each of these individually after the pin has been added. 

```
GpioInterrupt::instance()->addPin(pin, GpioInterrupt::GPIO_IRQ_FALLING, GpioInterrupt::GPIO_DIRECTION_OUT, GpioInterrupt::GPIO_PIN_LOW, 500);
```

The possible values to pass are as follows

Interrupts

* GPIO_IRQ_RISING
* GPIO_IRQ_FALLING
* GPIO_IRQ_BOTH
* GPIO_IRQ_NONE

Pin Direction

* GPIO_DIRECTION_IN
* GPIO_DIRECTION_OUT

Pin State

* GPIO_PIN_LOW
* GPIO_PIN_HIGH

Read the value of a pin
```
int value;
if (GpioInterrupt::instance()->value(pin, value))
    std::cout << "Pin value is " << value << std::endl;
````

Set the value of a pin to on
```
GpioInterrupt::instance()->setValue(pin, 1);
```

Set a callback for an interrupt event on the pin and start the event loop. Interrupts are only edge triggered, and the default edge is RISING.
```
GpioInterrupt::instance()->setPinCallback(pin, std::function<void()>);
GpioInterrupt::instance()->start();
```

