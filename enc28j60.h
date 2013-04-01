/**
 * @file enc28j60.h
 *
 * Prototype for ENC28J60 controller class. For use with the Stellaris
 * Launchpad, Stellaris-Pins library and a compatible ENC28J60 board.
 * 
 * It is possible to use this library without the Stellaris-Pins library, but
 * you get to write out the additional pin information yourself.
 */

#ifndef ENC28J60_STELLARIS_CHAPMAN_H_
#define ENC28J60_STELLARIS_CHAPMAN_H_

//TODO: Remove "common.h" requirement? Maybe?
#include "common.h"
#include "enc28j60reg.h"


namespace ENCJ_STELLARIS
{

	/**
	 * ENC28J60 Class
	 */
	class ENC28J60
	{
	public:
		/**
		 * Primary constructor, all arguments are optional except the MAC
		 * address.
		 *
		 * Blank arguments default to XPG's boosterpack pinout.
		 */
		ENC28J60
		( const uint8_t *mac					// MAC address array
		, uint32_t CSport		= 0x40005000	// PORT B
		, uint32_t CSpin		= 0x00000020	// PIN 5
		, uint32_t CSperiph		= 0x20000002	// GPIO B
		, uint32_t INTport		= 0x40024000	// PORT E
		, uint32_t INTpin		= 0x00000010	// PIN 4
		, uint32_t INTperiph	= 0x20000010	// GPIO E
		, uint32_t INTassign	= 20			// INT_GPIOE
		, uint32_t RESETport	= 0x40004000	// PORT A
		, uint32_t RESETpin		= 0x00000004	// PIN 2
		, uint32_t RESETperiph	= 0x20000001	// GPIO A
		, uint32_t SSIbase		= 0x4000A000	// SSI2 Base
		, uint32_t SSIperiph	= 0xf0001c02	// SSI2 Peripherial
		, uint32_t SSIGPIOperiph= 0x20000002	// PERIPH_GPIOB
		, uint32_t SSIGPIOport	= 0x40005000	// GPIO_PORTB_BASE
		, uint32_t SSIGPIOpins	= 0x00000104	// Pins 4 | 6 | 7
		, uint32_t SSIclk		= 0x00011002	// GPIO_PB4_SSI2CLK
		, uint32_t SSIrx		= 0x00011802	// GPIO_PB6_SSI2RX
		, uint32_t SSItx		= 0x00011C02	// GPIO_PB7_SSI2TX
		);

#ifdef STELLARIS_PINS_SSIPIN_CHAPMAN_H && STELLARIS_PINS_DIGITALIOPIN_CHAPMAN_H
		// TODO: Add in support for Pin Library
		/**
		 * Secondary constructor, allows use of StellarisPins library objects
		 * instead of the more verbose pin arguments.  This is only enabled
		 * if all pieces of the pin library required are actually present.
		 */
		ENC28J60
			( const uint8_t *mac
			); // Stellaris pins enabled constructor
#endif

		void Receive(void);
		bool Send(const uint8_t *buf, uint16_t count);
		void Reset(void);
		void Interrupt(void);

		uint8_t SPISend(uint8_t msg);

	private:
		/* Setup */
		void InitSPI
			( uint32_t SSIbase
			, uint32_t SSIperiph
			, uint32_t SSIGPIOperiph
			, uint32_t SSIGPIOport
			, uint32_t SSIGPIOpins
			, uint32_t SSICLK
			, uint32_t SSIRX
			, uint32_t SSITX
			);

		void InitPort
			( uint32_t CSport
			, uint32_t CSpin
			, uint32_t CSperiph
			, uint32_t INTport
			, uint32_t INTpin
			, uint32_t INTperiph
			, uint32_t RESETport
			, uint32_t RESETpin
			, uint32_t RESETperiph
			);

		void InitConfig(const uint8_t *mac);

		void InitInterrupt
			( uint32_t INTport
			, uint32_t INTpin
			, uint32_t INTassign
			);


		/* Pin bank */
		uint32_t CSport, INTport, RESETport;
		uint32_t CSpin, INTpin, RESETpin;
		uint32_t SSIbase;

		static uint8_t activeBank;		// Current memory bank
		static uint16_t nextPacket;	// Next data packet (?)

		/* Low level register controls */

		uint8_t RCR(uint8_t reg);				// Read Control Register
		uint8_t RCRM(uint8_t reg);				// Read Control Register MAC/MII
		void WCR(uint8_t reg, uint8_t val);		// Write Control Register
	
		void RBM(uint8_t *buf, uint16_t len);	// Read Buffer Memory
		void WBM(const uint8_t *buf, uint16_t len);	// Write Buffer Memory

		void BFS(uint8_t reg, uint8_t mask);	// Bit Field Set
		void BFC(uint8_t reg, uint8_t mask);	// Bit Field Clear

		/* High level register controls */

		void SwitchBank(uint8_t new_bank);	// Switch active memory bank

		uint8_t ReadRegister(uint8_t reg, uint8_t bank);
		uint8_t ReadMIIRegister(uint8_t reg, uint8_t bank);
		void WriteRegister(uint8_t reg, uint8_t bank, uint8_t value);

		// Batch bit field set controls
		void BitsFieldSet(uint8_t reg, uint8_t bank, uint8_t mask);
		void BitsFieldClear(uint8_t reg, uint8_t bank, uint8_t mask);

		// PHY memory access
		uint16_t ReadPHY(uint8_t address);
		void WritePHY(uint8_t address, uint16_t value);

		// Mass memory operations
		void SetRXMemoryArea(uint16_t startAddr, uint16_t endAddr);
		void SetMACAddress(const uint8_t *macAddr);
		void GetMACAddress(uint8_t *macAddr);
	};


} // Namespace ENCJ_STELLARIS

#endif // ENC28J60_STELLARIS_CHAPMAN_H_
