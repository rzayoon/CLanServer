#pragma once
#include <Windows.h>
#include "LockFreePool.h"



template<class T>
class LockFreeQueue
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

	LockFreeQueue(unsigned int size = 2000);
	~LockFreeQueue();

	bool Enqueue(T data);
	bool Dequeue(T* data);
	int GetSize();

	void Clear();

private:

	alignas(64) Node* _head;
	alignas(64) Node* _tail;

	alignas(64) ULONG64 _size;

	LockFreePool<Node>* _pool;

};

template<class T>
inline LockFreeQueue<T>::LockFreeQueue(unsigned int size)
{
	_size = 0;
	_pool = new LockFreePool<Node>(size + 1);
	_head = _pool->Alloc();

	_tail = _head;
}

template<class T>
inline LockFreeQueue<T>::~LockFreeQueue()
{
	delete _pool;
}

template<class T>
inline bool LockFreeQueue<T>::Enqueue(T data)
{
	Node* node = _pool->Alloc();
	unsigned long long old_tail;
	Node* tail;
	Node* new_tail;
	Node* next = nullptr;
	unsigned long long next_cnt;

	if (node == nullptr)
		return false;
	
	node->data = data;
	node->next = nullptr;

	while (true)
	{
		old_tail = (unsigned long long)_tail;
		tail = (Node*)(old_tail & dfADDRESS_MASK);
		next_cnt = (old_tail >> dfADDRESS_BIT) + 1;


		next = tail->next;

		if (next == nullptr)
		{
			if (InterlockedCompareExchangePointer((PVOID*)&tail->next, node, next) == next)
			{
				if (_tail == (PVOID)old_tail)
				{
					new_tail = (Node*)((unsigned long long)node | (next_cnt << dfADDRESS_BIT));
					InterlockedCompareExchangePointer((PVOID*)&_tail, new_tail, (PVOID)old_tail);
				}

				break;
			}
		}
		else
		{
			new_tail = (Node*)((unsigned long long)next | (next_cnt << dfADDRESS_BIT));
			
			InterlockedCompareExchangePointer((PVOID*)&_tail, new_tail, (PVOID)old_tail);
		}
		
	}
	InterlockedIncrement(&_size);

	return true;
}

template<class T>
inline bool LockFreeQueue<T>::Dequeue(T* data)
{
	ULONG64 size = InterlockedDecrement(&_size);
	if (size < 0) {
		InterlockedIncrement(&_size);
		*data = nullptr;
		return false;
	}
	unsigned long long old_head;
	Node* head;
	Node* next;
	unsigned long long next_cnt;

	while (true)
	{
		old_head = (unsigned long long)_head;
		head = (Node*)(old_head & dfADDRESS_MASK);
		next_cnt = (old_head >> dfADDRESS_BIT) + 1;

		next = head->next;

		if (next == nullptr)
		{
			InterlockedIncrement(&_size);
			data = nullptr;
			return false;
		}
		else
		{
			Node* new_head = (Node*)((unsigned long long)next | (next_cnt << dfADDRESS_BIT));
			*data = next->data; // data가 객체인 경우.. 느려질 것 사용자의 문제. template type이 복사 비용이 적은 포인터나 일반 타입이었어야 한다.
		
			if (InterlockedCompareExchangePointer((PVOID*)&_head, new_head, (PVOID)old_head) == (PVOID)old_head)
			{
				 // 더미 , 새 더미(새 head)

				_pool->Free(head);
				 // 반환
				break;
			}
		}
	}

	return true;
}

template<class T>
inline int LockFreeQueue<T>::GetSize()
{
	return _size;
}

template<class T>
inline void LockFreeQueue<T>::Clear()
{
	// 미구현
}
