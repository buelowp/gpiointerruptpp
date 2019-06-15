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
    struct pollfd fds[18];
    int index = 0;
    int pollrc = 0;
    
    onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Adding %d entries to the poll function\n", __FUNCTION__, __LINE__, m_metadata.size());
    for (std::map<int,GpioMetaData*>::iterator it = m_metadata.begin(); it != m_metadata.end(); ++it) {
        fds[index].fd = it->second->fd();
		fds[index].events = POLLPRI | POLLERR;
        m_activeDescriptors.insert(std::pair<int, int>(it->second->pin(), fds[index].fd));
        onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Added pollfd entry %d, fd %d\n", __FUNCTION__, __LINE__, index, fds[index].fd);
        index++;
    }

    while (m_enabled) {
        if ((pollrc = poll(fds, index, -1)) < 0) {
            if (errno == EINTR)
                continue;
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

                    lseek(fds[i].fd, 0, SEEK_SET);
                }
                if (fds[i].revents & POLLPRI) {
                    onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Event %d for index %d\n", __FUNCTION__, __LINE__, fds[i].revents, i);
                    int fd = fds[i].fd;
                    auto it = std::find_if(m_activeDescriptors.begin(), m_activeDescriptors.end(), [fd](const auto& mad) {return mad.second == fd; });
                    if (it != m_activeDescriptors.end()) {
                        auto mdit = m_metadata.find(it->first);
                        std::function<void(int)> func = mdit->second->callback();
                        try {
                            onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Executing callback for pin %d\n", __FUNCTION__, __LINE__, mdit->second->pin());
                            if (checkDebounce(&(*mdit->second)))
                                func(mdit->second->pin());
                        }
                        catch (const std::bad_function_call& e) {
                            onionPrint(ONION_SEVERITY_FATAL, "Unable to execute callback for pin %d\n", mdit->second->pin());
                            onionPrint(ONION_SEVERITY_FATAL, "exception: %s\n", e.what());
                        }
                    }
                    onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Seeking to 0 for fd %d\n", __FUNCTION__, __LINE__, fds[i].fd);
                    lseek(fds[i].fd, 0, SEEK_SET);
                }
                if (fds[i].revents & POLLERR) {
                    onionPrint(ONION_SEVERITY_FATAL, "Error reported for pin\n");
                    exit(0);
                }
            }
        }
        else {
            continue;   // Poll returned timeout to check if still enabled
        }
    }
}

bool GpioPoll::value(GpioMetaData *pin, int &value)
{
    char buf[4];
    
    memset(buf, '\0', 4);
    if (pin->isOpen()) {
        if (read(pin->fd(), buf, 3) < 0) {
            onionPrint(ONION_SEVERITY_FATAL, "read: %s(%d)\n", strerror(errno), errno);
            return false;
        }
        onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Read %s from pin %d, fd %d\n", __FUNCTION__, __LINE__, buf, pin->pin(), pin->fd());
        value = atoi(buf);
        return true;
    }
    return false;
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

	if (m_metadata.find(pin) == m_metadata.end()) {
		onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Pin %d is not active\n", __FUNCTION__, __LINE__, pin);
		return m_metadata.size();
	}

	gpioEdge(pin, GPIO_IRQ_NONE);
    unexportPin(pin);
    m_enabled = false;
    auto it = m_metadata.find(pin);
    if (it != m_metadata.end()) {
        m_metadata.erase(it);
    }
    
    return m_metadata.size();
}

bool GpioPoll::gpioEdge(int pin, GPIO_Irq_Type type)
{
    int fd;
    char buf[128];
    char path[256];
    
    memset(path, '\0', 256);
    sprintf(path, "/sys/class/gpio/gpio%d/edge", pin);
    if ((fd = open(path, O_WRONLY)) > 0) {
        switch (type) {
            case GPIO_IRQ_RISING:
                sprintf(buf, "rising");
                break;
            case GPIO_IRQ_FALLING:
                sprintf(buf, "falling");
                break;
            case GPIO_IRQ_BOTH:
                sprintf(buf, "both");
                break;
            case GPIO_IRQ_NONE:
                sprintf(buf, "none");
                break;
            default:
                sprintf(buf, "broke");
                break;
        }
		if (write(fd, buf, strlen(buf) + 1) < 0) {
			onionPrint(ONION_SEVERITY_FATAL, "Error writing %s to %s: %d\n", buf, path, errno);
			close(fd);
			return false;
		}
    }
    else {
        onionPrint(ONION_SEVERITY_FATAL, "open: %s(%d)\n", strerror(errno), errno);
    }
    onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Set edge to %s, type is %d\n", __FUNCTION__, __LINE__, buf, type);
    close(fd);
    return true;
}

bool GpioPoll::exportPin(int pin)
{
    int fd;
    char buf[128];
    
    memset(buf, '\0', 128);
    if ((fd = open("/sys/class/gpio/export", O_WRONLY)) > 0) {
		sprintf(buf, "%d", pin);
		onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Writing %s to /sys/class/gpio/export\n", __FUNCTION__, __LINE__, buf);
		if (write(fd, buf, strlen(buf) + 1) < 0) {
            if (errno == 16) {
                onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Pin %d has been exported, assuming control\n", __FUNCTION__, __LINE__, pin);
            }
            else {
                onionPrint(ONION_SEVERITY_FATAL, "write (%s): %s(%d)\n", buf, strerror(errno), errno);
                close(fd);
                return false;
            }
		}
    }
    close(fd);
    return true;
}

bool GpioPoll::unexportPin(int pin)
{
    int fd;
    char buf[128];
    
    auto fdit = m_activeDescriptors.find(pin);
    if (fdit != m_activeDescriptors.end()) {
        close(fdit->second);
    }
    gpioEdge(pin, GPIO_IRQ_NONE);
    if ((fd = open("/sys/class/gpio/unexport", O_WRONLY)) > 0) {
		sprintf(buf, "%d", pin);
		onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Writing %s to /sys/class/gpio/unexport\n", __FUNCTION__, __LINE__, buf);
		if (write(fd, buf, strlen(buf) + 1) < 0) {
			onionPrint(ONION_SEVERITY_FATAL, "write (/sys/class/gpio/unexport): %s(%d)\n", strerror(errno), errno);
			close(fd);
			return false;
		}
    }
    close(fd);
    return true;
}

bool GpioPoll::set(GpioMetaData *pin)
{
	int fd;

	std::lock_guard<std::mutex> guard(m_mutex);

	if (pin->direction() != GPIO_DIRECTION_IN) {
		onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Pin is set as output, cannot continue\n", __FUNCTION__, __LINE__);
		return false;
	}

	if (m_metadata.find(pin->pin()) != m_metadata.end()) {
		onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Pin %d is already active, cancel first\n", __FUNCTION__, __LINE__, pin);
		return false;
	}

	if (!exportPin(pin->pin()))
        return false;
    
    if (!gpioEdge(pin->pin(), pin->type())) {
        unexportPin(pin->pin());
        return false;
    }
    
    if (!pin->isOpen()) {
        onionPrint(ONION_SEVERITY_FATAL, "Unable to open gpio value file: %s(%d)\n", strerror(errno), errno);
        unexportPin(pin->pin());
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

