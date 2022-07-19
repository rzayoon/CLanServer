#pragma once
#include <Windows.h>
#include "LockFreePool.h"



template<class T>
class LockFreeStack
{
	struct Node
	{
		T data;
		Node* next;
		int del_cnt;

		Node()
		{

			next = nullptr;
			del_cnt = 0;
		}

		~Node()
		{

		}
	};


public:

	LockFreeStack(unsigned int size = 10000);
	~LockFreeStack();

	bool Push(T data);
	bool Pop(T* data);
	int GetSize();


private:

	alignas(64) Node* _top;
	alignas(64) ULONG64 _size;

	LockFreePool<Node> *_pool;

};

template<class T>
inline LockFreeStack<T>::LockFreeStack(unsigned int size)
{
	_size = 0;
	_top = nullptr;
	_pool = new LockFreePool<Node>(size);
}

template<class T>
inline LockFreeStack<T>::~LockFreeStack()
{
	delete _pool;
}

template<class T>
inline bool LockFreeStack<T>::Push(T data)
{
	Node* temp = _pool->Alloc();

	if (temp == nullptr)
		return false;

	temp->del_cnt = 0;
	temp->data = data;

	long long old_top;
	Node* old_top_addr;
	Node* new_top;

	while (1)
	{
		old_top = (long long)_top;
		old_top_addr = (Node*)(old_top & dfADDRESS_MASK);
		long long next_cnt = (old_top >> dfADDRESS_BIT) + 1;
		temp->next = old_top_addr;

		new_top = (Node*)((long long)temp | (next_cnt << dfADDRESS_BIT));

		if (old_top == (long long)InterlockedCompareExchangePointer((PVOID*)&_top, new_top, (PVOID)old_top))
		{
			InterlockedIncrement(&_size);
			break;
		}

	}

	return true;

} 

template<class T>
inline bool LockFreeStack<T>::Pop(T* data)
{
	long long old_top;
	Node* old_top_addr;
	Node* next;
	Node* new_top;

	while (1)
	{
		old_top = (long long)_top;
		old_top_addr = (Node*)(old_top & dfADDRESS_MASK);
		if (old_top_addr == nullptr)
		{
			data = nullptr;
			return false;
		};

		long long next_cnt = (old_top >> dfADDRESS_BIT) + 1;
		next = old_top_addr->next;

		new_top = (Node*)((long long)next | (next_cnt << dfADDRESS_BIT));

		if (old_top == (long long)InterlockedCompareExchangePointer((PVOID*)&_top, (PVOID)new_top, (PVOID)old_top))
		{
			*data = old_top_addr->data;
			old_top_addr->del_cnt--;
			if (old_top_addr->del_cnt == -2)
				int a = 0;
			_pool->Free(old_top_addr);

			InterlockedDecrement(&_size);
			break;
		}
	}

	return true;
}

template<class T>
inline int LockFreeStack<T>::GetSize()
{
	return _size;
}
