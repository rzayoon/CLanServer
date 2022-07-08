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

	bool used;
	SOCKET sock;
	char ip[16];
	unsigned short port;
	unsigned int session_id;
	RingBuffer recv_q = RingBuffer(2000);
	RingBuffer send_q = RingBuffer(2000);
	OVERLAPPED recv_overlapped;
	OVERLAPPED send_overlapped;
	// interlock
	alignas(64) int io_count;
	alignas(64) int send_flag;
	alignas(64) int send_packet_cnt;  // Send에 넣은 Packet 객체 삭제에 필요
	alignas(64) int thread_run;
	CRITICAL_SECTION session_cs;
	DWORD lastTransferredZero;
	DWORD lastSend;
	DWORD lastRecv;
	DWORD lastSendError;
};
