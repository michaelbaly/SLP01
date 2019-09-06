/******************************************************************************
*@file    atel_bg96_app.c
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
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "events_if.h"



#define LEFT_SHIFT_OP(N)		(1 << (N))

typedef enum {
	NODE_SIG_EVT_PUT_E		= LEFT_SHIFT_OP(0),
	NODE_SIG_EVT_GET_E		= LEFT_SHIFT_OP(1),
	NODE_SIG_EVT_CONN_E		= LEFT_SHIFT_OP(2),
	NODE_SIG_EVT_DIS_E		= LEFT_SHIFT_OP(3),
	NODE_SIG_EVT_EXIT_E		= LEFT_SHIFT_OP(4),
	NODE_SIG_EVT_MAX_E		= LEFT_SHIFT_OP(5)
} rep_sig_evt_e;

/**************************************************************************
*                                 DEFINE
***************************************************************************/
TX_BYTE_POOL *byte_pool_report;
#define REP_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_report[REP_BYTE_POOL_SIZE];


/* thread handle */
TX_THREAD* qt_sub1_thread_handle; 
char *qt_sub1_thread_stack = NULL;

TX_THREAD* qt_sub2_thread_handle; 
char *qt_sub2_thread_stack = NULL;

#define QT_SUB1_THREAD_PRIORITY   	180
#define QT_SUB1_THREAD_STACK_SIZE 	(1024 * 16)

#define QT_SUB2_THREAD_PRIORITY   	180
#define QT_SUB2_THREAD_STACK_SIZE 	(1024 * 16)

que_table* qt = NULL;


/**************************************************************************
*                                 GLOBAL
***************************************************************************/

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/


/**************************************************************************
*                                 FUNCTION
***************************************************************************/
void* get_buf(que_table* q)
{
	static node* head = NULL;

	if(q->pool)
	{
	
		atel_dbg_print("q->pool address is %p", q->pool);
		head = q->pool;
	}
	else
	{
		atel_dbg_print("pool is full");
	}

	return head;
}

void put_buf(que_table* q, node* buf)
{
	node* head = buf;

	if(q->pool)
	{
		head->next = q->pool;

		/* update pool head */
		q->pool = head;
	}
	else
	{
		q->pool = buf;
	}
}

void display_pool(que_table* q)
{

  int i = 1;
  node* tmp = q->pool;

  while(tmp) {
    atel_dbg_print("[node%d]addr %p\n", i++, tmp);
    tmp = tmp->next;
  }
}


int create_pool(que_table **q, int num_node)
{
	int ret = 0xff;
	node* head = NULL;
	node* tmp = NULL;
	node* tmp_alloc = NULL;
	
    ret = tx_byte_allocate(byte_pool_report, (VOID *)&head, sizeof(node), TX_NO_WAIT);
  	if (ret != TX_SUCCESS)
  	{
  		atel_dbg_print("tx_byte_allocate [create_pool] tx_byte_allocate failed!");
    	return ret;
  	}

	/* first node of pool */
	tmp = head;

	while(num_node-- > 1)
	{
		ret =  tx_byte_allocate(byte_pool_report, (VOID *)&tmp_alloc, sizeof(node), TX_NO_WAIT);
	  	if (ret != TX_SUCCESS)
	  	{
	  		atel_dbg_print("tx_byte_allocate [create_pool] tx_byte_allocate failed!");
	    	return ret;
	  	}
		tmp->next = tmp_alloc;
		tmp = tmp->next;
	}

	tmp->next = NULL;

	/* specify the head node of pool */
	if(!(*q)->pool)
	{
		(*q)->pool = head;
	}

	#ifndef DEBUG
    display_pool(*q);
	#endif

	return TX_SUCCESS;
}

/*
 * Create a priority queue object of priority ranging from 0..PRIMAX-1
 */
int que_create(que_table* q)
{

  int i = 0;
  int ret = TX_SUCCESS;
  que_stat* stat = NULL;

  /*
   * Initialize the queue table
   */
  for(i = 0; i < PRIO_MAX; i++) {
    q->list_entry[i].prio = i;
    q->list_entry[i].ele = NULL;
    q->last_ele[i] = NULL;
  }

  create_pool(&q, NODE_NUM_MAX);

  ret = tx_byte_allocate(byte_pool_report, (VOID *)&stat, sizeof(que_stat), TX_NO_WAIT);
  if (ret != TX_SUCCESS)
  {
	  atel_dbg_print("tx_byte_allocate [que_create] tx_byte_allocate failed!");
	  return ret;
  }

  q->stat = stat;

  memset ( &(q->lock), 0, sizeof(TX_MUTEX));
  memset ( &(q->cv), 0, sizeof(TX_EVENT_FLAGS_GROUP));

  q->is_avail = false;
  q->entry_cnt = PRIO_MAX;

  return ret;
}



void add_node_tail(que_table* q, node** last, node** cur, int key, int prio)
{
	
	ULONG node_event;
	
	/* lock the area */
	tx_mutex_get(&q->lock, TX_NO_WAIT);

	/* allocate mem for node from pool */
	node* nd = (node*)get_buf(q);
	if (NULL == nd)
	{
		/* pool is full, wait for dequeue */
		tx_event_flags_get(&q->cv, NODE_SIG_EVT_PUT_E, TX_OR, &node_event, TX_WAIT_FOREVER);
		
		if(node_event&NODE_SIG_EVT_PUT_E)
		{
		
			tx_event_flags_set(&q->cv, ~NODE_SIG_EVT_PUT_E, TX_AND);
			atel_dbg_print("[get_node]got node, start processing...");
			nd = (node*)get_buf(q);
		}
	}

	/* fill the node */
	nd->key = key;
	nd->prio = prio;
	nd->next = NULL;

	/* if head node doesn't exist */
	if (NULL == *cur)
		* cur = nd;
	else
		(*last)->next = nd;
	/* update the last indicate */
	*last = nd;

	printf("[enqueue]%d\n", q->stat->enque++);

	/* queue avaliable */
	q->is_avail = true;

	/* send signal and release lock */
	tx_event_flags_set(&q->cv, NODE_SIG_EVT_GET_E, TX_OR);
	tx_mutex_put(&q->lock);

}

/* add a node to the queue */
void put_node(que_table* q, int key, int prio)
{
	//assert(q);
	//assert(prio < PRIO_MAX);

	/* add node */
	add_node_tail(q, &(q->last_ele[prio]), &(q->list_entry[prio].ele), key, prio);

}

void get_node(que_table* q, int* key, int* prio)
{
	//assert(q);

	node* tmp = NULL;
	int index = 0xff;
	ULONG node_event;

	/* get the mutex */
	tx_mutex_get(&q->lock, TX_NO_WAIT);

wait_node:
	
	while(false == q->is_avail)
	{
		atel_dbg_print("[get_node]queue is empty, waiting...");
		tx_event_flags_get(&q->cv, NODE_SIG_EVT_GET_E, TX_OR, &node_event, TX_WAIT_FOREVER);
		if(node_event&NODE_SIG_EVT_GET_E)
		{
		
			tx_event_flags_set(&q->cv, ~NODE_SIG_EVT_GET_E, TX_AND);
			atel_dbg_print("[get_node]got node, start processing...");
			break;
		}
	}
	
	for (index = PRIO_MAX - 1; index >= 0; index--)
	{
		/* have node on cur list */
		if (q->list_entry[index].ele)
		{
			tmp = q->list_entry[index].ele;
			/* get cur node */
			*key  = tmp->key;
			*prio = tmp->prio;

			/* move to next node */
			q->list_entry[index].ele = tmp->next;

			printf("[dequeue]%d\n", q->stat->deque++);
			put_buf(q, tmp);

			/* send event and release the mutex */
			tx_event_flags_set(&q->cv, NODE_SIG_EVT_PUT_E, TX_OR);
			tx_mutex_put(&q->lock);
			/* return once got the highest prio node */
			return;
		}
	}

	q->is_avail = false;

	goto wait_node;
}


void release_buff(que_table *q)
{
  node *n = q->pool;
  node* temp = NULL;

  while(n) {
    temp = n;
    n = n->next;
    free(temp);
  }
  free(q);
  temp = NULL;

  return;
}


/*
@func
  gen_process
@brief
  event or alarm generate entry
*/
void gen_process(void)
{
	que_table *table = qt;
	int i = 0;

    atel_dbg_print("event report thread");
	
	while(1) {

		/* exit after put 16 nodes */
		if (i++ == 16) break;
		
		atel_dbg_print("Calling put_node %d", i);

		put_node(table, i, (i % 3));
	}
}


/*
@func
  rep_process
@brief
  event or alarm report entry
*/
void rep_process(void)
{

	que_table *table = qt;
	
	int key, prio;
	
	atel_dbg_print("event generate thread\n");
	int i = 0;
	while(1) {
		
		atel_dbg_print("calling get_node\n");
		get_node(table, &key, &prio);
	
		atel_dbg_print("[rep_process] prio=%d key= %d\n", prio, key);
		if (i++ == 15) break;
	}
	
	return;
}



/*
@func
  events_report
@brief
  report events based on event type
*/
void events_report(EVENTS_T type, int report_flag)
{
	return;
}

/*
@func
  alarms_report
@brief
  report events based on event type
*/
void alarms_report(EVENTS_T type, int report_flag)
{
	return;
}


void alarm_event_process(void)
{
	

	return;
}



void gen_exit(TX_THREAD* gen, UINT status)
{
	if(status == 1)
	{
		atel_dbg_print("gen thread exit");
	}

	return ;
}

void rep_exit(TX_THREAD* gen, UINT status)
{
	if(status == 1)
	{
		atel_dbg_print("rep thread exit");
	}
	return ;
}

bool ig_off_int()
{
	/* get ignition status from ble */

	/* call ig status api */
}

char* atel_itoa(int n, char* chBuffer)
{
	int i = 1;
	char* pch = chBuffer;
	if (!pch) return 0;
	while (n / i) i *= 10;

	if (n < 0)
	{
		n = -n;
		*pch++ = '-';
	}
	if (0 == n) i = 10;

	while (i /= 10)
	{
		*pch++ = n / i + '0';
		n %= i;
	}
	*pch = '\0';
	return chBuffer;
}


char * get_event_type(MSG_TYPE_REL_E evt_bit)
{
	char evt_tmp[5] = {0};
	switch(evt_bit)
	{
		case AUTO_REPORT_EVT:
			return atel_itoa(AUTO_REPORT_E, evt_tmp);
		case IG_ON_EVT:
			return atel_itoa(IG_ON_E, evt_tmp);
		case IG_OFF_EVT:
			return atel_itoa(IG_OFF_E, evt_tmp);

		default:
		    atel_dbg_print("under development");
			break;
	}

	return NULL;
}



#if 0
int prioque_task_entry(void)
{
	int ret = -1;
	UINT status = 0;

	//que_table* qt = NULL;
	
    

	/* prompt task running */
	atel_dbg_print("[quectel_task_entry] start task ~");

	ret = txm_module_object_allocate(&byte_pool_report, sizeof(TX_BYTE_POOL));
	if(ret != TX_SUCCESS)
	{
		atel_dbg_print("[create_pool] txm_module_object_allocate [byte_pool_report] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_pool_create(byte_pool_report, "package report pool", free_memory_report, REP_BYTE_POOL_SIZE);
	if(ret != TX_SUCCESS)
	{
		atel_dbg_print("[create_pool] tx_byte_pool_create [byte_pool_report] failed, %d", ret);
		return ret;
	}

	/* create a prio queue */
	ret = tx_byte_allocate(byte_pool_report, (VOID *)&qt, sizeof(que_table), TX_NO_WAIT);
  	if (ret != TX_SUCCESS)
  	{
  		atel_dbg_print("tx_byte_allocate [quectel_task_entry] tx_byte_allocate failed!");
    	return ret;
  	}


	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&qt_sub1_thread_handle, sizeof(TX_THREAD))) 
	{
		atel_dbg_print("[task] txm_module_object_allocate gen_process failed");
		return - 1;
	}

	ret = tx_byte_allocate(byte_pool_report, (VOID **) &qt_sub1_thread_stack, QT_SUB1_THREAD_STACK_SIZE, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		atel_dbg_print("[task] tx_byte_allocate qt_sub1_thread_stack failed");
		return ret;
	}

	/* create event generate thread */
	ret = tx_thread_create(qt_sub1_thread_handle,
						   "atel event generate thread",
						   gen_process,
						   NULL,
						   qt_sub1_thread_stack,
						   QT_SUB1_THREAD_STACK_SIZE,
						   QT_SUB1_THREAD_PRIORITY,
						   QT_SUB1_THREAD_PRIORITY,
						   TX_NO_TIME_SLICE,
						   TX_AUTO_START
						   );
	      
	if(ret != TX_SUCCESS)
	{
		atel_dbg_print("[task] : Thread creation failed");
	}

	/* create a new task2 */
	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&qt_sub2_thread_handle, sizeof(TX_THREAD))) 
	{
		atel_dbg_print("[task] txm_module_object_allocate qt_sub1_thread_handle failed");
		return - 1;
	}

	ret = tx_byte_allocate(byte_pool_report, (VOID **) &qt_sub2_thread_stack, QT_SUB2_THREAD_STACK_SIZE, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		atel_dbg_print("[task] tx_byte_allocate qt_sub1_thread_stack failed");
		return ret;
	}

	/* create a new task : sub2 */
	ret = tx_thread_create(qt_sub2_thread_handle,
						   "atel event process hread",
						   rep_process,
						   NULL,
						   qt_sub2_thread_stack,
						   QT_SUB2_THREAD_STACK_SIZE,
						   QT_SUB2_THREAD_PRIORITY,
						   QT_SUB2_THREAD_PRIORITY,
						   TX_NO_TIME_SLICE,
						   TX_AUTO_START
						   );
	      
	if(ret != TX_SUCCESS)
	{
		atel_dbg_print("[task] : Thread creation failed");
	}

	//tx_thread_entry_exit_notify(TX_THREAD *thread_ptr, VOID (*entry_exit_notify)(TX_THREAD *, UINT))

	while (1)
	{
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		atel_dbg_print("[quectel_task_entry] main task running...");
		//tx_thread_entry_exit_notify(qt_sub1_thread_handle, gen_exit);
		
		//tx_thread_entry_exit_notify(qt_sub2_thread_handle, rep_exit);
	}
}
#endif




#endif /*__EXAMPLE_BG96_APP__*/



