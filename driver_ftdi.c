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

/* This file contains the implementation of a driver for FTDI devices. */

#include "common.h"
#include "driver.h"

#include <assert.h>
#include <stdlib.h>

#define FTDI_VENDOR_ID 0x0403

#define FTDI_PRODUCT_ID_FT232R 0x6001
#define FTDI_PRODUCT_ID_FT2232 0x6010
#define FTDI_PRODUCT_ID_FT4232H 0x6011
#define FTDI_PRODUCT_ID_FT231X 0x6015

#define FTDI_SIO_REQUEST_RESET 0
#define FTDI_SIO_REQUEST_SET_BAUD_RATE 3
#define FTDI_SIO_REQUEST_SET_LINE_CONFIG 4

#define FTDI_SIO_RESET 0
#define FTDI_SIO_RESET_PURGE_RX 1
#define FTDI_SIO_RESET_PURGE_TX 2

#define FTDI_DEVICE_IN_REQTYPE LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | 0x80
#define FTDI_DEVICE_OUT_REQTYPE LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE

#define FTDI_MODEM_STATUS_BYTES_COUNT 2

#define FTDI_PARITY_LINE_CONFIG_VALUE_SHIFT 8
#define FTDI_STOP_BITS_LINE_CONFIG_VALUE_SHIFT 11

#define FTDI_PARITY_NONE_LINE_CONFIG_VALUE (0x00 << FTDI_PARITY_LINE_CONFIG_VALUE_SHIFT)
#define FTDI_PARITY_ODD_LINE_CONFIG_VALUE (0x01 << FTDI_PARITY_LINE_CONFIG_VALUE_SHIFT)
#define FTDI_PARITY_EVEN_LINE_CONFIG_VALUE (0x02 << FTDI_PARITY_LINE_CONFIG_VALUE_SHIFT)
#define FTDI_PARITY_MARK_LINE_CONFIG_VALUE (0x03 << FTDI_PARITY_LINE_CONFIG_VALUE_SHIFT)
#define FTDI_PARITY_SPACE_LINE_CONFIG_VALUE (0x04 << FTDI_PARITY_LINE_CONFIG_VALUE_SHIFT)

#define FTDI_STOP_BITS_1_LINE_CONFIG_VALUE (0x00 << FTDI_STOP_BITS_LINE_CONFIG_VALUE_SHIFT)
#define FTDI_STOP_BITS_1_5_LINE_CONFIG_VALUE (0x01 << FTDI_STOP_BITS_LINE_CONFIG_VALUE_SHIFT)
#define FTDI_STOP_BITS_2_LINE_CONFIG_VALUE (0x02 << FTDI_STOP_BITS_LINE_CONFIG_VALUE_SHIFT)

#define FTDI_READ_ENDPOINT(i) (0x81 + 2 * i)
#define FTDI_WRITE_ENDPOINT(i) (0x02 + 2 * i)

static const char* FTDI_DEVICE_NAME_FT232R = "FT232R";
static const char* FTDI_DEVICE_NAME_FT2232 = "FT2232";
static const char* FTDI_DEVICE_NAME_FT4232H = "FT4232H";
static const char* FTDI_DEVICE_NAME_FT231X = "FT231X";
static const char* FTDI_DEVICE_NAME_GENERIC = "FTDI";

enum ftdi_device_type
{
    FTDI_DEVICE_TYPE_4232H,
    FTDI_DEVICE_TYPE_2232,
    FTDI_DEVICE_TYPE_OTHER
};

struct ftdi_baud_data
{
    unsigned int best_baud; uint16_t index; uint16_t value;
};

struct ftdi_port_data
{
    struct libusb_transfer* transfer;
    enum ftdi_device_type device_type;
    uint16_t control_idx;
};

static struct ftdi_baud_data convert_baudrate(
        unsigned int baud,
        enum ftdi_device_type device_type,
        uint16_t control_idx)
{
    /* Algorithm derived from usbserial-for-android, which
     * borrowed the code from libftdi */

    int divisor = 24000000 / baud;
    int best_divisor = 0;
    unsigned int best_baudrate = 0;
    int best_baud_diff = 0;
    int frac_code[] = { 0, 3, 2, 4, 1, 5, 6, 7 };

    {
        int i = 0;
        for (i = 0; i < 2; i++)
        {
            int try_divisor = divisor + i;
            int baud_estimate;
            int baud_diff;

            if (try_divisor <= 8)
            {
                /*  Round up to minimum supported divisor */
                try_divisor = 8;
            }
            else if (try_divisor < 12)
            {
                try_divisor = 12;
            }
            else if (divisor < 16)
            {
                try_divisor = 16;
            }
            else
            {
                if (try_divisor > 0x1FFFF)
                {
                    try_divisor = 0x1FFFF;
                }
            }

            /* Get estimated baud rate (to nearest integer) */
            baud_estimate = (24000000 + (try_divisor / 2)) / try_divisor;

            /* Get absolute difference from requested baud rate */
            if (baud_estimate < baud)
            {
                baud_diff = baud - baud_estimate;
            }
            else
            {
                baud_diff = baud_estimate - baud;
            }

            if ((0 == i) || (baud_diff < best_baud_diff))
            {
                /* Closest to requested baud rate so far */
                best_divisor = try_divisor;
                best_baudrate = baud_estimate;
                best_baud_diff = baud_diff;
                if (0 == baud_diff)
                {
                    /* Spot on! No point trying */
                    break;
                }
            }
        }
    }

    /* Encode the best divisor value */
    long encoded_divisor = (best_divisor >> 3) | (frac_code[best_divisor & 7] << 14);
    /* Deal with special cases for encoded value */
    if (1 == encoded_divisor)
    {
        encoded_divisor = 0; /* 3000000 baud */
    }
    else if (0x4001 == encoded_divisor)
    {
        encoded_divisor = 1; /* 2000000 baud (BM only) */
    }

    /* Split into "value" and "index" values */
    long value = encoded_divisor & 0xFFFF;
    long index;
    if ((FTDI_DEVICE_TYPE_2232 == device_type)
            || (FTDI_DEVICE_TYPE_4232H == device_type))
    {
        index = (encoded_divisor >> 8) & 0xffff;
        index &= 0xFF00;
        index |= control_idx;
    }
    else
    {
        index = (encoded_divisor >> 16) & 0xffff;
    }

    struct ftdi_baud_data ret = { best_baudrate, index, value };
    return ret;
}

static int ftdi_reset_ctrl(
        struct usbserial_port* port,
        uint16_t sio,
        uint16_t control_idx)
{
    assert(port);

    return libusb_control_transfer(
                port->usb_device_handle,
                FTDI_DEVICE_OUT_REQTYPE,
                FTDI_SIO_REQUEST_RESET,
                sio,
                control_idx,
                NULL,
                0,
                DEFAULT_CONTROL_TIMEOUT_MILLIS);
}

static int ftdi_check_supported_by_vid_pid(
        uint16_t vendor_id,
        uint16_t product_id)
{
    if (FTDI_VENDOR_ID == vendor_id)
    {
        switch (product_id)
        {
        case FTDI_PRODUCT_ID_FT232R:
        case FTDI_PRODUCT_ID_FT2232:
        case FTDI_PRODUCT_ID_FT4232H:
        case FTDI_PRODUCT_ID_FT231X:
            return 1;

        default:
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

static const char* ftdi_get_device_short_name(
        uint16_t vendor_id,
        uint16_t product_id,
        uint8_t device_class,
        uint8_t device_subclass)
{
    assert(FTDI_VENDOR_ID == vendor_id);

    switch (product_id)
    {
    case FTDI_PRODUCT_ID_FT232R:
        return FTDI_DEVICE_NAME_FT232R;
    case FTDI_PRODUCT_ID_FT2232:
        return FTDI_DEVICE_NAME_FT2232;
    case FTDI_PRODUCT_ID_FT4232H:
        return FTDI_DEVICE_NAME_FT4232H;
    case FTDI_PRODUCT_ID_FT231X:
        return FTDI_DEVICE_NAME_FT231X;

    default:
        return FTDI_DEVICE_NAME_GENERIC;
    }
}

static unsigned int ftdi_get_ports_count(uint16_t vendor_id, uint16_t product_id)
{
    assert(FTDI_VENDOR_ID == vendor_id);

    switch (product_id)
    {
    case FTDI_PRODUCT_ID_FT232R:
    case FTDI_PRODUCT_ID_FT231X:
        return 1;
    case FTDI_PRODUCT_ID_FT2232:
        return 2;
    case FTDI_PRODUCT_ID_FT4232H:
        return 4;

    default:
        return 0;
    }
}

static int ftdi_port_init(struct usbserial_port* port)
{
    struct ftdi_port_data* port_data;
    int ret;
    enum ftdi_device_type device_type;
    uint16_t control_idx;

    assert(port);

    switch (port->usb_device_descriptor.idProduct)
    {
    case FTDI_PRODUCT_ID_FT2232:
        device_type = FTDI_DEVICE_TYPE_2232;
        control_idx = port->port_idx + 1;
        break;
    case FTDI_PRODUCT_ID_FT4232H:
        device_type = FTDI_DEVICE_TYPE_4232H;
        control_idx = port->port_idx + 1;
        break;

    default:
        device_type = FTDI_DEVICE_TYPE_OTHER;
        if (0 != port->port_idx) return USBSERIAL_ERROR_INVALID_PORT_IDX;
        control_idx = 0;
    }

    ret = libusb_claim_interface(port->usb_device_handle, port->port_idx);
    if (0 != ret) return ret;

    ret = ftdi_reset_ctrl(port, FTDI_SIO_RESET, control_idx);
    if (0 != ret) goto relase_if_and_return;

    port_data = (struct ftdi_port_data*) malloc(sizeof(struct ftdi_port_data));
    if (!port_data)
    {
        ret = USBSERIAL_ERROR_RESOURCE_ALLOC_FAILED;
        goto relase_if_and_return;
    }
    port_data->transfer = NULL;
    port_data->device_type = device_type;
    port_data->control_idx = control_idx;

    port->driver_specific_data = port_data;

    return 0;

relase_if_and_return:
    assert(ret != 0);
    libusb_release_interface(port->usb_device_handle, port->port_idx);
    return ret;
}

static int ftdi_port_deinit(struct usbserial_port* port)
{
    assert(port);

    if (!port->driver_specific_data) return USBSERIAL_ERROR_ILLEGAL_STATE;

    free(port->driver_specific_data);
    port->driver_specific_data = NULL;

    return libusb_release_interface(
                port->usb_device_handle,
                port->port_idx);
}

static int ftdi_port_set_line_config(
        struct usbserial_port* port,
        const struct usbserial_line_config* line_config)
{
    assert(port);
    assert(line_config);

    struct ftdi_port_data* port_data;
    uint16_t ftdi_line_config_value;
    struct ftdi_baud_data converted_baudrate;
    int ret;

    if (!port->driver_specific_data) return USBSERIAL_ERROR_ILLEGAL_STATE;

    port_data = (struct ftdi_port_data*) port->driver_specific_data;

    converted_baudrate
            = convert_baudrate(
                line_config->baud,
                port_data->device_type,
                port_data->control_idx);
    if (line_config->baud != converted_baudrate.best_baud)
    {
        return USBSERIAL_ERROR_UNSUPPORTED_BAUD_RATE;
    }

    ftdi_line_config_value = line_config->data_bits;

    switch (line_config->stop_bits)
    {
    case USBSERIAL_STOPBITS_1:
        ftdi_line_config_value |= FTDI_STOP_BITS_1_LINE_CONFIG_VALUE;
        break;
    case USBSERIAL_STOPBITS_1_5:
        ftdi_line_config_value |= FTDI_STOP_BITS_1_5_LINE_CONFIG_VALUE;
        break;
    case USBSERIAL_STOPBITS_2:
        ftdi_line_config_value |= FTDI_STOP_BITS_2_LINE_CONFIG_VALUE;
        break;

    default:
        return USBSERIAL_ERROR_INVALID_PARAMETER;
    }

    switch (line_config->parity)
    {
    case USBSERIAL_PARITY_NONE:
        ftdi_line_config_value |= FTDI_PARITY_NONE_LINE_CONFIG_VALUE;
        break;
    case USBSERIAL_PARITY_ODD:
        ftdi_line_config_value |= FTDI_PARITY_NONE_LINE_CONFIG_VALUE;
        break;
    case USBSERIAL_PARITY_EVEN:
        ftdi_line_config_value |= FTDI_PARITY_NONE_LINE_CONFIG_VALUE;
        break;
    case USBSERIAL_PARITY_MARK:
        ftdi_line_config_value |= FTDI_PARITY_NONE_LINE_CONFIG_VALUE;
        break;
    case USBSERIAL_PARITY_SPACE:
        ftdi_line_config_value |= FTDI_PARITY_NONE_LINE_CONFIG_VALUE;
        break;

    default:
        return USBSERIAL_ERROR_INVALID_PARAMETER;
    }

    ret = libusb_control_transfer(
                port->usb_device_handle,
                FTDI_DEVICE_OUT_REQTYPE,
                FTDI_SIO_REQUEST_SET_BAUD_RATE,
                converted_baudrate.value,
                converted_baudrate.index,
                NULL,
                0,
                DEFAULT_CONTROL_TIMEOUT_MILLIS);
    if (0 != ret) return ret;

    ret = libusb_control_transfer(
                port->usb_device_handle,
                FTDI_DEVICE_OUT_REQTYPE,
                FTDI_SIO_REQUEST_SET_LINE_CONFIG,
                ftdi_line_config_value,
                port_data->control_idx,
                NULL,
                0,
                DEFAULT_CONTROL_TIMEOUT_MILLIS);
    if (0 != ret) return ret;
}

static int ftdi_start_reader(struct usbserial_port* port)
{
    struct libusb_transfer* transfer;
    struct ftdi_port_data* port_data;
    int submit_ret;

    assert(port);
    assert(port->read_cb);

    if (!port->driver_specific_data) return USBSERIAL_ERROR_ILLEGAL_STATE;

    port_data = (struct ftdi_port_data*) port->driver_specific_data;
    if (port_data->transfer) return USBSERIAL_ERROR_ILLEGAL_STATE;

    transfer = libusb_alloc_transfer(0);
    if (!transfer) return USBSERIAL_ERROR_RESOURCE_ALLOC_FAILED;

    usbserial_common_init_bulk_read_transfer(
                transfer,
                FTDI_READ_ENDPOINT(port->port_idx),
                port);
    submit_ret = libusb_submit_transfer(transfer);
    if (0 != submit_ret)
    {
        libusb_free_transfer(transfer);
        return submit_ret;
    }

    port_data->transfer = transfer;

    return 0;
}

static int ftdi_stop_reader(struct usbserial_port* port)
{
    assert(port);

    struct ftdi_port_data* port_data;
    int ret;

    port_data = (struct ftdi_port_data*) port->driver_specific_data;
    if ((!port_data) || (!port_data->transfer))
    {
        return USBSERIAL_ERROR_ILLEGAL_STATE;
    }
    ret = usbserial_common_cancel_read_transfer_sync(port, port_data->transfer);

    libusb_free_transfer(port_data->transfer);
    port_data->transfer = NULL;

    return ret;
}

static int ftdi_write(
        struct usbserial_port* port,
        const void* data,
        unsigned int bytes_count)
{
    assert(port);

    if (!port->driver_specific_data) return USBSERIAL_ERROR_ILLEGAL_STATE;

    return usbserial_common_bulk_write(
                port->usb_device_handle,
                FTDI_WRITE_ENDPOINT(port->port_idx),
                data,
                bytes_count);
}

static int ftdi_purge(
        struct usbserial_port* port,
        int purge_rx,
        int purge_tx)
{
    int purge_rx_ret = 0, purge_tx_ret = 0;
    struct ftdi_port_data* port_data;

    if (!port->driver_specific_data) return USBSERIAL_ERROR_ILLEGAL_STATE;

    port_data = (struct ftdi_port_data*) port->driver_specific_data;

    if (purge_rx) purge_rx_ret = ftdi_reset_ctrl(
                port,
                FTDI_SIO_RESET_PURGE_RX,
                port_data->control_idx);
    if (purge_tx) purge_tx_ret = ftdi_reset_ctrl(
                port,
                FTDI_SIO_RESET_PURGE_TX,
                port_data->control_idx);

    return (0 == purge_rx_ret) ? purge_tx_ret : purge_rx_ret;
}

static void ftdi_read_data_postprocessor(
        struct usbserial_port* port,
        void* data,
        unsigned int* bytes_count)
{
    assert(port);
    assert(data);
    assert(bytes_count);

    int i;
    int skip_bytes_count = FTDI_MODEM_STATUS_BYTES_COUNT;
    const unsigned int unfiltered_bytes_count = *bytes_count;
    char* data_as_chars = (char*) data;
    const unsigned int max_packet_size = port->usb_device_descriptor.bMaxPacketSize0;

    for (i = FTDI_MODEM_STATUS_BYTES_COUNT; i < unfiltered_bytes_count; ++i)
    {
        if (0 == (i % max_packet_size))
        {
            skip_bytes_count += FTDI_MODEM_STATUS_BYTES_COUNT;
            ++i;
        }
        else
        {
            data_as_chars[i - skip_bytes_count] = data_as_chars[i];
        }
    }

    *bytes_count -= skip_bytes_count;
}

void ftdi_driver_init(struct usbserial_driver* driver)
{
    driver->check_supported_by_vid_pid = ftdi_check_supported_by_vid_pid;
    driver->check_supported_by_class = NULL;
    driver->get_device_short_name = ftdi_get_device_short_name;
    driver->get_ports_count = ftdi_get_ports_count;
    driver->port_init = ftdi_port_init;
    driver->port_deinit = ftdi_port_deinit;
    driver->port_set_line_config = ftdi_port_set_line_config;
    driver->start_reader = ftdi_start_reader;
    driver->stop_reader = ftdi_stop_reader;
    driver->write = ftdi_write;
    driver->purge = ftdi_purge;
    driver->read_data_postprocessor = ftdi_read_data_postprocessor;
}
