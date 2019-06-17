/*
 * Copyright (c) 2019 <copyright holder> <email>
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef GPIOMETADATA_H
#define GPIOMETADATA_H

#include <functional>
#include <cstdio>
#include <string>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <onion-debug.h>

enum GPIO_Irq_Type {
    GPIO_IRQ_NONE = 0,
    GPIO_IRQ_RISING = 1,
    GPIO_IRQ_FALLING = 2,
    GPIO_IRQ_BOTH = 3
};

enum GPIO_Pin_Direction {
	GPIO_DIRECTION_IN = 0,
	GPIO_DIRECTION_OUT = 1
};

enum GPIO_Pin_State {
	GPIO_PIN_LOW = 0,
	GPIO_PIN_HIGH = 1
};

/**
 * @todo write docs
 */
class GpioMetaData
{
public:
    GpioMetaData(int p, unsigned long b = 0, GPIO_Pin_Direction d = GPIO_DIRECTION_IN, GPIO_Irq_Type t = GPIO_IRQ_NONE);
    ~GpioMetaData();

    int pin() { return m_pin; }
    void setPin(int p) { m_pin = p; }
    
    GPIO_Pin_Direction direction() { return m_direction; }
    void setDirection(GPIO_Pin_Direction d) { m_direction = d; }
    
    GPIO_Irq_Type interruptType();
    bool setInterruptType(GPIO_Irq_Type);
    
    unsigned long debounce() { return m_debounce; }
    void setDebounce(unsigned long d) { m_debounce = d; }
    
    std::function<void(GpioMetaData*)> callback() { return m_callback; }
    void setCallback(std::function<void(GpioMetaData*)> c) { m_callback = c; }
    
    useconds_t time() { return m_time; }
    void setTime(useconds_t t) { m_time = t; }
    
    bool isOpen();
    int fd() { return m_fd; }
    
    bool value(int&);
    
    bool exportGpio();
    bool unexportGpio();
    
private:
    int m_pin;
    unsigned long m_debounce;
    GPIO_Pin_Direction m_direction;
    GPIO_Irq_Type m_type;
    std::function<void(GpioMetaData*)> m_callback;
    useconds_t m_time;
    int m_fd;
    bool m_isOpen;
    bool m_enabled;
};

#endif // GPIOMETADATA_H
