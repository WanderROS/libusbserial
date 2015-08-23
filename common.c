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

/* This file contains helper functions. */

#include "common.h"

#include "config.h"
#include "driver.h"

#include <assert.h>
#include <string.h>

union uint32_bytes
{
    uint32_t uint32_value;
    unsigned char byte_values[4];
};

static void usbserial_common_default_read_transfer_callback(struct libusb_transfer* transfer)
{
    assert(transfer);

    struct usbserial_port* port = (struct usbserial_port*) transfer->user_data;
#ifndef _WIN32
    int pthread_ret;
#   ifdef NDEBUG
        USBSERIAL_UNUSED_VAR(pthread_ret);
#   endif
#endif
    assert(port);

#ifdef _WIN32
    EnterCriticalSection(&port->mutex);
#else
    pthread_ret = pthread_mutex_lock(&port->mutex);
    assert(0 == pthread_ret);
#endif

    if ((LIBUSB_TRANSFER_COMPLETED == transfer->status)
            || (LIBUSB_TRANSFER_TIMED_OUT == transfer->status))
    {
        unsigned int count = (unsigned int) transfer->actual_length;
        if (count > 0)
        {
            if (port->driver->read_data_postprocessor)
            {
                port->driver->read_data_postprocessor(port, transfer->buffer, &count);
            }
        }

        if (count > 0)
        {
            port->read_cb(
                        transfer->buffer,
                        count,
                        port->cb_user_data);
        }

        libusb_submit_transfer(transfer);
    }
    else if (LIBUSB_TRANSFER_CANCELLED == transfer->status)
    {
#ifdef _WIN32
        BOOL set_event_ret = SetEvent(port->cancel_event);
        assert(set_event_ret);
#else
        pthread_ret = pthread_cond_signal(&port->cancel_cond);
        assert(0 == pthread_ret);
#endif
    }
    else
    {
        port->read_error_flag = 1;
#ifdef _WIN32
        BOOL set_event_ret = SetEvent(port->cancel_event);
        assert(set_event_ret);
#endif
        if (port->read_error_cb) port->read_error_cb(transfer->status, port->cb_user_data);
    }

#ifdef _WIN32
    LeaveCriticalSection(&port->mutex);
#else
    pthread_ret = pthread_mutex_unlock(&port->mutex);
    assert(0 == pthread_ret);
#endif
}

void usbserial_common_init_bulk_read_transfer(
        struct libusb_transfer* transfer,
        unsigned char endpoint,
        struct usbserial_port* port)
{
    assert(transfer);
    assert(port);

    libusb_fill_bulk_transfer(
                transfer,
                port->usb_device_handle,
                endpoint,
                port->read_buffer,
                sizeof(port->read_buffer),
                usbserial_common_default_read_transfer_callback,
                port,
                DEFAULT_READ_TIMEOUT_MILLIS);
}

int usbserial_common_cancel_read_transfer_sync(
        struct usbserial_port* port,
        struct libusb_transfer* transfer)
{
    assert(port);
    assert(transfer);

#ifndef _WIN32
    int pthread_ret;
#   ifdef NDEBUG
        USBSERIAL_UNUSED_VAR(pthread_ret);
#   endif
#endif
    int cancel_ret = 0;
    int read_error_flag_set = 0;

    do
    {
#ifdef _WIN32
        EnterCriticalSection(&port->mutex);
#else
        pthread_ret = pthread_mutex_lock(&port->mutex);
        assert(0 == pthread_ret);
#endif

        if (port->read_error_flag)
        {
            read_error_flag_set = 1;
        }
        else
        {
            cancel_ret = libusb_cancel_transfer(transfer);
            if (0 == cancel_ret)
            {
#ifdef _WIN32
                DWORD wait_ret = WaitForSingleObject(port->cancel_event, INFINITE);
                assert(WAIT_OBJECT_0 == wait_ret);
#else
                pthread_ret = pthread_cond_wait(&port->cancel_cond, &port->mutex);
                assert(0 == pthread_ret);
#endif
            }
        }

#ifdef _WIN32
        LeaveCriticalSection(&port->mutex);
#else
        pthread_ret = pthread_mutex_unlock(&port->mutex);
        assert(0 == pthread_ret);
#endif
    } while ((0 == read_error_flag_set) && (LIBUSB_ERROR_NOT_FOUND == cancel_ret));

    return cancel_ret;
}

int usbserial_common_bulk_write(
        libusb_device_handle* usb_device_handle,
        unsigned char endpoint,
        const void* data,
        unsigned int bytes_count)
{
    assert(usb_device_handle);
    assert((0 == bytes_count) || data);

    int actual_length;
    int bulk_transfer_ret;

    if (0 == bytes_count) return 0;

    bulk_transfer_ret = libusb_bulk_transfer(
                    usb_device_handle,
                    endpoint,
                    (unsigned char*) data,
                    (int) bytes_count,
                    &actual_length,
                    0);
    if ((0 == bulk_transfer_ret) || (bulk_transfer_ret == LIBUSB_ERROR_TIMEOUT))
    {
        if (((actual_length) < 0)
                || (actual_length == ((int) bytes_count))) return bulk_transfer_ret;
        else return usbserial_common_bulk_write(
                    usb_device_handle,
                    endpoint,
                    ((unsigned char*) data) + actual_length,
                    bytes_count - actual_length);
    }
    else return bulk_transfer_ret;
}
