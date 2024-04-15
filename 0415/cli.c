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
#include "uart.h"
#include "app.h"
//#include "tim.h"
//#include "led.h"
#include "cli.h"

typedef struct {
	char *cmd;					// 명령어
	uint8_t no;					// 인자 최소 갯수
	int (*cbf)(int, char **);  	// int argc, char *argv[]
	char *remark; 				// 설명
} CMD_LIST_T;

osThreadId_t cli_thread_hnd;		// 쓰레드 핸들
osEventFlagsId_t cli_evt_id;		// 이벤트 플래그 핸들


const osThreadAttr_t cli_thread_attr = {
  .stack_size = 128 * 8,
  .priority = (osPriority_t) osPriorityNormal,
};

static int cli_led(int argc, char **argv);
static int cli_echo(int argc, char *argv[]);
static int cli_help(int argc, char *argv[]);
static int cli_mode(int argc, char *argv[]);
static int cli_dump(int argc, char *argv[]);
static int cli_duty(int argc, char *argv[]);


// 각 명령어의 배열, 구조체로 만듦
// 	{명령어,		받을 byte수, 	각 명령의 함수포인터,		각 명령의 설명}
const CMD_LIST_T gCmdListObjs[] = {
	{ "duty",		2,		cli_duty,		"led 1 pwm duty\r\n duty [duty:0~999]"						},
	{ "dump",		3,		cli_dump,		"memory dump\r\n dump [address:hex] [length:max:10 lines]"	},
	{ "mode",		2,		cli_mode,		"change application mode\r\n mode [0/1]"					},
	{ "led",		3,		cli_led,		"led [1/2/3] [on/off]"										},
	{ "echo",		2,		cli_echo,		"echo [echo data]"											},
	{ "help", 		1, 		cli_help, 		"help" 														},
	{ NULL,			0,		NULL,			NULL														}
};

// duty의 콜백함수
static int cli_duty(int argc, char *argv[])
{

	uint16_t duty;

	// 추가로 받은 byte(문자) 수가 2보다 작으면 = 데이터를 충분히 받지 못했으면 return -1
	if (argc < 2) {
		printf("Err : Arg No\r\n");
		return -1;
	}

	// 문자 스트링을 long integer 값으로 변환하여 duty변수에 저장
	duty = (uint16_t)strtol(argv[1], NULL, 10);

	// 출력
	printf("tim_duty_set(%d)\r\n", duty);
	//tim_duty_set(duty);

	return 0;
}

// 메모리 덤프를 해주는 함수
// argv는 명령어, 주소, 알고 싶은 주소길이 -> 이 세가지를 가진 배열이 들어옴
static int cli_dump(int argc, char *argv[])
{

	// 주소와 길이, 임시저장소 사용할거임
	uint32_t address, length, temp;

	// 추가로 받은 byte 수가 2보다 작으면 = 데이터를 충분히 받지 못했으면 return -1
	if (argc < 3) {
		printf("Err : Arg No\r\n");
		return -1;
	}

	//	입력 받아보고 맨 앞이 0x로 시작하면
	// 	0x를 때고 문자 스트링을 long integer 값으로 변환해서 주소 변수에 저장
	if (strncmp(argv[1], "0x", 2) == 0) address = (uint32_t)strtol(&argv[1][2], NULL, 16);

	// 그렇지 않으면 그대로 문자 스트링을 long integer 값으로 변환해서 주소 변수에 저장
	else address = (uint32_t)strtol(&argv[1][0], NULL, 16);

	// 길이를 받아보고 10보다 크면 입력한 주소부터 10bytes 만 저장
	length = (uint32_t)strtol(argv[2], NULL, 10);
	if (length > 10) length = 10;

	// 저장 한 값 출력
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

static void cli_parser(BUF_T *arg);
static void cli_event_set(void *arg);

static BUF_T gBufObj[1];

void cli_thread(void *arg)
{

	// cli_thread가 시작 될 때 콜백함수 등록해주면
	// 인터럽트에 안전함
	// uart에서 RX로 데이터가 들어오고 나서
	// 그 데이터를 처리해줄 함수 등록
	uart_regcbf(cli_event_set);


	// 32비트의 flag를 담을 변수 만들어주고
	uint32_t flags;

	(void)arg;

	printf("CLI Thread Started...\r\n");

	// cli_thread는
	// osEventFlagsNew로 받은 cli_evt_id

	// osEventFlagsWait은 현재 RUNNING인 쓰레드를
	// flag 매개변수에서 특정된 Any 또는 ALL 이벤트 플래그가
	// 매개변수의 ef_id로 특정된 flag가 set될때 까지 Wait 상태로 만듦
	while (1) {
		flags = osEventFlagsWait(cli_evt_id, 0xffff, osFlagsWaitAny, osWaitForever);

		// 근데 만약 flag가 0x0001이 되었다면 cli_parser에 RX로 받은 데이터를 매개변수로 넣어 함수를 호출
		if (flags & 0x0001) cli_parser(&gBufObj[0]);
	}
}

void cli_init(void)
{
	// 여기에서 콜백함수를 등록해주면 커널이 시작하기 전에
	// 인터럽트가 걸릴 위험이 있음
	//uart_regcbf(cli_event_set);


	// cli 이벤트를 감지할 evt_id 생성
	cli_evt_id = osEventFlagsNew(NULL);

	// cli_evt_id 생성되었다면 -> 출력
	if (cli_evt_id != NULL) printf("CLI Event Flags Created...\r\n");
	else {
		printf("CLI Event Flags Create File...\r\n");
		while (1);
	}

	// 새로운 쓰레드 생성
	// 새로운 쓰레드에서 동작할 함수 : cli_thread
	// 새로운 쓰레드에서 동작할 cli_thread의 시작 argument로 전해질 포인터 <- 뭔지 모르겠음
	// thread attributes; 쓰레드의 초기 환경설정을 위한 값?
	cli_thread_hnd = osThreadNew(cli_thread, NULL, &cli_thread_attr);


	if (cli_thread_hnd != NULL) printf("CLI Thread Created...\r\n");
	else {
		printf("CLI Thread Create Fail...\r\n");
		while (1);
	}
}


// uart의 RX를 통해 충분히 데이터를 받았으면 이 함수가 호출됨


static void cli_event_set(void *arg)
{
	BUF_T *pBuf = (BUF_T *)arg;

	// pBuf라는 이름의 BUF_T 구조체형 변수에 RX로 받은 값을 복사
	memcpy(&gBufObj[0], pBuf, sizeof(BUF_T));

	// cli에 이벤트 들어왔다고 OS에 알림
	osEventFlagsSet(cli_evt_id, 0x0001);
}


#define D_DELIMITER		" ,\r\n"


// cli 명령어를 파싱하는 함수
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
