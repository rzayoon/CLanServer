#pragma once
#include <Windows.h>

#include "CPacket.h"
#include "session.h"
#include "Tracer.h"
#include "monitor.h"


class CLanServer
{
	enum {
		MAX_WSABUF = 100
	};

public:

	CLanServer()
	{
		ZeroMemory(_ip, sizeof(_ip));
	}

	~CLanServer()
	{
		if(isRunning)
			Stop();
	}

	bool Start(const wchar_t* ip, unsigned short port, int num_create_worker, int num_run_worker, bool nagle, int max_client);
	void Stop();

	bool SendPacket(unsigned int session_id, CPacket* packet);
	void DisconnectSession(unsigned int session_id);
	int GetSessionCount();

	virtual bool OnConnectionRequest(wchar_t* ip, unsigned short port) = 0;
	virtual void OnClientJoin(unsigned int session_id/**/) = 0;
	virtual void OnClientLeave() = 0;

	virtual void OnRecv(unsigned int session_id, CPacket* packet) = 0;
	virtual void OnSend(unsigned int session_id, int send_size) = 0;

	virtual void OnWorkerThreadBegin() = 0;
	virtual void OnWorkerThreadEnd() = 0;

	virtual void OnError(int errorcode, const wchar_t* msg) = 0;
	

private:


	HANDLE hcp;

	HANDLE hAcceptThread;
	HANDLE* hWorkerThread;
	int num_of_worker;
	int _max_client;
	// IP Port
	unsigned short _port;
	wchar_t _ip[16];

	static unsigned long _stdcall AcceptThread(void* param);
	static unsigned long _stdcall IoThread(void* param);

	void RunAcceptThread();
	void RunIoThread();


	__inline bool RecvPost(Session* session);
	__inline bool SendPost(Session* session);

	__inline int UpdateIOCount(Session* session);
	__inline void ReleaseSession(unsigned int session_id);

	bool exit_flag = false;
	bool isRunning = false;

	Session* session_arr;
	DWORD session_id = 1;
	
	Monitor monitor;
	Tracer tracer;

	alignas(64) int session_cnt = 0;
};

