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

#include <gpiointerrupt.h>

bool GpioInterrupt::addPin(int pin, int irqtype, int pindirection, int pinstate, unsigned long debounce)
{
    MetaData *md = new MetaData();
    
    md->m_pin = pin;
    md->m_type = irqtype;
    md->m_direction = pindirection;
    md->m_state = pinstate;
    md->m_debounce = debounce;
    
    if (!exportGpio(pin)) {
        syslog(LOG_ERR, "Unable to export pin %d", pin);
        free(md);
        return false;
    }
    
    if (!setPinInterruptType(pin, irqtype)) {
        syslog(LOG_ERR, "Unable to set interrupt type for pin %d", pin);
        unexportGpio(pin);
        free(md);
        return false;
    }
    
    set(md);
    md->m_enabled = true;
}

bool GpioInterrupt::value(int pin, int &value)
{
    char buf[4];
    MetaData *md = nullptr;
    
    try {
        md = m_metadata.at(pin);
    }
    catch (std::out_of_range &e) {
        syslog(LOG_ERR, "Exception (%s) trying to insert callback for pin %d", e.what(), pin);
    }

    if (md) {
        memset(buf, '\0', 4);
        if (md->m_isOpen) {
            if (read(md->m_fd, buf, 3) < 0) {
                syslog(LOG_ERR, "read: %s(%d)", strerror(errno), errno);
                return false;
            }
            try {
                value = std::stoi(buf);
            }
            catch (const std::exception& e) {
                syslog(LOG_ERR, "exception converting gpio read: %s", e.what());
                syslog(LOG_NOTICE, "%s:%d: read .%s.", __FUNCTION__, __LINE__, buf);
                return false;
            }
            lseek(md->m_fd, 0, SEEK_SET);
            return true;
        }
    }
    return false;
}

void GpioInterrupt::setPinCallback(int pin, std::function<void(MetaData*)> cbk)
{
    MetaData *md = nullptr;
    
    try {
        md = m_metadata.at(pin);
    }
    catch (std::out_of_range &e) {
        syslog(LOG_ERR, "Exception (%s) trying to insert callback for pin %d", e.what(), pin);
    }
    
    md->m_callback = cbk;
}

void GpioInterrupt::setValue(int pin, bool value)
{
    MetaData *md = nullptr;
    
    try {
        md = m_metadata.at(pin);
    }
    catch (std::out_of_range &e) {
        syslog(LOG_ERR, "Exception (%s) trying to insert callback for pin %d", e.what(), pin);
    }
    
    if (md) {
        if (md->m_isOpen) {
            if (value)
                write(md->m_fd, "1", 1);
            else
                write(md->m_fd, "0", 1);
        }
    }
}

bool GpioInterrupt::exportGpio(int pin)
{    
    int fd;
    char buf[128];
    
    memset(buf, '\0', 128);
    if ((fd = open("/sys/class/gpio/export", O_WRONLY)) > 0) {
		sprintf(buf, "%d", pin);
		syslog(LOG_NOTICE, "%s:%d: Writing %s to /sys/class/gpio/export", __FUNCTION__, __LINE__, buf);
		if (write(fd, buf, strlen(buf) + 1) < 0) {
            if (errno == 16) {
                syslog(LOG_NOTICE, "%s:%d: Pin %d has been exported, assuming control", __FUNCTION__, __LINE__, pin);
            }
            else {
                syslog(LOG_ERR, "write (%s): %s(%d)", buf, strerror(errno), errno);
                close(fd);
                return false;
            }
		}
    }
    else {
        syslog(LOG_ERR, "open: /sys/class/gpio/export: %s(%d)", strerror(errno), errno);
    }
    close(fd);
    return true;
}

bool GpioInterrupt::unexportGpio(int pin)
{
    int fd;
    char buf[128];
    
    setPinInterruptType(pin, GPIO_IRQ_NONE);
    if ((fd = open("/sys/class/gpio/unexport", O_WRONLY)) > 0) {
		sprintf(buf, "%d", pin);
		syslog(LOG_NOTICE, "%s:%d: Writing %s to /sys/class/gpio/unexport", __FUNCTION__, __LINE__, buf);
		if (write(fd, buf, strlen(buf) + 1) < 0) {
			syslog(LOG_ERR, "write (/sys/class/gpio/unexport): %s(%d)", strerror(errno), errno);
			close(fd);
			return false;
		}
    }
    close(fd);
    return true;
}

bool GpioInterrupt::setPinInterruptType(int pin, int type)
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
			syslog(LOG_ERR, "Error writing %s to %s: %d", buf, path, errno);
			close(fd);
			return false;
		}

        syslog(LOG_NOTICE, "%s:%d: Set edge to %s", __FUNCTION__, __LINE__, buf);
        close(fd);
    }
    else {
        syslog(LOG_ERR, "open: %s: %s(%d)", path, strerror(errno), errno);
        return false;
    }
    return false;
}

void GpioInterrupt::run()
{
    struct epoll_event ev, events[GPIO_MAX_POLL];
    int epollfd;
    int index = 0;
    int nfds;
    
    if ((epollfd = epoll_create1(0)) < 0) {
        syslog(LOG_ERR, "Unable to create epoll instance: %s(%d)", strerror(errno), errno);
        return;
    }
    
    syslog(LOG_DEBUG, "%s:%d: Adding %d entries to the poll function", __FUNCTION__, __LINE__, m_metadata.size());
    for (std::map<int,MetaData*>::iterator it = m_metadata.begin(); it != m_metadata.end(); ++it) {
        ev.data.fd = it->second->m_fd;
        ev.events = EPOLLIN;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, it->second->m_fd, &ev) == -1) {
            syslog(LOG_ERR, "epoll_ctl: %s(%d)", strerror(errno), errno);
            continue;
        }
        else {
            m_activeDescriptors.insert(std::pair<int, int>(it->second->m_pin, it->second->m_fd));
            syslog(LOG_DEBUG, "%s:%d: Added pollfd entry %d, fd %d", __FUNCTION__, __LINE__, index, it->second->m_fd);
            index++;
        }
    }

    while (m_enabled) {
        if ((nfds = epoll_wait(epollfd, events, GPIO_MAX_POLL, -1)) < 0) {
            if (errno == EINTR) {
                syslog(LOG_DEBUG, "%s:%d: EINTR\n", __FUNCTION__, __LINE__);
                continue;
            }
            else {
                syslog(LOG_ERR, "poll: %s(%d)\n", strerror(errno), errno);
                m_enabled = false;
                return;
            }
        }
        else if (nfds > 0) {
            for (int i = 0; i < nfds; ++i) {
                int fd = events[i].data.fd;
                auto it = std::find_if(m_activeDescriptors.begin(), m_activeDescriptors.end(), [fd](const auto& mad) {return mad.second == fd; });
                if (it != m_activeDescriptors.end()) {
                    auto mdit = m_metadata.find(it->first);
                    std::function<void(MetaData*)> func = mdit->second->m_callback;
                    try {
                        if (checkDebounce(&(*mdit->second))) {
                            syslog(LOG_DEBUG, "%s:%d: Executing callback for pin %d", __FUNCTION__, __LINE__, mdit->second->m_pin);
                            func(&(*mdit->second));
                        }
                    }
                    catch (const std::bad_function_call& e) {
                        syslog(LOG_ERR, "Unable to execute callback for pin %d", mdit->second->m_pin);
                        syslog(LOG_ERR, "exception: %s", e.what());
                    }
                }
            }
        }
        else {
            int v;
            
            for (std::map<int,MetaData*>::iterator it = m_metadata.begin(); it != m_metadata.end(); ++it) {
                if (value(it->second->m_pin, v)) {
                    syslog(LOG_DEBUG, "%s:%d: Got pin value %d for gpio %d on timeout \n", __FUNCTION__, __LINE__, v, it->second->m_pin);
                }
            }
            continue;   // Poll returned timeout to check if still enabled
        }
    }
}

bool GpioInterrupt::checkDebounce(MetaData *pin)
{
    timeval timeNow;
    gettimeofday(&timeNow, NULL);
    suseconds_t nowMs = (timeNow.tv_sec * 1000L) + (timeNow.tv_usec / 1000L);

    suseconds_t timeDiff = nowMs - pin->m_time;

    // Don't want it if insufficient time has elapsed for debounce
    if (timeDiff < pin->m_debounce)
        return false;

    syslog(LOG_DEBUG, "%s:%d: Setting interrupt time to %ld", __FUNCTION__, __LINE__, nowMs);
    pin->m_time = nowMs;
    return true;
}

GpioInterrupt::MetaData* GpioInterrupt::getPinMetaData(int pin)
{
	auto it = m_metadata.find(pin);
	if (it != m_metadata.end()){
		return &(*it->second);
	}

	syslog(LOG_DEBUG, "%s:%d: Pin %d cannot be found", __FUNCTION__, __LINE__, pin);
	return nullptr;
}

int GpioInterrupt::removePin(int pin)
{
	std::lock_guard<std::mutex> guard(m_mutex);
    auto it = m_metadata.find(pin);
    
    if (it != m_metadata.end()) {
        MetaData *md = it->second;
        md->m_type = GPIO_IRQ_NONE;
        unexportGpio(pin);
        m_metadata.erase(it);
        free(md);
    }
    
    return m_metadata.size();
}

bool GpioInterrupt::set(MetaData *pin)
{
	std::lock_guard<std::mutex> guard(m_mutex);

	if (pin->m_direction != GPIO_DIRECTION_IN) {
		syslog(LOG_DEBUG, "%s:%d: Pin is set as output, cannot continue", __FUNCTION__, __LINE__);
		return false;
	}

	if (m_metadata.find(pin->m_pin) != m_metadata.end()) {
		syslog(LOG_ERR, "Pin %d is already active, cancel first", pin->m_pin);
		return false;
	}

    if (!openPin(pin)) {
        syslog(LOG_ERR, "Unable to open gpio value file: %s(%d)", strerror(errno), errno);
        return false;
    }

    m_metadata[pin->m_pin] = pin;
    
	return true;
}

void GpioInterrupt::start(int)
{
    m_thread = new std::thread(&GpioInterrupt::run, this);
    m_enabled = true;
    syslog(LOG_DEBUG, "%s:%d: Enabling IRQ Handler", __FUNCTION__, __LINE__);
}

void GpioInterrupt::stop(int)
{
    m_enabled = false;
    syslog(LOG_DEBUG, "%s:%d: Disabling IRQ Handler", __FUNCTION__, __LINE__);
}

bool GpioInterrupt::openPin(MetaData *pin)
{
    MetaData *md = nullptr;
    
    if (!pin->m_enabled) {
        syslog(LOG_ERR, "GPIO has not been sucessfully exported");
        return false;
    }
    
    if (!pin->m_isOpen || pin->m_fd == 0) {
        std::string path = "/sys/class/gpio/gpio" + std::to_string(pin->m_pin) + "/value";
        
        if ((pin->m_fd = open(path.c_str(), O_RDWR|O_NONBLOCK)) < 0) {
            syslog(LOG_ERR, "open: %s: %s(%d)\n", path.c_str(), strerror(errno), errno);
            pin->m_isOpen = false;
        }
        else {
            pin->m_isOpen = true;
            syslog(LOG_NOTICE, "%s:%d: Opened %s with fd %d", __FUNCTION__, __LINE__, path.c_str(), pin->m_fd);
        }
    }
    return pin->m_isOpen;
}

