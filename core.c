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

/* This file contains the implementation of all functions declared in
 * libusbserial.h, except usbserial_get_error_str(). */

#include "libusbserial.h"

#include "config.h"
#include "driver.h"
#include "drivers.h"
#include "internal.h"

#include <assert.h>
#include <stdlib.h>

struct usbserial_driver drivers[3];

static struct usbserial_driver* find_driver_for_usb_device(
        uint16_t vendor_id,
        uint16_t product_id,
        uint8_t device_class,
        uint8_t device_subclass)
{
    unsigned int i;
    for (i = 0; i < sizeof(drivers); ++i)
    {
        if ((drivers[i].check_supported_by_vid_pid)
            && (drivers[i].check_supported_by_vid_pid(vendor_id, product_id)))
        {
            return &drivers[i];
        }
    }

    for (i = 0; i < sizeof(drivers); ++i)
    {
        if ((drivers[i].check_supported_by_class)
            && (drivers[i].check_supported_by_class(device_class, device_subclass)))
        {
            return &drivers[i];
        }
    }

    return NULL;
}

int usbserial_init()
{
    ftdi_driver_init(&drivers[0]);
    silabs_driver_init(&drivers[1]);
    cdc_driver_init(&drivers[2]);
    return 0;
}

int usbserial_deinit()
{
    return 0;
}

int usbserial_is_device_supported(
        uint16_t vendor_id,
        uint16_t product_id,
        uint8_t device_class,
        uint8_t device_subclass)
{
    return (find_driver_for_usb_device(
                vendor_id,
                product_id,
                device_class,
                device_subclass))
            ? 1 : 0;
}

const char* usbserial_get_device_short_name(
        uint16_t vendor_id,
        uint16_t product_id,
        uint8_t device_class,
        uint8_t device_subclass)
{
    struct usbserial_driver* driver
            = find_driver_for_usb_device(
                vendor_id,
                product_id,
                device_class,
                device_subclass);
    if (!driver) return NULL;
    return driver->get_device_short_name(
                vendor_id,
                product_id,
                device_class,
                device_subclass);
}

unsigned int usbserial_get_ports_count(
        uint16_t vendor_id,
        uint16_t product_id,
        uint8_t device_class,
        uint8_t device_subclass)
{
    struct usbserial_driver* driver
            = find_driver_for_usb_device(
                vendor_id,
                product_id,
                device_class,
                device_subclass);
    if (!driver) return 0;
    return driver->get_ports_count(vendor_id, product_id);
}

int usbserial_port_init(
        struct usbserial_port** out_port,
        libusb_device_handle* usb_device_handle,
        unsigned int port_idx,
        unsigned int baud,
        usbserial_read_cb_fn read_cb,
        usbserial_error_cb_fn read_error_cb,
        void* cb_user_data)
{
    struct usbserial_port* port = 0;
    int ret;
    struct usbserial_driver* driver;
    libusb_device* usb_device;
    struct libusb_device_descriptor usb_device_descriptor;
    int pthread_ret;
#ifndef WIN32
    int mutex_initialized = 0, cancel_cond_initialized = 0;
#endif

    if ((!out_port) || (!usb_device_handle)) return USBSERIAL_ERROR_INVALID_PARAMETER;

    *out_port = NULL;

    usb_device = libusb_get_device(usb_device_handle);
    if (!usb_device) return USBSERIAL_ERROR_NO_SUCH_DEVICE;

    ret = libusb_get_device_descriptor(usb_device, &usb_device_descriptor);
    if (0 != ret) goto fail;

    driver = find_driver_for_usb_device(
                usb_device_descriptor.idVendor,
                usb_device_descriptor.idProduct,
                usb_device_descriptor.bDeviceClass,
                usb_device_descriptor.bDeviceSubClass);
    if (!driver)
    {
        ret = USBSERIAL_ERROR_UNSUPPORTED_DEVICE;
        goto fail;
    }
    port = (struct usbserial_port*) malloc(sizeof(struct usbserial_port));
    if (!port)
    {
        ret = USBSERIAL_ERROR_RESOURCE_ALLOC_FAILED;
        goto fail;
    }

#ifdef WIN32
    InitializeCriticalSection(&port->mutex);
    EnterCriticalSection(&port->mutex);

    port->cancel_event = CreateEvent(NULL, TRUE, FALSE, NULL);
#else
    pthread_ret = pthread_mutex_init(&port->mutex, NULL);
    if (0 != pthread_ret)
    {
        ret = USBSERIAL_ERROR_RESOURCE_ALLOC_FAILED;
        goto fail;
    }
    mutex_initialized = 1;

    pthread_ret = pthread_mutex_lock(&port->mutex);
    if (0 != pthread_ret)
    {
        ret = USBSERIAL_ERROR_RESOURCE_ALLOC_FAILED;
        goto fail;
    }

    pthread_ret = pthread_cond_init(&port->cancel_cond, NULL);
    if (0 != pthread_ret)
    {
        ret = USBSERIAL_ERROR_RESOURCE_ALLOC_FAILED;
        goto fail;
    }
    cancel_cond_initialized = 1;
#endif

    port->driver = driver;
    port->usb_device_handle = usb_device_handle;
    port->usb_device = usb_device;
    port->usb_device_descriptor = usb_device_descriptor;
    port->port_idx = port_idx;
    port->read_cb = read_cb;
    port->read_error_cb = read_error_cb;
    port->cb_user_data = cb_user_data;
    port->driver_specific_data = NULL;
    port->read_error_flag = 0;

#ifdef WIN32
    LeaveCriticalSection(&port->mutex);
#else
    pthread_ret = pthread_mutex_unlock(&port->mutex);
    if (0 != pthread_ret)
    {
        ret = USBSERIAL_ERROR_RESOURCE_ALLOC_FAILED;
        goto fail;
    }
#endif

    ret = driver->port_init(port);
    if (0 != ret) goto fail;

    *out_port = port;

    ret = driver->port_set_baud_rate(port, baud);
    if (0 != ret) goto fail;

    return 0;

fail:
    assert(0 != ret);

    if (port)
    {
#ifdef WIN32
        if (port->cancel_event)
        {
            CloseHandle(port->cancel_event);
        }
#else
        if (mutex_initialized)
        {
            pthread_ret = pthread_mutex_destroy(&port->mutex);
            assert(0 == pthread_ret);
        }
        if (cancel_cond_initialized)
        {
            pthread_ret = pthread_cond_destroy(&port->cancel_cond);
            assert(0 == pthread_ret);
        }
#endif
        free(port);
    }

    return ret;
}

int usbserial_port_deinit(struct usbserial_port* port)
{
    int deinit_ret;

    if (!port) return USBSERIAL_ERROR_INVALID_PARAMETER;

    deinit_ret = port->driver->port_deinit(port);
    free(port);
    return deinit_ret;
}

int usbserial_start_reader(struct usbserial_port* port)
{
    if (!port) return USBSERIAL_ERROR_INVALID_PARAMETER;
    if (!port->read_cb) return USBSERIAL_ERROR_ILLEGAL_STATE;

#ifdef WIN32
    BOOL set_event_ret = ResetEvent(port->cancel_event);
    assert(set_event_ret);
#endif

    return port->driver->start_reader(port);
}

int usbserial_stop_reader(struct usbserial_port* port)
{
    if (!port) return USBSERIAL_ERROR_INVALID_PARAMETER;

    return port->driver->stop_reader(port);
}

int usbserial_write(
        struct usbserial_port* port,
        const void* data,
        unsigned int bytes_count)
{
    if (!port) return USBSERIAL_ERROR_INVALID_PARAMETER;

    return port->driver->write(port, data, bytes_count);
}

int usbserial_purge(
        struct usbserial_port* port,
        int purge_rx,
        int purge_tx)
{
    if ((!port) || (!purge_rx && !purge_tx))
    {
        return USBSERIAL_ERROR_INVALID_PARAMETER;
    }

    return port->driver->purge(port, purge_rx, purge_tx);
}
