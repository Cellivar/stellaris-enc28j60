/**
 * @file enc28j60.cpp
 * 
 * Definitions for the ENC28J60 class
 */

#include <enc28j60.h>


/* Macros for accessing registers.
 * These macros should be used instead of calling the functions directly.
 * They simply pass the register's bank as an argument, so the caller
 * doesn't have to deal with that.
 */
#define READ_REG(reg) enc_read_reg(reg, reg ## _BANK)
#define WRITE_REG(reg, value) enc_write_reg(reg, reg ## _BANK, value)
#define READ_MREG(reg) enc_read_mreg(reg, reg ## _BANK)
#define SET_REG_BITS(reg, mask) enc_set_bits(reg, reg ## _BANK, mask)
#define CLEAR_REG_BITS(reg, mask) enc_clear_bits(reg, reg ## _BANK, mask)

// uIP library's buffer
#define BUF	((struct uip_eth_hdr *)uip_buf)


namespace ENCJ_STELLARIS
{
	
	ENC28J60::ENC28J60
		( const uint8_t *mac
		, uint8_t CSport
		, uint8_t CSpin
		, uint8_t CSperiph
		, uint8_t INTport
		, uint8_t INTpin
		, uint8_t INTperiph
		, uint8_t INTassign
		, uint8_t RESETport
		, uint8_t RESETpin
		, uint8_t RESETperiph
		, uint8_t SSIBase
		, uint8_t SSIperiph
		, uint8_t SSIGPIOperiph
		, uint8_t SSIGPIOport
		, uint8_t SSIGPIOpins
		, uint8_t SSICLK
		, uint8_t SSIRX
		, uint8_t SSITX
		)
	{
		this->InitSPI
			( SSIbase
			, SSIperiph
			, SSIGPIOperiph
			, SSIGPIOport
			, SSIGPIOpins
			, SSICLK
			, SSIRX
			, SSITX
			);

		this->InitPort
			( CSport
			, CSpin
			, CSperiph
			, INTport
			, INTpin
			, INTperiph
			, RESETport
			, RESETpin
			, RESETperiph
			);

		this->InitConfig();

		this->InitInterrupt
			( INTport
			, INTpin
			, INTassign
			);


	}

	/**
	 * Config the ENCJ chip's registers.
	 */
	void ENC28J60::InitConfig()
	{

		// Config
		this->nextPacket = 0x0000;
		this->Reset();

		// Check the ESTAT read bit from the chip
		uint8_t reg;
		do {
			reg = READ_REG(ENC_ESTAT);
			MAP_SysCtlDelay(3333200);

#ifdef _DEBUG
			printf("ENC_ESTAT value: %x\n", reg);
#endif

		}
		while ((reg & ENC_ESTAT_CLKRDY) == 0);

		// Chip has stated it's clock is stabilized, ready to continue

		this->SwitchBank(0);

#ifdef _DEBUG
		printf("Econ: %x\n", READ_REG(ENC_ECON1));
		printf("Silicon Revision: %d\n", READ_REG(ENC_EREVID));
#endif

		// Reset transmit logic
		SET_REG_BITS(ENC_ECON1, ENC_ECON1_TXRST);
		// Clear packet recieve bit
		CLEAR_REG_BITS(ENC_ECON1, ENC_ECON1_RXEN);

		// Enable automatic buffer pointer increment
		SET_REG_BITS(ENC_ECON2, ENC_ECON2_AUTOINC);	

		this->SetRXMemoryArea(0x000, RX_END);

		// Configure DPXSTAT for full-duplex operation
		uint16_t phyreg = this->ReadPHY(ENC_PHSTAT2);
		phyreg &= ~ENC_PHSTAT2_DPXSTAT;
		this->WritePHY(ENC_PHSTAT2, phyreg);

		// Configure PDPXMD for full-duplex operation
		phyreg = this->ReadPHY(ENC_PHCON1);
		phyreg &= ~ENC_PHCON_PDPXMD;
		this->WritePHY(ENC_PHCON1, phyreg);

		/* Section 6.5, MAC initialization steps */

#ifdef _DEBUG
		printf("Setting MAC: %x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2],
			mac[3], mac[4], mac[5]);
#endif

		this->SetMACAddress(mac);

		// Set up recieve filter
		// Allow broadcasts, multicast, unicast, and invalid CRCs are dumped
		WRITE_REG
			( ENC_ERXFCON
			, ENC_ERXFCON_UCEN 
				| ENC_ERXFCON_CRCEN 
				| ENC_ERXFCON_BCEN 
				| ENC_ERXFCON_MCEN
			);
	
		// Allow MAC to use TX/RX pause frames, enable MAC packet receive.
		WRITE_REG
			( ENC_MACON1
			, ENC_MACON1_TXPAUS | ENC_MACON1_RXPAUS | ENC_MACON1_MARXEN
			);

		// All short frames are zero-padded to 60 bytes and CRC'ed
		// MAC will append a CRC
		// Frame length checking enabled
		WRITE_REG
			( ENC_MACON3
			, (0x1 << ENC_MACON3_PADCFG_SHIFT)
				| ENC_MACON3_TXRCEN
				| ENC_MACON3_FRMLNEN
			);

		// Max frame length set to 0x05EE
		WRITE_REG(ENC_MAMXFLL, 1518 & 0xFF);		// Low  0xEE
		WRITE_REG(ENC_MAMXFLH, (1518 >> 8) & 0xFF);	// High 0x05

		// MAC initialization, section 6.5 steps 5-7
		WRITE_REG(ENC_MABBIPG, 0x12);	// Back-to-back packet gap
		WRITE_REG(ENC_MAIPGL, 0x12);	// Non-btb inter-packet gap low
		WRITE_REG(ENC_MAIPGH, 0x0C);	// Non-btb inter-packet gap high
		
		// Enable interrupts, and enable packet recieve pending
		SET_REG_BITS(ENC_EIE, ENC_EIE_INTIE | ENC_EIE_PKTIE);

		// Clear the reset status on TX/RX logic
		CLEAR_REG_BITS(ENC_ECON1, ENC_ECON1_TXRST | ENC_ECON1_RXRST);

		// Enable receiving packets
		SET_REG_BITS(ENC_ECON1, ENC_ECON1_RXEN);

#ifdef _DEBUG
		uint8_t mc[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		enc_get_mac_addr(mc);
		printf("Mac addr set to: %x:%x:%x:%x:%x:%x\n", mc[0], mc[1], mc[2],
			mc[3], mc[4], mc[5]);
#endif

	}


	/**
	 * Enable the SSI connection for SPI to the ENC chip
	 *
	 * @param SSIbase is the only value we care about after this initialization
	 *  and as such it is stored in the object for later use in the SPISend
	 *  function.
	 */
	void ENC28J60::InitSPI
		( uint8_t SSIbase
		, uint8_t SSIperiph
		, uint8_t SSIGPIOperiph
		, uint8_t SSIGPIOport
		, uint8_t SSIGPIOpins
		, uint8_t SSICLK
		, uint8_t SSIRX
		, uint8_t SSITX
		)
	{
		MAP_SysCtlPeripheralEnable(SSIGPIOperiph);
		MAP_SysCtlPeripheralEnable(SSIperiph);

		MAP_GPIOPinConfigure(SSICLK);
		MAP_GPIOPinConfigure(SSIRX);
		MAP_GPIOPinConfigure(SSITX);
		MAP_GPIOPinTypeSSI(SSIGPIOport, SSIGPIOpins);

		MAP_SSIConfigSetExpClk
			( SSIBase
			, MAP_SysCtlClockGet()
			, SSI_FRF_MOTO_MODE_0
			, SSI_MODE_MASTER
			, 1000000
			, 8
			);

		MAP_SSIEnable(SSIBase);
		this->SSIbase = SSIbase;

		// Make sure the SSI FIFO is empty?
		unsigned long b;
		while(MAP_SSIDataGetNonBlocking(this->SSIbase, &b)) {}
	}

	/**
	 * Initialize all of the ports and pins that the ENC will use
	 *
	 * We care about the ports and pins for each one so that we may call them
	 * later on, so those are stored for later use
	 */
	void ENC28J60::InitPort
		( uint8_t CSport
		, uint8_t CSpin
		, uint8_t CSperiph
		, uint8_t INTport
		, uint8_t INTpin
		, uint8_t INTperiph
		, uint8_t RESETport
		, uint8_t RESETpin
		, uint8_t RESETperiph
		)
	{
		// CSport/pin
		MAP_SysCtlPeripheralEnable(CSport);
		MAP_GPIOPinTypeGPIOOutput(CSport, CSpin);
		this->CSpin = CSpin;
		this->CSport = CSport;

		// INTport/pin
		MAP_SysCtlPeripheralEnable(INTperiph);
		MAP_GPIOPinTypeGPIOOutput(INTport, INTpin);
		this->INTpin = INTpin;
		this->INTport = INTport;

		// RESETport/pin
		MAP_SysCtlPeripheralEnable(RESETperiph);
		MAP_GPIOPinTypeGPIOOutput(RESETport, RESETpin);
		this->INTpin = RESETpin;
		this->RESETport = RESETport;

		// Bring CS high
		MAP_GPIOPinWrite(this->CSport, this->CSpin, this->CSpin);
	}

	void ENC28J60::InitInterrupt
		( uint8_t INTport
		, uint8_t INTpin
		, uint8_t INTassign
		)
	{
		MAP_IntEnable(INTassign);
		MAP_IntMasterEnable();

		// Disable sleep interrupts (power saving?)
		MAP_SysCtlPeripheralClockGating(false);

		MAP_GPIOIntTypeSet(INTport, INTpin, GPIO_FALLING_EDGE);
		MAP_GPIOPinIntClear(INTport, INTpin);
		MAP_GPIOPinIntEnable(INTport, INTpin);

	}


	/**
	 * Handle a packet recieved interrupt request from the ENC
	 */
	void ENC28J60::Interrupt()
	{
		uint8_t reg = READ_REG(ENC_EIR);

		if (reg & ENC_EIR_PKTIF)
		{
			while (READ_REG(ENC_EPKTCNT) > 0)
			{
				this->Recieve();
			}
		}
	}

	/**
	 * Recieve a single packet. Contents will be placed in uip_buf, and uIP is
	 * called as appropriate
	 * 
	 * @todo: Decouple this from uIP, the IP stack should be elsewhere.
	 * Possibly have this return a struct with the appropriate data? Difficult
	 * as it needs to actively read the information
	 */
	void ENC28J60::Receive()
	{

		uint8_t header[6];
		uint8_t *status = header + 2;

		// Set the read pointer to the next packet location
		WRITE_REG(ENC_ERDPTL, this->nextPacket & 0xFF);			// Low byte
		WRITE_REG(ENC_ERDPTH, (this->nextPacket >> 8) & 0xFF);	// High byte
		this->RBM(header, 6);

		// Update next packet pointer
		this->nextPacket = header[0] | (header[1] << 8);

		// TODO
		uint16_t dataCount = status[0] | (status[1] << 8);

		if (status[2] & (1 << 7))
		{
			uip_len = data_count;
			this->RBM(uip_buf, data_count);

			if (BUF->type == htons(UIP_ETHTYPE_IP)) 
			{
				uip_arp_ipin();
				uip_input();

				if (uip_len > 0)
				{
					uip_arp_out();
					this->Send(uip_buf, upi_len);
					uip_len = 0;
				}
			}
			else if (BUF->type == htons(UIP_ETHTYPE_ARP))
			{
				uip_arp_arpin();
				if(uip_len > 0)
				{
					this->Send(uip_buf, uip_len);
					uip_len = 0;
				}
			}
		}

		uing16_t erxst = READ_REG(ENC_ERXSTL) | (READ_REG(ENC_ERXSTH) << 8);

		// Mark packets as read
		if (this->nextPacket == erxst)
		{
			WRITE_REG(ENC_ERXRDPTL, READ_REG(ENC_ERXNDL));
			WRITE_REG(ENC_ERXRDPTH, READ_REG(ENC_ERXNDH));
		}
		else
		{
			WRITE_REG(ENC_ERXRDPTL, (this->nextPacket - 1) & 0xFF);
			WRITE_REG(ENC_ERXRDPTH, ((this->nextPacket - 1) >> 8) & 0xFF);
		}
		SET_REG_BITS(ENC_ECON2, ENC_ECON2_PKTDEC);
	}

	/**
	 * Send a packet. Function will block until transmission is complete.
	 * Returns true if transmission was successful, false otherwise.
	 */
	bool ENC28J60::Send(const uint8_t *buf, uint16_t count)
	{
		WRITE_REG(ENC_ETXSTL, TX_START & 0xFF);
		WRITE_REG(ENC_ETXSTH, TX_START >> 8);

		WRITE_REG(ENC_EWRPTL, TX_START & 0xFF);
		WRITE_REG(ENC_EWRPTH, TX_START >> 8);

#ifdef _DEBUG
		printf("dest: %X:%X:%X:%X:%X:%X\n", BUF->dest.addr[0], BUF->dest.addr[1],
			BUF->dest.addr[2],  BUF->dest.addr[3], BUF->dest.addr[4],
			BUF->dest.addr[5]);
		printf("src : %X:%X:%X:%X:%X:%X\n", BUF->src.addr[0], BUF->src.addr[1],
			BUF->src.addr[2], BUF->src.addr[3], BUF->src.addr[4],
			BUF->src.addr[5]);

		printf("Type: %X\n", htons(BUF->type));
#endif

		uint8_t control = 0x00;
		this->WBM(&control, 1);

		this->WBM(buf, count);

		uint16_t txEnd = TX_START + count;
		WRITE_REG(ENC_ETXNDL, txEnd & 0xFF);
		WRITE_REG(ENC_ETXNDH, txEnd >> 8);

		// Eratta 12
		SET_REG_BITS(ENC_ECON1, ENC_ECON1_TXRST);
		CLEAR_REG_BITS(ENC_ECON1, ENC_ECON1_TXRST);

		CLEAR_REG_BITS(ENC_EIR, ENC_EIR_TXIF);
		SET_REG_BITS(ENC_ECON1, ENC_ECON1_TXRTS);

		// Wait for busy signal to clear
		uint8_t r;
		do
		{
			r = READ_REG(ENC_ECON1);
		}
		while ((r & ENC_ECON1_TXRTS) == 0);

		// Read status bits
		uint8_t status[7];
		txEnd++;
		WRITE_REG(ENC_ERDPTL, txEnd & 0xFF);
		WRITE_REG(ENC_ERDPTH, txEnd >> 8);
		this->RBM(status, 7);

		uint16_t transmit_count = status[0] | (status[1] << 8);
		
		bool retStatus = false;
		if (status[2] & 0x80)
		{
			// Transmit OK
			retStatus = true;

#ifdef _DEBUG
			printf("Transmit OK\n");
#endif

		}

		return retStatus;
	}

	/**
	 * Perform a soft reset of the ENC chip
	 */
	void ENC28J60::Reset()
	{
		GPIOPinWrite(this->resetPort, this->resetPin, 0);

		this->SPISend(0xFF);

		GPIOPinWrite(this->resetPort, this->resetPin, this->resetPin);
	}



	// Send SPI request and read return value
	uint8_t ENC28J60::SPISend(uint8_t msg)
	{
		unsigned long val;
		MAP_SSIDataPut(this->SSIbase, msg);
		MAP_SSIDataGet(this->SSIbase, &val);
		return (uint8_t)val;
	}

	

	// Read Control Register 
	uint8_t ENC28J60::RCR(uint8_t reg)
	{
		GPIOPinWrite(this->csPort, this->csPin, 0);
		this->SPISend(reg);
		uint8_t b = SPISend(0xFF);

		GPIOPinWrite(this->csPort, this->csPin, this->csPin);
		return b;
	}

	/**
	 * Read Control Register for MAC and MII registers.
	 * Reading MAC and MII registers produces an initial dummy
	 * byte. Presumably because it takes longer to fetch the values
	 * of those registers.
	 */
	uint8_t ENC28J60::RCRM(uint8_t reg)
	{
		GPIOPinWrite(this->csPort, this->csPin, 0);
		this->SPISend(reg);
		this->SPISend(0xFF);
		uint8_t b = this->SPISend(0xFF);
		GPIOPinWrite(this->csPort, this->csPin, this->csPin);

		return b;
	}

	// Write Control Register
	void ENC28J60::WCR(uint8_t reg, uint8_t val)
	{
		GPIOPinWrite(this->csPort, this->csPin, 0);
		this->SPISend(0x40 | reg);
		this->SPISend(val);
		GPIOPinWrite(this->csPort, this->csPin, this->csPin);
	}

	// Read Buffer Memory
	void ENC28J60::RBM(uint8_t *buf, uint8_t len)
	{
		GPIOPinWrite(this->csPort, this->csPin, 0);
		this->SPISend(0x20 | 0x1A);
		for (int i = 0; i < len; ++i)
		{
			*buf = this->SPISend(0xFF);
			++buf;
		}
		GPIOPinWrite(this->csPort, this->csPin, this->csPin);
	}

	// Write Buffer Memory
	void ENC28J60::WBM(const uint8_t *buf, uint8_t len)
	{
		GPIOPinWrite(this->csPort, this->csPin, 0);
		this->SPISend(0x60 | 0x1A);
		for (int i = 0; i < len; ++i)
		{
			this->SPISend(*buff);
			++buf;
		}
		GPIOPinWrite(this->csPort, this->csPin, this->csPin);
	}

	/**
	 * Bit Field Set.
	 * Set the bits of argument 'mask' in the register 'reg'.
	 * Not valid for MAC and MII registers.
	 */
	void ENC28J60::BFS(uint8_t reg, uint8_t mask)
	{
		GPIOPinWrite(this->csPort, this->csPin, 0);
		this->SPISend(0x80 | reg);
		this->SPISend(mask);
		GPIOPinWrite(this->csPort, this->csPin, this->csPin);
	}

	// Bit Field Clear
	void ENC28J60::BFC(uint8_t reg, uint8_t mask)
	{
		GPIOPinWrite(this->csPort, this->csPin, 0);
		this->SPISend(0xA0 | reg);
		this->SPISend(mask);
		GPIOPinWrite(this->csPort, this->csPin, this->csPin);
	}



	// Switch active memory bank
	void ENC28J60::SwitchBank(uint8_t new_bank)
	{
		if (new_bank == this->activeBank || new_bank == ANY_BANK)
		{
			return;
		}
		uint8_t econ1 = this->RCR(ENC_ECON1);

		econ1 &= ~ENC_ECON1_BSEL_MASK;
		econ1 |= (new_bank & ENC_ECON1_BSEL_MASK) << ENC_ECON1_BSEL_SHIFT;
		this->WCR(ENC_ECON1, econ1);
		this->activeBank = new_bank;
	}

	// Read a register's contents
	uint8_t ENC28J60::ReadRegister(uint8_t reg, uint8_t bank)
	{
		if (bank != this->activeBank)
		{
			this->SwitchBank(bank);
		}

		return this->RCR(reg);
	}

	// Read a MII register's contents 
	uint8_t ENC28J60::ReadMIIRegister(uint8_t reg, uint8_t bank)
	{
		if (bank != this->activeBank)
		{
			this->SwitchBank(bank);
		}

		return this->RCRM(reg);
	}

	// Write to a register 
	void ENC28J60::WriteRegister(uint8_t reg, uint8_t bank, uint8_t value)
	{
		if (bank != this->activeBank)
		{
			this->SwitchBank(bank);
		}

		this->WCR(reg, value);
	}



	// Batch bit field set 
	void ENC28J60::BitsFieldSet(uint8_t reg, uint8_t bank, uint8_t mask)
	{
		if (bank != this->activeBank)
		{
			this->SwitchBank(bank);
		}

		this->BFS(reg, mask);
	}

	// Batch bit field clear
	void ENC28J60::BitsFieldClear(uint8_t reg, uint8_t bank, uint8_t mask)
	{
		if (bank != this->activeBank)
		{
			this->SwitchBank(bank);
		}

		this->BFC(reg, mask);
	}


	
	/**
	* Read value from PHY address.
	* Reading procedure is described in ENC28J60 datasheet
	* section 3.3.
	*/
	uint16_t ENC28J60::ReadPHY(uint8_t address)
	{
		/*Write the address of the PHY register to read from into the
		MMIREGADR register. */
		WRITE_REG(ENC_MIREGADR, addr);

		/* Set the MICMD.MIIRD bit. The read operation begins and the
		MISTAT.BUSY bit is set. */
		WRITE_REG(ENCMICMD, 0x1);

		/* wait 10.24 μs. Poll the MISTAT.BUSY bit to be certain that the
		operation is complete. While busy, the host controller should not
		start any MIISCAN operations or write to the MIWRH register.

		When the MAC has obtained the register contents, the BUSY bit will
		clear itself. */

		MAP_SysCtlDelay(((MAP_SysCtlClockGet()/3)/1000));

		uint8_t stat;
		do
		{
			stat = READ_MREG(ENC_MISTAT);
		}
		while (stat & EMC_MISTAT_BUSY);

		/* Clear the MICMD.MIIRD bit. */
		WRITE_REG(ENCMICMD, 0x00);

		/* Read the desired data from the MIRDL and MIRDH registers. The order
		that these bytes are accessed is unimportant. */
		uint16_t reg;
		ret = READ_MREG(ENC_MIRDL) & 0xFF;
		ret |= READ_MREG(ENC_MIRDH) << 8;

		return ret;
	}

	// PHY memory write 
	void ENC28J60::WritePHY(uint8_t address, uint16_t value)
	{
		WRITE_REG(ENC_MIREGADR, addr);
		WRITE_REG(ENC_MIWRL, value & 0xFF);
		WRITE_REG(ENC_MIWRH, value >> 8);

		MAP_SysCtlDelay(((MAP_SysCtlClockGet()/3)/1000));

		// Wait for busy status to be clear before continuing
		uint8_t stat;
		do
		{
			stat = READ_MREG(ENC_MISTAT);
		}
		while (stat & ENC_MISTAT_BUSY);

	}

	// Set recieve memory area
	void ENC28J60::SetRXMemoryArea(uint16_t startAddr, uint16_t endAddr)
	{
		WRITE_REG(ENC_ERXSTL, startAddr & 0xFF);
		WRITE_REG(ENC_ERXSTH, (startAddr >> 8) & 0xFFF);

		WRITE_REG(ENC_ERXNDL, endAddr & 0xFF);
		WRITE_REG(ENC_ERXNDH, (endAddr >> 8) & 0xFFF);

		WRITE_REG(ENC_ERXRDPTL, startAddr & 0xFF);
		WRITE_REG(ENC_ERXRDPTH, (startAddr >> 8) & 0xFFF);
	}

	// Set MAC address
	void ENC28J60::SetMACAddress(const uint8_t *macAddr)
	{
		WRITE_REG(ENC_MAADR1, mac_addr[0]);
		WRITE_REG(ENC_MAADR2, mac_addr[1]);
		WRITE_REG(ENC_MAADR3, mac_addr[2]);
		WRITE_REG(ENC_MAADR4, mac_addr[3]);
		WRITE_REG(ENC_MAADR5, mac_addr[4]);
		WRITE_REG(ENC_MAADR6, mac_addr[5]);
	}

	// Get MAC address
	void ENC28J60::GetMACAddress(uint8_t *macAddr)
	{
		mac_addr[0] = READ_REG(ENC_MAADR1);
		mac_addr[1] = READ_REG(ENC_MAADR2);
		mac_addr[2] = READ_REG(ENC_MAADR3);
		mac_addr[3] = READ_REG(ENC_MAADR4);
		mac_addr[4] = READ_REG(ENC_MAADR5);
		mac_addr[5] = READ_REG(ENC_MAADR6);
	}



} // ENCJ_STELLARIS