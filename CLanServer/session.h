#pragma once
#include <Windows.h>
#include "CPacket.h"
#include "RingBuffer.h"
#include "LockFreeQueue.h"

class alignas(64) Session
{
public:

	Session();
	~Session();

	void Lock();
	void Unlock();

	bool used;
	unsigned int session_id;

	// interlock
	alignas(64) SOCKET sock;
	alignas(64) int io_count; // 경계에만 세우고 뒤에 다른 변수 들어올 수 있음.
	int release_flag;
	alignas(64) int send_flag;
	alignas(64) int send_packet_cnt;  // Send에 넣은 Packet 객체 삭제에 필요
	alignas(64) int b;

	RingBuffer recv_q = RingBuffer(2000);
	LockFreeQueue<CPacket*> send_q;
	
	OVERLAPPED recv_overlapped;
	OVERLAPPED send_overlapped;
	wchar_t ip[16];
	unsigned short port;
	CRITICAL_SECTION session_cs;

	CPacket* temp_packet[200];

};
