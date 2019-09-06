#if defined (__ATEL_BG96_APP__)

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include "txm_module.h"
#include "if_server.h"



/*===========================================================================
                             DEFINITION
===========================================================================*/




/*===========================================================================
                           Global variable
===========================================================================*/
#define DFT_INTEVAL 20000 //auto report interval(seconds) after system up

/* begin: rq for cmd from server */
r_queue_s rq;
/* end */

/* this contains apn/username/password */
APN_INFO apn_fix = { {0} };
APN_INFO apn_tmp = { {0} };

intval_info auto_intval_fix = {{0}};
intval_info auto_intval_tmp = {{0}};
ULONG auto_intval_last = DFT_INTEVAL;//default auto report interval

ADC_INFO adc_info = { {0} };


/* get first ele of the cmd queue */
char *g_que_first = NULL;
char ack_buffer[ACK_SET_LEN_MAX] = {0};

extern g_daily_timer_restart;


/*===========================================================================
                               FUNCTION
===========================================================================*/
/* begin  */
void apn_info_init(APN_INFO *apn_data)
{
	strncpy(apn_data->apn, DFT_APN, strlen(DFT_APN));
	strncpy(apn_data->apn_usrname, DFT_APN_USRNAME, strlen(DFT_APN_USRNAME));
	strncpy(apn_data->apn_passwd, DFT_APN_PASSWD, strlen(DFT_APN_PASSWD));

}

bool apn_set(p_arg arg_each, uint8 arg_cnt)
{
	/* apn offset */
	p_arg arg_list = arg_each + ACTUAL_ARG_OFFSET;

	/* clear apn info */
	memset(&apn_fix, 0, sizeof(apn_fix));

	strncpy(apn_fix.apn, arg_list, strlen(arg_list));
	if (strncmp(apn_fix.apn, arg_list, strlen(arg_list)))
	{
		atel_dbg_print("g_apn:%s\n", apn_fix.apn);
		return FALSE;
	}

	/* if username and password specified */
	if (arg_cnt == ACTUAL_ARG_OFFSET + 3)
	{
		arg_list++;
		strncpy(apn_fix.apn_usrname, arg_list, strlen(arg_list));
		if (strncmp(apn_fix.apn_usrname, arg_list, strlen(arg_list)))
		{
			atel_dbg_print("apn_usrname:%s\n", apn_fix.apn_usrname);
			return FALSE;
		}

		arg_list++;
		strncpy(apn_fix.apn_passwd, arg_list, strlen(arg_list));
		if (strncmp(apn_fix.apn_passwd, arg_list, strlen(arg_list)))
		{
			atel_dbg_print("apn_passwd:%s\n", apn_fix.apn_passwd);
			return FALSE;
		}

	}

	return TRUE;
}

bool apn_query(APN_INFO *apn_data)
{
	APN_INFO* apn_tmp = apn_data;

	/* apn info valid ? */
	if (!strlen(apn_fix.apn) || (!strlen(apn_fix.apn_usrname) && !strlen(apn_fix.apn_passwd)))
	{
		/* apn info invalid */
		return FALSE;
	}

	memset(apn_tmp, 0, sizeof(APN_INFO));
	/* fill mem of apn_data */
	strncpy(apn_tmp->apn, apn_fix.apn, strlen(apn_fix.apn));
	strncpy(apn_tmp->apn_usrname, apn_fix.apn_usrname, strlen(apn_fix.apn_usrname));
	strncpy(apn_tmp->apn_passwd, apn_fix.apn_passwd, strlen(apn_fix.apn_passwd));


	return TRUE;
}

bool auto_interval_set(p_arg arg_each, uint8 arg_cnt)
{
	/* interval param offset */
	p_arg arg_list = arg_each + ACTUAL_ARG_OFFSET;
	ULONG cur_intval = 0xff;
	//ULONG last_intval = 0xff;

	cur_intval = atoi(arg_list);
	if(auto_intval_last == cur_intval)
	{	
		atel_dbg_print("[rejected]new interval same with old one");
		return FALSE;
	}
		
	/* clear auto report intval info */
	memset(&auto_intval_fix, 0, sizeof(auto_intval_fix));

	strncpy(auto_intval_fix.intval, arg_list, strlen(arg_list));
	if (strncmp(auto_intval_fix.intval, arg_list, strlen(arg_list)))
	{
		atel_dbg_print("auto_intval set failed, cur intval:%s\n", arg_list);
		return FALSE;
	}

	/* store last intval */
	auto_intval_last = atoi(auto_intval_fix.intval);

	/* set auto report timer re-activate flag */
	g_daily_timer_restart = TRUE;

	return TRUE;
	
}

bool auto_interval_query(intval_info *cur_intval)
{
	intval_info * int_tmp = cur_intval;
	char * intval_tmp = NULL;
	char intval_buf[5] = {0};

	atel_itoa(DFT_INTEVAL, intval_buf);

	/* copy auto interval */
	if(auto_intval_last == DFT_INTEVAL)
	{
		strncpy(int_tmp->intval, intval_buf, strlen(intval_buf));
	}
	else
	{
		strncpy(int_tmp->intval, auto_intval_fix.intval, strlen(auto_intval_fix.intval));
	}
	
	return TRUE;
}


bool server_set(p_arg arg_each, uint8 arg_cnt)
{
	
}

bool server_query(SER_INFO *ser_data)
{

}

bool getadc(ADC_INFO* adc_data)
{
	ADC_INFO* adc_tmp = adc_data;

	/* get adc data from BLE through the get_adc_ble api */




	/* apn info valid ? */
	if (!strlen(adc_info.power) && !strlen(adc_info.in_batt) && !strlen(adc_info.ex_batt))
	{
		/* apn info invalid */
		return FALSE;
	}

	/* fill mem of apn_data */
	strncpy(adc_tmp->power, adc_info.power, strlen(adc_info.power));
	strncpy(adc_tmp->in_batt, adc_info.in_batt, strlen(adc_info.in_batt));
	strncpy(adc_tmp->ex_batt, adc_info.ex_batt, strlen(adc_info.ex_batt));


	return TRUE;
}

/* end */






/* begin:cmd register framework */
CMD_PRO_ARCH_T cmd_pro_reg[TOTAL_CMD_NUM] = {

	/* APN */
	{.cmd_code = "APN", 			.cmd_rel_st = (void*)&apn_tmp, 			.ele_num = APN_ELE_NUM, 	\
	 .cmd_set_f = apn_set, 			.cmd_get_f = apn_query },
	 
	{.cmd_code = "SERVER", 																		.cmd_set_f = server_set, 	.cmd_get_f = server_query},

	{.cmd_code = "NOMOVE",																		.cmd_set_f = apn_set,		.cmd_get_f = apn_query },

	{.cmd_code = "INTERVAL", 		.cmd_rel_st = (void*)&auto_intval_tmp,	.ele_num = 	INTVAL_ELE_NUM,		\
	 .cmd_set_f = auto_interval_set, 	.cmd_get_f = auto_interval_query},

	{.cmd_code = "CONFIG",																		.cmd_set_f = apn_set,		.cmd_get_f = apn_query },
	{.cmd_code = "SPEEDALARM", 																	.cmd_set_f = server_set, 	.cmd_get_f = server_query},
	{.cmd_code = "PROTOCOL",																	.cmd_set_f = apn_set,		.cmd_get_f = apn_query },
	{.cmd_code = "VERSION", 																	.cmd_set_f = NULL, 			.cmd_get_f = server_query},
	{.cmd_code = "PASSWORD",																	.cmd_set_f = apn_set,		.cmd_get_f = NULL },
	{.cmd_code = "GPS", 																		.cmd_set_f = server_set, 	.cmd_get_f = NULL},
	{.cmd_code = "MILEAGE",																		.cmd_set_f = apn_set,		.cmd_get_f = apn_query },
	{.cmd_code = "RESENDINTERVAL", 																.cmd_set_f = server_set, 	.cmd_get_f = server_query},
	{.cmd_code = "PDNTYPE",																		.cmd_set_f = apn_set,		.cmd_get_f = apn_query },
	{.cmd_code = "FACTORYRESET", 																.cmd_set_f = server_set, 	.cmd_get_f = NULL},
	{.cmd_code = "OTASTART",																	.cmd_set_f = apn_set,		.cmd_get_f = NULL },
	{.cmd_code = "GETADC", 																		.cmd_set_f = NULL, 			.cmd_get_f = getadc},
	{.cmd_code = "REPORT",																		.cmd_set_f = NULL,			.cmd_get_f = apn_query },
	{.cmd_code = "HEARTBEAT", 																	.cmd_set_f = server_set, 	.cmd_get_f = server_query},
	{.cmd_code = "GEOFENCE",																	.cmd_set_f = apn_set,		.cmd_get_f = apn_query },
	{.cmd_code = "OUTPUT", 																		.cmd_set_f = server_set, 	.cmd_get_f = server_query},
	{.cmd_code = "HEADING",																		.cmd_set_f = apn_set,		.cmd_get_f = apn_query },
	{.cmd_code = "IDLINGALERT", 																.cmd_set_f = server_set, 	.cmd_get_f = server_query},
	{.cmd_code = "HARSHBRAKING",																.cmd_set_f = apn_set,		.cmd_get_f = apn_query },
	{.cmd_code = "HARSHACCEL", 																	.cmd_set_f = server_set, 	.cmd_get_f = server_query},
	{.cmd_code = "HARSHIMPACT",																	.cmd_set_f = apn_set,		.cmd_get_f = apn_query },
	{.cmd_code = "HARSHSWERVE", 																.cmd_set_f = server_set, 	.cmd_get_f = server_query},
	{.cmd_code = "LOWINBATT",																	.cmd_set_f = apn_set,		.cmd_get_f = apn_query },
	{.cmd_code = "LOWEXBATT", 																	.cmd_set_f = server_set, 	.cmd_get_f = server_query},
	{.cmd_code = "VIRTUALIGNITION",																.cmd_set_f = apn_set,		.cmd_get_f = apn_query },
	{.cmd_code = "CLEARBUFFER", 																.cmd_set_f = server_set, 	.cmd_get_f = NULL},
	{.cmd_code = "THEFTMODE",																	.cmd_set_f = apn_set,		.cmd_get_f = apn_query },
	{.cmd_code = "TOWALARM", 																	.cmd_set_f = server_set, 	.cmd_get_f = server_query},
	{.cmd_code = "WATCHDOG",																	.cmd_set_f = apn_set,		.cmd_get_f = apn_query },
	{.cmd_code = "REBOOT", 																		.cmd_set_f = server_set, 	.cmd_get_f = server_query},

};

/* end */



/* init the ring queue */
void rq_init()
{
	char cnt = 0;
	
	rq.front = rq.rear = 0;
	while(cnt < R_QUEUE_SIZE)
		rq.p_ack[cnt++] = NULL;
}

/* judge empty */
bool isempty(r_queue_s rq)
{
	if(rq.front == rq.rear)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/* judge full */
bool isfull(r_queue_s rq)
{
	if ((rq.rear + 1) % R_QUEUE_SIZE == rq.front)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/* enqueue */
void enqueue(char *cmd)
{
	if(isfull(rq))
	{
		atel_dbg_print("queue is full\n");
		return;
	}

	rq.p_ack[rq.rear] = cmd;
	rq.rear = (rq.rear + 1) % R_QUEUE_SIZE;

}

void dequeue()
{
	if (isempty(rq))
	{
		atel_dbg_print("queue is empty\n");
		return;
	}

	atel_dbg_print("[dequeue]the release addr is %p\n", rq.p_ack[rq.front]);
	/* release the allocated buff */
	tx_byte_release(rq.p_ack[rq.front]);

	/* move head pointer to next */
	rq.front = (rq.front + 1) % R_QUEUE_SIZE;

}

char * get_first_ele()
{
	atel_dbg_print("[get_first_ele]the returned addr is %p\n", rq.p_ack[rq.front]);
	return rq.p_ack[rq.front];
}


void show_queue_cont()
{
	char *tmp = rq.p_ack[rq.front];
	
	while(tmp)
	{
		atel_dbg_print("[cur_queue]%s", tmp);
        rq.front++;
		tmp = rq.p_ack[rq.front];
	}
}


/* show apn info */
void apn_info_show()
{
	atel_dbg_print("the apn info:\n");
	atel_dbg_print("apn:%s\n", apn_fix.apn);
	atel_dbg_print("apn_usrname:%s\n", apn_fix.apn_usrname);
	atel_dbg_print("apn_passwd:%s\n", apn_fix.apn_passwd);

	return;
}





/*
@func
	para_cmd_str
@brief
	parse the cmd buff from server.
	[input]:
		cmd_str ---> cmd string from socket buffer
	[output]:
		arg_list ---> store each paramters
		c_type ---> get command type(set:1; query:0)
*/

uint8 parse_cmd_str(char* cmd_str, p_arg arg_list, CMD_TYPE_E *c_type)
{
	char* cmd_tmp = cmd_str;
	char* arg_start = NULL;
	uint8 arg_cnt = 0;

#ifdef ATEL_DEBUG
	atel_dbg_print("[cmd from server] %s\n", cmd_str);
#endif

	/* parase the count of parameters */
	while (NULL != cmd_tmp)
	{
		arg_start = cmd_tmp;
		cmd_tmp = strchr(cmd_tmp, ',');

		if (cmd_tmp)
		{
			/* extract each param */
			strncpy(*arg_list++, arg_start, cmd_tmp - arg_start);
			cmd_tmp++;	/* skip ',' */
		}
		else
		{	
			//cmd_tmp = strchr(arg_start, '#');
			if (cmd_tmp = strchr(arg_start, '#'))
			{
				*c_type = SET_CMD_E;
				strncpy(*arg_list++, arg_start, cmd_tmp - arg_start);
				break;
			}
			else if (cmd_tmp = strchr(arg_start, '?'))
			{
				*c_type = QUERY_CMD_E;
				strncpy(*arg_list++, arg_start, cmd_tmp - arg_start);
				break;
			}
			else
			{
				printf("error command format\n");
			}
		}

		arg_cnt++;
	}

	return ++arg_cnt;
}

/*
@func
	para_cmd_str
@brief
	parse the cmd buff from server.
	[input]:
		cmd_str ---> cmd string from socket buffer
	[output]:
		arg_list ---> store each paramters
		c_type ---> get command type(set:1; query:0)
*/
bool cmd_match(char* cmd_code, unsigned char *index)
{
	int i = 0;

	for (; i < TOTAL_CMD_NUM; ++i)
	{
		if (!strncmp(cmd_code, cmd_pro_reg[i].cmd_code, strlen(cmd_pro_reg[i].cmd_code)))
		{
			*index = i;
			return TRUE;
		}
	}

	return FALSE;
}


/* obtain IMEI info through api */
char* atel_get_imei()
{

	/* return the addr which contains imei info */
	//return g_imei_buf;
	return "866425037710391";
	
}

char* atel_get_dname()
{
	//return g_dev_name;
	return DFT_DEV_NAME;
}

char* atel_get_real_time()
{
	//return g_real_time;
	return "20190820171650";
}



/* begin: version 2 for build ack */
/* build the packet based on cmd and cmd type */
void build_ack(uint8 cmd_idx, bool cmd_type, bool status, void* cmd_rel_s)
{
	char* p_ack = ack_buffer;
	char* p_imei = NULL;
	char* p_dev_name = NULL;
	char* p_real_time = NULL;
	int use_len = 0;
	int i = 0;


	/* aquery struct type */
	p_arg cmd_rel_tmp = (p_arg)cmd_rel_s;
	
	//+ACK:APN,<IMEI>,<device_name>,<status>,<send_time>#

	memset(ack_buffer, 0, sizeof(ack_buffer));
	
	//compound ack
	p_imei = atel_get_imei();
	p_dev_name = atel_get_dname();
	p_real_time = atel_get_real_time();
	
	sprintf(ack_buffer, "+ACK:%s,%s,%s,%s", cmd_pro_reg[cmd_idx].cmd_code, p_imei, p_dev_name, p_real_time);
	use_len += strlen(ack_buffer);

	/* attach execution status */
	if (status)
	{
		sprintf(ack_buffer + use_len, ",%s", EXEC_STATUS_OK);
		use_len += strlen(EXEC_STATUS_OK) + 1;
	}
	else
	{
		sprintf(ack_buffer + use_len, ",%s", EXEC_STATUS_ERROR);
		use_len += strlen(EXEC_STATUS_ERROR) + 1;
	}

	/* query cmd_type */
	if (!cmd_type)
	{
		/* extract each mem from the struct */

		/* fill mem of struct pointer */
		for (; i < cmd_pro_reg[cmd_idx].ele_num; i++)
		{
			sprintf(ack_buffer + use_len, ",%s", (char*)cmd_rel_tmp);
			use_len += strlen((char*)cmd_rel_tmp) + 1;
			cmd_rel_tmp++;
		}

	}
	/* attch terminiter charactor */
	sprintf(ack_buffer + use_len, "%s", "#");
	use_len += 1;

#ifdef ATEL_DEBUG
	atel_dbg_print("cnt of ack_buffer:%s\nthe use_len is:%d\n", ack_buffer, use_len);
#endif

}

/* end */

/* begin:interface with server */
/* [input] head ele of the cmd queue */
int cmd_parse_entry(char* que_first)
{
	uint8 arg_cnt = 0;
	uint8 status = 0xff;
	int i = 0;
	uint8 index = 0xff;
	char arg_list[MAX_ARG_NUM][MAX_ARG_LEN] = { 0 };
	CMD_TYPE_E cmd_type = 0xff;
	//COMMON_LOAD com_ack;
	APN_INFO apn_data = { {0} };
	p_arg arg_each = NULL;

	memset(arg_list, 0, sizeof(arg_list));
		
	//atel_dbg_print("[cmd_parse_entry]: %p\n", rq.p_ack[rq.front]);
	
	arg_cnt = parse_cmd_str(que_first, arg_list, &cmd_type);

	atel_dbg_print("cmd_type: %d\n", cmd_type);
	atel_dbg_print("arg_cnt: %d\n", arg_cnt);

	for (; i < arg_cnt; i++)
	{
		atel_dbg_print("arg_list[arg_cnt]: %s\n", *(arg_list + i));
	}
	
	
	/* compound each arg for different use */

	/* return if password is invalid */
	if (!cmd_match(arg_list[0], &index) || strncmp(g_sys_passwd, arg_list[1], SYS_PASSWD_MAX_LEN))
	{
		atel_dbg_print("error cmd or password!\n");
		return ERROR_CMD_PWD_E;
	}

	/* continue the flow */
	switch (cmd_type)
	{
		case QUERY_CMD_E:
			atel_dbg_print("query command type\n");
			status = cmd_pro_reg[index].cmd_get_f(cmd_pro_reg[index].cmd_rel_st);
			break;

		case SET_CMD_E:
			
			atel_dbg_print("set command type\n");
			/* execute the registered set function */
			arg_each = (p_arg)arg_list[0];
			status = cmd_pro_reg[index].cmd_set_f(arg_each, arg_cnt);
			break;

		default:
		{
			atel_dbg_print("unknown command type\n");
			break;
		}

	}

	/* apn info */
	//apn_info_show();

	/* build ack for server */
	build_ack(index, cmd_type, status, cmd_pro_reg[index].cmd_rel_st);

	
	atel_dbg_print("frame build over\n");
	//enqueue the ack
	
	
	return TRUE;

}





/* end */






#endif
