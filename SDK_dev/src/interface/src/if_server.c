#if defined (__ATEL_BG96_APP__)

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "if_server.h"
//#include "cmd_type.h"



/*===========================================================================
                             DEFINITION
===========================================================================*/




/*===========================================================================
                           Global variable
===========================================================================*/


/*===========================================================================
                               FUNCTION
===========================================================================*/
/* begin  */
/* this contains apn/username/password */
APN_INFO apn_info = { {0} };
APN_INFO apn_data = { {0} };

ADC_INFO adc_info = { {0} };


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
	memset(&apn_info, 0, sizeof(apn_info));

	strncpy(apn_info.apn, arg_list, strlen(arg_list));
	if (strncmp(apn_info.apn, arg_list, strlen(arg_list)))
	{
		atel_dbg_print("g_apn:%s\n", apn_info.apn);
		return FALSE;
	}

	/* if username and password specified */
	if (arg_cnt == ACTUAL_ARG_OFFSET + 3)
	{
		arg_list++;
		strncpy(apn_info.apn_usrname, arg_list, strlen(arg_list));
		if (strncmp(apn_info.apn_usrname, arg_list, strlen(arg_list)))
		{
			atel_dbg_print("apn_usrname:%s\n", apn_info.apn_usrname);
			return FALSE;
		}

		arg_list++;
		strncpy(apn_info.apn_passwd, arg_list, strlen(arg_list));
		if (strncmp(apn_info.apn_passwd, arg_list, strlen(arg_list)))
		{
			atel_dbg_print("apn_passwd:%s\n", apn_info.apn_passwd);
			return FALSE;
		}

	}

	return TRUE;
}

bool apn_query(APN_INFO *apn_data)
{
	APN_INFO* apn_tmp = apn_data;

	/* apn info valid ? */
	if (!strlen(apn_info.apn) || (!strlen(apn_info.apn_usrname) && !strlen(apn_info.apn_passwd)))
	{
		/* apn info invalid */
		return FALSE;
	}

	/* fill mem of apn_data */
	strncpy(apn_tmp->apn, apn_info.apn, strlen(apn_info.apn));
	strncpy(apn_tmp->apn_usrname, apn_info.apn_usrname, strlen(apn_info.apn_usrname));
	strncpy(apn_tmp->apn_passwd, apn_info.apn_passwd, strlen(apn_info.apn_passwd));


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
	{.cmd_code = "APN", .cmd_rel_st = (void*)&apn_data, .ele_num = APN_ELE_NUM, .cmd_set_f = apn_set, .cmd_get_f = apn_query },
	{.cmd_code = "SERVER", .cmd_set_f = server_set, .cmd_get_f = server_query},
	{.cmd_code = "NOMOVE",.cmd_set_f = apn_set,.cmd_get_f = apn_query },
	{.cmd_code = "INTERVAL", .cmd_set_f = server_set, .cmd_get_f = server_query},
	{.cmd_code = "CONFIG",.cmd_set_f = apn_set,.cmd_get_f = apn_query },
	{.cmd_code = "SPEEDALARM", .cmd_set_f = server_set, .cmd_get_f = server_query},
	{.cmd_code = "PROTOCOL",.cmd_set_f = apn_set,.cmd_get_f = apn_query },
	{.cmd_code = "VERSION", .cmd_set_f = NULL, .cmd_get_f = server_query},
	{.cmd_code = "PASSWORD",.cmd_set_f = apn_set,.cmd_get_f = NULL },
	{.cmd_code = "GPS", .cmd_set_f = server_set, .cmd_get_f = NULL},
	{.cmd_code = "MILEAGE",.cmd_set_f = apn_set,.cmd_get_f = apn_query },
	{.cmd_code = "RESENDINTERVAL", .cmd_set_f = server_set, .cmd_get_f = server_query},
	{.cmd_code = "PDNTYPE",.cmd_set_f = apn_set,.cmd_get_f = apn_query },
	{.cmd_code = "FACTORYRESET", .cmd_set_f = server_set, .cmd_get_f = NULL},
	{.cmd_code = "OTASTART",.cmd_set_f = apn_set,.cmd_get_f = NULL },
	{.cmd_code = "GETADC", .cmd_set_f = NULL, .cmd_get_f = getadc},
	{.cmd_code = "REPORT",.cmd_set_f = NULL,.cmd_get_f = apn_query },
	{.cmd_code = "HEARTBEAT", .cmd_set_f = server_set, .cmd_get_f = server_query},
	{.cmd_code = "GEOFENCE",.cmd_set_f = apn_set,.cmd_get_f = apn_query },
	{.cmd_code = "OUTPUT", .cmd_set_f = server_set, .cmd_get_f = server_query},
	{.cmd_code = "HEADING",.cmd_set_f = apn_set,.cmd_get_f = apn_query },
	{.cmd_code = "IDLINGALERT", .cmd_set_f = server_set, .cmd_get_f = server_query},
	{.cmd_code = "HARSHBRAKING",.cmd_set_f = apn_set,.cmd_get_f = apn_query },
	{.cmd_code = "HARSHACCEL", .cmd_set_f = server_set, .cmd_get_f = server_query},
	{.cmd_code = "HARSHIMPACT",.cmd_set_f = apn_set,.cmd_get_f = apn_query },
	{.cmd_code = "HARSHSWERVE", .cmd_set_f = server_set, .cmd_get_f = server_query},
	{.cmd_code = "LOWINBATT",.cmd_set_f = apn_set,.cmd_get_f = apn_query },
	{.cmd_code = "LOWEXBATT", .cmd_set_f = server_set, .cmd_get_f = server_query},
	{.cmd_code = "VIRTUALIGNITION",.cmd_set_f = apn_set,.cmd_get_f = apn_query },
	{.cmd_code = "CLEARBUFFER", .cmd_set_f = server_set, .cmd_get_f = NULL},
	{.cmd_code = "THEFTMODE",.cmd_set_f = apn_set,.cmd_get_f = apn_query },
	{.cmd_code = "TOWALARM", .cmd_set_f = server_set, .cmd_get_f = server_query},
	{.cmd_code = "WATCHDOG",.cmd_set_f = apn_set,.cmd_get_f = apn_query },
	{.cmd_code = "REBOOT", .cmd_set_f = server_set, .cmd_get_f = server_query},

};

/* end */



/* init the ring queue */
void rq_init()
{
	rq.front = rq.rear = 0;
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
void enqueue(char *packet)
{
	if(isfull(rq))
	{
		atel_dbg_print("queue is full\n");
		return;
	}

	rq.p_ack[rq.rear] = packet;
	rq.rear = (rq.rear + 1) % R_QUEUE_SIZE;

}

void dequeue()
{
	if (isempty(rq))
	{
		atel_dbg_print("queue is empty\n");
		return;
	}
	/* send packet via the socket */
	//send_packet(sfd, rq.p_ack[rq.front]);

	/* release packet buffer */
	/* show the allocate addr */
	atel_dbg_print("[dequeue]rq.p_ack[rq.front] addr ---> %p\n", rq.p_ack[rq.front]);
	tx_byte_release(rq.p_ack[rq.front]);

	/* move head pointer to next */
	rq.front = (rq.front + 1) % R_QUEUE_SIZE;
}


/* enqueue the packet */
void enq_ack(int sockfd, char *packet)
{
	/* put each ack to ring queue */
	enqueue(packet);

	/* send msg to udp client to triger the dequeue which means send ack to server */
	//tx_queue_send(TX_QUEUE * queue_ptr, VOID * source_ptr, ULONG wait_option);

	return;
}

/* show apn info */
void apn_info_show()
{
	atel_dbg_print("the apn info:\n");
	atel_dbg_print("apn:%s\n", apn_info.apn);
	atel_dbg_print("apn_usrname:%s\n", apn_info.apn_usrname);
	atel_dbg_print("apn_passwd:%s\n", apn_info.apn_passwd);

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
	p_arg cmd_rel_tmp = (p_arg*)cmd_rel_s;
	
	//+ACK:APN,<IMEI>,<device_name>,<status>,<send_time>#

	memset(ack_buffer, 0, sizeof(ack_buffer));
	
	//compound ack
	p_imei = atel_get_imei();
	p_dev_name = atel_get_dname();
	p_real_time = atel_get_real_time();
	
	//sprintf_s(ack_buffer, sizeof(ack_buffer), "+ACK:%s,%s,%s,%s,%s%s", cmd_pro_reg[cmd_idx].cmd_code, p_imei, p_dev_name, "OK", p_real_time,"#");
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

	/* if cmd_type is query */
	if (!cmd_type)
	{
		/* extract each mem from the struct */

		/* fill mem of struct pointer */
		for (; i < cmd_pro_reg[cmd_idx].ele_num; i++)
		{
			sprintf(ack_buffer + use_len, ",%s", cmd_rel_tmp);
			use_len += strlen(cmd_rel_tmp) + 1;
			cmd_rel_tmp++;
		}

	}
	/* attch terminiter charactor */
	sprintf(ack_buffer + use_len, "%s", "#");
	use_len += 1;


	atel_dbg_print("cnt of ack_buffer:%s\nthe use_len is:%d\n", ack_buffer, use_len);

	/* send ack to server through socket */
	//send_packet(sfd, ack_buffer);
}

/* end */

/* begin:interface with server */
int cmd_parse_entry(char* sock_buf)
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


	/* enqueue server command */
	enqueue(sock_buf);

	memset(arg_list, 0, sizeof(arg_list));
	//arg_cnt = para_cmd_str(cmd_set, arg_list, &cmd_type);
	
	arg_cnt = parse_cmd_str(cmd_set, arg_list, &cmd_type);

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
			arg_each = arg_list[0];
			status = cmd_pro_reg[index].cmd_set_f(arg_each, arg_cnt);
			break;

		default:
		{
			atel_dbg_print("unknown command type\n");
			break;
		}

	}

	/* apn info */
	apn_info_show();


	/* query result test */
#ifndef UNIT_TEST
	memset(arg_list, 0, sizeof(arg_list));
	arg_cnt = para_cmd_str(cmd_query, arg_list, &cmd_type);

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

#ifndef ATEL_DEBUG
	/* aquery the struct type */
	
#endif

	/* continue the flow */
#if 1
	switch (cmd_type)
	{
		case QUERY_CMD_E:
			atel_dbg_print("query command type\n");
			status = cmd_pro_reg[index].cmd_get_f(cmd_pro_reg[index].cmd_rel_st);
			break;

		case SET_CMD_E:
			/* execute the registered set function */
			arg_each = arg_list[0];
			status = cmd_pro_reg[index].cmd_set_f(arg_each, arg_cnt);
			break;

		default:
		{
			atel_dbg_print("unknown command type\n");
			break;
		}

	}
#endif
	

#endif

	/* build ack for server */
	build_ack(index, cmd_type, status, cmd_pro_reg[index].cmd_rel_st);

	
	atel_dbg_print("build frame over\n");
	//enqueue the ack
	
	
	return 0;

}





/* end */






#endif
