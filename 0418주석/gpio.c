/*
 * gpio.c
 *
 *  Created on: Apr 15, 2024
 *      Author: iot00
 */

#include "gpio.h"

// 바보 외부 입출력 인터럽트
static void io_exti_dummy(uint8_t rf, void *arg);

// GPIO 입출력의 정보를 담고 있는 IO_EXTI_T 구조체 형 배열 16개 선언
// 왜 16개 일까? 그 문제는 StartUp 코드에서 확인 할 수 있다.
// EXTI0_IRQHandler0 ~ 4 5개가 있고,
// EXTI9_5_IRQHandler 가 배열 형태로 5번부터 9번을 차지
// EXTI15_10_IRQHandler 가 배열 형태로 10번부터 15번을 차지
#define D_IO_EXTI_MAX		16



// EXTI0은 PA0~PH0번의 인터럽트를 받고
// EXTI1는 PA1~PH1번의 인터럽트를 받는 식이니까
// GPIO PC13의 인터럽트는 EXTI13번이 받겠다!
// 그래서 13번에 정보를 저장하는 것!
static IO_EXTI_T gIOExtiObjs[D_IO_EXTI_MAX];


// gIOExtiObjs 배열에 초기 정보들을 넣기
void io_exti_init(void)
{
	// 순회하며 gIOExtiObjs 배열에 초기 정보들을 넣기
	for (int i=0; i<D_IO_EXTI_MAX; i++) {
		gIOExtiObjs[i].port = NULL;
		gIOExtiObjs[i].pin = 0;
		gIOExtiObjs[i].cbf = io_exti_dummy;
	}
	// gIOExtiObjs[13]에만
	// port = 파란색 버튼의 GPIO 포트 = PC13번 포트
	// pin = 파란색 버튼의 상태를 바꾸는 Pin 저장
	gIOExtiObjs[13].port = USER_Btn_GPIO_Port;
	gIOExtiObjs[13].pin = USER_Btn_Pin;
}

// gIOExtiObjs 배열에 IO EXTI (입출력 외부인터럽트)가 들어왔을 때 수행할 동작 넣어주기 -> 콜백함수의 주소를 배열에 넣는것!
bool io_exti_regcbf(uint8_t idx, IO_CBF_T cbf)
{
	if (idx > D_IO_EXTI_MAX) return false;
	gIOExtiObjs[idx].cbf = cbf;
	return true;
}

// 원하는 GPIO핀 (port와 pin으로 특정)의 상태를 읽어서 GPIO pin의 상태를 GPIO_PinState라는 구조체에 저장하고
// 해당 GPIO_PIN이 1이면, 파란색 버튼은 0x0001 0000 0000 0000이면,파란색 핀의 콜백함수에 상태와, 핀을 저장한 변수의 주소값을 보냄
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	for (volatile uint16_t i=0; i<D_IO_EXTI_MAX; i++) {
		volatile GPIO_PinState sts = HAL_GPIO_ReadPin(gIOExtiObjs[i].port, gIOExtiObjs[i].pin);
		if (GPIO_Pin & (0x01 << i)) 	gIOExtiObjs[i].cbf((uint8_t)sts, (void *)&i);
	}
}

// 바보 외부 입출력 인터럽트
static void io_exti_dummy(uint8_t rf, void *arg)
{
	(void)rf;
	(void)arg;
}

