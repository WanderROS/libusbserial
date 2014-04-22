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

/* This file contains the implementation of a driver for Silicon Labs devices. */

#include "common.h"
#include "driver.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define SILABS_VENDOR_ID 0x10c4

#define SILABS_PRODUCT_ID_CP2102 0xea60
#define SILABS_PRODUCT_ID_CP2105 0xea70
#define SILABS_PRODUCT_ID_CP2108 0xea71
#define SILABS_PRODUCT_ID_CP2110 0xea80

#define SILABS_HOST_TO_DEVICE_REQTYPE 0x41

#define SILABS_IFC_REQUEST_CODE 0x00
#define SILABS_BAUDDIV_REQUEST_CODE 0x01
#define SILABS_LINE_CTL_REQUEST_CODE 0x03
#define SILABS_MHS_REQUEST_CODE 0x07
#define SILABS_BAUDRATE_REQUEST_CODE 0x1e
#define SILABS_FLUSH_REQUEST_CODE 0x12

#define SILABS_IFC_UART_ENABLE_VALUE 0x0001
#define SILABS_IFC_UART_DISABLE_VALUE 0x0000

#define SILABS_MHS_MCR_DTR_VALUE 0x0001
#define SILABS_MHS_MCR_RTS_VALUE 0x0002
#define SILABS_MHS_CTRL_DTR_VALUE 0x0100
#define SILABS_MHS_CTLR_RTS_VALUE 0x0200

#define SILABS_FLUSH_RX_VALUE 0x0a
#define SILABS_FLUSH_TX_VALUE 0x05

#define SILABS_BAUDDIV_GEN_FREQ_VALUE 0x384000

#define SILABS_DEFAULT_BAUD_RATE 9600

#define SILABS_READ_ENDPOINT(i) (0x81 + i)
#define SILABS_WRITE_ENDPOINT(i) (0x01 + i)

static const char* SILABS_DEVICE_NAME_CP2102 = "CP2102";
static const char* SILABS_DEVICE_NAME_CP2105 = "CP2105";
static const char* SILABS_DEVICE_NAME_CP2108 = "CP2108";
static const char* SILABS_DEVICE_NAME_CP2110 = "CP2110";
static const char* SILABS_DEVICE_NAME_CP21XX = "CP21XX";

struct silabs_port_data
{
    struct libusb_transfer* transfer;
};

static int silabs_set_config(
        struct usbserial_port* port,
        uint8_t request_code,
        uint16_t value)
{
    assert(port);

    return libusb_control_transfer(
                port->usb_device_handle,
                SILABS_HOST_TO_DEVICE_REQTYPE,
                request_code,
                value,
                (uint16_t) port->port_idx,
                NULL,
                0,
                DEFAULT_CONTROL_TIMEOUT_MILLIS);
}

static int silabs_check_supported_by_vid_pid(
        uint16_t vendor_id,
        uint16_t product_id)
{
    if (SILABS_VENDOR_ID == vendor_id)
    {
        switch (product_id)
        {
        case SILABS_PRODUCT_ID_CP2102:
        case SILABS_PRODUCT_ID_CP2105:
        case SILABS_PRODUCT_ID_CP2108:
        case SILABS_PRODUCT_ID_CP2110:
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

static const char* silabs_get_device_short_name(
        uint16_t vendor_id,
        uint16_t product_id,
        uint8_t device_class,
        uint8_t device_subclass)
{
    assert(SILABS_VENDOR_ID == vendor_id);

    switch (product_id)
    {
    case SILABS_PRODUCT_ID_CP2102:
        return SILABS_DEVICE_NAME_CP2102;
    case SILABS_PRODUCT_ID_CP2105:
        return SILABS_DEVICE_NAME_CP2105;
    case SILABS_PRODUCT_ID_CP2108:
        return SILABS_DEVICE_NAME_CP2108;
    case SILABS_PRODUCT_ID_CP2110:
        return SILABS_DEVICE_NAME_CP2110;

    default:
        return SILABS_DEVICE_NAME_CP21XX;
    }
}

static unsigned int silabs_get_ports_count(uint16_t vendor_id, uint16_t product_id)
{
    assert(SILABS_VENDOR_ID == vendor_id);

    switch (product_id)
    {
    case SILABS_PRODUCT_ID_CP2102:
    case SILABS_PRODUCT_ID_CP2110:
        return 1;
    case SILABS_PRODUCT_ID_CP2105:
        return 2;
    case SILABS_PRODUCT_ID_CP2108:
        return 4;

    default:
        return 0;
    }
}

static int silabs_port_init(struct usbserial_port* port)
{
    struct silabs_port_data* port_data;
    int ret;

    assert(port);

    ret = libusb_claim_interface(port->usb_device_handle, port->port_idx);
    if (0 != ret) return ret;

    ret = silabs_set_config(
                port,
                SILABS_IFC_REQUEST_CODE,
                SILABS_IFC_UART_ENABLE_VALUE);
    if (0 != ret) goto relase_if_and_return;

    ret = silabs_set_config(
                port,
                SILABS_BAUDDIV_REQUEST_CODE,
                SILABS_MHS_MCR_DTR_VALUE
                    | SILABS_MHS_MCR_RTS_VALUE
                    | SILABS_MHS_CTRL_DTR_VALUE
                    | SILABS_MHS_CTLR_RTS_VALUE);
    if (0 != ret) goto relase_if_and_return;

    ret = silabs_set_config(
                port,
                SILABS_BAUDDIV_REQUEST_CODE,
                SILABS_BAUDDIV_GEN_FREQ_VALUE
                    / SILABS_DEFAULT_BAUD_RATE);
    if (0 != ret) goto relase_if_and_return;

    port_data = (struct silabs_port_data*) malloc(sizeof(struct silabs_port_data));
    if (!port_data)
    {
        ret = USBSERIAL_ERROR_RESOURCE_ALLOC_FAILED;
        goto relase_if_and_return;
    }
    port_data->transfer = NULL;

    port->driver_specific_data = port_data;

    return 0;

relase_if_and_return:
    assert(0 != ret);
    libusb_release_interface(port->usb_device_handle, port->port_idx);
    return ret;
}

static int silabs_port_deinit(struct usbserial_port* port)
{
    assert(port);

    if (!port->driver_specific_data) return USBSERIAL_ERROR_ILLEGAL_STATE;

    free(port->driver_specific_data);
    port->driver_specific_data = NULL;

    return libusb_release_interface(
                port->usb_device_handle,
                port->port_idx);
}

static int silabs_port_set_line_config(
        struct usbserial_port* port,
        const struct usbserial_line_config* line_config)
{
    assert(port);
    assert(line_config);

    int ctrl_ret;
    unsigned char data[8];
    unsigned char parity_byte, flow_control_byte, data_bits_byte, stop_bits_byte;
    uint32_t baud_le = usbserial_common_convert_to_le((uint32_t) line_config->baud);

    switch (line_config->parity)
    {
    case USBSERIAL_PARITY_NONE:
        parity_byte = 0;
        break;
    case USBSERIAL_PARITY_ODD:
        parity_byte = 1;
        break;
    case USBSERIAL_PARITY_EVEN:
        parity_byte = 2;
        break;
    case USBSERIAL_PARITY_MARK:
        parity_byte = 3;
        break;
    case USBSERIAL_PARITY_SPACE:
        parity_byte = 4;
        break;

    default:
        return USBSERIAL_ERROR_INVALID_PARAMETER;
    }

    flow_control_byte = 0; /* Hardware flow control not supported (yet) */

    data_bits_byte = (unsigned char) line_config->data_bits;

    switch (line_config->stop_bits)
    {
    case USBSERIAL_STOPBITS_1:
        stop_bits_byte = 0;
        break;
    case USBSERIAL_STOPBITS_1_5:
        stop_bits_byte = 1;
        if (USBSERIAL_DATABITS_5 != line_config->data_bits)
        {
            return USBSERIAL_ERROR_UNSUPPORTED_OPERATION;
        }
        break;
    case USBSERIAL_STOPBITS_2:
        stop_bits_byte = 1;
        if (USBSERIAL_DATABITS_5 == line_config->data_bits)
        {
            return USBSERIAL_ERROR_UNSUPPORTED_OPERATION;
        }
        break;

    default:
        return USBSERIAL_ERROR_INVALID_PARAMETER;
    }

    memcpy(data, &baud_le, sizeof(baud_le));
    data[4] = parity_byte;
    data[5] = flow_control_byte;
    data[6] = data_bits_byte;
    data[7] = stop_bits_byte;

    ctrl_ret = libusb_control_transfer(
                port->usb_device_handle,
                SILABS_HOST_TO_DEVICE_REQTYPE,
                SILABS_BAUDRATE_REQUEST_CODE,
                0,
                (uint16_t) port->port_idx,
                data,
                sizeof(data),
                DEFAULT_CONTROL_TIMEOUT_MILLIS);
    if (ctrl_ret > 0)
    {
        if (ctrl_ret == sizeof(data)) return 0;
        else return USBSERIAL_ERROR_CTRL_CMD_FAILED;
    }
    else return ctrl_ret;
}

static int silabs_start_reader(struct usbserial_port* port)
{
    struct libusb_transfer* transfer;
    struct silabs_port_data* port_data;
    int submit_ret;

    assert(port);
    assert(port->read_cb);

    if (!port->driver_specific_data) return USBSERIAL_ERROR_ILLEGAL_STATE;

    port_data = (struct silabs_port_data*) port->driver_specific_data;
    if (port_data->transfer) return USBSERIAL_ERROR_ILLEGAL_STATE;

    transfer = libusb_alloc_transfer(0);
    if (!transfer) return USBSERIAL_ERROR_RESOURCE_ALLOC_FAILED;

    usbserial_common_init_bulk_read_transfer(
                transfer,
                SILABS_READ_ENDPOINT(port->port_idx),
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

static int silabs_stop_reader(struct usbserial_port* port)
{
    assert(port);

    struct silabs_port_data* port_data;
    int ret;

    port_data = (struct silabs_port_data*) port->driver_specific_data;
    if ((!port_data) || (!port_data->transfer))
    {
        return USBSERIAL_ERROR_ILLEGAL_STATE;
    }
    ret = usbserial_common_cancel_read_transfer_sync(port, port_data->transfer);

    libusb_free_transfer(port_data->transfer);
    port_data->transfer = NULL;

    return ret;
}

static int silabs_write(
        struct usbserial_port* port,
        const void* data,
        unsigned int bytes_count)
{
    assert(port);

    if (!port->driver_specific_data) return USBSERIAL_ERROR_ILLEGAL_STATE;

    return usbserial_common_bulk_write(
                port->usb_device_handle,
                SILABS_WRITE_ENDPOINT(port->port_idx),
                data,
                bytes_count);
}

static int silabs_purge(
        struct usbserial_port* port,
        int purge_rx,
        int purge_tx)
{
    uint16_t value;

    if (!port->driver_specific_data) return USBSERIAL_ERROR_ILLEGAL_STATE;

    value = (purge_rx ? SILABS_FLUSH_RX_VALUE : 0)
                | (purge_tx ? SILABS_FLUSH_TX_VALUE : 0);
    return silabs_set_config(port, SILABS_FLUSH_REQUEST_CODE, value);
}

void silabs_driver_init(struct usbserial_driver* driver)
{
    driver->check_supported_by_vid_pid = silabs_check_supported_by_vid_pid;
    driver->check_supported_by_class = NULL;
    driver->get_device_short_name = silabs_get_device_short_name;
    driver->get_ports_count = silabs_get_ports_count;
    driver->port_init = silabs_port_init;
    driver->port_deinit = silabs_port_deinit;
    driver->port_set_line_config = silabs_port_set_line_config;
    driver->start_reader = silabs_start_reader;
    driver->stop_reader = silabs_stop_reader;
    driver->write = silabs_write;
    driver->purge = silabs_purge;
    driver->read_data_postprocessor = NULL;
}
