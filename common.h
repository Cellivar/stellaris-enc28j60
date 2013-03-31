#ifndef ENC28J60_STELLARIS_CHAPMAN_COMMON_H_
#define ENC28J60_STELLARIS_CHAPMAN_COMMON_H_

#include <stdint.h>

#include <inc/hw_types.h>
#include <inc/hw_memmap.h>
#include <inc/hw_ints.h>

#include <driverlib/gpio.h>
#include <driverlib/interrupt.h>
#include <driverlib/pin_map.h>
#include <driverlib/rom.h>
#include <driverlib/rom_map.h>
#include <driverlib/sysctl.h>
#include <driverlib/ssi.h>
#include <driverlib/uart.h>

#include <utils/uartstdio.h>
#include <utils/ustdlib.h>

#include <uip/uip.h>
#include <uip/uip_arp.h>

#define printf		UARTprintf

#endif //ENC28J60_STELLARIS_CHAPMAN_COMMON_H_
