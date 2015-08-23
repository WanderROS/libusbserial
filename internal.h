/*
This file is part of libusbserial.

libusbserial is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 2 of the License.

libusbserial is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with libusbserial. If not, see <http://www.gnu.org/licenses/>.
*/

/* This file contains internally used prototypes. */

#ifndef LIBUSBSERIAL_INTERNAL_H
#define LIBUSBSERIAL_INTERNAL_H

#include "libusbserial.h"

#include "config.h"

#define USBSERIAL_UNUSED_VAR(x) ((void)x)

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.>
#else
#   include <pthread.h>
#endif

struct usbserial_port
{
    struct usbserial_driver* driver;
    libusb_device_handle* usb_device_handle;
    libusb_device* usb_device;
    struct libusb_device_descriptor usb_device_descriptor;
    unsigned int port_idx;
    usbserial_read_cb_fn read_cb;
    usbserial_error_cb_fn read_error_cb;
    void* cb_user_data;
    unsigned char read_buffer[READ_BUFFER_SIZE];
    void* driver_specific_data;
    int read_error_flag;
#ifdef _WIN32
    CRITICAL_SECTION mutex;
    HANDLE cancel_event;
#else
    pthread_mutex_t mutex;
    pthread_cond_t cancel_cond;
#endif
};

#endif // LIBUSBSERIAL_INTERNAL_H
