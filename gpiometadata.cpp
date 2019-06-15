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
}

GpioMetaData::~GpioMetaData()
{
    close(m_fd);
}

bool GpioMetaData::isOpen()
{
    if (!m_isOpen || m_fd == 0) {
        std::string path = "/sys/class/gpio/gpio" + std::to_string(m_pin) + "/value";
        
        if ((m_fd = open(path.c_str(), O_RDWR|O_NONBLOCK)) < 0) {
            onionPrint(ONION_SEVERITY_FATAL, "open: %s(%d)\n", strerror(errno), errno);
            m_isOpen = false;
        }
        else {
            m_isOpen = true;
            onionPrint(ONION_SEVERITY_DEBUG, "%s:%d: Opened %s with fd %d\n", __FUNCTION__, __LINE__, path.c_str(), m_fd);
        }
    }
    return m_isOpen;
}
