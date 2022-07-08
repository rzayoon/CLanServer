#pragma once
#include <Windows.h>

#define MEGA_ARG 1000000.0

class alignas(64) Monitor
{
public:
	Monitor()
	{
		total_accept = 0;
		recv_per_sec = 0;
		send_per_sec = 0;
		accept_err = 0;
		accept = 0;
		sendpost_time = 0;
		sendpost_in_recv_cnt = 0;
		sendpacket_cnt = 0;
		MaxThreadOneSession = 0;
		MaxIOCount = 0;
		recv_completion_time = 0;

		max_send_packet = 0;
		min_send_packet = 9999999999;
		min_cnt = 0;
		total_send_packet = 0;
		___cnt = 0;

		QueryPerformanceFrequency(&frq);
	}

	~Monitor()
	{

	}

	void IncAccept();
	void IncSend();
	void IncRecv();
	void IncAcceptErr();
	void IncSendPostInRecv();
	void IncSendPacket();
	void AddSendTime(LARGE_INTEGER* start, LARGE_INTEGER* end);
	void AddRecvCompTime(LARGE_INTEGER* st, LARGE_INTEGER* end);

	void UpdateMaxThread(int max);
	void UpdateMaxIOCount(int temp);
	void UpdateSendPacket(LONG size);

	void Show();

private:

	alignas(64) LONG total_accept;
	alignas(64) LONG send_per_sec;
	alignas(64) LONG recv_per_sec;
	alignas(64) LONG accept_err;
	alignas(64) LONG accept;
	alignas(64) double sendpost_time;
	alignas(64) LONG sendpost_in_recv_cnt;
	alignas(64) LONG sendpacket_cnt;
	alignas(64) LONG MaxThreadOneSession;
	alignas(64) LONG MaxIOCount;
	alignas(64) double recv_completion_time;

	alignas(64) LONG max_send_packet;
	alignas(64) LONG min_send_packet;
	alignas(64) LONG min_cnt;
	alignas(64) LONG total_send_packet;
	alignas(64) LONG ___cnt;

	LARGE_INTEGER frq;
};