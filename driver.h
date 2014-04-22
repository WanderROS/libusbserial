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

/* This file contains the prototypes for the driver API. */

#ifndef LIBUSBSERIAL_DRIVER_H
#define LIBUSBSERIAL_DRIVER_H

#include "libusbserial.h"

struct usbserial_driver
{
    int (*check_supported_by_vid_pid)(
            uint16_t vendor_id,
            uint16_t product_id);
    int (*check_supported_by_class)(
            uint8_t device_class,
            uint8_t device_subclass);
    const char* (*get_device_short_name)(
            uint16_t vendor_id,
            uint16_t product_id,
            uint8_t device_class,
            uint8_t device_subclass);
    unsigned int (*get_ports_count)(uint16_t vendor_id, uint16_t product_id);

    int (*port_init)(struct usbserial_port* port);
    int (*port_deinit)(struct usbserial_port* port);

    int (*port_set_line_config)(
            struct usbserial_port* port,
            const struct usbserial_line_config* line_config);

    int (*start_reader)(struct usbserial_port* port);
    int (*stop_reader)(struct usbserial_port* port);

    int (*write)(
            struct usbserial_port* port,
            const void* data,
            unsigned int bytes_count);
    int (*purge)(
            struct usbserial_port* port,
            int purge_rx,
            int purge_tx);

    void (*read_data_postprocessor)(
            struct usbserial_port* port,
            void* data,
            unsigned int* bytes_count);
};

#endif // LIBUSBSERIAL_DRIVER_H
