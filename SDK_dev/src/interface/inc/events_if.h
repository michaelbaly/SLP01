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
#ifndef __EVENTS_IF_H__
#define __EVENTS_IF_H__

#if defined(__ATEL_BG96_APP__)

#define SHIFT_L(n) (1UL<<n)

typedef enum {
	EVENT_E=0,
	ALARM_E,
	EVENT_IF_MAX
}EVENTS_T;


typedef enum {
	BATTARY_LOW_E=0,
	GEO_BREAK_E,
	DAILY_HEARTBEAT_E,
	//IG_ON_E,
	SPEEDING_E,
	//IG_OFF_E,
	TOWED_E,
	PARK_ALERT_E,
	NORMAL_EVENTS_NR_MAX
	
}ALARM_EVENT_E;

typedef enum {
	HARD_ACC_E=0,
	HARD_BRAKE_E,
	SHARP_TURN_E,
	Q_LANE_CHANGE_E,
	//SPEEDING_E,
	ENGINE_IDLE_E,
	FATIGUE_DRIVE_E,
	DRV_BEHAV_NR_MAX,
	
}DRV_BEHAV_E;

typedef struct bitsmap_alarm_event_s {
	
	/* common */
	unsigned int battery_low:1; /* bit0 battery low alarm */
	unsigned int geo_break:1; /* bit1 geofence break alarm */
	unsigned int daily_hb:1; /* bit2 daily heartbeat event */
	
	/* for ig on situation */
	unsigned int ig_on_e:1; /* bit3 ignition on event */
	unsigned int speeding:1; /* bit4 speeding */

    /* for ig off situation */
	unsigned int ig_off_e:1; /* bit5 ignition off event */
	unsigned int towed_e:1; /* bit6 towed */
	unsigned int park_alert_e:1; /* bit7 parking alert event */
	
	unsigned int alarm_eve_extend:24; /* reserved */
	
}AE_BITMAP_T;

typedef struct bitsmap_drv_behav_s {

	unsigned int hard_acc:1; /* bit0 hard acc */
	unsigned int hard_brake:1; /* bit1 hard brake */
	unsigned int sharp_turn:1; /* bit2 sharp turn */
	unsigned int q_lane_c:1; /* bit3 quick lane change */
	unsigned int speeding:1; /* bit4 speeding */
	unsigned int eng_idle_t:1; /* bit5 engine idle time */
	unsigned int fati_drv:1; /* bit6 fatigue drive */
	
	unsigned int drv_behav_extend:25; /* reserved */
	
}DRV_BEHAV_BITMAP_T;

typedef enum {
	COMMAND_E=0,
	ACK_E,
	//REPORT_E,
	BUF_REP_E,
	SACK_E,
	MSG_TYPE_MAX,
}REP_MSG_TYPE_E;

#define MAX_CMD_LEN	20
#define MAX_PWD_LEN	6


typedef struct rec_command_s {
	char cmd_code[MAX_CMD_LEN + 1];
	char passwd[MAX_PWD_LEN + 1];
	
}REC_CMD_T;



typedef enum {
	
	APN_E = 0,
	SERVER_E,
	NOMOVE_E,
	INTERVAL_E,
	CONFIG_E,
	SPEEDALARM_E,
	PROTOCOL_E,
	VERSION_E,
	PASSWORD_E,
	GPS_E,
	MILEAGE_E,
	RESEND_INTERVAL_E,
	PND_TYPE_E,
	FACTORY_RST_E,
	OTA_START_E,
	GET_ADC_E,
	REPORT_E,
	//HEART_BEAT_E,
	GEOFENCE_E,
	OUTPUT_E,
	HEADING_E,
	IDLING_ALERT_E,
	HARSH_BRAKING_E,
	HARSH_ACCEL_E,
	HARSH_IMPACT_E,
	//HARSH_SWERVE_E,
	LOW_IN_BATT_E,
	LOW_EX_BATT_E,
	VIRTUAL_IGNITION_E,
	CLEAR_BUFFER_E,
	THEFT_MODE_E,
	TOW_ALARM_E,
	WATCH_DOG_E,
	REBOOT_E,
	
}CMD_TYPE_E;

#define GPS_DATA	"R"
#define GSM_DATA	"R2"
#define LTE_DATA	"R3"

/* report msg format, separator: ',' */
typedef struct rep_msg_s {

	char head[6];			 //"+RESP" or "+BUFF", fixed
	char imei[15];           //query by AT+GSN
	char dev_name[20];       //user defined, default: "NAME"
	union data_type {
			char gps[1];	 //"R"
			char gsm[2];	 //"R2"
			char lte[2];	 //"R3"
		}rep_tp;
	char rep_buffer[65];
	
}REP_MSG_T;

typedef enum {
	AUTO_REPORT_E = 0,
	POWER_CUT_E = 2,
	IG_ON_E = 4,
	IG_OFF_E = 5,
	HEART_BEAT_E = 9,
	INTER_BATT_LOW_E = 10,
	G_SENSOR_ALIG_E = 15,
	ROLL_OVER_E,
	HARSH_BRAKE_E,
	HARSH_ACC_E,
	IMPACT_E,
	HARSH_SWERVE_E,
	OVER_SPEED_E,
	TOW_E = 23,
	HEADING_CHG_REP_E,
	IDLING_REP_E,
	POWER_RECOVER_E,

	FIR_CIR_GEO_IN_E,
	FIR_CIR_GEO_OUT_E,
	SEC_CIR_GEO_IN_E,
	SEC_CIR_GEO_OUT_E,
	THD_CIR_GEO_IN_E,
	THD_CIR_GEO_OUT_E,
	
#ifdef EVENT_EXTENSION
	FOR_CIR_GEO_IN_E, 	//forth geofence break
	FOR_CIR_GEO_OUT_E,
	FIF_CIR_GEO_IN_E,	//fifth geofence break
	FIF_CIR_GEO_OUT_E,
#endif

	FIR_POLY_GEO_IN_E,
	FIR_POLY_GEO_OUT_E,
	SEC_POLY_GEO_IN_E,
	SEC_POLY_GEO_OUT_E,

	EXT_BATT_LOW_E,
	VIR_IG_ON_E = 44,
	VIR_IG_OFF_E,
	
	
}MSG_TYPE_E;

typedef enum {
	AUTO_REPORT_EVT = 			SHIFT_L(0),
	POWER_CUT_EVT = 			SHIFT_L(1),
	IG_ON_EVT = 				SHIFT_L(2),
	IG_OFF_EVT = 				SHIFT_L(3),
	HEART_BEAT_EVT = 			SHIFT_L(4),
	INTER_BATT_LOW_EVT = 		SHIFT_L(5),
	G_SENSOR_ALIG_EVT = 		SHIFT_L(6),
	ROLL_OVER_EVT = 			SHIFT_L(7),
	HARSH_BRAKE_EVT = 			SHIFT_L(8),
	HARSH_ACC_EVT = 			SHIFT_L(9),
	IMPACT_EVT = 				SHIFT_L(10),
	HARSH_SWERVE_EVT = 			SHIFT_L(11),
	OVER_SPEED_EVT = 			SHIFT_L(12),
	TOW_EVT = 					SHIFT_L(13),
	HEADING_CHG_REP_EVT = 		SHIFT_L(14),
	IDLING_REP_EVT = 			SHIFT_L(15),
	POWER_RECOVER_EVT = 		SHIFT_L(16),
	
	FIR_CIR_GEO_IN_EVT = 		SHIFT_L(17),
	FIR_CIR_GEO_OUT_EVT = 		SHIFT_L(18),
	SEC_CIR_GEO_IN_EVT = 		SHIFT_L(19),
	SEC_CIR_GEO_OUT_EVT = 		SHIFT_L(20),
	THD_CIR_GEO_IN_EVT = 		SHIFT_L(21),
	THD_CIR_GEO_OUT_EVT = 		SHIFT_L(22),
	
	FIR_POLY_GEO_IN_EVT = 		SHIFT_L(23),
	FIR_POLY_GEO_OUT_EVT = 		SHIFT_L(24),
	SEC_POLY_GEO_IN_EVT = 		SHIFT_L(25),
	SEC_POLY_GEO_OUT_EVT = 		SHIFT_L(26),
	
	EXT_BATT_LOW_EVT = 			SHIFT_L(27),
	VIR_IG_ON_EVT = 			SHIFT_L(28),
	VIR_IG_OFF_EVT = 			SHIFT_L(29),
}MSG_TYPE_REL_E;

#define BITS_PER_BYTE		8
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define BITS_TO_LONG(nr)	DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))
#define ATEL_BIT_MAP(name, bits)	unsigned long name[BITS_TO_LONG(bits)]

/* separator: ';' */
typedef struct rep_gps_S {
	
	char sate_no[2];		//have 4 to fix
	char time_stamp[12];	//UTC time "YYMMDDhhmmss"
	char lati[9];
	char logi[10];
	char speed[8];
	char heading[3];		//track angle in degrees, from 0~359
	char input_s[3];		//state of digital input
	char msg_type[2];		//refer to enum of MSG_TYPE_E
	char miles[5];
	char power_v[4];
	char ext_adc[4];
	char seq_num[3];
	
}REP_GPS_T;

/* separator: '-' */
typedef struct gsm_cel_data {
	
	char time_stamp[12];	//UTC time "YYMMDDhhmmss"
	char nmc[2];
	char cel_id[9];
	char lac[5];
	char mcc[3];
	char input_s[3];		//state of digital input
	char msg_type[2];		//refer to enum of MSG_TYPE_E
	char miles[5];
	char power_v[4];
	char ext_adc[4];
	char seq_num[3];
	
}GSM_CEL_T;


/* separator: '-' */
typedef struct lte_cel_data {
	
	char time_stamp[12];	//UTC time "YYMMDDhhmmss"
	char nmc[2];
	char cel_id[9];
	char lac[5];
	char mcc[3];
	char pcid[3];
	char input_s[3];		//state of digital input
	char msg_type[2];		//refer to enum of MSG_TYPE_E
	char miles[5];
	char power_v[4];
	char ext_adc[4];
	char seq_num[3];
	
}LTE_CEL_T;

typedef enum {
	SEPARATOR_COMMA_E,	/* ',' */
	SEPARATOR_SEMI_E,	/* ';' */
	SEPARATOR_DASH_E,	/* '-' */
}SEP_TYPE_E;


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
extern void atel_dbg_print(const char* fmt, ...);



/* begin: prio queue */

#define PRIO_MAX	3
#define NODE_NUM_MAX 20
int prio_arr[PRIO_MAX];

/* report node in a hash bucket */
typedef struct node_s {
	int key;
	int prio;
	struct node_s* next;
}node;

/* list of nodes in a hash bucket */
typedef struct pnode_list_s {
	int prio;
	node* ele;
}pnode_list;

typedef struct que_stat_s {
	int enque;
	int deque;
	int wait_time;
}que_stat;

/* overall table of priority list */
typedef struct queue_table_s {
	pnode_list list_entry[PRIO_MAX]; //number of list
	node* last_ele[PRIO_MAX];
	node* pool;
	node* free_pool;
	int entry_cnt;
	//pthread_mutex_t lock;
	TX_MUTEX lock;
	//pthread_cond_t cv;
	TX_EVENT_FLAGS_GROUP cv;
	bool is_avail;
	que_stat* stat;
}que_table;


/* end: prio queue */




#endif /*__ATEL_BG96_APP__*/



#endif
