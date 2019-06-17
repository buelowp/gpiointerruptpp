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

#include "gpiometadata.h"

GpioMetaData::GpioMetaData(int p, unsigned long b, GPIO_Pin_Direction d, GPIO_Irq_Type t) :
    m_pin(p), m_debounce(b), m_direction(d), m_type(t)
{
    m_time = 0;
    m_fd = 0;
    m_enabled = exportGpio();
}

GpioMetaData::~GpioMetaData()
{
    unexportGpio();
    close(m_fd);
}

bool GpioMetaData::isOpen()
{
    int v;
    
    if (!m_enabled) {
        onionPrint(ONION_SEVERITY_FATAL, "GPIO has not been sucessfully exported\n");
        return false;
    }
    
    if (!m_isOpen || m_fd == 0) {
        std::string path = "/sys/class/gpio/gpio" + std::to_string(m_pin) + "/value";
        
        if ((m_fd = open(path.c_str(), O_RDWR|O_NONBLOCK)) < 0) {
            onionPrint(ONION_SEVERITY_FATAL, "open: %s: %s(%d)\n", path.c_str(), strerror(errno), errno);
            m_isOpen = false;
        }
        else {
            m_isOpen = true;
            onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Opened %s with fd %d\n", __FUNCTION__, __LINE__, path.c_str(), m_fd);
        }
        value(v);
    }
    return m_isOpen;
}

bool GpioMetaData::value(int &value)
{
    char buf[4];
    
    if (!m_enabled)
        return false;
    
    memset(buf, '\0', 4);
    if (isOpen()) {
        if (read(m_fd, buf, 3) < 0) {
            onionPrint(ONION_SEVERITY_FATAL, "read: %s(%d)\n", strerror(errno), errno);
            return false;
        }
        try {
            value = std::stoi(buf);
        }
        catch (const std::exception& e) {
            onionPrint(ONION_SEVERITY_FATAL, "exception converting gpio read: %s\n", e.what());
            onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: read .%s.\n", __FUNCTION__, __LINE__, buf);
            return false;
        }
        lseek(m_fd, 0, SEEK_SET);
        return true;
    }
    return false;
}

GPIO_Irq_Type GpioMetaData::interruptType()
{
    return m_type;
}

bool GpioMetaData::setInterruptType(GPIO_Irq_Type type)
{
    int fd;
    char buf[128];
    char path[256];
    
    if (!m_enabled)
        return false;
    
    memset(path, '\0', 256);
    sprintf(path, "/sys/class/gpio/gpio%d/edge", m_pin);
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
		m_type = type;
        onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Set edge to %s, type is %d\n", __FUNCTION__, __LINE__, buf, type);
        close(fd);
    }
    else {
        onionPrint(ONION_SEVERITY_FATAL, "open: %s: %s(%d)\n", path, strerror(errno), errno);
        return false;
    }
    return false;
}

bool GpioMetaData::unexportGpio()
{
    int fd;
    char buf[128];
    
    if (!m_enabled)
        return false;
    
    setInterruptType(GPIO_IRQ_NONE);
    if ((fd = open("/sys/class/gpio/unexport", O_WRONLY)) > 0) {
		sprintf(buf, "%d", m_pin);
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

bool GpioMetaData::exportGpio()
{    
    int fd;
    char buf[128];
    
    memset(buf, '\0', 128);
    if ((fd = open("/sys/class/gpio/export", O_WRONLY)) > 0) {
		sprintf(buf, "%d", m_pin);
		onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Writing %s to /sys/class/gpio/export\n", __FUNCTION__, __LINE__, buf);
		if (write(fd, buf, strlen(buf) + 1) < 0) {
            if (errno == 16) {
                onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Pin %d has been exported, assuming control\n", __FUNCTION__, __LINE__, m_pin);
            }
            else {
                onionPrint(ONION_SEVERITY_FATAL, "write (%s): %s(%d)\n", buf, strerror(errno), errno);
                close(fd);
                return false;
            }
		}
    }
    else {
        onionPrint(ONION_SEVERITY_FATAL, "open: /sys/class/gpio/export: %s(%d)\n", strerror(errno), errno);
    }
    close(fd);
    return true;
}

