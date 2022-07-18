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
	alignas(64) int io_count; // ��迡�� ����� �ڿ� �ٸ� ���� ���� �� ����.
	int release_flag;
	alignas(64) int send_flag;
	alignas(64) int send_packet_cnt;  // Send�� ���� Packet ��ü ������ �ʿ�
	alignas(64) int b;
	wchar_t ip[16];
	unsigned short port;
	CRITICAL_SECTION session_cs;
};
