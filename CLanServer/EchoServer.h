#pragma once
#include "CLanServer.h"

class EchoServer : public CLanServer
{
	bool OnConnectionRequest(wchar_t* ip, unsigned short port);
	void OnClientJoin(unsigned int session_id/**/);
	void OnClientLeave();

	void OnRecv(unsigned int session_id, CPacket* packet);
	void OnSend(unsigned int session_id, int send_size);

	void OnWorkerThreadBegin();
	void OnWorkerThreadEnd();

	void OnError(int errorcode, const wchar_t* msg);


};