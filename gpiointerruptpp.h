/*
 * Fast GPIO Library for the Onion.io Omega2+ board
 * Copyright (C) 2019  Peter Buelow <goballstate at gmail>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __FASTIRQHANDLER_H__
#define __FASTIRQHANDLER_H__

#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <string>
#include <fstream>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>


#define GPIO_MAX_POLL   18

/**
 * \class GpioInterrupt
 * Provides a singleton for handling GPIO IRQ events for specified pins
 */
class GpioInterrupt {
public:
	static const int GPIO_IRQ_NONE = 0;
    static const int GPIO_IRQ_RISING = 1;
    static const int GPIO_IRQ_FALLING = 2;
    static const int GPIO_IRQ_BOTH = 3;
    static const int GPIO_DIRECTION_IN = 0;
    static const int GPIO_DIRECTION_OUT = 1;
    static const int GPIO_PIN_ACTIVE_LOW = 0;
    static const int GPIO_PIN_ACTIVE_HIGH = 1;
    
    typedef struct METADATA {
        int m_pin;
        int m_fd;
        int m_direction;
        int m_type;
        int m_state;
        bool m_isOpen;
        bool m_enabled;
        useconds_t m_time;
        unsigned long m_debounce;
        std::function<void(struct METADATA*)> m_callback;
    } MetaData;

    bool addPin(int pin, int irqtype = GPIO_IRQ_RISING, int pindirection = GPIO_DIRECTION_IN, int pinstate = GPIO_PIN_ACTIVE_HIGH, unsigned long debounce = 100);
    int removePin(int pin);
    bool setPinCallback(int pin, std::function<void(MetaData*)> cbk);
    bool setPinInterruptType(int pin, int type = GPIO_IRQ_RISING);
    bool setPinDirection(int pin, int dir = GPIO_DIRECTION_IN);
    bool setPinState(int pin, int state = GPIO_PIN_ACTIVE_HIGH);
    bool setPinDebounce(int pin, int debounce = 100);
    bool value(int pin, int &value);
    void setValue(int pin, int value);
    
    void start();
    void stop();
    
   	MetaData* getPinMetaData(int);

	static GpioInterrupt* instance()
	{
		static GpioInterrupt instance;
		return &instance;
	}

private:
    GpioInterrupt() { m_enabled = false; }
    ~GpioInterrupt()
    {
        for (std::pair<int, MetaData*> element : m_metadata) {
            unexportGpio(element.first);
        }
    }

    GpioInterrupt& operator=(GpioInterrupt const&) {};
    GpioInterrupt(GpioInterrupt&);

    void run();
    bool set(MetaData *pin);
    bool openPin(MetaData *pin);
    bool exportGpio(int pin);
    bool unexportGpio(int pin);
    bool checkDebounce(MetaData*);

    std::map<int, MetaData*> m_metadata;
    std::map<int, int> m_activeDescriptors;
    std::mutex m_mutex;
    std::thread *m_thread;
    bool m_enabled;
};
#endif

