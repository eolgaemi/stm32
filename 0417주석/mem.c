/*
 * mem.c
 *
 *  Created on: Apr 16, 2024
 *      Author: iot00
 */

#include <stdbool.h>
#include <stdio.h>
#include "cmsis_os.h"
#include "mem.h"

#define D_MEM_COUNT_MAX		10

static osMemoryPoolId_t mem_id = NULL;

// 메모리 Pool 초기화
void mem_init(void)
{

	// osMemoryPoolNew는
	// 메모리Pool의 최대 메모리 블록 수 = D_MEM_COUNT_MAX
	// 한 메모리 블록의 크기 byte로 = sizeof(MEM_T)
	// const osMemoryPoolAttr = NULL로 초기화

	mem_id = osMemoryPoolNew(D_MEM_COUNT_MAX, sizeof(MEM_T), NULL);

	// memory pool ID를 return
	// oosMemoryPoolNew 함수는 메모리 풀 오브젝트를 생성하고 초기화 합니다.
	// 그리고 메모리 풀 오브젝트 식별자 또는 에러가 발생한 경우 NULL을 리턴합니다.

	// mem_id가 NULL 이면 ERROR가 발생한 것!
	if (mem_id != NULL) printf("Memory Pool Created...\\n");
	else {
		printf("Memory Pool Create Fail...\r\n");
		while (1);
	}
}

// Timeout is always 0, when mem_alloc is called in ISR.
MEM_T *mem_alloc(uint16_t size, uint32_t timeout)
{
	// 101바이트 짜리 메모리 버퍼
	MEM_T *pMem = NULL;

	// 블로킹 함수인 osMemoryPoolAlloc은 매개변수인 mp_id에게 메모리를 할당 해줍니다.
	// 그리고 할당 된 메모리의 주소의 포인터 또는 에러가 났을때 NULL을 리턴합니다.
	// osMemoryPoolNew를 통해 얻은 메모리 풀 ID = mem_id
	// 타임아웃 값 = timeout
	pMem = osMemoryPoolAlloc(mem_id, timeout);

	// NULL이 들어갔으면 메모리를 더 이상 할 수 없다는 소리!
	if (pMem == NULL) return NULL;

	// 메모리 구조체에 id에 mem_id를 넣어주면 구조체 완성!
	pMem->id = mem_id;
	return pMem;
}

bool mem_free(MEM_T *pMem)
{
	// osMemoryPoolId_t 형태의 변수 id룰 생성
	 osMemoryPoolId_t id;

	 // 전달 받은 메모리 구조체 포인터가 NULL 이거나 id가 없으면 false를 리턴
	 if (pMem == NULL) return false;
	 if (pMem->id == NULL) return false;

	 // 그렇지 않으면 생성한 id에 구조체의 id를 넣고
	 id = pMem->id;

	 // 구조체 속의 osMemoryPoolId 자체를 NULL로 초기화 해주고
	 pMem->id = NULL;

	 // id와 메모리 구조체의 주소로 osMemory 해제
	 osMemoryPoolFree(id, pMem);
	 return true;
 }
