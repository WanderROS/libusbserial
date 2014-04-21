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

/* This file contains hardware / configuration specific
 * definitions / macros. */

#ifndef LIBUSBSERIAL_CONFIG_H
#define LIBUSBSERIAL_CONFIG_H

#define DEFAULT_CONTROL_TIMEOUT_MILLIS 1000
#define DEFAULT_READ_TIMEOUT_MILLIS 200

#define READ_BUFFER_SIZE 256

#if defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01001234)
#define HAS_LIBUSB_STRERROR 1
#else
#define HAS_LIBUSB_STRERROR 0
#endif

#define IS_BIG_ENDIAN ((0 == (1 >> 1)) ? 0 : 1)

#endif // LIBUSBSERIAL_CONFIG_H
