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

/* This file contains prototypes for driver initialization functions. */

#ifndef LIBUSBSERIAL_DRIVERS_H
#define LIBUSBSERIAL_DRIVERS_H

void ftdi_driver_init(struct usbserial_driver* driver);
void silabs_driver_init(struct usbserial_driver* driver);
void cdc_driver_init(struct usbserial_driver* driver);

#endif // LIBUSBSERIAL_DRIVERS_H
