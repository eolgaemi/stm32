/*
 * uart.c
 *
 *  Created on: Apr 11, 2024
 *      Author: iot00
 */
#include <stdbool.h>
#include <stdio.h>
#include "uart.h"

// UART2, 3 를 다루는 핸들러 main.c 에서 가져오기
// 핸들러에는 다양한 정보가 들어있다.
//extern UART_HandleTypeDef huart2;		// idx = 0
extern UART_HandleTypeDef huart3;		// idx = 1

// CLI 입력을 통해 UART RX로 받을 데이터는 최대 3byte이기 때문에 3 설정
#define D_BUF_OBJ_MAX	3

// uint8_t 타입의 UART 인터럽트 버퍼 생성 (잠시 저장)
// rxdata[0]은 UART2 의 버퍼
// rxdata[1]은 UART3 의 버퍼
static uint8_t rxdata[D_BUF_OBJ_MAX];

// BUF_T 구조체 타입의 cli입력을 UART RX로 받을 저장소 생성 (오래 저장)
static BUF_T gBufObjs[D_BUF_OBJ_MAX];

//static void (*uart_cbf[D_BUF_OBJ_MAX])(void *);
//typedef void (*UART_CBF)(void *);

// UART_CBF라는 이름의 함수의 주소를 저장하는 배열 생성 = 함수 포인터의 배열
static UART_CBF uart_cbf[D_BUF_OBJ_MAX];

// gBufObjs = UART RX로 받은 데이터 저장소의 값들을 초기화
void uart_init(void)
{
	for (int i=0; i<D_BUF_OBJ_MAX; i++) {
		gBufObjs[i].idx = 0;
		gBufObjs[i].flag = false;
	}

	// 인터럽트 방식 수신 시작 : 1바이트
	//HAL_UART_Receive_IT(&huart2, (uint8_t *)&rxdata[E_UART_0], 1); // uart 2
	HAL_UART_Receive_IT(&huart3, (uint8_t *)&rxdata[E_UART_1], 1);	// uart 3
}

// uart_cbf 함수 포인터형 배열의 해당하는 idx에 콜백함수를 등록해주는 함수
bool uart_regcbf(uint8_t idx, UART_CBF cbf)
{
	if (idx > D_BUF_OBJ_MAX) return false;
	uart_cbf[idx] = cbf;
	return true;
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
// RX가 100바이트가 넘어오면 100바이트 다 받고 아래의 콜백함수고 호출됨 Rx Complete 함수
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	// Uart RX로 받은 데이터를 저장할 변수
	volatile uint8_t rxd;

//	if (huart == &huart2) {	// idx = 0
//		rxd = rxdata[E_UART_0];
//		HAL_UART_Receive_IT(huart, (uint8_t *)&rxdata[E_UART_0], 1);
//
//		BUF_T *p = (BUF_T *)&gBufObjs[E_UART_0];
//		p->buf[0] = rxd;
//		if (uart_cbf[0] != NULL) uart_cbf[E_UART_0]((void *)&gBufObjs[E_UART_0]);
//	}


	// E_UART_1 = 1 = uart3 왜냐면 rxdata[1]은 UART3번의 버퍼이니까!
	if (huart == &huart3) { // idx = 1
		rxd = rxdata[E_UART_1];

		// 1바이트 받고 다시 1바이트 받기 시작!
		HAL_UART_Receive_IT(huart, (uint8_t *)&rxdata[E_UART_1], 1);

		// uart3의 정보를 저장할 저장소의 주소를 가져옴
		BUF_T *p = (BUF_T *)&gBufObjs[E_UART_1];


		// flag가 false라면 = 아직 데이터를 받은 적이 없다면

		if (p->flag == false) {

			// BUF_T *p에 idx번째 버퍼에 이번에 받은 정보를 저장하고
			p->buf[p->idx] = rxd;
			//p->idx++;
			//p->idx %= D_BUF_MAX;

			// idx를 1 늘림
			if (p->idx < D_BUF_MAX) p->idx++;

			// 만약에 데이터를 받았는데 문자열의 끝을 가리킨다면
			if (rxd == '\r' || rxd == '\n') {

				// 버퍼의 마지막에 널문자를 넣고
				p->buf[p->idx] = 0; //'\0';

				// flag를 true로 변경 --> 나 이제 데이터 다 받았고 콜백함수 처리할 준비 되었어!
				p->flag = true;

				// 1번째 (uart3) 콜백함수에다가 uart3의 데이터의 주소를 넘겨줌
				if (uart_cbf[1] != NULL) uart_cbf[1]((void *)&gBufObjs[E_UART_1]);

				// 다 처리 했으면 다시 idx와 flag를 초기화
				p->idx = 0;
				p->flag = false;
			}
		}
	}
}
