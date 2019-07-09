/******************************************************************************
 *
 *  (C)Copyright 2005 - 2012 Marvell. All Rights Reserved.
 *
 *  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF MARVELL.
 *  The copyright notice above does not evidence any actual or intended
 *  publication of such source code.
 *  This Module contains Proprietary Information of Marvell and should be
 *  treated as Confidential.
 *  The information in this file is provided for the exclusive use of the
 *  licensees of Marvell.
 *  Such users have the right to use, modify, and incorporate this code into
 *  products for purposes authorized by the license agreement provided they
 *  include this notice and the associated copyright notice with any such
 *  product.
 *  The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/
#ifndef __SPI_H__
#define __SPI_H__

#include "UART.h"
#include "Typedef.h"
#include "predefines.h"
#include "xllp_SSP.h"
#include "xllp_dmac.h"
#include "Flash_wk.h"


/* Supported SPI Devices */
typedef struct{
	unsigned int SPI_ID;
	unsigned int SPI_CAPACITY;
	unsigned int SPI_SECTOR_SIZE;
	unsigned int SPI_PAGE_SIZE;
} SPI_DEVICE_INFO_T, *P_SPI_DEVICE_INFO_T;

/*
* SPI Command Set
*/
#define SPI_CMD_JEDEC_ID			0x009F
#define SPI_CMD_RELEASE_POWER_DOWN	0x00AB
#define SPI_CMD_READ_STATUS			0x05
#define SPI_CMD_WRITE_ENABLE		0x06
#define SPI_CMD_WRITE_STATUS_REG	0x01
#define SPI_CMD_PROGRAM				0x02
#define SPI_CMD_READ				0x03
#define SPI_CMD_SECTOR_ERASE		0xd8
#define SPI_CMD_CHIP_ERASE			0xc7

#define DEFAULT_TIMEOUT   			3000
#define DEFAULT_WAIT_TIME 			1000
#define TIME_OUT_ERROR				0x01
#define SPINOR_WRITE_PAGE_SIZE		(0x100 / 4)
#define SPINOR_READ_PAGE_SIZE		(0x100)
#define WRITE_SIZE (256)
#define READ_SIZE	(0x10000)

// SPI
#define SSP_BASE_FOR_SPI		    SSP3_BASE

#define SIZE_64KB	                (0x10000 - 8)

#define SPI_READ_STATUS_RDY_BIT		BIT0
#define MAX_TEST_SIZE	            (65536+4)

typedef struct {
  UINT32 SPI_READ_CMD;
  UINT32 SPI_READ_STAUS_CMD;
  UINT32 SPI_WRITE_STATUS_CMD;
  UINT32 SPI_WRITE_ENABLE_CMD;
  UINT32 SPI_WRITE_STAGE1_CMD;
  UINT32 SPI_WRITE_STAGE2_CMD;
  UINT32 SPI_ERASE_CMD;
} SPI_COMMANDS_T, *P_SPI_COMMANDS_T;

/*
* SSP Register locations.
*
* Note:  SSP_BASE_FOR_SPI set in platform_config.h
*/

#define SSP_CR0	            ((volatile UINT32 *) (SSP_BASE_FOR_SPI+SSP_SSCR0))	// SSP Control Register 0
#define SSP_CR1	            ((volatile UINT32 *) (SSP_BASE_FOR_SPI+SSP_SSCR1))	// SSP Control Register 1
#define SSP_SR	            ((volatile UINT32 *) (SSP_BASE_FOR_SPI+SSP_SSSR))	// SSP Status Register
#define SSP_DR	            ((volatile UINT32 *) (SSP_BASE_FOR_SPI+SSP_SSDR))	// SSP Data Write/Read Register
#define SSP_SP	            ((volatile UINT32 *) (SSP_BASE_FOR_SPI+SSP_SSPSP))	// SSP Serial Protocol Register
#define SSP_TO	            ((volatile UINT32 *) (SSP_BASE_FOR_SPI+SSP_SSTO))	// SSP Time Out Register

//Control Register special values
#define SSP_CR0_INITIAL 	(SSP_SSCR0_TIM | SSP_SSCR0_RIM | 0x7)
#define SSP_CR0_SSE			SSP_SSCR0_SSE
#define SSP_CR0_FPCKE		SSP_SSCR0_FPCKE
#define SSP_CR0_DSS_32		SSP_SSCR0_EDSS | 0xF
#define SSP_CR0_DSS_24		SSP_SSCR0_EDSS | 0x7
#define SSP_CR0_DSS_16		0x0000000F
#define SSP_CR0_DSS_8		0x00000007

#define SSP_CR1_INITIAL 	SSP_SSCR1_TTELP | SSP_SSCR1_TTE
#define SSP_CR1_RWOT		SSP_SSCR1_RWOT

#define SSP_SSSR_TFL			0xF00       // BIT 11:8 --Transmit FIFO Level, when it's 0x0, TXFIFO is emptry or full.

//limited for timing and stack concerns (see SPI_page_write routine)
#define SSP_READ_TIME_OUT_MILLI		0x2000
#define SSP_READ_DMA_DESC			0x10	//total RX descriptors
#define SSP_READ_DMA_SIZE			0x1ff8	//bytes per descriptor
#define SSP_MAX_TX_SIZE_WORDS		64
#define SSP_MAX_TX_SIZE_BYTES		SSP_MAX_TX_SIZE_WORDS << 2


/* Tx DMA descriptor */
extern __align(16) XLLP_DMAC_DESCRIPTOR_T	TX_Desc[];

/* Rx DMA descriptor */
extern __align(16) XLLP_DMAC_DESCRIPTOR_T	RX_Desc[];

// contains command opcode and address
extern __align(16) UINT_T	 tx1_command;

extern __align(16) UINT_T	 tx_4bytes_read[2];

// receive garbage clocked in while command is transmitted
extern __align(16) UINT_T	 rx1_fromcommand;

extern __align(16) UINT8_T	 tx_1bytes[5];

extern __align(16) UINT8_T	 rx_1bytes[5];

extern __align(8) UINT_T DMA_PP[66];

extern __align(8) UINT8_T DMA_Buf[260];

extern __align(8) UINT8_T DMA_Read_Buffer[];


UINT_T SPINOR_Page_Program_DMA(UINT_T Address, UINT_T Buffer, UINT_T Size);
UINT_T InitializeSPIDevice(UINT8_T FlashNum, FlashBootType_T FlashBootType, UINT8_T *P_DefaultPartitionNum);
UINT_T SPINOR_Read(UINT_T FlashOffset, UINT_T Buffer, UINT_T Size);
UINT_T SPINOR_Write(UINT_T Address, UINT_T Buffer, UINT_T Size);
UINT_T SPINOR_Erase(UINT_T Address, UINT_T Size);
UINT_T SPINOR_Read_32(UINT_T FlashOffset, UINT_T Buffer, UINT_T Size);
UINT_T SPINOR_Write_32(UINT_T Address, UINT_T Buffer, UINT_T Size);
UINT_T SPINOR_Erase_32(UINT_T Address, UINT_T Size);
UINT_T SPINOR_Wipe(void);
UINT_T SPINOR_EraseSector(UINT_T secAddr);
UINT_T SPINOR_Endian_Convert (UINT_T in);
UINT_T SPINOR_WaitSSPComplete(void);
UINT_T SPINOR_ReadStatus(UINT_T Wait);
void   SPINOR_DisableSSP(void);
void   SPINOR_Reset(void);
void   SPINOR_ReadId(UINT_T *pID);
void   SPINOR_WriteEnable(void);
UINT_T SPINOR_WaitForWEL(UINT_T Wait);

#endif
