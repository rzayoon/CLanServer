#pragma once
#include "CLanServer.h"

class EchoServer : public CLanServer
{
	bool OnConnectionRequest(wchar_t* ip, unsigned short port);
	void OnClientJoin(unsigned long long session_id/**/);
	void OnClientLeave(unsigned long long session_id);

#ifdef AUTO_PACKET
	void OnRecv(unsigned long long session_id, PacketPtr packet);
#else
	void OnRecv(unsigned long long session_id, CPacket* packet);
#endif

	void OnSend(unsigned long long session_id, int send_size);

	void OnWorkerThreadBegin();
	void OnWorkerThreadEnd();

	void OnError(int errorcode, const wchar_t* msg);


};