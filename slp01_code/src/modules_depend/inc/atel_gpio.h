/******************************************************************************
*@file    example_gpio.h
*@brief   example of gpio operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#ifndef __ATEL_GPIO_H__
#define __ATEL_GPIO_H__

#if defined(__ATEL_BG96_APP__)
/* Modify this which mode(input/output) you want to test */
#define GPIO_TEST_MODE   0	//1: test pin output; 0: test pin input;

extern void qt_uart_dbg(const char* fmt, ...);


#endif /*__EXAMPLE_TASK_CREATE__*/

#endif /*__ATEL_GPIO_H__*/

