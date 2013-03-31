#include "common.h"
#include <driverlib/systick.h>
#include "utils/uartstdio.c"

#include "enc28j60.h"

static volatile unsigned long g_ulFlags;

volatile unsigned long g_ulTickCounter = 0;

#define UIP_PERIODIC_TIMER_MS	500
#define UIP_ARP_TIMER_MS		10000

#define FLAG_SYSTICK		0
#define FLAG_RXPKT			1
#define FLAG_TXPKT			2
#define FLAG_RXPKTPEND		3
#define FLAG_ENC_INT		4

#define SYSTICKHZ		CLOCK_CONF_SECOND
#define SYSTICKMS		(1000 / SYSTICKHZ)

void uip_log(char *msg) {
	printf("UIP: %s\n", msg);
}



/**
 * Various config options are present here
 *
 * MAC address of interface. Presumably unique, from a defunct Denmark tech
 * company.
 */
const uint8_t mac_addr[] = { 0x00, 0xC0, 0x033, 0x38, 0x22, 0xA4 };
/**
 * Uncomment this first line for a static IP. Leave it for DHCP. Don't touch
 * the second line.
 */
//#define STATIC_IP

#ifdef STATIC_IP
/**
 * Default IP address of gateway
 */
#define DEFAULT_IPADDR0 10
#define DEFAULT_IPADDR1 0
#define DEFAULT_IPADDR2 0
#define DEFAULT_IPADDR3 201
/**
 * Default netmask
 */
#define DEFAULT_NETMASK0 255
#define DEFAULT_NETMASK1 255
#define DEFAULT_NETMASK2 255
#define DEFAULT_NETMASK3 0

#endif





static void cpu_init(void) {
	// A safety loop in order to interrupt the MCU before setting the clock (wrongly)
	int i;
	for(i=0; i<1000000; i++);

	// Setup for 16MHZ external crystal, use 200MHz PLL and divide by 4 = 50MHz
	MAP_SysCtlClockSet(SYSCTL_SYSDIV_16 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
		SYSCTL_XTAL_16MHZ);
}

static void uart_init(void) {
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

	// Configure PD0 and PD1 for UART
	MAP_GPIOPinConfigure(GPIO_PA0_U0RX);
	MAP_GPIOPinConfigure(GPIO_PA1_U0TX);
	MAP_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
	UARTStdioInitExpClk(0, 115200);
}


int main(void) {
	static struct uip_eth_addr eth_addr;
	uip_ipaddr_t ipaddr;

	cpu_init();
	uart_init();
	printf("Welcome\n");

	// One line config! Woo!
	static ENCJ_STELLARIS::ENC28J60 chip(mac_addr);

	printf("ENC28J60 online\n");

	//
	// Configure SysTick for a periodic interrupt.
	//
	MAP_SysTickPeriodSet(MAP_SysCtlClockGet() / SYSTICKHZ);
	MAP_SysTickEnable();
	MAP_SysTickIntEnable();	

	uip_init();

	// Dump MAC into uIP
	for (int i = 0; i < 6; ++i)
	{
		eth_addr.addr[i] = mac_addr[i];
	}
	uip_setethaddr(eth_addr);


#ifdef STATIC_IP
	// Static IP
	uip_ipaddr
		( ipaddr
		, DEFAULT_IPADDR0
		, DEFAULT_IPADDR1
		, DEFAULT_IPADDR2
		, DEFAULT_IPADDR3
		);
	uip_sethostaddr(ipaddr);

	// Static Netmask
	uip_ipaddr
		( ipaddr
		, DEFAULT_NETMASK0
		, DEFAULT_NETMASK1
		, DEFAULT_NETMASK2
		, DEFAULT_NETMASK3
		);
	uip_setnetmask(ipaddr);

#ifdef _DEBUG
	printf("IP: %d.%d.%d.%d\n", DEFAULT_IPADDR0, DEFAULT_IPADDR1,
		DEFAULT_IPADDR2, DEFAULT_IPADDR3);
#endif

#else
	// DHCP
	uip_ipaddr(ipaddr, 0, 0, 0, 0);
	uip_sethostaddr(ipaddr);
	uip_ipaddr(ipaddr, 0, 0, 0, 0);
	uip_setnetmask(ipaddr);

//#ifdef _DEBUG
	printf("Waiting for IP address...\n");
//#endif

#endif

	httpd_init();

#ifndef STATIC_IP

	// DHCP request
	dhcpc_init(mac_addr, 6);
	dhcpc_request();

#endif







	// TODO









	long lPeriodicTimer, lARPTimer;
	lPeriodicTimer = lARPTimer = 0;

	//int i; // = MAP_GPIOPinRead(GPIO_PORTA_BASE, ENC_INT) & ENC_INT;
	while(true) {
		//MAP_IntDisable(INT_UART0);
		MAP_SysCtlSleep();
		//MAP_IntEnable(INT_UART0);

		//i = MAP_GPIOPinRead(GPIO_PORTA_BASE, ENC_INT) & ENC_INT;
		/*while(i != 0 && g_ulFlags == 0) {
		i = MAP_GPIOPinRead(GPIO_PORTA_BASE, ENC_INT) & ENC_INT;
		}*/

		if( HWREGBITW(&g_ulFlags, FLAG_ENC_INT) == 1 ) {
			HWREGBITW(&g_ulFlags, FLAG_ENC_INT) = 0;
			enc_action();
		}

		if( HWREGBITW(&g_ulFlags, FLAG_SYSTICK) == 1) {
			HWREGBITW(&g_ulFlags, FLAG_SYSTICK) = 0;
			lPeriodicTimer += SYSTICKMS;
			lARPTimer += SYSTICKMS;
			printf("%d %d\n", lPeriodicTimer, lARPTimer);
		}

		if( lPeriodicTimer > UIP_PERIODIC_TIMER_MS ) {
			lPeriodicTimer = 0;
			int l;
			for(l = 0; l < UIP_CONNS; l++) {
				uip_periodic(l);

				//
				// If the above function invocation resulted in data that
				// should be sent out on the network, the global variable
				// uip_len is set to a value > 0.
				//
				if(uip_len > 0) {
					uip_arp_out();
					enc_send_packet(uip_buf, uip_len);
					uip_len = 0;
				}
			}

			for(l = 0; l < UIP_UDP_CONNS; l++) {
				uip_udp_periodic(l);
				if( uip_len > 0) {
					uip_arp_out();
					enc_send_packet(uip_buf, uip_len);
					uip_len = 0;
				}
			}
		}

		if( lARPTimer > UIP_ARP_TIMER_MS) {
			lARPTimer = 0;
			uip_arp_timer();
		}

	}

	return 0;
}


void dhcpc_configured(const struct dhcpc_state *s)
{
	uip_sethostaddr(&s->ipaddr);
	uip_setnetmask(&s->netmask);
	uip_setdraddr(&s->default_router);
	printf("IP: %d.%d.%d.%d\n", s->ipaddr[0] & 0xff, s->ipaddr[0] >> 8,
		s->ipaddr[1] & 0xff, s->ipaddr[1] >> 8);
}

void SysTickIntHandler(void)
{
	//
	// Increment the system tick count.
	//
	g_ulTickCounter++;

	//
	// Indicate that a SysTick interrupt has occurred.
	//
	HWREGBITW(&g_ulFlags, FLAG_SYSTICK) = 1;
	//g_ulFlags |= FLAG_SYSTICK;
}


void GPIOPortEIntHandler(void) {
	uint8_t p = MAP_GPIOPinIntStatus(GPIO_PORTE_BASE, true) & 0xFF;

	MAP_GPIOPinIntClear(GPIO_PORTE_BASE, p);

	HWREGBITW(&g_ulFlags, FLAG_ENC_INT) = 1;
}

// Not referenced anywhere? 
clock_time_t clock_time(void)
{
	return((clock_time_t)g_ulTickCounter);
}