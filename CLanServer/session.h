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
	
	OVERLAPPED recv_overlapped;
	OVERLAPPED send_overlapped;
	RingBuffer recv_q = RingBuffer(2000);
	LockFreeQueue<CPacket*> send_q = LockFreeQueue<CPacket*>(0);

	// interlock
	alignas(64) SOCKET sock;
	alignas(64) int io_count; // ��迡�� ����� �ڿ� �ٸ� ���� ���� �� ����.
	int release_flag;
	alignas(64) int send_flag;
	alignas(64) int send_packet_cnt;  // Send�� ���� Packet ��ü ������ �ʿ�
	alignas(64) int b;

	wchar_t ip[16];
	unsigned short port;
	CRITICAL_SECTION session_cs;

	CPacket* temp_packet[200];

};
