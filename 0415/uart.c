/*
 * uart.c
 *
 *  Created on: Apr 11, 2024
 *      Author: iot00
 */
#include <stdbool.h>
#include <stdio.h>
#include "uart.h"

// uart3을 다루는 핸들러 자료형 -> uart3을 다루기 위한 정보가 담겨있다.
extern UART_HandleTypeDef huart3;


// RX로 받을 버퍼 배열: uint8_t 이므로 1byte 저장함
static uint8_t rxdata[1];
//static bool rxd_flag = false;

// uart로 전송 받을 데이터 최대 3bytes
#define D_BUF_OBJ_MAX	3

// BUF_T형 구조체형 길이 3짜리 배열 생성
static BUF_T gBufObjs[D_BUF_OBJ_MAX];

// uart_cbf라는 이름의 함수의 주소를 담을 수 있는 함수 포인터 변수 생성
static void (*uart_cbf)(void *);

// uart 초기화
void uart_init(void)
{
	// uart로 받은 데이터 저장할 버퍼 출력할때
	// 가리키는 idx, 버퍼가 끝났는지 알려주는 flag 초기화
	for (int i=0; i<D_BUF_OBJ_MAX; i++) {
		gBufObjs[i].idx = 0;
		gBufObjs[i].flag = false;
	}

	// 인터럽트 방식 수신 시작 : 1바이트
	// 1바이트씩 수신해서 rxdata 버퍼에 저장하는 함수인데
	// 여기서는 초기화
	HAL_UART_Receive_IT(&huart3, (uint8_t *)&rxdata[0], 1);
}

// 콜백 함수를 등록하는 함수
void uart_regcbf(void (*cbf)(void *))
{
	// 여기서는 cli_event_set이 들어가게 됨
	// cli.c의 170번 라인 참고
	uart_cbf = cbf;
}

//void uart_thread(void *arg)
//{
//	for (int i=0; i<D_BUF_OBJ_MAX; i++) {
//		if (gBufObjs[i].flag == true) {
//			if (uart_cbf != NULL) uart_cbf((void *)&gBufObjs[i]);
//			gBufObjs[i].idx = 0;
//			gBufObjs[i].flag = false;
//		}
//	}
//}

// 인터럽트 서비스 루틴 (ISR)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	volatile uint8_t rxd;

	if (huart == &huart3) {
		// 순서가 바뀌어야 하는 것 아닌가?
		// 아니면 초기화 할 때 데이터 들어온거 받는 건가?
		rxd = rxdata[0];

		// 1바이트 uart로 수신 할 때마다 콜백함수 호출 되고
		// 여기서 버퍼에 저장함
		HAL_UART_Receive_IT(huart, (uint8_t *)&rxdata[0], 1);

		//수신한 데이터를 저장할 구조체를 포인터로 불러옴
		BUF_T *p = (BUF_T *)&gBufObjs[0];

		// flag가 false라는 것은 아직 cli_event_set이 call되기 전이다.
		// 아직 충분히 데이터를 받지 못했다.
		if (p->flag == false) {

			// gBufObjs[0]의 버퍼[idx]에 rx로 수신한 데이터 저장
			p->buf[p->idx] = rxd;
			//p->idx++;
			//p->idx %= D_BUF_MAX;

			// gBufObjs[0].idx가 3보다 작으면 idx를 늘림
			if (p->idx < D_BUF_MAX) p->idx++;

			// 만약에 cli 명령의 끝을 만났다면
			if (rxd == '\r' || rxd == '\n') {

				// 널 문자를 맨 마지막에 넣고(이미 idx는 커진 상태)
				p->buf[p->idx] = 0; //'\0';

				// 나 RX로 받을 만큼 받았다. 라고 flag를 통해 표시해줌
				// 받을 만큼 받았으니까 gBufObjs[0]의 buf있는
				// 데이터 콜백함수로 처리해줘!
				p->flag = true;

				// 콜백함수에 gBufObjs[0] 실어보내기
				if (uart_cbf != NULL) uart_cbf((void *)&gBufObjs[0]);

				// 다시 gBufObjs[0] 초기화
				p->idx = 0;
				p->flag = false;
			}
		}
	}
}
