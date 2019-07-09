/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                SPI.c


GENERAL DESCRIPTION

    This file is for SPI nor flash.

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
#include "spi.h"
#include "Typedef.h"
#include "Errors.h"
#include "giga.h"
#include "macronix.h"
#include "PlatformConfig.h"
#include "spi_platform.h"
#include "gpio.h"


/*===========================================================================

            LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE

This section contains local definitions for constants, macros, types,
variables and other items needed by this module.

===========================================================================*/

/* SPI flash ID */
UINT_T SPI_FlashID = 0;

#pragma arm section rwdata="DMARdBuf", zidata="DMARdBuf"

/* Tx DMA descriptor */
__align(16) XLLP_DMAC_DESCRIPTOR_T	TX_Desc[SSP_READ_DMA_DESC];

/* Rx DMA descriptor */
__align(16) XLLP_DMAC_DESCRIPTOR_T	RX_Desc[SSP_READ_DMA_DESC];

// contains command opcode and address
__align(16) UINT_T tx1_command, tx_4bytes_read[2];

__align(16) UINT8_T tx_1bytes[5], rx_1bytes[5];

__align(8) UINT_T DMA_PP[66];

__align(8) UINT8_T DMA_Buf[260];

__align(8) UINT8_T DMA_Read_Buffer[MAX_TEST_SIZE+16] = {0};

// receive garbage clocked in while command is transmitted
__align(16) UINT_T rx1_fromcommand;

#pragma arm section rwdata, zidata

/*===========================================================================

            EXTERN DEFINITIONS AND DECLARATIONS FOR MODULE

===========================================================================*/


/*===========================================================================

                          INTERNAL FUNCTION DEFINITIONS

===========================================================================*/




#if defined(FEATURE_FLASH256_SUPPORT)

#define APBC_SSP2_CLK_RST       (APB_AP_CLOCK_CTRL_BASE + 0x004C)   //Clock/Reset Control Register for SSP 2
#define APB_AP_CLOCK_CTRL_BASE    0xD4015000

#define SPINOR_CS1		1
#define SPINOR_CS2		2

#define  SPI_FLASH_OUT1      	*(( volatile UINT32 *)(0xD4019000+0x0C)) |= (1 << 25) //GPIO25 OUT
#define  SPI_FLASH_CS1_L 		*(( volatile UINT32 *)(0xD4019000+0x24)) |= (1 << 25)
#define  SPI_FLASH_CS1_H		*(( volatile UINT32 *)(0xD4019000+0x18)) |= (1 << 25)

#define  SPI_FLASH_OUT2    		*(( volatile UINT32 *)(0xD4019004+0x0C)) |= (1 << 17)  //GPIO49 OUT
#define  SPI_FLASH_CS2_L 		*(( volatile UINT32 *)(0xD4019004+0x24)) |= (1 << 17)
#define  SPI_FLASH_CS2_H		*(( volatile UINT32 *)(0xD4019004+0x18)) |= (1 << 17)

#define GPIO_CS_BIT25   25 /* SPI_CS: using GPIO025, 25%32=25 */
#define GPIO_CS_BIT49 49
#define	pGPIO_LR  	(volatile int *)(GPIO0_BASE + GPIO_PLR)  //Pin level. set 0    
#define	pGPIO_DR  	(volatile int *)(GPIO0_BASE + GPIO_PDR)  //Direction. set 0
#define	pGPIO_SR  	(volatile int *)(GPIO0_BASE + GPIO_PSR)  //Set. set 0
#define	pGPIO_CR  	(volatile int *)(GPIO0_BASE + GPIO_PCR)  //Clear. set 0   0xD4019000
#define	pGPIO_SDR 	(volatile int *)(GPIO0_BASE + GPIO_SDR)  //Bit set. set 0

#define	pGPIO_LR1  	(volatile int *)(GPIO1_BASE + GPIO_PLR)  //Pin level. set 0    
#define	pGPIO_DR1  	(volatile int *)(GPIO1_BASE + GPIO_PDR)  //Direction. set 0
#define	pGPIO_SR1  	(volatile int *)(GPIO1_BASE + GPIO_PSR)  //Set. set 0
#define	pGPIO_CR1  	(volatile int *)(GPIO1_BASE + GPIO_PCR)  //Clear. set 0   0xD4019000
#define	pGPIO_SDR1 	(volatile int *)(GPIO1_BASE + GPIO_SDR)  //Bit set. set 0


#define GPIO_CS_SET25    (1<< GPIO_CS_BIT25)
#define GPIO_CS_CLEAR25  (!GPIO_CS_BIT25)
#define GPIO_CS_SET49    (1<< (GPIO_CS_BIT49%32))
#define GPIO_CS_CLEAR49  (!(GPIO_CS_BIT49%32))
UINT8 status_flag=1;

const CS_REGISTER_PAIR_S spi_pins_cs2[]=
{
  		(int *) (APPS_PAD_BASE | 0x10), 0x0882, 0x0,	//RX
		(int *) (APPS_PAD_BASE | 0x14), 0x0882, 0x0,	//TX
  		(int *) (APPS_PAD_BASE | 0x18), 0xc8c1, 0x0,	//GPIO_25[SSP2_FRM]
  		(int *) (APPS_PAD_BASE | 0x1C), 0x1082, 0x0,	//CLK
  		(int *) (APPS_PAD_BASE | 0x140), 0xc8c7, 0x0,	//NULL[GPIO_25]
  		0x0,0x0,0x0 //termination
};

static void ConfigRegWrite( P_CS_REGISTER_PAIR_S regPtr)
{
    UINT32_T i,tmp;
	while(regPtr->registerAddr != 0x0)
    {
      *(regPtr->registerAddr) = regPtr->regValue;
	  tmp = *(regPtr->registerAddr);  // ensure write complete
      regPtr++;
    }
}
void Assert_CS(void)
{
	if(1 == status_flag){
 		*pGPIO_CR |= GPIO_CS_SET25;
 		while (*pGPIO_LR & GPIO_CS_SET25);
	}
	if(2 == status_flag){
 		*pGPIO_CR1 |= GPIO_CS_SET49;
 		while (*pGPIO_LR1 & GPIO_CS_SET49);
	}
}

void Deassert_CS()
{
	if(1 == status_flag){
		*pGPIO_SR |= GPIO_CS_SET25;
		while (!(*pGPIO_LR & GPIO_CS_SET25));
	}
	if(2 == status_flag){
		*pGPIO_SR1 |= GPIO_CS_SET49;
		while (!(*pGPIO_LR1 & GPIO_CS_SET49));
	}
	
}

void ChipSelectSPI_CS2( void )
{
	//enabled SSP2 clock, then take out of reset
  	*(volatile unsigned int *)(APBC_SSP2_CLK_RST) = 0x7;	  // (This is really SSP3, inconsistent naming nomenclature) 0xD401504C
  	*(volatile unsigned int *)(APBC_SSP2_CLK_RST) = 0x3;	  // (This is really SSP3, inconsistent naming nomenclature) 0xD401504C

	ConfigRegWrite(spi_pins_cs2);
	
	return;
}

#endif

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      InitializeSPIDevice                                              */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function intializes SPI nor device                           */
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
BOOL CheckIf32MNOR( void )
{
    P_FlashProperties_T pFlashP = GetFlashProperties(BOOT_FLASH);

    if((pFlashP->BlockSize * pFlashP->NumBlocks) == 0x2000000)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      InitializeSPIDevice                                              */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function intializes SPI nor device                           */
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
UINT_T InitializeSPIDevice(UINT8_T FlashNum, FlashBootType_T FlashBootType, UINT8_T *P_DefaultPartitionNum)
{
	UINT_T Retval = NoError;
	P_FlashProperties_T pFlashP = GetFlashProperties(BOOT_FLASH);

#if defined(FEATURE_FLASH256_SUPPORT)	
	ChipSelectSPI_CS2();
	CPUartLogPrintf("[#01#]enter1...%s",__FUNCTION__);
	
	//*(( volatile UINT32 *)(0xD401E1A0)) = 0xc8c0;
	SPINOR_CS_Enable(SPINOR_CS1);	
#else
	ChipSelectSPI(SSP_CLOCK_26M);
#endif
	SPINOR_Reset();

	SPINOR_ReadId(&SPI_FlashID);

	//setup Flash Properties info
	pFlashP->BlockSize = 0x10000; // 64KB
    pFlashP->PageSize = 0x100; // 256B
    pFlashP->NumBlocks = 0x100;	//256B

	SetCurrentFlashBootType(BOOT_FLASH);

	switch(SPI_FlashID)
	{
		case 0x010219:
		case 0xc22539:
		{
            pFlashP->NumBlocks      = 0x200;	// 32MB
			pFlashP->ReadFromFlash 	= (ReadFlash_F)(&MX_SPINOR_Read);
			pFlashP->WriteToFlash 	= (WriteFlash_F)(&MX_SPINOR_Write);
			pFlashP->EraseFlash 	= (EraseFlash_F)(&MX_SPINOR_Erase);
			break;
        }

		case 0xc86019:
		{
    	    pFlashP->NumBlocks      = 0x200;	// 32MB
			pFlashP->ReadFromFlash 	= (ReadFlash_F)(&Giga_SPINor_Read);
			pFlashP->WriteToFlash 	= (WriteFlash_F)(&Giga_SPINor_Write);
			pFlashP->EraseFlash 	= (EraseFlash_F)(&Giga_SPINor_Erase);
			break;
        }
		case 0xf84218:
		{
	#if defined(FEATURE_FLASH256_SUPPORT)
			pFlashP->ReadFromFlash 	= (ReadFlash_F)(&SPINOR_Read_32);
			pFlashP->WriteToFlash 	= (WriteFlash_F)(&SPINOR_Write_32);
			pFlashP->EraseFlash 	= (EraseFlash_F)(&SPINOR_Erase_32);
			break;
	#endif
		}

		default:
        {
			pFlashP->ReadFromFlash 	= (ReadFlash_F)(&SPINOR_Read);
			pFlashP->WriteToFlash 	= (WriteFlash_F)(&SPINOR_Write);
			pFlashP->EraseFlash 	= (EraseFlash_F)(&SPINOR_Erase);
			break;
		}
	}

    pFlashP->ResetFlash = NULL;
    pFlashP->FlashSettings.UseBBM = 0;
    pFlashP->FlashSettings.UseSpareArea = 0;
    pFlashP->FlashSettings.SASize = 0;
    pFlashP->FlashSettings.UseHwEcc = 0;
	pFlashP->FlashType = SPI_NOR_FLASH;
	pFlashP->FinalizeFlash = NULL;
    //---------------------------------------
	pFlashP->TimFlashAddress = 0;
	*P_DefaultPartitionNum = 0;

    CPUartLogPrintf("InitializeSPIDevice done, flash ID 0x%x", SPI_FlashID);
	return Retval;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_ROW_DELAY                                                 */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function delay some times.                                   */
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
void SPINOR_ROW_DELAY(UINT_T x)
{
    volatile long i = 0;

    i = x;

	while (i > 0)
	{
		i--;
	}
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_Endian_Convert                                            */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function is endian conversion function.                      */
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
UINT_T SPINOR_Endian_Convert (UINT_T in)
{
	unsigned int out;
	out = in << 24;
	out |= (in & 0xFF00) << 8;
	out |= (in & 0xFF0000) >> 8;
	out |= (in & 0xFF000000) >> 24;
	return out;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_DisableSSP                                                */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function disable SSP.                                        */
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
void SPINOR_DisableSSP(void)
{
	//make sure SSP is disabled
	spi_reg_bit_clr(SSP_CR0, SSP_CR0_SSE);
}


/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_WaitSSPComplete                                           */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function wait for SSP completion.                            */
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
UINT_T SPINOR_WaitSSPComplete(void)
{
    volatile int timeout = 0;

	timeout = 0xFFFFF;

	while (*SSP_SR & (SSP_SSSR_BSY | SSP_SSSR_TFL))
	{
	    if((timeout--) <= 0)
		{
		    CPUartLogPrintf("SSP Status 0x%x", *SSP_SR);
			return SSPWaitCompleteTimeOutError;
		}

		SPINOR_ROW_DELAY(DEFAULT_TIMEOUT);
	}

	return NoError;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_Reset                                                     */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function reset SPI nor.                                      */
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
void SPINOR_Reset(void)
{
	UINT_T temp;
    UINT_T Retval = NoError;
	
#if defined(FEATURE_FLASH256_SUPPORT)
	*pGPIO_SDR |= GPIO_CS_SET25;
	*pGPIO_DR  |= GPIO_CS_SET25;
	
	Deassert_CS();
#endif
	spi_reg_write(SSP_CR0, SSP_CR0_DSS_8);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);
	spi_reg_bit_set(SSP_CR0, SSP_SSCR0_SSE);
	
#if defined(FEATURE_FLASH256_SUPPORT)	
	Assert_CS();
#endif	
	spi_reg_write(SSP_DR, SPI_CMD_RELEASE_POWER_DOWN);

	Retval = SPINOR_WaitSSPComplete();
    if(Retval != NoError)
    {
        CPUartLogPrintf("SPINOR_Reset status 0x%x", Retval);
    }
	
#if defined(FEATURE_FLASH256_SUPPORT)	
	Deassert_CS();
#endif

	temp = *SSP_DR;

	SPINOR_DisableSSP();

	SPINOR_ROW_DELAY(1000);

	return;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_ReadStatus                                                */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function read SPI nor status.                                */
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
UINT_T SPINOR_ReadStatus(UINT_T Wait)
{
	UINT_T read = 0, ready = 0, dummy = 0, status = 0;
	volatile int timeout = 0, timeout2 = 0;

	timeout = 0xFFFFF;

	read = FALSE;	//this flag gets set when we read first entry from fifo
	//if the caller waits to 'Wait' for the BUSY to be cleared, start READY off as FALSE
	//if the caller doesn't wait to wait, set READY as true, so we don't wait on the bit
	ready = (Wait) ? FALSE : TRUE;

	do{
		//make sure SSP is disabled
		spi_reg_bit_clr(SSP_CR0, SSP_CR0_SSE);
		//reset SSP CR's
		spi_reg_write(SSP_CR0, SSP_CR0_INITIAL);
		spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);
		//need to use 16bits data
		spi_reg_bit_set(SSP_CR0, SSP_CR0_DSS_16);
		//fire it up
		spi_reg_bit_set(SSP_CR0, SSP_CR0_SSE);
		
#if defined(FEATURE_FLASH256_SUPPORT)	
		Assert_CS();
#endif
		//load the command + 1 dummy byte
		*SSP_DR = SPI_CMD_READ_STATUS << 8;

		//wait till the TX fifo is empty, then read out the status
		timeout2 = 0xFFFFF;
		while((*SSP_SR & 0xF10) != 0x0)
		{
			if((timeout2--) <= 0)
			{
			    CPUartLogPrintf("SSP Status 0x%x", *SSP_SR);
				return SSPWaitTxEmptyTimeOutError;
			}
		}
#if defined(FEATURE_FLASH256_SUPPORT)	
		Deassert_CS();
#endif
		dummy = *SSP_DR;

		//set the READ flag, and read the status
		read = TRUE;
		status = dummy & 0xFF;	//the status will be in the second byte

		//set the READY flag if the status wait bit is cleared
		if((status & 1) == 0)		// operation complete (eg. not busy)?
			ready = TRUE;

		//make sure SSP is disabled
		spi_reg_bit_clr(SSP_CR0, SSP_CR0_SSE);
		//reset SSP CR's
		spi_reg_write(SSP_CR0, SSP_CR0_INITIAL);
		spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);

		//if we've waited long enough, fail
		if((timeout--) <= 0)
		{
		    CPUartLogPrintf("SSP Read Status 0x%x", *SSP_DR);
			return SSPRdStatusTimeOutError;
		}
		//we need to wait until we read at least 1 valid status entry
		//if we're waiting for the Write, wait till WIP bits goes to 0
	}while ((!read) || (!ready));


	//return last known status
	return NoError;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_WaitForWEL                                                */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function wait for write enablle.                                */
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
UINT_T SPINOR_WaitForWEL(UINT_T Wait)
{
	UINT_T read = 0, ready = 0, dummy = 0, status = 0;
	volatile int timeout = 0, timeout2 = 0;

	read = FALSE;	//this flag gets set when we read first entry from fifo
	//if the caller waits to 'Wait' for the BUSY to be cleared, start READY off as FALSE
	//if the caller doesn't wait to wait, set READY as true, so we don't wait on the bit
	ready = (Wait) ? FALSE : TRUE;

	timeout = 0xFF;

	do{
		//make sure SSP is disabled
		spi_reg_bit_clr(SSP_CR0, SSP_CR0_SSE);
		//reset SSP CR's
		spi_reg_write(SSP_CR0, SSP_CR0_INITIAL);
		spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);
		//need to use 16bits data
		spi_reg_bit_set(SSP_CR0, SSP_CR0_DSS_16);
		//fire it up
		spi_reg_bit_set(SSP_CR0, SSP_CR0_SSE);
#if defined(FEATURE_FLASH256_SUPPORT)	
		Assert_CS();
#endif
		//load the command + 1 dummy byte
		*SSP_DR = SPI_CMD_READ_STATUS << 8;

		//wait till the TX fifo is empty, then read out the status
		timeout2 = 0xFFFFF;
		while((*SSP_SR & 0xF10) != 0x0)
		{
			if((timeout2--) <= 0)
			{
			    CPUartLogPrintf("Wait For WEL, SSP Status 0x%x", *SSP_SR);
				return SSPWelWaitTxEmptyTimeOutError;
			}
		}
#if defined(FEATURE_FLASH256_SUPPORT)	
		Deassert_CS();
#endif
		dummy = *SSP_DR;

		//set the READ flag, and read the status
		read = TRUE;
		status = dummy & 0xFF;	//the status will be in the second byte

		//set the READY flag if the status wait bit is cleared*/
		/* WIP bit(S0):The Write in Progress (WIP) bit indicates whether the memory is busy in program/erase/write status register progress. */
		/* When WIP bit sets to 1, means the device is busy in program/erase/write status register progress, */
		/* when WIP bit sets 0, means the device is not in program/erase/write status register progress. */

		/*WEL bit(S1):The Write Enable Latch (WEL) bit indicates the status of the internal Write Enable Latch. */
		/*When set to 1 the internal Write Enable Latch is set, */
		/*when set to 0 the internal Write Enable Latch is reset and no Write Status Register, Program or Erase command is accepted. */
		if((status & 0x03) == 0x02)		// Write Enable Latch and Operation not in Progress
			ready = TRUE;

		//make sure SSP is disabled
		spi_reg_bit_clr(SSP_CR0, SSP_CR0_SSE);
		//reset SSP CR's
		spi_reg_write(SSP_CR0, SSP_CR0_INITIAL);
		spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);

		//if we've waited long enough, fail
		if((timeout--) <= 0)
		{
		    CPUartLogPrintf("Wait For WEL, SSP Read Status 0x%x", *SSP_DR);
			return SSPWaitForWELTimeOutError;
		}

		//we need to wait until we read at least 1 valid status entry
		//if we're waiting for the Write, wait till WIP bits goes to 0
	}while ((!read) || (!ready));


	//return last known status
	return NoError;
}



/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_WriteEnable                                               */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function enable SPI nor write operation.                     */
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
void SPINOR_WriteEnable(void)
{
	UINT32 temp;
    UINT_T Retval = NoError;

	spi_reg_write(SSP_CR0, SSP_CR0_DSS_8);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);
	spi_reg_bit_set(SSP_CR0, SSP_CR0_SSE);
#if defined(FEATURE_FLASH256_SUPPORT)	
		Assert_CS();
#endif
	//load the command
	spi_reg_write(SSP_DR, SPI_CMD_WRITE_ENABLE);

	//wait till TX fifo is empty
	Retval = SPINOR_WaitSSPComplete();
    if(Retval != NoError)
    {
        CPUartLogPrintf("SPINOR_WriteEnable status 0x%x", Retval);
    }
	
#if defined(FEATURE_FLASH256_SUPPORT)	
		Deassert_CS();
#endif
	temp = *SSP_DR;

	//make sure SSP is disabled
	SPINOR_DisableSSP();

	return;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_ReadId                                                    */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function read the Nor ID.                                     */
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
void SPINOR_ReadId(UINT_T *pID)
{
	UINT_T ID;
    UINT_T Retval = NoError;

	spi_reg_write(SSP_CR0, SSP_CR0_DSS_32);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);
	spi_reg_bit_set(SSP_CR0, SSP_SSCR0_SSE);
	
#if defined(FEATURE_FLASH256_SUPPORT)	
	Assert_CS();
#endif
	spi_reg_write(SSP_DR, SPI_CMD_JEDEC_ID << 24);

	Retval = SPINOR_WaitSSPComplete();
    if(Retval != NoError)
    {
        CPUartLogPrintf("SPINOR_ReadId status 0x%x", Retval);
    }
	
#if defined(FEATURE_FLASH256_SUPPORT)	
	Deassert_CS();
#endif
	ID = *SSP_DR;

	//make sure SSP is disabled
	//it must be executed lastly,
	//because disable SP will result in RXFIFO/TXFIFO reset.
	SPINOR_DisableSSP();

	*pID = ID;

	return;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_Page_Program_DMA                                          */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function do DMA page program operation.                      */
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
UINT_T SPINOR_Page_Program_DMA(UINT_T Address, UINT_T Buffer, UINT_T Size)
{
	DMA_CMDx_T TX_Cmd, RX_Cmd;
	UINT_T Retval = NoError, i = 0;
	UINT_T *ptr = NULL;
    volatile int timeout = 0;

	//turn off UINT_Terrupts during the Read
	//DisableIrqInterrupts();

	//because the SSP controller is crap, we need to
	//use DMA to input the five bytes of data:
	TX_Cmd.value = 0;
	TX_Cmd.bits.IncSrcAddr = 1;
	TX_Cmd.bits.IncTrgAddr = 0;
	TX_Cmd.bits.FlowSrc = 0;
	TX_Cmd.bits.FlowTrg = 1;
	TX_Cmd.bits.Width = 3;
	TX_Cmd.bits.MaxBurstSize = 2;
	//TX_Cmd.bits.Length = 8; // 8 bytes

	RX_Cmd.value = 0;
	RX_Cmd.bits.IncSrcAddr = 0;
	RX_Cmd.bits.IncTrgAddr = 0;
	RX_Cmd.bits.FlowSrc = 0;
	RX_Cmd.bits.FlowTrg = 0;
	RX_Cmd.bits.Width = 3;
	RX_Cmd.bits.MaxBurstSize = 2;
	//RX_Cmd.bits.Length = 8; // 8 bytes

	//setup DMA
	//Map Device to Channel
	XllpDmacMapDeviceToChannel(SSP_TX_DMA_DEVICE, SSP_TX_CHANNEL);
	XllpDmacMapDeviceToChannel(SSP_RX_DMA_DEVICE, SSP_RX_CHANNEL);

	//turn ON user alignment - in case buffer address is 64bit aligned
	alignChannel(SSP_TX_CHANNEL, 1);

	spi_reg_write(SSP_CR0, SSP_CR0_INITIAL);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL | 0x300cc3);

	//setup in 32bit mode
	spi_reg_bit_set(SSP_CR0, SSP_CR0_DSS_32);

	//fire SSP up
	spi_reg_bit_set(SSP_CR0, SSP_CR0_SSE);

	memcpy(DMA_Buf, Buffer, Size );

	DMA_PP[0] = (SPI_CMD_PROGRAM << 24) | (Address & 0x00FFFFFF);
	ptr = (UINT_T *)DMA_Buf;
	for( i = 1; i < ((Size >> 2) + 1); i++ )
	{
		DMA_PP[i] = SPINOR_Endian_Convert( ptr[i-1] );
	}

	//configDescriptor(&RX_Desc[0], &RX_Desc[1],(UINT_T)SSP_DR,			(UINT_T)&rx1_fromcommand, &RX_Cmd, 4, 0);
	//configDescriptor(&TX_Desc[0], &TX_Desc[1],(UINT_T)&tx1_command,	(UINT_T)SSP_DR, 			&TX_Cmd, 4, 0);
	configDescriptor(&RX_Desc[0], NULL,	(UINT_T)SSP_DR,	(UINT_T)&rx1_fromcommand, &RX_Cmd, Size + 4, 1);
	configDescriptor(&TX_Desc[0], NULL,	(UINT_T)DMA_PP,	(UINT_T)SSP_DR, 			&TX_Cmd, Size + 4, 1);
	
#if defined(FEATURE_FLASH256_SUPPORT)	
		Assert_CS();
#endif
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
	
#if defined(FEATURE_FLASH256_SUPPORT)	
		Deassert_CS();
#endif
	//make sure SSP is disabled
	SPINOR_DisableSSP();

	//clear out DMA settings
	XllpDmacUnMapDeviceToChannel(SSP_TX_DMA_DEVICE, SSP_TX_CHANNEL);
	XllpDmacUnMapDeviceToChannel(SSP_RX_DMA_DEVICE, SSP_RX_CHANNEL);

	//turn UINT_Terrupts back on
	//EnableIrqInterrupts();

	return Retval;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_Read_DMA                                                  */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function do DMA read operation.                              */
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
UINT_T SPINOR_Read_DMA(UINT_T FlashOffset, UINT_T Buffer, UINT_T Size, UINT_T CopySize)
{
	UINT_T  i = 0;
	UINT_T	*temp_buff = NULL;
	UINT_T	read_size = 0, total_size = 0, un_read_size = 0;
	UINT_T	Retval = NoError, read_buff = 0;
	DMA_CMDx_T RX_Cmd, TX_Cmd;
	volatile int timeout = 0;
    P_FlashProperties_T pFlashP = GetFlashProperties(BOOT_FLASH);

	if ((FlashOffset + Size) > (pFlashP->BlockSize * pFlashP->NumBlocks))
	{
		return FlashAddrOutOfRange;
    }

	//remember how much (in words) is Read - for endian convert at the end of routine
	total_size = Size >> 2;

	read_buff = DMA_Read_Buffer;

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

	//Size = Size + 4;
	un_read_size=Size +12;

	//for each loop iteration, one Read Command will be issued with a link descriptor chain
	//	the chain will read a total of (SSP_READ_DMA_DESC-1)*SSP_READ_DMA_SIZE bytes from SPI

	//while (Size > 0)
	{
		// * configure RX & TX descriptors * //
		//initial 1 word transfer:
		//		TX: send command+address
		//		RX: receive dummy word
		tx1_command  = (SPI_CMD_READ << 24) | (FlashOffset & 0x00FFFFFF);

		//configDescriptor(&RX_Desc[0], &RX_Desc[1],(UINT_T)SSP_DR,			(UINT_T)&rx1_fromcommand, &RX_Cmd, 4, 0);
		//configDescriptor(&TX_Desc[0], &TX_Desc[1],(UINT_T)&tx1_command,	(UINT_T)SSP_DR, 			&TX_Cmd, 4, 0);

		i = 0;
		//chaining of full transfers (descriptors that are not "stop"ped)
		//fill out descriptors until either:
		//	- there is only enough data for 1 more descriptor (which needs to be the "stopped" descriptor)
		//	- the pool of descriptors is depleted (minus 1 because the last needs to be "stopped")
		while( (un_read_size > SSP_READ_DMA_SIZE) && (i < (SSP_READ_DMA_DESC-1)) )
		{
			configDescriptor(&RX_Desc[i], &RX_Desc[i+1],	(UINT_T)SSP_DR,			(UINT_T)read_buff, &RX_Cmd, SSP_READ_DMA_SIZE, 0);
			configDescriptor(&TX_Desc[i], &TX_Desc[i+1],	(UINT_T)&tx1_command,	(UINT_T)SSP_DR, &TX_Cmd, SSP_READ_DMA_SIZE, 0);

			//Update counters
			read_buff 		+=	SSP_READ_DMA_SIZE;
			//FlashOffset +=	SSP_READ_DMA_SIZE;
			un_read_size 		-=	SSP_READ_DMA_SIZE;
			i++;

		}

		//last link: descriptor must be "stopped"
		read_size 	=  un_read_size > SSP_READ_DMA_SIZE ? SSP_READ_DMA_SIZE : un_read_size;
		configDescriptor(&RX_Desc[i], NULL,	(UINT_T)SSP_DR,			(UINT_T)read_buff, &RX_Cmd, read_size, 1);
		configDescriptor(&TX_Desc[i], NULL,	(UINT_T)&tx1_command,	(UINT_T)SSP_DR, &TX_Cmd, read_size, 1);

		//update counters after "stop" descriptor
		read_buff 		+= read_size;
		//FlashOffset += read_size;
		un_read_size 		-= read_size;
#if defined(FEATURE_FLASH256_SUPPORT)	
		Assert_CS();
#endif	
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
		//if(Retval != NoError)
		//	break;

		Retval = SPINOR_WaitSSPComplete();
#if defined(FEATURE_FLASH256_SUPPORT)	
		Deassert_CS();
#endif			
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

	//turn UINT_Terrupts back on
	//EnableIrqInterrupts();

	temp_buff = (UINT_T *)DMA_Read_Buffer;
	for( i = 0; i < total_size + 2; i ++ )
	{
		temp_buff[i] = SPINOR_Endian_Convert( temp_buff[i] );
	}
	//temp_buff[i] = Endian_Convert( temp_buff[i] ); // last byte

	memcpy( (UINT8_T *)Buffer, (UINT8_T *)(DMA_Read_Buffer + 4), CopySize );

	//postprocessing... endian convert
	//for(i = 0; i < total_size; i++)
	//	buff[i] = Endian_Convert(buff[i]);

	return Retval;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_Read                                                      */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function do SPI Nor read operation.                          */
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
UINT_T SPINOR_Read(UINT_T FlashOffset, UINT_T Buffer, UINT_T Size)
{
	UINT_T Retval = NoError, read_size = 0;

	do {

		read_size = Size > SIZE_64KB ? SIZE_64KB : Size;

		Retval = SPINOR_Read_DMA(FlashOffset, Buffer, SIZE_64KB, read_size);

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
/*      SPINOR_EraseSector                                               */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function erase spi nor sector.                               */
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
UINT_T SPINOR_EraseSector(UINT_T secAddr)
{
	UINT_T temp, command;
    UINT_T	Retval = NoError;
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

	command  = SPI_CMD_SECTOR_ERASE << 24;
	command |= secAddr & 0xFFFFFF;

	spi_reg_write(SSP_CR0, SSP_CR0_DSS_32);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);
	spi_reg_bit_set(SSP_CR0, SSP_CR0_SSE);
#if defined(FEATURE_FLASH256_SUPPORT)	
		Assert_CS();
#endif

	spi_reg_write(SSP_DR, command);

	//wait for TX fifo to empty AND busy signal to go away
	Retval = SPINOR_WaitSSPComplete();
#if defined(FEATURE_FLASH256_SUPPORT)	
		Deassert_CS();
#endif
	temp = *SSP_DR;

	//make sure SSP is disabled
	SPINOR_DisableSSP();

	Retval = SPINOR_ReadStatus(TRUE);

	return Retval;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_Wipe                                                      */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function do SPI Nor wipe operation.                          */
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
UINT_T SPINOR_Wipe(void)
{
	UINT_T temp;
    UINT_T	Retval = NoError;
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

	// sequence for issuing the wipe command (aka chip erase, aka bulk erase)
	//make sure SSP is disabled
	spi_reg_write(SSP_CR0, SSP_CR0_DSS_8);
	spi_reg_write(SSP_CR1, SSP_CR1_INITIAL);
	spi_reg_bit_set(SSP_CR0, SSP_CR0_SSE);

	// write the command to the fifo. this starts the spi clock running and the command appears on the bus.
	spi_reg_write(SSP_DR, SPI_CMD_CHIP_ERASE);

	//wait for TX fifo to empty AND busy signal to go away
	Retval = SPINOR_WaitSSPComplete();

	temp = *SSP_DR;

	//make sure SSP is disabled
	SPINOR_DisableSSP();

	Retval = SPINOR_ReadStatus(TRUE);

	return Retval;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_Wipe                                                      */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function do SPI Nor wipe operation.                          */
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
UINT_T SPINOR_Write(UINT_T Address, UINT_T Buffer, UINT_T Size)
{
    volatile int timeout = 0;
	UINT_T Retval = NoError, total_size = 0, write_size = 0;
    P_FlashProperties_T pFlashP = GetFlashProperties(BOOT_FLASH);

    //make size 4bytes-align
	if(Size & 0x3)
	{
    	Size = (Size+4)&(~3);
	}

	total_size = Size >> 2;

	//postprocessing... endian convert
	if ((Address + Size) > (pFlashP->BlockSize * pFlashP->NumBlocks))
	{
		return FlashAddrOutOfRange;
    }

	//for(i = 0; i < total_size; i++)
	//	buff[i] = Endian_Convert(buff[i]);

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
			Retval = SPINOR_Page_Program_DMA(Address, Buffer, WRITE_SIZE);

			//update counters
			Address+=WRITE_SIZE;
			Buffer+=WRITE_SIZE;
			Size-=WRITE_SIZE;
		}
		else
		{
			Retval = SPINOR_Page_Program_DMA(Address, Buffer, write_size);
			Size=0;
		}

		Retval = SPINOR_ReadStatus(TRUE);

	} while( (Size > 0) && (Retval == NoError) );

	return Retval;
}


/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*                                                                       */
/*      SPINOR_Erase                                                     */
/*                                                                       */
/* DESCRIPTION                                                           */
/*                                                                       */
/*      The function do SPI Nor erase operation.                         */
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
UINT_T SPINOR_Erase(UINT_T Address, UINT_T Size)
{
	UINT_T numSectors = 0, i = 0;
	UINT_T sector_size = 0, Retval = NoError;
	P_FlashProperties_T pFlashP = GetFlashProperties(BOOT_FLASH);

	if ((Address + Size) > (pFlashP->BlockSize * pFlashP->NumBlocks))
	{
		return FlashAddrOutOfRange;
    }

	sector_size = pFlashP->BlockSize;

	if ((Size % pFlashP->BlockSize) == 0)
	{
		numSectors = Size / pFlashP->BlockSize;
	}
	else
	{
		numSectors = (Size / pFlashP->BlockSize) + 1;
    }

	for (i = 0; i < numSectors; i++)
	{
		//erase this sector
		Retval = SPINOR_EraseSector(Address);

		Address += sector_size;

		if (Retval != NoError)
		{
			break;
		}
	}

	return Retval;
}

#if defined(FEATURE_FLASH256_SUPPORT)

void SPINOR_CS_Enable(int index)
{
		SPI_FLASH_OUT2;
		SPI_FLASH_OUT1;
	if(index == SPINOR_CS1)
	{
		SPI_FLASH_CS2_H;
		SPI_FLASH_CS1_L;	
		//*(( volatile UINT32 *)(0xD4019004+0x18)) |= (1 << 17);  //GPIO49 H
		status_flag=1;
		CPUartLogPrintf("[##001]%s",__FUNCTION__);
	}
	else if(index == SPINOR_CS2)
	{
		SPI_FLASH_CS1_H;
		SPI_FLASH_CS2_L;
		status_flag=2;
		//*(( volatile UINT32 *)(0xD4019004+0x24)) |= (1 << 17);  //GPIO49 L	
	}
	
}


UINT_T SPINOR_Read_32(UINT_T FlashOffset, UINT_T Buffer, UINT_T Size)
{
	UINT_T Retval = NoError;
	UINT_T size_l=0,size_h=0;
	UINT_T Address ;
	Address = FlashOffset;
	if ((Address + Size) > 0xFFFFFF)
	{
		if(Address > 0xFFFFFF)
		{
			Address = Address - 0xFFFFFF;
			size_l = 0;
			size_h = Size;
		}
		else
		{
			size_l = 0xFFFFFF - Address;
			size_h = Size - size_l;
		}
	}
	else
	{
		size_l = Size;
		size_h = 0;
	}
	
	if(size_l && size_h)
	{
		
		SPINOR_CS_Enable(SPINOR_CS1);
		Size = size_l;
		Retval = SPINOR_Read(Address, Buffer, Size);
		Uart_Log_Printf("10 SPINOR_Erase Address=%x,Size=%d, Retval=%d:",Address, Size, Retval);
		if(Retval != NoError)
		{
			SPINOR_CS_Enable(SPINOR_CS2);
			Size = size_h;
			Address = 0;
			Retval = SPINOR_Read(Address, Buffer+size_l, Size);
			Uart_Log_Printf("12 SPINOR_Erase Address=%x,Size=%d, Retval=%d:",Address, Size, Retval);
		}
		
	}
	else
	{
		if(size_l)
		{
			SPINOR_CS_Enable(SPINOR_CS1);
		}
		else
		{
			SPINOR_CS_Enable(SPINOR_CS2);
		}
		Retval = SPINOR_Read(Address, Buffer, Size);
		Uart_Log_Printf("13 SPINOR_Erase Address=%x,Size=%d, Retval=%d:",Address, Size, Retval);
	}
	return Retval;
}

UINT_T SPINOR_Write_32(UINT_T Address, UINT_T Buffer, UINT_T Size)
{
	
	UINT_T size_l=0,size_h=0;
	UINT_T Retval = NoError;
	
	if(Size & 0x3)    //make size 4bytes-align
    	Size = (Size+4)&(~3);

	if ((Address + Size) > 0xFFFFFF)
	{
		if(Address > 0xFFFFFF)
		{
			Address = Address - 0xFFFFFF;
			size_l = 0;
			size_h = Size;
		}
		else
		{
			size_l = 0xFFFFFF - Address;
			size_h = Size - size_l;
		}
	}
	else
	{
		size_l = Size;
		size_h = 0;
	}
	
	
	if(size_l && size_h)
	{
			SPINOR_CS_Enable(SPINOR_CS1);
			Size = size_l;
			Retval = SPINOR_Write(Address, Buffer, Size);
			Uart_Log_Printf("20 SPINOR_Erase Address=%x,Size=%d, Retval=%d:",Address, Size, Retval);
			if(Retval != NoError)
			{
				SPINOR_CS_Enable(SPINOR_CS2);
				Size = size_h;
				Address = 0;
				Retval = SPINOR_Write(Address, Buffer+size_l, Size);
				Uart_Log_Printf("21 SPINOR_Erase Address=%x,Size=%d, Retval=%d:",Address, Size, Retval);
			}
			
	}
	else
	{
			if(size_l)
			{
				SPINOR_CS_Enable(SPINOR_CS1);
			}
			else
			{
				SPINOR_CS_Enable(SPINOR_CS2);
			}
			Retval = SPINOR_Write(Address, Buffer, Size);
			Uart_Log_Printf("22 SPINOR_Erase Address=%x,Size=%d, Retval=%d:",Address, Size, Retval);
	}

	return Retval;
}

extern void Uart_Log_Printf(const char *fmt, ...);

UINT_T SPINOR_Erase_32(UINT_T Address, UINT_T Size)
{
	UINT_T Retval = NoError;
	UINT_T size_l=0,size_h=0;
	
	if ((Address + Size) > 0xFFFFFF)
	{
		if(Address > 0xFFFFFF)
		{
			Address = Address - 0xFFFFFF;
			size_l = 0;
			size_h = Size;
		}
		else
		{
			size_l = 0xFFFFFF - Address;
			size_h = Size - size_l;
		}
	}
	else
	{
		size_l = Size;
		size_h = 0;
	}

	if(size_l && size_h)
	{
		SPINOR_CS_Enable(SPINOR_CS1);
		Size = size_l;
		Retval = SPINOR_Erase(Address, Size);
		Uart_Log_Printf("00 SPINOR_Erase Address=%x,Size=%d, Retval=%d:",Address, Size, Retval);
		if(Retval != NoError)
		{
			SPINOR_CS_Enable(SPINOR_CS2);
			Size = size_h;
			Address = 0;
			Retval = SPINOR_Erase(Address, Size);
			Uart_Log_Printf("01 SPINOR_Erase Address=%x,Size=%d, Retval=%d:",Address, Size, Retval);
		}
	}
	else
	{
		if(size_l)
		{
			SPINOR_CS_Enable(SPINOR_CS1);
		}
		else
		{
			SPINOR_CS_Enable(SPINOR_CS2);
		}
		
		Retval = SPINOR_Erase(Address, Size);
		Uart_Log_Printf("02 SPINOR_Erase Address=%x,Size=%d, Retval=%d:",Address, Size, Retval);
	}

	return Retval;
}
#endif

