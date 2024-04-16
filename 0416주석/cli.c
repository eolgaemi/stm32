/*
 * cli.c
 *
 *  Created on: Apr 12, 2024
 *      Author: iot00
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include "cmsis_os.h"
#include "type.h"
#include "uart.h"
#include "app.h"
#include "tim.h"
#include "led.h"
#include "cli.h"

typedef struct {
	char *cmd;	// 명령어
	uint8_t no;	// 인자 최소 갯수
	int (*cbf)(int, char **);  // int argc, char *argv[]
	char *remark; // 설명
} CMD_LIST_T;

static osThreadId_t cli_thread_hnd;	// 쓰레드 핸들
//static osEventFlagsId_t cli_evt_id;		// 이벤트 플래그 핸들
static osMessageQueueId_t cli_msg_id;  //메시지큐 핸들

static const osThreadAttr_t cli_thread_attr = {
  .stack_size = 128 * 8,
  .priority = (osPriority_t) osPriorityNormal,
};

static int cli_led(int argc, char **argv);
static int cli_echo(int argc, char *argv[]);
static int cli_help(int argc, char *argv[]);
static int cli_mode(int argc, char *argv[]);
static int cli_dump(int argc, char *argv[]);
static int cli_duty(int argc, char *argv[]);
static int cli_btn_uart(int argc, char *argv[]);


static void cli_parser(BUF_T *arg);
//static void cli_event_set(void *arg);
static void cli_msg_put(void *arg);

static BUF_T gBufObj[1];

const CMD_LIST_T gCmdListObjs[] = {
	{ "btn",		2,		cli_btn_uart,		"button(uart)\r\n btn ['a'~'Z']"	},
	{ "duty",		2,		cli_duty,			"led 1 pwm duty\r\n duty [duty:0~999]"	},
	{ "dump",	3,		cli_dump,		"memory dump\r\n dump [address:hex] [length:max:10 lines]"	},
	{ "mode",	2,		cli_mode,		"change application mode\r\n mode [0/1]"	},
	{ "led",		3,		cli_led,			"led [1/2/3] [on/off]"	},
	{ "echo",		2,		cli_echo,			"echo [echo data]"	},
	{ "help", 		1, 		cli_help, 			"help" 					},
	{ NULL,		0,		NULL,				NULL						}
};

// cli = 문자를 받아서, Queue에 넣고 뭔가 하는 앱 초기화
void cli_init(void)
{
	//cli_evt_id = osEventFlagsNew(NULL);

	// 우리 메시지로 통신 할 것 인데, 그 메시지를 담을 Queue를 생성하고 초기화
	cli_msg_id = osMessageQueueNew(3, sizeof(MSG_T), NULL);

	// 만약에 생성 했으면,
	if (cli_msg_id != NULL) printf("CLI Message Queue Created...\r\n");

	// 생성이 안 되었으면
	else {
		printf("CLI Message Queue  Create File...\r\n");
		while (1);
	}

	// cli 앱을 실행할 쓰레드 생성
	cli_thread_hnd = osThreadNew(cli_thread, NULL, &cli_thread_attr);

	// 만약에 생성 했으면,
	if (cli_thread_hnd != NULL) printf("CLI Thread Created...\r\n");

	// 생성이 안 되었으면
	else {
		printf("CLI Thread Create Fail...\r\n");
		while (1);
	}
}


//extern UART_HandleTypeDef huart2;


// ---------------------------------------------- cli 명령어 처리 부 시작 ---------------------------------------------- //


// 각 cli 명령어("btn", "duty", "dump", "mode" ~~~~ "help")에 대응되는 기능을 구현 쭈욱~
// 구현하고 각 함수의 포인터(주소)를 gCmdListObjs[] 에 넣었음 위에서
static int cli_btn_uart(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Err : Arg No\r\n");
		return -1;
	}

	//HAL_UART_Transmit(&huart2, (uint8_t *)&argv[1][0], 1, 0xffff);
	printf("cli_btn:%c\r\n", argv[1][0]);

	return 0;
}

static int cli_duty(int argc, char *argv[])
{
	uint16_t duty;

	if (argc < 2) {
		printf("Err : Arg No\r\n");
		return -1;
	}

	duty = (uint16_t)strtol(argv[1], NULL, 10);
	printf("tim_duty_set(%d)\r\n", duty);
	//tim_duty_set(duty);

	return 0;
}

static int cli_dump(int argc, char *argv[])
{
	uint32_t address, length, temp;

	if (argc < 3) {
		printf("Err : Arg No\r\n");
		return -1;
	}

	if (strncmp(argv[1], "0x", 2) == 0) address = (uint32_t)strtol(&argv[1][2], NULL, 16);
	else address = (uint32_t)strtol(&argv[1][0], NULL, 16);

	length = (uint32_t)strtol(argv[2], NULL, 10);
	if (length > 10) length = 10;

	printf("address  %08lX, length = %ld\r\n", address, length);

	for (int i=0; i<length; i++) {
		printf("\r\n%08lX : ", (uint32_t)address);

		temp=address;
		for (int j=0; j<16; j++) {
			printf("%02X ", *(uint8_t *)temp);
			temp++;
		}

		temp=address;
		for (int j=0; j<16; j++) {
			char c = *(char *)temp;
			c = isalnum(c) ? c : (char)' ';
			printf("%c", c);
			temp++;
		}

		address = temp;
	}
	printf("\r\n");

	return 0;
}

static int cli_mode(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Err : Arg No\r\n");
		return -1;
	}

	long mode = strtol(argv[1], NULL, 10);
	//app_mode((int)mode);

	return 0;
}

static int cli_led(int argc, char *argv[])
{
	if (argc < 3) {
		printf("Err : Arg No\r\n");
		return -1;
	}

	long no = strtol(argv[1], NULL, 10);
	int onoff = strcmp(argv[2], "off");

	if (onoff != 0) onoff = 1;
	bool sts = onoff ? true : false;

	printf("led %ld, %d\r\n", no, onoff);
	 //led_onoff((uint8_t)no, sts);

	return 0;
}

static int cli_echo(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Err : Arg No\r\n");
		return -1;
	}
	printf("%s\r\n", argv[1]);

	return 0;
}

static int cli_help(int argc, char *argv[])
{
	for (int i=0; gCmdListObjs[i].cmd != NULL; i++) {
		printf("%s\r\n", gCmdListObjs[i].remark);
	}

	return 0;
}

// cli 명령어는 문자열이기 때문에 각 명령어를 파싱해서 저장해야 함. 그걸 하는 함수
static void cli_parser(BUF_T *arg)
{
	int argc = 0;
	char *argv[10];
	char *ptr;

	char *buf = (char *)arg->buf;

	//printf("rx:%s\r\n", (char *)arg);
	// token 분리
	ptr = strtok(buf, D_DELIMITER);
	if (ptr == NULL) return;

	while (ptr != NULL) {
		argv[argc] = ptr;
		argc++;
		ptr = strtok(NULL, D_DELIMITER);
	}

//	for (int i=0; i<argc; i++) {
//		printf("%d:%s\r\n", i, argv[i]);
//	}

	for (int i=0; gCmdListObjs[i].cmd != NULL; i++) {
		if (strcmp(gCmdListObjs[i].cmd, argv[0]) == 0) {
			gCmdListObjs[i].cbf(argc, argv);
			return;
		}
	}

	printf("Unsupported Command\r\n");
}
// ---------------------------------------------- cli 명령어 처리 부 종료  ---------------------------------------------- //




// cli 명령어에 대한 일을 하는 thread 함수
void cli_thread(void *arg)
{
	(void)arg;

	// osStatus 자료형(osOK, osError ... ??)을 저장할 변수
	// URL 참조
	// https://www.keil.com/pack/doc/CMSIS/RTOS2/html/group__CMSIS__RTOS__Definitions.html#ga6c0dbe6069e4e7f47bb4cd32ae2b813e
	osStatus sts;

	// Message 형태 구조체 변수 (수신,송신) 변수 생성
	MSG_T rxMsg, txMsg;

	// 현재 전송 메세지의 id에다가 0x1001을 넣음
	// osMessageQueueid -> 근데 id 정하는 기준이 뭐지? ㅎㅎ
	txMsg.id = E_MSG_CLI_INIT;

	// txMsg라는 주소에 있는 메시지를 cli_msg_id라는 id의 메시지 큐에 집어 넣음
	// 초기화 되어 있는 메시지가 queue에 들어가는데
	// 메시지의 우선순위는 0
	// 메시지 timeout은 osWaitForever 하염없이 기다림 -> 이 쓰레드는 OS한테 Wait 상태로 내려감
	// 큐 처음에 왜 이거 넣는 거지?? 쓰레드의 특성을 기록?
	osMessageQueuePut(cli_msg_id, &txMsg, 0, osWaitForever);

	// 무한루프를 돌며,
	while (1) {

		// osMessageQueueGet를 하고 osStatus = OS의 상태를 저장
		sts = osMessageQueueGet(cli_msg_id, &rxMsg, NULL, osWaitForever);

		// Operation completed successfully. = osOK
		if (sts == osOK) {
			// MSG id를 받고
			switch (rxMsg.id) {

				// 그 id가 0x1001 이면 cli_msg_put 함수를 uart의 uart_cbf[1]에 저장 == uart3
				case E_MSG_CLI_INIT: {
					printf("CLI Thread Started...\r\n");
					printf("Sizeof(MSG_T)=%d\r\n", sizeof(MSG_T));
					uart_regcbf(E_UART_1, cli_msg_put);
				} break;

				// 그 id가 0x1000 이면
				// 수신한 메시지를 cli_parser에 넣어서 파싱함
				case E_MSG_CLI: {
					cli_parser((BUF_T *)rxMsg.body.vPtr);
				} break;
			}
		}
	}
}

// ISR
// 여기서 E_MSG_CLI enum 값을 가진 rxMsg.id 바뀜
// uart에 콜백 함수로 등록되어 있는데
// 이 함수는 cli를 받다가 \r이나 \n을 받으면 실행됨

static void cli_msg_put(void *arg)
{
	// cli_msg_put에는 (void *)&gBufObjs[E_UART_1]라는 매개변수가 들어옴
	// 이 버퍼에는 BUF_T 형 구조체가 들어있고
	// BUF_T 구조체는 uart3으로 들어온 cli 문자열이 들어있다.
	BUF_T *pBuf = (BUF_T *)arg;

	// &gBufObj[0]에 pBuf가 가지고 있는 주소 넘겨주고, BUF_T의 사이즈 넘겨서 얼만큼 메모리를 복사할 지 알려줌
	memcpy(&gBufObj[0], pBuf, sizeof(BUF_T));

	// MSG_T 구조체 형 txMsg 변수 생성 txMsg는 이름일 뿐
	MSG_T txMsg;

	// 이 메시지의 id를 0x1000으로 바꾸고
	txMsg.id = E_MSG_CLI;
	// 메시지의 내용에 아까 memcpy한 &gBufObj[0]를 넣어 줌.
	txMsg.body.vPtr = (void *)&gBufObj[0];

	//cli_msg_id 0x1000, txMsg의 주소, 큐의 0번 우선순위, time_out 0: 즉시 처리
	osMessageQueuePut(cli_msg_id, &txMsg, 0, 0);
}


#define D_DELIMITER		" ,\r\n"



//static void cli_event_set(void *arg)
//{
//	BUF_T *pBuf = (BUF_T *)arg;
//
//	memcpy(&gBufObj[0], pBuf, sizeof(BUF_T));
//
//	osEventFlagsSet(cli_evt_id, 0x0001);
//}




