#include <Windows.h>
#include <stdio.h>

#include "CLanServer.h"
#include "monitor.h"
#include "session.h"

void Monitor::IncAccept()
{
	InterlockedIncrement(&accept);
	return;
}

void Monitor::IncSend()
{
	InterlockedIncrement(&send_per_sec);
	return;
}

void Monitor::IncSendPostInRecv()
{
	InterlockedIncrement(&sendpost_in_recv_cnt);
	return;
}

void Monitor::IncSendPacket()
{
	InterlockedIncrement(&sendpacket_cnt);
	return;
}

void Monitor::IncFaultSession()
{
	InterlockedIncrement(&fault_session);
	return;
}

void Monitor::IncNoSession()
{
	InterlockedIncrement(&no_session);
	return;
}

void Monitor::IncRecv()
{
	InterlockedIncrement(&recv_per_sec);
	return;
}

void Monitor::IncAcceptErr()
{
	accept_err++;

	return;
}

void Monitor::UpdateMaxThread(int max)
{
	int temp = max;
	if (temp > MaxThreadOneSession)
	{
		InterlockedExchange(&MaxThreadOneSession, temp);
	}
	return;
}

void Monitor::UpdateMaxIOCount(int temp)
{
	if (temp > MaxIOCount)
	{
		InterlockedExchange(&MaxIOCount, temp);
	}
	return;

}

void Monitor::UpdateSendPacket(LONG size)
{
	if (max_send_packet < size)
		InterlockedExchange(&max_send_packet, size);
	if (min_send_packet > size) {
		InterlockedExchange(&min_send_packet, size);
		InterlockedExchange(&min_cnt, 1);
	}
	else
		InterlockedIncrement(&min_cnt);

	InterlockedAdd(&total_send_packet, size);
	InterlockedIncrement(&___cnt);
}

void Monitor::AddSendTime(LARGE_INTEGER* start, LARGE_INTEGER* end)
{
	unsigned long long deltaTime = end->QuadPart - start->QuadPart;


	InterlockedAdd64((LONG64*)&sendpost_time, deltaTime);

}

void Monitor::AddRecvCompTime(LARGE_INTEGER* start, LARGE_INTEGER* end)
{
	unsigned long long deltaTime = end->QuadPart - start->QuadPart;

	
	InterlockedAdd64((LONG64*)&recv_completion_time, deltaTime);
	InterlockedIncrement(&recv_comp_cnt);
}

void Monitor::AddSendCompTime(LARGE_INTEGER* start, LARGE_INTEGER* end)
{
	unsigned long long deltaTime = end->QuadPart - start->QuadPart;


	InterlockedAdd64((LONG64*)&send_completion_time, deltaTime);
	InterlockedIncrement(&send_comp_cnt);
}

void Monitor::AddOnRecvTime(LARGE_INTEGER* start, LARGE_INTEGER* end)
{
	unsigned long long deltaTime = end->QuadPart - start->QuadPart;


	InterlockedAdd64((LONG64*)&on_recv_time, deltaTime);
	InterlockedIncrement(&on_recv_cnt);
}


void Monitor::Show(int session_cnt)
{

	int now_accept = InterlockedExchange(&accept, 0);
	total_accept += now_accept;
	unsigned long long now_send_time = InterlockedExchange64((LONG64*)&sendpost_time, 0);
	int now_send = InterlockedExchange(&send_per_sec, 0);
	int now_recv = InterlockedExchange(&recv_per_sec, 0);
	int now_send_packet = InterlockedExchange(&sendpacket_cnt, 0);
	unsigned long long now_recv_comp_time = InterlockedExchange64((LONG64*)&recv_completion_time, 0);
	int now_send_post_in_recv = InterlockedExchange(&sendpost_in_recv_cnt, 0);
	unsigned long long now_send_comp_time = InterlockedExchange64((LONG64*)&send_completion_time, 0);
	int rcv_cnt = InterlockedExchange(&recv_comp_cnt, 0);
	int snd_cnt = InterlockedExchange(&send_comp_cnt, 0);
	unsigned long long now_on_recv_time = InterlockedExchange64((LONG64*)&on_recv_time, 0);
	int on_rcv_cnt = InterlockedExchange(&on_recv_cnt, 0);

	double total_wsa = (double)now_send_time / frq.QuadPart * MEGA_ARG;

	double send_time_avg = 0;
	if (now_send != 0) send_time_avg = (double)now_send_time / now_send / frq.QuadPart * MEGA_ARG;
	double recv_comp_time_avg = 0;
	if (rcv_cnt != 0) recv_comp_time_avg = (double)now_recv_comp_time / rcv_cnt / frq.QuadPart * MEGA_ARG;
	double send_comp_time_avg = 0;
	if (snd_cnt != 0) send_comp_time_avg = (double)now_send_comp_time / snd_cnt / frq.QuadPart * MEGA_ARG;
	double on_recv_time_avg = 0;
	if (on_rcv_cnt != 0) on_recv_time_avg = (double)now_on_recv_time / on_rcv_cnt / frq.QuadPart * MEGA_ARG;

	int max_thread_one_session = InterlockedExchange(&MaxThreadOneSession, 0);
	int max_io_cnt = InterlockedExchange(&MaxIOCount, 0);

	LONG max_packet = InterlockedExchange(&max_send_packet, 0);
	LONG min_packet = InterlockedExchange(&min_send_packet, 999999999);
	LONG total_packet = InterlockedExchange(&total_send_packet, 0);
	LONG cnt = InterlockedExchange(&___cnt, 0);
	double avg_packet = 0;
	if (cnt != 0)
		avg_packet = (double)total_packet / cnt;
	LONG _min_cnt = InterlockedExchange(&min_cnt, 0);


	printf("----------------------------------------------------\n"
		"total accpet : %d\n"
		"accept tps : %d\n"
		"accept error : %d\n"
		"Session Count : %d\n"
		"SEND/sec : %d\n"
		"SendPacket/sec : %d\n"
		"Send Completion : %.2lf us\n"
		" > Packet max : %d\n"
		" > Packet min : %d | count : %d\n"
		" > Packet avg : %.1lf\n"
		"RECV/sec : %d\n"
		"Recv Completion : %.2lf us\n"
		" > OnRecv : %.2lf us\n"
		"----------------------------------\n"
		"Total WSASend Time : %.2lf us\n"
		" > Avg : %.2lf us\n"
		"Max Running Thread of Session : %d\n"
		"Max IO Count : %d\n"
		"Fault Session : %d\n"
		"Not Found Session : %d\n"
		,
		total_accept, now_accept, accept_err, session_cnt, now_send, now_send_packet
		, send_comp_time_avg
		, max_packet, min_packet, _min_cnt, avg_packet
		, now_recv, recv_comp_time_avg, on_recv_time_avg
		, total_wsa, send_time_avg, max_thread_one_session, max_io_cnt, fault_session, no_session);


	return;
}