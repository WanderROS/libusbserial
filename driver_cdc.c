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

/* This file contains the implementation of a driver for CDC/ACM
 * and Prolific PL2303 devices. */

#include "common.h"
#include "driver.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define ARDUINO_VENDOR_ID 0x2341
#define PROLIFIC_VENDOR_ID 0x067b

#define PROLIFIC_PRODUCT_ID_PL2303 0x2303

#define CDC_DEVICE_CLASS 0x02
#define CDC_ACM_DEVICE_SUBCLASS 0x02

#define CDC_ACM_REQTYPE LIBUSB_RECIPIENT_INTERFACE | 0x20

#define CDC_SET_LINE_CODING_REQUEST_CODE 0x20

#define PROLIFIC_VENDOR_OUT_REQTYPE 0x40

#define PROLIFIC_VENDOR_WRITE_REQUEST_CODE 0x01

#define PROLIFIC_FLUSH_RX_VALUE 0x08
#define PROLIFIC_FLUSH_TX_VALUE 0x09

static const char* PROLIFIC_DEVICE_NAME_PL2303 = "PL2303";
static const char* CDC_DEVICE_NAME_ARDUINO = "Arduino";
static const char* CDC_DEVICE_NAME_CDC_ACM = "CDC";

struct cdc_port_data
{
    struct libusb_transfer* transfer;
    uint8_t read_ep;
    uint8_t write_ep;
    int read_ep_if;
    int write_ep_if;
};

static int prolific_vendor_out(
        struct usbserial_port* port,
        uint16_t value,
        uint16_t index,
        void* data,
        unsigned int data_length)
{
    assert(port);
    assert(data || (0 == data_length));

    int ctrl_ret = libusb_control_transfer(
                port->usb_device_handle,
                PROLIFIC_VENDOR_OUT_REQTYPE,
                PROLIFIC_VENDOR_WRITE_REQUEST_CODE,
                value,
                index,
                (unsigned char*) data,
                data_length,
                DEFAULT_CONTROL_TIMEOUT_MILLIS);
    if (ctrl_ret > 0)
    {
        if (ctrl_ret == data_length) return 0;
        else return USBSERIAL_ERROR_CTRL_CMD_FAILED;
    }
    else return ctrl_ret;
}

static int cdc_check_supported_by_vid_pid(
        uint16_t vendor_id,
        uint16_t product_id)
{
    return ((PROLIFIC_VENDOR_ID == vendor_id)
            && (PROLIFIC_PRODUCT_ID_PL2303 == product_id));
}

static int cdc_check_supported_by_class(
        uint8_t device_class,
        uint8_t device_subclass)
{
    /* Arduino compatible devices report 0 as subclass,
     * which is against the CDC specification :-|| */
    return ((CDC_DEVICE_CLASS == device_class)
            && ((CDC_ACM_DEVICE_SUBCLASS == device_subclass)
                || (0 == device_subclass)));
}

static const char* cdc_get_device_short_name(
        uint16_t vendor_id,
        uint16_t product_id,
        uint8_t device_class,
        uint8_t device_subclass)
{
    if ((PROLIFIC_VENDOR_ID == vendor_id)
            && (PROLIFIC_PRODUCT_ID_PL2303 == product_id))
    {
        return PROLIFIC_DEVICE_NAME_PL2303;
    }

    switch (vendor_id)
    {
    case ARDUINO_VENDOR_ID:
        return CDC_DEVICE_NAME_ARDUINO;
    default:
        return CDC_DEVICE_NAME_CDC_ACM;
    }
}

static unsigned int cdc_get_ports_count(uint16_t vendor_id, uint16_t product_id)
{
    /* Are there any multiport CDC/ACM or Prolific devices out there? */
    return 1;
}

static int cdc_port_init(struct usbserial_port* port)
{
    struct cdc_port_data* port_data;
    uint8_t read_ep, write_ep;
    int read_ep_if, write_ep_if;
    int found_read_ep = 0, found_write_ep = 0;
    int claimed_read_ep_if = 0, claimed_write_ep_if = 0;
    int ret;

#if defined(__GNUC__) && !defined(__clang__)
    /* Silence unjustified warnings for broken legacy compilers. */
    read_ep = 0;
    write_ep = 0;
    read_ep_if = -1;
    write_ep_if = -1;
#endif

    assert(port);

    {
        struct libusb_config_descriptor* config = NULL;
        uint8_t i, j;
        ret = libusb_get_active_config_descriptor(port->usb_device, &config);
        if (0 != ret) return ret;
        assert(config);

        for (i = 0; i < config->bNumInterfaces; ++i)
        {
            const struct libusb_interface* interface = &config->interface[i];
            assert(interface);
            if (interface->altsetting && (interface->altsetting->bNumEndpoints > 0))
            {
                for (j = 0; j < interface->altsetting->bNumEndpoints; ++j)
                {
                    const struct libusb_endpoint_descriptor* endpoint
                            = &interface->altsetting->endpoint[j];
                    if (endpoint->bEndpointAddress & LIBUSB_TRANSFER_TYPE_BULK)
                    {
                        if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN)
                        {
                            if (!found_read_ep)
                            {
                                found_read_ep = 1;
                                read_ep = endpoint->bEndpointAddress;
                                read_ep_if = i;
                            }
                        }
                        else
                        {
                            if (!found_write_ep)
                            {
                                found_write_ep = 1;
                                write_ep = endpoint->bEndpointAddress;
                                write_ep_if = i;
                            }
                        }
                    }
                }
            }
        }
        libusb_free_config_descriptor(config);
    }

    if (!found_read_ep || ! found_write_ep) return USBSERIAL_ERROR_UNSUPPORTED_DEVICE;

    ret = libusb_claim_interface(port->usb_device_handle, read_ep_if);
    if (0 != ret) goto relase_if_and_return;
    claimed_read_ep_if = 1;

    if (read_ep_if != write_ep_if)
    {
        ret = libusb_claim_interface(port->usb_device_handle, write_ep_if);
        if (0 != ret) goto relase_if_and_return;
    }
    claimed_write_ep_if = 1;

    port_data = (struct cdc_port_data*) malloc(sizeof(struct cdc_port_data));
    if (!port_data)
    {
        ret = USBSERIAL_ERROR_RESOURCE_ALLOC_FAILED;
        goto relase_if_and_return;
    }
    port_data->transfer = NULL;
    port_data->read_ep = read_ep;
    port_data->write_ep = write_ep;
    port_data->read_ep_if = read_ep_if;
    port_data->write_ep_if = write_ep_if;

    port->driver_specific_data = port_data;

    return 0;

relase_if_and_return:
    assert(ret != 0);
    if (claimed_read_ep_if)
    {
        libusb_release_interface(port->usb_device_handle, read_ep_if);
    }
    if (claimed_write_ep_if && (read_ep_if != write_ep_if))
    {
        libusb_release_interface(port->usb_device_handle, write_ep_if);
    }
    return ret;
}

static int cdc_port_deinit(struct usbserial_port* port)
{
    assert(port);

    struct cdc_port_data* port_data;
    int ret;

    if (!port->driver_specific_data) return USBSERIAL_ERROR_ILLEGAL_STATE;

    port_data = (struct cdc_port_data*) port->driver_specific_data;
    ret = libusb_release_interface(port->usb_device_handle, port_data->read_ep_if);
    if (port_data->read_ep_if != port_data->write_ep_if)
    {
        int release_write_ret = libusb_release_interface(
                    port->usb_device_handle,
                    port_data->write_ep_if);
        if (0 == ret) ret = release_write_ret;
    }

    free(port->driver_specific_data);
    port->driver_specific_data = NULL;

    return ret;
}

static int cdc_port_set_baudrate(
        struct usbserial_port* port,
        unsigned int baud)
{
    assert(port);

    int ctrl_ret;
    uint32_t baud_le = usbserial_common_convert_to_le((uint32_t) baud);
    unsigned char data[7];

    memcpy(data, &baud_le, sizeof(baud_le));
    data[4] = 0;
    data[5] = 0;
    data[6] = 8;

    ctrl_ret = libusb_control_transfer(
                port->usb_device_handle,
                CDC_ACM_REQTYPE,
                CDC_SET_LINE_CODING_REQUEST_CODE,
                0,
                0,
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

static int cdc_start_reader(struct usbserial_port* port)
{
    struct libusb_transfer* transfer;
    struct cdc_port_data* port_data;
    int submit_ret;

    assert(port);
    assert(port->read_cb);

    if (!port->driver_specific_data) return USBSERIAL_ERROR_ILLEGAL_STATE;

    port_data = (struct cdc_port_data*) port->driver_specific_data;
    if (port_data->transfer) return USBSERIAL_ERROR_ILLEGAL_STATE;

    transfer = libusb_alloc_transfer(0);
    if (!transfer) return USBSERIAL_ERROR_RESOURCE_ALLOC_FAILED;

    usbserial_common_init_bulk_read_transfer(
                transfer,
                port_data->read_ep,
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

static int cdc_stop_reader(struct usbserial_port* port)
{
    assert(port);

    struct cdc_port_data* port_data;
    int ret;

    port_data = (struct cdc_port_data*) port->driver_specific_data;
    if ((!port_data) || (!port_data->transfer))
    {
        return USBSERIAL_ERROR_ILLEGAL_STATE;
    }
    ret = usbserial_common_cancel_read_transfer_sync(port, port_data->transfer);

    libusb_free_transfer(port_data->transfer);
    port_data->transfer = NULL;

    return ret;
}

static int cdc_write(
        struct usbserial_port* port,
        const void* data,
        unsigned int bytes_count)
{
    assert(port);

    struct cdc_port_data* port_data;

    port_data = (struct cdc_port_data*) port->driver_specific_data;
    if (!port_data) return USBSERIAL_ERROR_ILLEGAL_STATE;

    return usbserial_common_bulk_write(
                port->usb_device_handle,
                port_data->write_ep,
                data,
                bytes_count);
}

static int cdc_purge(
        struct usbserial_port* port,
        int purge_rx,
        int purge_tx)
{
    assert(port);

    if (PROLIFIC_VENDOR_ID == port->usb_device_descriptor.idVendor)
    {
        int purge_rx_ret = 0, purge_tx_ret = 0;

        if (!port->driver_specific_data) return USBSERIAL_ERROR_ILLEGAL_STATE;

        prolific_vendor_out(port, PROLIFIC_FLUSH_RX_VALUE, 0, NULL, 0);

        if (purge_rx) purge_rx_ret = prolific_vendor_out(port, PROLIFIC_FLUSH_RX_VALUE, 0, NULL, 0);
        if (purge_tx) purge_tx_ret = prolific_vendor_out(port, PROLIFIC_FLUSH_TX_VALUE, 0, NULL, 0);

        return (0 == purge_rx_ret) ? purge_tx_ret : purge_rx_ret;
    }
    else return USBSERIAL_ERROR_UNSUPPORTED_OPERATION;
}

void cdc_driver_init(struct usbserial_driver* driver)
{
    driver->check_supported_by_vid_pid = cdc_check_supported_by_vid_pid;
    driver->check_supported_by_class = cdc_check_supported_by_class;
    driver->get_device_short_name = cdc_get_device_short_name;
    driver->get_ports_count = cdc_get_ports_count;
    driver->port_init = cdc_port_init;
    driver->port_deinit = cdc_port_deinit;
    driver->port_set_baud_rate = cdc_port_set_baudrate;
    driver->start_reader = cdc_start_reader;
    driver->stop_reader = cdc_stop_reader;
    driver->write = cdc_write;
    driver->purge = cdc_purge;
    driver->read_data_postprocessor = NULL;
}
