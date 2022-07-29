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

	LockFreeQueue(unsigned int size = 10000, bool placement_new = false);
	~LockFreeQueue();

	bool Enqueue(T data);
	bool Dequeue(T* data);
	long long GetSize();


private:
	// 읽는 순간
	// pool -> Enqueue 들어오자마자 Dequeue 성공 시 각각 1회
	// tail -> Dequeue 루프마다, enqueue 중 tail* 밀렸는지 확인 
	// head -> Dequeue 루프마다
	// size -> 인큐, 디큐마다. 1~2회 씩
	alignas(64) Node* _tail;
	alignas(64) Node* _head;
	alignas(64) LONG64 _size;
	// 고정값, 위의 멤버 변경 시 무효화 방지
	alignas(64) LockFreePool<Node>* _pool;
	bool _placement_new;
};

template<class T>
inline LockFreeQueue<T>::LockFreeQueue(unsigned int size, bool placement_new)
{
	_size = 0;
	_placement_new = placement_new;
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

	new(&node->data) T;

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
		else // 아직 tail을 밀어줘야 할 스레드가 안했으면 지금 한다.
		{
			new_tail = (Node*)((unsigned long long)next | (next_cnt << dfADDRESS_BIT));
			InterlockedCompareExchangePointer((PVOID*)&_tail, new_tail, (PVOID)old_tail);

		}
		
	}
	InterlockedIncrement64(&_size);

	return true;
}

template<class T>
inline bool LockFreeQueue<T>::Dequeue(T* data)
{
	InterlockedDecrement64(&_size);
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
			// 정말 없었으면 안들어왔어야 함.
			// 그럼에도 원인이 될 수 있는 상황
			// 1. 지금 보고 있는 head가 다른 스레드에서 dequeue되었고 다시 enque과정에서 next가 null로 쓰여졌다.
			// 2. 한 스레드가 enque에서 노드 하나를 tail로 저장하고 잠시 안돌았음
			//    -> 해당 노드가 queue에서 빠져나감 -> 다시 enqueue에서 next = null 됨
			//    -> 멈춰있던 스레드가 동작하면서 next에 이어버림 -> 사이즈 증가
			//    -> 해당 노드를 연결해야하는 스레드가 정지중.
			//    -> 이 때 디큐 시도하는 스레드는 기다리느라 못하는 상태가 됨.
			//    이 문제는 enqueue 중인 스레드가 일을 마치면 해결이 된다.
			//    하지만 다른 스레드 때문에 해야할 일을 못하는 것은 락프리의 목적에 위배된다.
			//    next가 null로 읽히면 1, 2의 상황 구분하지 않고 그냥 없는 것으로 본다..
			InterlockedIncrement64(&_size);
			return false;
		}
		else
		{
			unsigned long long old_tail = (unsigned long long)_tail;
			if (old_head == old_tail) // cnt까지 일치하면 같은 것
			{
				Node* tail = (Node*)(old_tail & dfADDRESS_MASK);
				unsigned long long tail_cnt = (old_tail >> dfADDRESS_BIT) + 1;
				Node* new_tail = (Node*)((unsigned long long)next | (tail_cnt << dfADDRESS_BIT));
				InterlockedCompareExchangePointer((PVOID*)&_tail, new_tail, (PVOID)old_tail);
			}

			Node* new_head = (Node*)((unsigned long long)next | (next_cnt << dfADDRESS_BIT));

			*data = next->data; // data가 객체인 경우.. 느려질 것 사용자의 문제. template type이 복사 비용이 적은 포인터나 일반 타입이었어야 한다.
			
			if (InterlockedCompareExchangePointer((PVOID*)&_head, new_head, (PVOID)old_head) == (PVOID)old_head)
			{
				head->data.~T();
				_pool->Free(head);
				break;
			}
		}
	}

	return true;
}

template<class T>
inline long long LockFreeQueue<T>::GetSize()
{
	return _size;
}
