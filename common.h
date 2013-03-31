#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#include <inc/hw_types.h>
#include <inc/hw_memmap.h>
#include <driverlib/rom.h>
#include <driverlib/rom_map.h>
#include <driverlib/pin_map.h>
#include <driverlib/gpio.h>
#include <driverlib/sysctl.h>
#include <driverlib/ssi.h>
#include <driverlib/uart.h>

#include <utils/uartstdio.h>
#include <utils/ustdlib.h>

#include <uip/uip.h>
#include <uip/uip_arp.h>

#define printf          UARTprintf

#endif
