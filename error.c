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

/* This file contains the implementation of usbserial_get_error_str(). */

#include "libusbserial.h"

static const char* ERROR_MSG_UNSUPPORTED_OPERATION = "Unsupported operation";
static const char* ERROR_MSG_ILLEGAL_STATE = "Illegal state";
static const char* ERROR_MSG_INVALID_PARAMETER = "Invalid parameter";
static const char* ERROR_MSG_RESOURCE_ALLOC_FAILED = "Resource allocation failed";
static const char* ERROR_MSG_NO_SUCH_DEVICE = "No such device";
static const char* ERROR_MSG_UNSUPPORTED_DEVICE = "Unsupported device";
static const char* ERROR_MSG_UNSUPPORTED_BAUD_RATE = "Unsupported baud rate";
static const char* ERROR_MSG_INVALID_PORT_IDX = "Invalid port index";
static const char* ERROR_MSG_CTRL_CMD_FAILED = "Control command failed";

static const char* ERROR_MSG_UNKNOWN = "Unknown error";

#if !HAS_LIBUSB_STRERROR
static const char* ERROR_MSG_LIBUSB = "libusb error";
#endif

const char* usbserial_get_error_str(int error_code)
{
    if (0 == error_code) return NULL;

    if ((-1 >= error_code) && (-99 <= error_code))
    {
#ifdef HAS_LIBUSB_STRERROR
        return libusb_strerror((enum libusb_error) error_code);
#else
        return ERROR_MSG_LIBUSB;
#endif
    }
    else
    {
        switch (error_code)
        {
        case USBSERIAL_ERROR_UNSUPPORTED_OPERATION:
            return ERROR_MSG_UNSUPPORTED_OPERATION;

        case USBSERIAL_ERROR_ILLEGAL_STATE:
            return ERROR_MSG_ILLEGAL_STATE;

        case USBSERIAL_ERROR_INVALID_PARAMETER:
            return ERROR_MSG_INVALID_PARAMETER;

        case USBSERIAL_ERROR_RESOURCE_ALLOC_FAILED:
            return ERROR_MSG_RESOURCE_ALLOC_FAILED;

        case USBSERIAL_ERROR_NO_SUCH_DEVICE:
            return ERROR_MSG_NO_SUCH_DEVICE;

        case USBSERIAL_ERROR_UNSUPPORTED_DEVICE:
            return ERROR_MSG_UNSUPPORTED_DEVICE;

        case USBSERIAL_ERROR_UNSUPPORTED_BAUD_RATE:
            return ERROR_MSG_UNSUPPORTED_BAUD_RATE;

        case USBSERIAL_ERROR_INVALID_PORT_IDX:
            return ERROR_MSG_INVALID_PORT_IDX;

        case USBSERIAL_ERROR_CTRL_CMD_FAILED:
            return ERROR_MSG_CTRL_CMD_FAILED;

        default:
            return ERROR_MSG_UNKNOWN;
        }
    }
}
