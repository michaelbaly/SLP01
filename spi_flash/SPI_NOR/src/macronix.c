/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                macronix.c


GENERAL DESCRIPTION

    This file is for macronix SPI nor flash.

EXTERNALIZED FUNCTIONS

INITIALIZATION AND SEQUENCING REQUIREMENTS

   Copyright (c) 2014 by Marvell, Incorporated.  All Rights Reserved.
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

*===========================================================================

                        EDIT HISTORY FOR MODULE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


when         who        what, where, why
--------   ------     ----------------------------------------------------------
06/23/2014   zhoujin    Created module
===========================================================================*/

/*===========================================================================

                     INCLUDE FILES FOR MODULE

===========================================================================*/
#include "macronix.h"
#include "Flash_wk.h"

/*===========================================================================

            LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE

This section contains local definitions for constants, macros, types,
variables and other items needed by this module.

===========================================================================*/

/*===========================================================================

            EXTERN DEFINITIONS AND DECLARATIONS FOR MODULE

===========================================================================*/


/*===========================================================================

                          INTERNAL FUNCTION DEFINITIONS

===========================================================================*/
/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      MX_SPINOR_ReadReg                                                */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function read spi nor register.                              */
/*                                                                       */
/* CALLED BY                                                             */
/*                                                                       */
/*      Application                                                      */
/*                                                                       */
/* CALLS                                                                 */
/*                                                                       */
/*      Application                         The application function     */
/*                                                                       */
/* INPUTS                                                                */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/* OUTPUTS                                                               */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/*************************************************************************/
UINT8_T MX_SPINOR_ReadReg(MX_Register reg)
{
	UINT_T dummy;
	UINT8_T status, command;

	//make sure SSP is disabled
	spi_reg_bit_clr(SSP_CR0, SSP_CR0_SSE);
	//reset SSP CR's
	spi_reg_write(SSP_CR0, SSP_CR0_INITIAL);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);
	//need to use 32bits data
	spi_reg_bit_set(SSP_CR0, SSP_CR0_DSS_16);
	//fire it up
	spi_reg_bit_set(SSP_CR0, SSP_CR0_SSE);

	//load the command + 1 dummy byte
	switch (reg)
	{
		case MX_Status_Register:
			command = MX_SPI_CMD_READ_STATUS_REG;
			break;

		case MX_Config_Register:
			command = MX_SPI_CMD_READ_CONFIG_REG;
			break;

		case MX_Security_Register:
			command = MX_SPI_CMD_READ_SECURITY_REG;
			break;

		default:
			return NoError;
	}

	*SSP_DR = command << 8;

	//wait till the TX fifo is empty, then read out the status
	while((*SSP_SR & 0xF10) != 0x0);

	dummy = *SSP_DR;
	status = dummy & 0xFF;	//the status will be in the second byte

	//make sure SSP is disabled
	spi_reg_bit_clr(SSP_CR0, SSP_CR0_SSE);
	//reset SSP CR's
	spi_reg_write(SSP_CR0, SSP_CR0_INITIAL);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);

	//return last known status
	return status;
}

UINT_T MX_SPINOR_CheckStatus(void)
{
	UINT8_T status;

	status = MX_SPINOR_ReadReg(MX_Security_Register);
	//serial_outstr("security register value\n");
	//serial_outnum(status);
	//serial_outstr("\n");
	if ((status & MX_CONFIG_PFAIL) == MX_CONFIG_PFAIL)
	{
		CPUartLogPrintf("Program fail");
		return SPINORPROGRAMFAIL;
	}

	if ((status & MX_CONFIG_EFAIL) == MX_CONFIG_EFAIL)
	{
		CPUartLogPrintf("Erase fail");
		return SPINORERASEFAIL;
	}

	return NoError;
}

void MX_SPINOR_WriteStatus(unsigned char status, unsigned char config)
{
	unsigned int temp;
    UINT_T Retval = NoError;

	//reset SSP CR's
	spi_reg_write(SSP_CR0, SSP_CR0_INITIAL);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);

	//need to use 24bit data
	spi_reg_bit_set(SSP_CR0, SSP_CR0_DSS_24);
	//fire it up
	spi_reg_bit_set(SSP_CR0, SSP_CR0_SSE);

	temp = MX_SPI_CMD_WRITE_STATUS_REG << 16;
	temp |= status << 8;
	temp |= config;

	spi_reg_write(SSP_DR, temp);

	Retval = SPINOR_WaitSSPComplete();
    if(Retval != NoError)
    {
        CPUartLogPrintf("MX_SPINOR_WriteStatus status 0x%x", Retval);
    }

	temp = *SSP_DR;

	//make sure SSP is disabled
	spi_reg_bit_clr(SSP_CR0, SSP_CR0_SSE);
	//reset SSP CR's
	spi_reg_write(SSP_CR0, SSP_CR0_INITIAL);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);

	return;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      MX_Read                                                          */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function read data form spi nor flash.                       */
/*                                                                       */
/* CALLED BY                                                             */
/*                                                                       */
/*      Application                                                      */
/*                                                                       */
/* CALLS                                                                 */
/*                                                                       */
/*      Application                         The application function     */
/*                                                                       */
/* INPUTS                                                                */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/* OUTPUTS                                                               */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/*************************************************************************/
UINT_T MX_Read(UINT_T FlashOffset, UINT_T Buffer, UINT_T Size, UINT_T CopySize)
{
	UINT_T  i = 0;
	UINT_T *temp_buff = NULL;
	UINT_T	read_size = 0, total_size = 0, Retval = NoError, un_read_size = 0, read_buff = 0;
	DMA_CMDx_T RX_Cmd, TX_Cmd;
    volatile int timeout = 0;

	//remember how much (in words) is Read - for endian convert at the end of routine
	total_size = Size >> 2;

	read_buff = DMA_Read_Buffer;

	//tx0 - load the SPI command and address (0xCmd_AddrHI_AddrMid_AddrLow)
	//tx1_command  = (pSPI_commands->SPI_READ_CMD << 24) | (FlashOffset & 0x00FFFFFF);

	//fill out commands
	RX_Cmd.value = 0;
	RX_Cmd.bits.IncSrcAddr = 0;
	RX_Cmd.bits.IncTrgAddr = 1;
	RX_Cmd.bits.FlowSrc = 1;
	RX_Cmd.bits.FlowTrg = 0;
	RX_Cmd.bits.Width = 3;
	RX_Cmd.bits.MaxBurstSize = 2;

	TX_Cmd.value = 0;
	TX_Cmd.bits.IncSrcAddr = 0;
	TX_Cmd.bits.IncTrgAddr = 0;
	TX_Cmd.bits.FlowSrc = 0;
	TX_Cmd.bits.FlowTrg = 1;
	TX_Cmd.bits.Width = 3;
	TX_Cmd.bits.MaxBurstSize = 2;

	//setup DMA
	//Map Device to Channels
	XllpDmacMapDeviceToChannel(SSP_RX_DMA_DEVICE, SSP_RX_CHANNEL);
	XllpDmacMapDeviceToChannel(SSP_TX_DMA_DEVICE, SSP_TX_CHANNEL);

	//turn ON user alignment - in case buffer address is 64bit aligned
	alignChannel(SSP_RX_CHANNEL, 1);

	//reset SSP CR's
	spi_reg_write(SSP_CR0, SSP_CR0_INITIAL);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL | 0x300cc3 | SSP_SSCR1_TRAIL | SSP_SSCR1_TINTE);
    spi_reg_write(SSP_TO, 0xFFFFF);

	//setup in 32bit mode
	spi_reg_bit_set(SSP_CR0, SSP_CR0_DSS_32);

	//fire SSP up
	spi_reg_bit_set(SSP_CR0, SSP_CR0_SSE);

	//for each loop iteration, one Read Command will be issued with a link descriptor chain
	//	the chain will read a total of (SSP_READ_DMA_DESC-1)*SSP_READ_DMA_SIZE bytes from SPI

	un_read_size = Size + 12;
	//un_read_size = Size;

	//while (Size > 0)
	{
		// * configure RX & TX descriptors * //
		//initial 1 word transfer:
		//		TX: send command+address
		//		RX: receive dummy word
		tx_4bytes_read[0] = (MX_SPI_CMD_READ << 24) | (FlashOffset >> 8);
		tx_4bytes_read[1] = (FlashOffset & 0xFF) << 24;

		//configDescriptor(&RX_Desc[0], &RX_Desc[1],(UINT_T)SSP_DR,			(UINT_T)&rx1_fromcommand, &RX_Cmd, 4, 0);
		//configDescriptor(&TX_Desc[0], &TX_Desc[1],(UINT_T)&tx1_command,	(UINT_T)SSP_DR, 			&TX_Cmd, 4, 0);
		//Buffer -=1;

		i = 0;
		//chaining of full transfers (descriptors that are not "stop"ped)
		//fill out descriptors until either:
		//	- there is only enough data for 1 more descriptor (which needs to be the "stopped" descriptor)
		//	- the pool of descriptors is depleted (minus 1 because the last needs to be "stopped")
		while( (un_read_size > SSP_READ_DMA_SIZE) && (i < (SSP_READ_DMA_DESC-1)) )
		{
			configDescriptor(&RX_Desc[i], &RX_Desc[i+1],	(UINT_T)SSP_DR,			(UINT_T)read_buff, &RX_Cmd, SSP_READ_DMA_SIZE, 0);
			configDescriptor(&TX_Desc[i], &TX_Desc[i+1],	(UINT_T)&tx_4bytes_read,	(UINT_T)SSP_DR, &TX_Cmd, SSP_READ_DMA_SIZE, 0);

			//Update counters
			read_buff 		+=	SSP_READ_DMA_SIZE;
			//FlashOffset +=	SSP_READ_DMA_SIZE;
			un_read_size 		-=	SSP_READ_DMA_SIZE;
			i++;

		}

		//last link: descriptor must be "stopped"
		read_size 	=  un_read_size > SSP_READ_DMA_SIZE ? SSP_READ_DMA_SIZE : un_read_size;
		configDescriptor(&RX_Desc[i], NULL,	(UINT_T)SSP_DR,			(UINT_T)read_buff, &RX_Cmd, read_size, 1);
		configDescriptor(&TX_Desc[i], NULL,	(UINT_T)&tx_4bytes_read,	(UINT_T)SSP_DR, &TX_Cmd, read_size, 1);

		//update counters after "stop" descriptor
		read_buff 		+= read_size;
		//FlashOffset += read_size;
		un_read_size 		-= read_size;

		//Load descriptors
		loadDescriptor (&RX_Desc[0], SSP_RX_CHANNEL);
		loadDescriptor (&TX_Desc[0], SSP_TX_CHANNEL);

		//Kick off DMA's
		XllpDmacStartTransfer(SSP_RX_CHANNEL);
		XllpDmacStartTransfer(SSP_TX_CHANNEL);

		//setup a timer to fail gracefully in case of error
		timeout = 0xFFFFF;

		//wait until the RX channel gets the stop UINT_Terrupt and the TX fifo is drained
		while( ((readDmaStatusRegister(SSP_RX_CHANNEL) & XLLP_DMAC_DCSR_STOP_INTR) != XLLP_DMAC_DCSR_STOP_INTR) ||
			   ((*SSP_SR & 0xF10) != 0x0) )
		{
			//if we've waited long enough, fail
			if((timeout--) <= 0)
			{
			    CPUartLogPrintf("DMA Status 0x%x, SSP Status 0x%x", readDmaStatusRegister(SSP_RX_CHANNEL), *SSP_SR);
				return SSPRxChannelTimeOutError;
			}
		}

		Retval = SPINOR_WaitSSPComplete();
	}

	//if we errored out, kill the DMA transfers
	if(Retval != NoError)
	{
		XllpDmacStopTransfer( SSP_RX_CHANNEL );
		XllpDmacStopTransfer( SSP_TX_CHANNEL );
	}

	//make sure SSP is disabled
	spi_reg_bit_clr(SSP_CR0, SSP_CR0_SSE);
	//reset SSP CR's
	spi_reg_write(SSP_CR0, SSP_CR0_INITIAL);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);

	//clear out DMA settings
	XllpDmacUnMapDeviceToChannel(SSP_RX_DMA_DEVICE, SSP_RX_CHANNEL);
	XllpDmacUnMapDeviceToChannel(SSP_TX_DMA_DEVICE, SSP_TX_CHANNEL);

	temp_buff = (UINT_T *)DMA_Read_Buffer;
	for( i = 0; i < total_size + 2; i ++ )
	{
		temp_buff[i] = SPINOR_Endian_Convert( temp_buff[i] );
	}
	//temp_buff[i] = Endian_Convert( temp_buff[i] ); // last byte

	memcpy( (UINT8_T *)Buffer, (UINT8_T *)(DMA_Read_Buffer + 5), CopySize );

	//postprocessing... endian convert
	//for(i = 0; i < total_size; i++)
	//	buff[i] = Endian_Convert(buff[i]);

	return Retval;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      MX_SPINOR_Read                                                   */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function read data form spi nor flash.                       */
/*                                                                       */
/* CALLED BY                                                             */
/*                                                                       */
/*      Application                                                      */
/*                                                                       */
/* CALLS                                                                 */
/*                                                                       */
/*      Application                         The application function     */
/*                                                                       */
/* INPUTS                                                                */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/* OUTPUTS                                                               */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/*************************************************************************/
UINT_T MX_SPINOR_Read(UINT_T FlashOffset, UINT_T Buffer, UINT_T Size)
{
	UINT_T Retval = NoError, read_size = 0;

	do {

		read_size = Size > SIZE_64KB ? SIZE_64KB : Size;

		Retval = MX_Read(FlashOffset, Buffer, SIZE_64KB, read_size);

		//update counters
		FlashOffset += read_size;
		Buffer += read_size;
		Size -= read_size;

	} while( (Size > 0) && (Retval == NoError) );

	return Retval;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      MX_Write                                                         */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function write data to spi nor flash.                        */
/*                                                                       */
/* CALLED BY                                                             */
/*                                                                       */
/*      Application                                                      */
/*                                                                       */
/* CALLS                                                                 */
/*                                                                       */
/*      Application                         The application function     */
/*                                                                       */
/* INPUTS                                                                */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/* OUTPUTS                                                               */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/*************************************************************************/
UINT_T MX_Write(UINT_T Address, UINT_T Buffer, UINT_T Size)
{
	DMA_CMDx_T TX_Cmd, RX_Cmd;
	UINT_T *ptr = NULL;
    UINT_T Retval = NoError, i = 0;
    volatile int timeout = 0;

	//because the SSP controller is crap, we need to
	//use DMA to input the five bytes of data:
	TX_Cmd.value = 0;
	TX_Cmd.bits.IncSrcAddr = 1;
	TX_Cmd.bits.IncTrgAddr = 0;
	TX_Cmd.bits.FlowSrc = 0;
	TX_Cmd.bits.FlowTrg = 1;
	TX_Cmd.bits.Width = 3;
	TX_Cmd.bits.MaxBurstSize = 1;
	//TX_Cmd.bits.Length = 8; // 8 bytes

	RX_Cmd.value = 0;
	RX_Cmd.bits.IncSrcAddr = 0;
	RX_Cmd.bits.IncTrgAddr = 0;
	RX_Cmd.bits.FlowSrc = 0;
	RX_Cmd.bits.FlowTrg = 0;
	RX_Cmd.bits.Width = 3;
	RX_Cmd.bits.MaxBurstSize = 1;
	//RX_Cmd.bits.Length = 8; // 8 bytes

	//setup DMA
	//Map Device to Channel
	XllpDmacMapDeviceToChannel(SSP_TX_DMA_DEVICE, SSP_TX_CHANNEL);
	XllpDmacMapDeviceToChannel(SSP_RX_DMA_DEVICE, SSP_RX_CHANNEL);

	//turn ON user alignment - in case buffer address is 64bit aligned
	alignChannel(SSP_TX_CHANNEL, 1);

	spi_reg_write(SSP_CR0, SSP_CR0_INITIAL);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL | 0x300083);

	//setup in 32bit mode
	spi_reg_bit_set(SSP_CR0, SSP_CR0_DSS_32);

	//fire SSP up
	spi_reg_bit_set(SSP_CR0, SSP_CR0_SSE);

	DMA_Buf[0] = Address & 0xFF;
	memcpy(&DMA_Buf[1], Buffer, Size );
	memcpy(&DMA_Buf[Size+1], Buffer, 3 );

	DMA_PP[0] = (MX_SPI_CMD_PAGE_PROGRAM << 24) | (Address >> 8);
	ptr = (UINT_T *)DMA_Buf;

	for( i = 1; i < ((Size >> 2) + 2); i++ )
	{
		DMA_PP[i] = SPINOR_Endian_Convert( ptr[i-1] );
	}

	//configDescriptor(&RX_Desc[0], &RX_Desc[1],(UINT_T)SSP_DR,		(UINT_T)&rx1_fromcommand, &RX_Cmd, Size + 4, 0);
	//configDescriptor(&TX_Desc[0], &TX_Desc[1],(UINT_T)DMA_PP,	(UINT_T)SSP_DR, 	  &TX_Cmd, Size + 4, 0);
	configDescriptor(&RX_Desc[0], NULL,	(UINT_T)SSP_DR,	(UINT_T)&rx1_fromcommand, &RX_Cmd, Size + 8, 1);
	configDescriptor(&TX_Desc[0], NULL,	(UINT_T)DMA_PP,	(UINT_T)SSP_DR, 			&TX_Cmd, Size + 8, 1);


	//Load descriptors
	loadDescriptor (&TX_Desc[0], SSP_TX_CHANNEL);
	loadDescriptor (&RX_Desc[0], SSP_RX_CHANNEL);

	//Kick off DMA's
	XllpDmacStartTransfer(SSP_TX_CHANNEL);
	XllpDmacStartTransfer(SSP_RX_CHANNEL);

	//timer loop waiting for dma to finish
	//setup a timer to fail gracefully in case of error
	timeout = 0xFFFFF;

	//wait until the RX channel gets the stop UINT_Terrupt and the TX fifo is drained
	while( ((readDmaStatusRegister(SSP_TX_CHANNEL) & XLLP_DMAC_DCSR_STOP_INTR) != XLLP_DMAC_DCSR_STOP_INTR) ||
		   ((*SSP_SR & 0xF14) != 0x4) )
	{
		//if we've waited long enough, fail
		if((timeout--) <= 0)
		{
		    CPUartLogPrintf("DMA Status 0x%x, SSP Status 0x%x", readDmaStatusRegister(SSP_TX_CHANNEL), *SSP_SR);
			return SSPTxChannelTimeOutError;
		}
	}

	//if we errored out, kill the DMA transfers
	if(Retval != NoError)
	{
		XllpDmacStopTransfer( SSP_TX_CHANNEL );
		XllpDmacStopTransfer( SSP_RX_CHANNEL );
	}

	Retval = SPINOR_WaitSSPComplete();

	//make sure SSP is disabled
	SPINOR_DisableSSP();

	//clear out DMA settings
	XllpDmacUnMapDeviceToChannel(SSP_TX_DMA_DEVICE, SSP_TX_CHANNEL);
	XllpDmacUnMapDeviceToChannel(SSP_RX_DMA_DEVICE, SSP_RX_CHANNEL);

	return Retval;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      MX_SPINOR_Write                                                  */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function write data to spi nor flash.                        */
/*                                                                       */
/* CALLED BY                                                             */
/*                                                                       */
/*      Application                                                      */
/*                                                                       */
/* CALLS                                                                 */
/*                                                                       */
/*      Application                         The application function     */
/*                                                                       */
/* INPUTS                                                                */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/* OUTPUTS                                                               */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/*************************************************************************/
UINT_T MX_SPINOR_Write(UINT_T Address, UINT_T Buffer, UINT_T Size)
{
	UINT_T Retval = NoError, write_size = 0;
    volatile int timeout = 0;

    /* make size 4bytes-align */
	if(Size & 0x3)
	{
    	Size = (Size+4)&(~3);
    }


	do {

	    timeout = 0xFF;

	    do {
    		//make sure the device is ready to be written to
    		Retval = SPINOR_ReadStatus(TRUE);

    		//get device ready to Program
    		SPINOR_WriteEnable();

            Retval = SPINOR_WaitForWEL(TRUE);

		} while( ((--timeout) > 0) && (Retval != NoError) );

        if(Retval != NoError)
        {
            break;
        }

		write_size = Size > WRITE_SIZE ? WRITE_SIZE : Size;

		//write a byte
		if (write_size == WRITE_SIZE)
		{
			Retval = MX_Write(Address, Buffer, WRITE_SIZE);

			//update counters
			Address += WRITE_SIZE;
			Buffer += WRITE_SIZE;
			Size -= WRITE_SIZE;
		}
		else
		{
			Retval = MX_Write(Address, Buffer, write_size);
			Size = 0;
		}

		Retval = SPINOR_ReadStatus(TRUE);
        if(Retval != NoError)
    	{
    		break;
    	}

		Retval = MX_SPINOR_CheckStatus();

	} while( (Size > 0) && (Retval == NoError) );

	return Retval;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      MX_EraseSector                                                   */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function erase spi nor flash sector.                         */
/*                                                                       */
/* CALLED BY                                                             */
/*                                                                       */
/*      Application                                                      */
/*                                                                       */
/* CALLS                                                                 */
/*                                                                       */
/*      Application                         The application function     */
/*                                                                       */
/* INPUTS                                                                */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/* OUTPUTS                                                               */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/*************************************************************************/
UINT_T MX_EraseSector(UINT_T secAddr)
{
    UINT_T Retval = NoError;
	DMA_CMDx_T RX_Cmd, TX_Cmd;
    volatile int timeout = 0;

	timeout = 0xFF;

    do {
		//make sure the device is ready to be written to
		Retval = SPINOR_ReadStatus(TRUE);

		//get device ready to Program
		SPINOR_WriteEnable();

        Retval = SPINOR_WaitForWEL(TRUE);

	} while( ((--timeout) > 0) && (Retval != NoError) );

    if(Retval != NoError)
    {
        return Retval;
    }

	TX_Cmd.value = 0;
	TX_Cmd.bits.IncSrcAddr = 1;
	TX_Cmd.bits.IncTrgAddr = 0;
	TX_Cmd.bits.FlowSrc = 0;
	TX_Cmd.bits.FlowTrg = 1;
	TX_Cmd.bits.Width = 1;
	TX_Cmd.bits.MaxBurstSize = 1;
	//TX_Cmd.bits.Length = 5;

	RX_Cmd.value = 0;
	RX_Cmd.bits.IncSrcAddr = 0;
	RX_Cmd.bits.IncTrgAddr = 0;
	RX_Cmd.bits.FlowSrc = 0;
	RX_Cmd.bits.FlowTrg = 1;
	RX_Cmd.bits.Width = 1;
	RX_Cmd.bits.MaxBurstSize = 1;

	TX_Cmd.value = 0x90014000;
	RX_Cmd.value = 0x10014000;

	tx_1bytes[0] = MX_SPI_CMD_SECTOR_ERASE;
	tx_1bytes[1] = (secAddr >> 24) & 0xFF;
	tx_1bytes[2] = (secAddr >> 16) & 0xFF;
	tx_1bytes[3] = (secAddr >> 8) & 0xFF;
	tx_1bytes[4] = (secAddr >> 0) & 0xFF;

	//setup DMA
	//Map Device to Channel
	XllpDmacMapDeviceToChannel(SSP_TX_DMA_DEVICE, SSP_TX_CHANNEL);
	XllpDmacMapDeviceToChannel(SSP_RX_DMA_DEVICE, SSP_RX_CHANNEL);

	spi_reg_write(SSP_CR0, SSP_CR0_INITIAL);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL | 0x700083);

	spi_reg_write(SSP_CR0, SSP_CR0_DSS_8);
	spi_reg_bit_set(SSP_CR0, SSP_CR0_SSE);

	configDescriptor(&RX_Desc[0], NULL,(UINT_T)SSP_DR, (UINT_T)rx_1bytes, &RX_Cmd, 5, 1);
	configDescriptor(&TX_Desc[0], NULL,(UINT_T)tx_1bytes,	(UINT_T)SSP_DR,  &TX_Cmd, 5, 1);

	//Load descriptors
	loadDescriptor (&TX_Desc[0], SSP_TX_CHANNEL);
	loadDescriptor (&RX_Desc[0], SSP_RX_CHANNEL);

	//Kick off DMA's
	XllpDmacStartTransfer(SSP_TX_CHANNEL);
	XllpDmacStartTransfer(SSP_RX_CHANNEL);

	//timer loop waiting for dma to finish
	//setup a timer to fail gracefully in case of error
	timeout = 0xFFFFF;

	//wait until the RX channel gets the stop UINT_Terrupt and the TX fifo is drained
	while( ((readDmaStatusRegister(SSP_TX_CHANNEL) & XLLP_DMAC_DCSR_STOP_INTR) != XLLP_DMAC_DCSR_STOP_INTR) ||
		   ((*SSP_SR & 0xF14) != 0x4) )
	{
		//if we've waited long enough, fail
		if((timeout--) <= 0)
		{
		    CPUartLogPrintf("DMA Status 0x%x, SSP Status 0x%x", readDmaStatusRegister(SSP_TX_CHANNEL), *SSP_SR);
			return SSPTxChannelTimeOutError;
		}
	}

	//if we errored out, kill the DMA transfers
	if(Retval != NoError)
	{
		XllpDmacStopTransfer( SSP_TX_CHANNEL );
		XllpDmacStopTransfer( SSP_RX_CHANNEL );
	}

	Retval = SPINOR_WaitSSPComplete();

	//make sure SSP is disabled
	SPINOR_DisableSSP();

	//clear out DMA settings
	XllpDmacUnMapDeviceToChannel(SSP_TX_DMA_DEVICE, SSP_TX_CHANNEL);
	XllpDmacUnMapDeviceToChannel(SSP_RX_DMA_DEVICE, SSP_RX_CHANNEL);

	return Retval;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      MX_SPINOR_Erase                                                  */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function erase spi nor flash.                                */
/*                                                                       */
/* CALLED BY                                                             */
/*                                                                       */
/*      Application                                                      */
/*                                                                       */
/* CALLS                                                                 */
/*                                                                       */
/*      Application                         The application function     */
/*                                                                       */
/* INPUTS                                                                */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/* OUTPUTS                                                               */
/*                                                                       */
/*      None                                N/A                          */
/*                                                                       */
/*************************************************************************/
UINT_T MX_SPINOR_Erase(UINT_T Address, UINT_T Size)
{
	UINT_T numSectors = 0, i = 0, sector_size = 0, Retval = NoError;
	P_FlashProperties_T pFlashP = GetFlashProperties(BOOT_FLASH);

	sector_size = pFlashP->BlockSize;

	if ((Size % pFlashP->BlockSize) == 0)
	{
		numSectors = Size / pFlashP->BlockSize;
	}
	else
	{
		numSectors = Size / pFlashP->BlockSize + 1;
    }

	for (i = 0; i < numSectors; i++)
	{
		//erase this sector
		Retval = MX_EraseSector(Address);
		//Retval |= MX_SPINOR_CheckStatus();

		Address += sector_size;

		if (Retval != NoError)
		{
			break;
		}
	}

	return Retval;
}
