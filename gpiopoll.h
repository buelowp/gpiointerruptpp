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
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <signal.h>
#include <onion-debug.h>

#include "gpiometadata.h"

#define GPIO_MAX_POLL   18

/**
 * \class GpioPoll
 * Provides a singleton for handling GPIO IRQ events for specified pins
 */
class GpioPoll {
public:
	bool set(GpioMetaData*);		// Pin and Rise/Fall, debounce set to 0
	int clear(int);					// Turn IRQ off for pin
	int interruptsActive() { return m_metadata.size(); }
	bool enabled() { return m_enabled; }
	void setEnabled(bool e) { m_enabled = e; }
	GpioMetaData* getPinMetaData(int);
	bool checkDebounce(GpioMetaData*);
	void start();
    void stop();
    
	static GpioPoll* instance()
	{
		static GpioPoll instance;
		return &instance;
	}

private:
	GpioPoll() { m_enabled = false; }
	~GpioPoll() {};
	GpioPoll& operator=(GpioPoll const&) {};
	GpioPoll(GpioPoll&);

    void run();

	std::map<int, GpioMetaData*> m_metadata;
    std::map<int, int> m_activeDescriptors;
	std::mutex m_mutex;
	std::thread *m_thread;
	bool m_enabled;
};
#endif

