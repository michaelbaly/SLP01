/******************************************************************************
*@file    example_gps.h
*@brief   example of gps operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#ifndef __ATEL_GPS_H__
#define __ATEL_GPS_H__

#if defined(__ATEL_BG96_APP__)

#define R_QUEUE_SIZE	40


typedef struct r_queue {
	char* p_ack[R_QUEUE_SIZE];
	int front;
	int rear;
}r_queue_s;


#endif /*__ATEL_BG96_APP__*/

#endif /*__EXAMPLE_GPS_H__*/

