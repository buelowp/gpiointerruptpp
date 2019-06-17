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

#include <gpiopoll.h>


void GpioPoll::run()
{
    struct pollfd fds[GPIO_MAX_POLL];
    int index = 0;
    int pollrc = 0;
    
    onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Adding %d entries to the poll function\n", __FUNCTION__, __LINE__, m_metadata.size());
    for (std::map<int,GpioMetaData*>::iterator it = m_metadata.begin(); it != m_metadata.end(); ++it) {
        fds[index].fd = it->second->fd();
		fds[index].events = POLLPRI;
        m_activeDescriptors.insert(std::pair<int, int>(it->second->pin(), fds[index].fd));
        onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Added pollfd entry %d, fd %d\n", __FUNCTION__, __LINE__, index, fds[index].fd);
        index++;
    }

    while (m_enabled) {
        if ((pollrc = poll(fds, index, -1)) < 0) {
            if (errno == EINTR) {
                onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: EINTR\n", __FUNCTION__, __LINE__);
                continue;
            }
            else {
                onionPrint(ONION_SEVERITY_FATAL, "poll: %s(%d)\n", strerror(errno), errno);
                m_enabled = false;
                return;
            }
        }
        else if (pollrc > 0) {
            for (int i = 0; i < pollrc; i++) {
                if ((fds[i].revents & POLLHUP) || (fds[i].revents & POLLNVAL)) {
                    if (fds[i].revents & POLLHUP)
                        onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Got a HUP on fd %d\n", __FUNCTION__, __LINE__, fds[i].fd);
                    if (fds[i].revents & POLLNVAL)
                        onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Got a NVAL on fd %d\n", __FUNCTION__, __LINE__, fds[i].fd);
                }
                if (fds[i].revents & POLLPRI) {
                    int fd = fds[i].fd;
                    auto it = std::find_if(m_activeDescriptors.begin(), m_activeDescriptors.end(), [fd](const auto& mad) {return mad.second == fd; });
                    if (it != m_activeDescriptors.end()) {
                        auto mdit = m_metadata.find(it->first);
                        std::function<void(GpioMetaData*)> func = mdit->second->callback();
                        try {
                            if (checkDebounce(&(*mdit->second))) {
                                onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Executing callback for pin %d\n", __FUNCTION__, __LINE__, mdit->second->pin());
                                func(&(*mdit->second));
                            }
                        }
                        catch (const std::bad_function_call& e) {
                            onionPrint(ONION_SEVERITY_FATAL, "Unable to execute callback for pin %d\n", mdit->second->pin());
                            onionPrint(ONION_SEVERITY_FATAL, "exception: %s\n", e.what());
                        }
                    }
                }
                /*
                if (fds[i].revents & POLLERR) {
                    onionPrint(ONION_SEVERITY_DEBUG, "Error reported for pin\n");
//                    exit(0);
                }
                */
            }
        }
        else {
            int v;
            
            for (std::map<int,GpioMetaData*>::iterator it = m_metadata.begin(); it != m_metadata.end(); ++it) {
                if (it->second->value(v)) {
                    onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Got pin value %d for gpio %d on timeout \n", __FUNCTION__, __LINE__, v, it->second->pin());
                }
            }
            continue;   // Poll returned timeout to check if still enabled
        }
    }
}

bool GpioPoll::checkDebounce(GpioMetaData *pin)
{
    timeval timeNow;
    gettimeofday(&timeNow, NULL);
    suseconds_t nowMs = (timeNow.tv_sec * 1000L) + (timeNow.tv_usec / 1000L);

    suseconds_t timeDiff = nowMs - pin->time();

    // Don't want it if insufficient time has elapsed for debounce
    if (timeDiff < pin->debounce())
        return false;

    onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Setting interrupt time to %ld\n", __FUNCTION__, __LINE__, nowMs);
    pin->setTime(nowMs);
    return true;
}

GpioMetaData* GpioPoll::getPinMetaData(int pin)
{
	auto it = m_metadata.find(pin);
	if (it != m_metadata.end()){
		return &(*it->second);
	}

	onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Pin %d cannot be found\n", __FUNCTION__, __LINE__, pin);
	return nullptr;
}

int GpioPoll::clear(int pin)
{
	std::lock_guard<std::mutex> guard(m_mutex);
    auto it = m_metadata.find(pin);
    
    if (it != m_metadata.end()) {
        it->second->setInterruptType(GPIO_IRQ_NONE);
        it->second->unexportGpio();
    }
    m_metadata.erase(it);
    
    return m_metadata.size();
}

bool GpioPoll::set(GpioMetaData *pin)
{
	std::lock_guard<std::mutex> guard(m_mutex);

	if (pin->direction() != GPIO_DIRECTION_IN) {
		onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Pin is set as output, cannot continue\n", __FUNCTION__, __LINE__);
		return false;
	}

	if (m_metadata.find(pin->pin()) != m_metadata.end()) {
		onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Pin %d is already active, cancel first\n", __FUNCTION__, __LINE__, pin);
		return false;
	}

    if (!pin->isOpen()) {
        onionPrint(ONION_SEVERITY_FATAL, "Unable to open gpio value file: %s(%d)\n", strerror(errno), errno);
        return false;
    }

    m_metadata.insert(std::pair<int, GpioMetaData*>(pin->pin(), pin));
    
	return true;
}

void GpioPoll::start()
{
    m_thread = new std::thread(&GpioPoll::run, this);
    m_enabled = true;
    onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Enabling IRQ Handler\n", __FUNCTION__, __LINE__);
}

void GpioPoll::stop()
{
    m_enabled = false;
    onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Disabling IRQ Handler\n", __FUNCTION__, __LINE__);
}

