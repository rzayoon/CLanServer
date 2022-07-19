#pragma once
#include <Windows.h>
#include <stdio.h>
#include "Tracer.h"

#define dfPAD -1
#define dfADDRESS_MASK 0x00007fffffffffff
#define dfADDRESS_BIT 47

template <class DATA>
class LockFreePool
{
	struct BLOCK_NODE
	{
		__int64 pad1;
		DATA data;
		__int64 pad2;
		alignas(64) LONG use;
		BLOCK_NODE* next;
	

		BLOCK_NODE()
		{
			pad1 = dfPAD;

			pad2 = dfPAD;
			next = nullptr;
			use = 0;
		}
	};

public:
	/// <summary>
	/// 생성자 
	/// </summary>
	/// <param name="block_num"> 초기 블럭 수 </param>
	/// <param name="placement_new"> Alloc 또는 Free 시 생성자 호출 여부(미구현) </param>
	LockFreePool(int block_num, bool placement_new = false);

	virtual ~LockFreePool();

	/// <summary>
	/// 블럭 하나 할당 
	/// 
	/// </summary>
	/// <param name=""></param>
	/// <returns>할당된 DATA 포인터</returns>
	DATA* Alloc(void);

	/// <summary>
	/// 사용중이던 블럭 해제
	/// </summary>
	/// <param name="data"> 반환할 블럭 포인터</param>
	/// <returns> 해당 Pool의 블럭이 아닐 시 false 그 외 true </returns>
	bool Free(DATA* data);

	/// <summary>
	/// 
	/// 
	/// </summary>
	/// <returns>오브젝트 풀에서 생성한 전체 블럭 수</returns>
	int GetCapacity()
	{
		return capacity;
	}
	/// <summary>
	/// 
	/// </summary>
	/// <returns>오브젝트 풀에서 제공한 블럭 수</returns>
	int GetUseCount()
	{
		return use_count;
	}

protected:
	BLOCK_NODE* top;

	BLOCK_NODE** pool;

	int capacity;
	alignas(64) unsigned int use_count;

};

template<class DATA>
LockFreePool<DATA>::LockFreePool(int _capacity, bool _placement_new)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	if ((_int64)si.lpMaximumApplicationAddress != 0x00007ffffffeffff)
	{
		printf("Maximum Application Address Fault\n");
		int* a = (int*)0;
		a = 0;
	}

	capacity = _capacity;
	use_count = 0;


	pool = new BLOCK_NODE * [capacity];

	BLOCK_NODE* next = nullptr;
	for (int i = 0; i < capacity; i++)
	{
		BLOCK_NODE* temp = new BLOCK_NODE;
		temp->next = next;
		next = temp;

		pool[i] = temp;

	}
	top = next;


}

template<class DATA>
LockFreePool<DATA>::~LockFreePool()
{
	for (int i = 0; i < capacity; i++)
	{
		delete pool[i];
	}
	delete[] pool;

}

template<class DATA>
DATA* LockFreePool<DATA>::Alloc()
{
	unsigned long long old_top;  // 비교용
	BLOCK_NODE* old_top_addr; // 실제 주소
	BLOCK_NODE* next; // 다음 top
	BLOCK_NODE* new_top;

	while (1)
	{
		old_top = (unsigned long long)top;
		old_top_addr = (BLOCK_NODE*)(old_top & dfADDRESS_MASK);
		
		
		if (old_top_addr == nullptr)
		{
			return nullptr;
		}


		unsigned long long next_cnt = (old_top >> dfADDRESS_BIT) + 1;
		next = old_top_addr->next;
		

		new_top = (BLOCK_NODE*)((unsigned long long)next | (next_cnt << dfADDRESS_BIT));


		if (old_top == (unsigned long long)InterlockedCompareExchangePointer((PVOID*)&top, (PVOID)new_top, (PVOID)old_top))
		{
			break;
		}
	}
	InterlockedIncrement((LONG*)&use_count);

	

	return (DATA*)&old_top_addr->data;
}

template<class DATA>
bool LockFreePool<DATA>::Free(DATA* data)
{
	unsigned long long old_top;
	BLOCK_NODE* node = (BLOCK_NODE*)((char*)data - sizeof(BLOCK_NODE::pad1));
	BLOCK_NODE* old_top_addr;
	PVOID new_top;

	if (node->pad1 != dfPAD || node->pad2 != dfPAD)
	{
		wprintf(L"LockFree Pool Node overrun\n");
		int* a = (int*)0;
		a = 0;
	}
	


	while (1)
	{
		old_top = (unsigned long long)top;
		old_top_addr = (BLOCK_NODE*)(old_top & dfADDRESS_MASK);

		unsigned long long next_cnt = (old_top >> dfADDRESS_BIT) + 1;

		node->next = old_top_addr;

		new_top = (BLOCK_NODE*)((unsigned long long)node | (next_cnt << dfADDRESS_BIT));

		if (old_top == (unsigned long long)InterlockedCompareExchangePointer((PVOID*)&top, (PVOID)new_top, (PVOID)old_top))
		{
			break;
		}

	}
	
	InterlockedDecrement((LONG*)&use_count);

	return true;
}