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

/* This file contains prototypes for helper functions. */

#ifndef LIBUSBSERIAL_COMMON_H
#define LIBUSBSERIAL_COMMON_H

#include "internal.h"

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#else
#include <endian.h>
#endif

void usbserial_common_init_bulk_read_transfer(
        struct libusb_transfer* transfer,
        unsigned char endpoint,
        struct usbserial_port* port);

int usbserial_common_cancel_read_transfer_sync(
        struct usbserial_port* port,
        struct libusb_transfer* transfer);

int usbserial_common_bulk_write(
        libusb_device_handle* usb_device_handle,
        unsigned char endpoint,
        const void* data,
        unsigned int bytes_count);

#ifdef __APPLE__
#define usbserial_common_convert_to_le(x) OSSwapHostToLittleInt32(x)
#else
#define usbserial_common_convert_to_le(x) htole32(x)
#endif

#endif // LIBUSBSERIAL_COMMON_H
