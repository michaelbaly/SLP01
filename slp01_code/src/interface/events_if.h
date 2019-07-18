/******************************************************************************
*@file    events_if.c
*@brief   example of atel BG96 application
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__ATEL_BG96_APP__)


typedef enum {
	EVENT_E=0,
	ALARM_E,
	EVENT_IF_MAX
}EVENTS_T;


typedef enum {
	BATTARY_LOW_E=0,
	GEO_BREAK_E,
	DAILY_HEARTBEAT_E,
	IG_ON_E,
	SPEEDING_E,
	IG_OFF_E,
	TOWED_E,
	PARK_ALERT_E,
	NORMAL_EVENTS_NR_MAX
	
}ALARM_EVENT_E;

typedef enum {
	HARD_ACC_E=0,
	HARD_BRAKE_E,
	SHARP_TURN_E,
	Q_LANE_CHANGE_E,
	SPEEDING_E,
	ENGINE_IDLE_E,
	FATIGUE_DRIVE_E,
	DRV_BEHAV_NR_MAX,
	
}DRV_BEHAV_E;

typedef struct bitsmap_alarm_event_s {
	/* common */
	unsigned int alarm_eve:1; /* bit0 battery low alarm */
	unsigned int alarm_eve:1; /* bit1 geofence break alarm */
	unsigned int alarm_eve:1; /* bit2 daily heartbeat event */
	
	/* for ig on situation */
	unsigned int alarm_eve:1; /* bit3 ignition on event */
	unsigned int alarm_eve:1; /* bit4 speeding */

    /* for ig off situation */
	unsigned int alarm_eve:1; /* bit5 ignition off event */
	unsigned int alarm_eve:1; /* bit6 towed */
	unsigned int alarm_eve:1; /* bit7 parking alert event */
	
	unsigned int alarm_eve:24; /* reserved */

	unsigned int drv_behav:1; /* bit0 hard acc */
	unsigned int drv_behav:1; /* bit1 hard brake */
	unsigned int drv_behav:1; /* bit2 sharp turn */
	unsigned int drv_behav:1; /* bit3 quick lane change */
	unsigned int drv_behav:1; /* bit4 speeding */
	unsigned int drv_behav:1; /* bit5 engine idle time */
	unsigned int drv_behav:1; /* bit6 fatigue drive */
	
	unsigned int drv_behav:25; /* reserved */
}AE_BITMAP_T;

typedef enum 

#define BIT_SET(alarm_eve, n) 			(alarm_eve |= SHIFT_L(n))
#define BIT_CLEAR(alarm_eve, n)			(alarm_eve &= ~(SHIFT_L(n)))
/**************************************************************************
*                                 DEFINE
***************************************************************************/

/**************************************************************************
*                                 GLOBAL
***************************************************************************/

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/




#endif /*__EXAMPLE_BG96_APP__*/




