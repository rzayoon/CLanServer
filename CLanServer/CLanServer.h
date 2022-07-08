#pragma once
#include <Windows.h>

#include "CPacket.h"
#include "session.h"
#include "monitor.h"

class CLanServer
{
public:

	CLanServer()
	{

	}

	~CLanServer()
	{
		if(isRunning)
			Stop();
	}

	bool Start(char* ip, unsigned short port, int num_create_worker, int num_run_worker, bool nagle, int max_client);
	void Stop();

	int GetSessionCount();

	virtual bool OnConnectionRequest(char* ip, unsigned short port) = 0;
	virtual void OnClientJoin(/**/) = 0;
	virtual void OnClientLeave() = 0;

	virtual void OnRecv(unsigned int session_id, CPacket* packet) = 0;
	virtual void OnSend(unsigned int session_id, int send_size) = 0;

	virtual void OnWorkerThreadBegin() = 0;
	virtual void OnWorkerThreadEnd() = 0;

	virtual void OnError(int errorcode, const wchar_t* msg) = 0;
	

private:

	HANDLE hcp;
	Monitor monitor;
	HANDLE hAcceptThread;
	HANDLE* hWorkerThread;
	int num_of_worker;

	// IP Port
	unsigned short port;
	char* ip;

	static unsigned long _stdcall AcceptThread(void* param);
	static unsigned long _stdcall IoThread(void* param);

	bool exit_flag = false;
	bool isRunning = false;
};

