#pragma once
#include <Windows.h>
#include "RingBuffer.h"

class alignas(64) Session
{
public:

	Session();
	~Session();

	void Lock();
	void Unlock();
	unsigned int session_id;

	bool used;
	SOCKET sock;
	RingBuffer recv_q = RingBuffer(2000);
	RingBuffer send_q = RingBuffer(2000);
	OVERLAPPED recv_overlapped;
	OVERLAPPED send_overlapped;
	// interlock
	alignas(64) int io_count; // 경계에만 세우고 뒤에 다른 변수 들어올 수 있음.
	int release_flag;
	alignas(64) int send_flag;
	alignas(64) int send_packet_cnt;  // Send에 넣은 Packet 객체 삭제에 필요
	alignas(64) int b;
	wchar_t ip[16];
	unsigned short port;
	CRITICAL_SECTION session_cs;
};
