#ifndef __IF_SERVER_H__
#define __IF_SERVER_H__

#if defined (__ATEL_BG96_APP__)

#define ATEL_DEBUG
#define MAX_ARG_LEN 16
#define MAX_ARG_NUM 30
#define R_QUEUE_SIZE	40

typedef enum {
	QUERY_CMD_E = 0,
	SET_CMD_E = 1,
}CMD_TYPE_E;

typedef unsigned char uint8;

typedef enum {
	ERROR_CMD_PWD_E = 0x100, //error code base

}ERROR_CODE_T;



/* begin: cmd register */
#define TOTAL_CMD_NUM	34
/* end */

#define TRUE	1
#define FALSE	0

typedef unsigned char bool;
typedef char(*p_arg)[MAX_ARG_LEN];


typedef bool (*cmd_set_func)(p_arg arg_list, uint8 arg_cnt);
typedef bool (*cmd_get_func)(void* cmd_rel_st);


typedef struct cmd_pro_arch_s {

	char* cmd_code;
	void* cmd_rel_st;
	uint8 ele_num;
	cmd_set_func cmd_set_f;
	cmd_get_func cmd_get_f;

}CMD_PRO_ARCH_T;

#define ACTUAL_ARG_OFFSET 2
#define CMD_OK		"OK"
#define CMD_ERROR	"ERROR"

typedef struct apn_info_s {
	char apn[MAX_ARG_LEN];
	char apn_usrname[MAX_ARG_LEN];
	char apn_passwd[MAX_ARG_LEN];
}APN_INFO;


#define APN_ELE_NUM		sizeof(APN_INFO)/MAX_ARG_LEN


typedef struct adc_info_s {
	char power[MAX_ARG_LEN];
	char in_batt[MAX_ARG_LEN];
	char ex_batt[MAX_ARG_LEN];
}ADC_INFO;


typedef struct server_info_s {
	char main_ip[MAX_ARG_LEN];
	char main_port[MAX_ARG_LEN];
	char bk_ip[MAX_ARG_LEN];
	char bk_port[MAX_ARG_LEN];
}SER_INFO;



struct apn_info_with_type {
	char apn[MAX_ARG_LEN];
	char apn_usrname[MAX_ARG_LEN];
	char apn_passwd[MAX_ARG_LEN];
	struct apn_info_with_type *data_type;
}apn_info_t;

typedef struct apn_info_with_type APN_INFO_T;




//from common.h
#define SYS_DEFAULT_PASSWD	"123456"
#define SYS_PASSWD_MAX_LEN		6
#define DFT_APN				"iot.nb"
#define DFT_APN_USRNAME		"michael"
#define DFT_APN_PASSWD		"138114"
#define DFT_DEV_NAME		"ATEL-100"

#define ACK_PRE_LEN		4
#define MAX_IMEI_LEN	20
#define MAX_DEV_LEN		10
#define MAX_ST_LEN		15
#define MAX_RT_LEN		15


#define ACK_PREFIX		"+ACK"
#define RESP_PREFIX		"+RESP"
#define BUFF_PREFIX		"+BUFF"
#define SACK_PREFIX		"+SACK"
#define ACK_SET_LEN_MAX		256

#define EXEC_STATUS_OK			"OK"
#define EXEC_STATUS_ERROR		"ERROR"


#define APN_MAX_LEN		MAX_ARG_NUM


typedef struct apn_cmd_fmt_s {

	char cmd_code[MAX_ARG_LEN];
	char passwd[MAX_ARG_LEN];
	char apn_name[MAX_ARG_LEN];
	char usr_name[MAX_ARG_LEN];
	char usr_passwd[MAX_ARG_LEN];

}APN_CMD_FMT;

typedef struct r_queue {
	char* p_ack[R_QUEUE_SIZE];
	int front;
	int rear;
}r_queue_s;


typedef char(*p_arg)[MAX_ARG_LEN];

typedef enum {
	APN_E = 0,
	GADC_E = 15,
}CMD_CODE_E;

extern APN_INFO apn_info;
extern CMD_PRO_ARCH_T cmd_pro_reg[];

const char cmd_set[] = "APN,123456,iot.nb,michael,138114#";
const char cmd_query[] = "APN,123456?";

static char g_sys_passwd[SYS_PASSWD_MAX_LEN+1] = SYS_DEFAULT_PASSWD;

char g_imei_buf[MAX_IMEI_LEN] = { 0 };
char g_dev_name[MAX_DEV_LEN] = DFT_DEV_NAME;
char g_real_time[MAX_RT_LEN] = { 0 };

#define UNIT_TEST



#endif //__ATEL_BG96_APP__

#endif //__IF_SERVER_H__

