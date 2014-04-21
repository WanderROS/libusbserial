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

/* This is the public header of libusbserial. */

#ifndef LIBUSBSERIAL_H
#define LIBUSBSERIAL_H

#include <libusb.h>

struct usbserial_port;

typedef void (*usbserial_read_cb_fn)(
        void* data, unsigned int bytes_count,
        void* user_data);
typedef void (*usbserial_error_cb_fn)(
        enum libusb_transfer_status status,
        void* user_data);

/* Initialize / deinitialize this library.
 * Results are undefined, if usbserial functions are called
 * before usbserial_init() is called and after usbserial_deinit()
 * is called.
 * Returns zero on success, and an error code on failure.
 */
int usbserial_init();
int usbserial_deinit();

/* Returns a nonzero value, if a USB device is supported by one
 * of the libusbserial drivers. */
int usbserial_is_device_supported(
        uint16_t vendor_id,
        uint16_t product_id,
        uint8_t device_class,
        uint8_t device_subclass);
/* Get a short device name. It is guaranteed to return a valid C
 * string (not NULL), if the device is supported, see
 * usbserial_is_device_supported(). Otherwise, the results are
 * undefined. */
const char* usbserial_get_device_short_name(
        uint16_t vendor_id,
        uint16_t product_id,
        uint8_t device_class,
        uint8_t device_subclass);
/* Return the (expected) count of ports for a USB to Serial Adapter
 * device. If the device is not supported, see
 * usbserial_is_device_supported(), the results are undefined.
 * Never returns an error code, but might return zero if an error
 * occurs. */
unsigned int usbserial_get_ports_count(
        uint16_t vendor_id,
        uint16_t product_id,
        uint8_t device_class,
        uint8_t device_subclass);

/* Initialize a serial port instance.
 * Returns zero on success, and an error code on failure.
 * The usbserial_port instance object is stored in *out_port.
 * It is guaranteed that *out_port is NULL if an error occured
 * (non-zero return value) and that *out_port is not NULL on
 * success (zero return value).
 * Results are undefined, if port_idx >= 0 usbserial_get_ports_count()
 * return value.
 * read_cb must not be NULL, unless no read operations are performed
 * (usbserial_start_reader() is not called afterwards).
 * read_error_cb can be NULL, then no read error notifications are sent. */
int usbserial_port_init(
        struct usbserial_port** out_port,
        libusb_device_handle* usb_device_handle,
        unsigned int port_idx,
        unsigned int baud,
        usbserial_read_cb_fn read_cb,
        usbserial_error_cb_fn read_error_cb,
        void* cb_user_data);
/* Deinitialize / invalidate a serial port instance.
 * Returns zero on success, and an error code on failure.
 * Results are undefined, if usbserial_stop_reader() was
 * not called before, unless usbserial_start_reader() was
 * not called. */
int usbserial_port_deinit(struct usbserial_port* port);

/* Set the baud rate for a serial port instance.
 * Returns zero on success, and an error code on failure. */
int usbserial_port_set_baud_rate(
        struct usbserial_port* port,
        unsigned int baud);

/* Start reading from the port.
 * Returns zero on success, and an error code on failure. */
int usbserial_start_reader(struct usbserial_port* port);
/* Stop reading from the port.
 * Returns zero on success, and an error code on failure.
 * This function blocks until a pending read_cb call is finished.
 * It is guaranteed that read_cb is not called again after this
 * function has returned.
 * This function must not be called from the same thread in which
 * the libusb events are handled! */
int usbserial_stop_reader(struct usbserial_port* port);

/* Synchronously write data to a port.
 * Returns zero on success, and an error code on failure. */
int usbserial_write(
        struct usbserial_port* port,
        const void* data,
        unsigned int bytes_count);
/* Purge the hardware read (rx) / (tx) buffer.
 * Returns zero on success, and an error code on failure.
 * Not supported by all drivers / devices, returns
 * USBSERIAL_ERROR_UNSUPPORTED_OPERATION in this case. */
int usbserial_purge(
        struct usbserial_port* port,
        int purge_rx,
        int purge_tx);

/* Get the string represenation for an usbserial error code,
 * which can be a libusb error code.
 * Returns NULL, if 0 == error_code and is guaranteed to return
 * an non-NULL C string if 0 != error_code */
const char* usbserial_get_error_str(int error_code);

#define DEFINE_USBSERIAL_ERROR(num) (-1000000 - num)

#define USBSERIAL_ERROR_UNSUPPORTED_OPERATION DEFINE_USBSERIAL_ERROR(0)
#define USBSERIAL_ERROR_ILLEGAL_STATE DEFINE_USBSERIAL_ERROR(1)
#define USBSERIAL_ERROR_INVALID_PARAMETER DEFINE_USBSERIAL_ERROR(2)
#define USBSERIAL_ERROR_RESOURCE_ALLOC_FAILED DEFINE_USBSERIAL_ERROR(3)
#define USBSERIAL_ERROR_NO_SUCH_DEVICE DEFINE_USBSERIAL_ERROR(4)
#define USBSERIAL_ERROR_UNSUPPORTED_DEVICE DEFINE_USBSERIAL_ERROR(5)
#define USBSERIAL_ERROR_UNSUPPORTED_BAUD_RATE DEFINE_USBSERIAL_ERROR(6)
#define USBSERIAL_ERROR_INVALID_PORT_IDX DEFINE_USBSERIAL_ERROR(7)
#define USBSERIAL_ERROR_CTRL_CMD_FAILED DEFINE_USBSERIAL_ERROR(8)

#endif // LIBUSBSERIAL_H
