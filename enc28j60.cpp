/**
 * @file enc28j60.cpp
 *
 * Definitions for ENC28J60 controller class. For use with the Stellaris
 * Launchpad, Stellaris-Pins library and a compatible ENC28J60 board.
 * 
 * It is possible to use this library without the Stellaris-Pins library, but
 * you get to write out the additional pin information yourself.
 */

#ifndef ENC28J60_STELLARIS_CHAPMAN_H_
#define ENC28J60_STELLARIS_CHAPMAN_H_

namespace ENCJ_STELLARIS
{

	#include <stdint.h>
	#include "enc28j60reg.h"


	/**
	 * ENC28J60 Class
	 */
	class ENC28J60
	{
	public:
		ENC28J60( /*pins pins pins*/ );
		ENC28J60( /*Stellaris-Pins enabled constructor */ );
		ENC28J60( /*Default pins for XPG's board */ );

		void ENCReceive();
		void ENCSend(const uint8_t *buf, uint16_t len);
		void ENCReset();

	private:
		/* Pin bank */
		byte csPort, intPort, resetPort;
		byte csPin, intPin, resetPin;

		uint8_t activeBank;		// Current memory bank
		uint16_t nextPacket;	// Next data packet (?)

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
	}

	#endif // ENC28J60_STELLARIS_CHAPMAN_H_

} // Namespace ENCJ_STELLARIS

