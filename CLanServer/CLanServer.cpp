#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <stdio.h>

#include "CLanServer.h"
#include "Protocol.h"



bool CLanServer::Start(const wchar_t* ip, unsigned short port, int num_create_worker, int num_run_worker, bool nagle, int max_client)
{
	if (isRunning)
	{
		OnError(90, L"Duplicate Start Request\n");
	}
	_max_client = max_client;
	wcscpy_s(_ip, ip);
	_port = port;

	session_arr = new Session[_max_client];
	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		OnError(1, L"WSAStartup()\n");
		return false;
	}

	hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, num_run_worker);
	if (hcp == NULL)
	{
		OnError(2, L"Create IOCP()\n");
		return false;
	}

	hAcceptThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AcceptThread, this, 0, NULL);
	if (hAcceptThread == NULL)
	{
		OnError(3, L"Create Thread Failed\n");
		return false;
	}
	num_of_worker = num_create_worker;
	hWorkerThread = new HANDLE[num_of_worker];
	for (int i = 0; i < num_of_worker; i++)
	{
		hWorkerThread[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)IoThread, this, 0, NULL);
		if (hWorkerThread[i] == NULL)
		{
			OnError(3, L"Create Thread Failed\n");
			return false;
		}
	}


	return false;
}

void CLanServer::Stop()
{
	if (!isRunning)
	{
		OnError(98, L"Not Running Yet\n");
		return;
	}

	// 종료 메시지 
	// PostQueuedCompletionStatus 0 0 0
	HANDLE* hExit = new HANDLE[num_of_worker + 1];

	for (int i = 0; i < num_of_worker; i++)
	{
		hExit[i] = hWorkerThread[i];
	}
	delete[] hWorkerThread;
	hExit[num_of_worker] = hAcceptThread;

	WaitForMultipleObjects(num_of_worker + 1, hExit, TRUE, INFINITE);
	// interval 마다 확인하기?

	delete[] hExit;
	delete[] session_arr;

	WSACleanup();

	return;
}

int CLanServer::GetSessionCount()
{
	return session_cnt;
}

unsigned long _stdcall CLanServer::AcceptThread(void* param)
{
	CLanServer* server = (CLanServer*)param;
	server->RunAcceptThread();
	// this call이나 server-> 콜이나 그게 그거지만 편의상.. 

	return 0;
}

unsigned long _stdcall CLanServer::IoThread(void* param)
{
	CLanServer* server = (CLanServer*)param;
	server->RunIoThread();

	return 0;
}

void CLanServer::RunAcceptThread()
{
	int retval;
	wprintf(L"%d Accept thread On...\n", GetCurrentThreadId());
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) {
		OnError(4, L"Listen socket()\n");
		return;
	}

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	InetPtonW(AF_INET, _ip, &serveraddr.sin_addr.s_addr);
	serveraddr.sin_port = htons(_port);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) return;

	int size = 0;
	setsockopt(listen_sock, SOL_SOCKET, SO_SNDBUF, (char*)&size, sizeof(size));


	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) return;

	wprintf_s(L"Listen Port: %d\n", _port);

	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	wchar_t temp_ip[16];
	unsigned short temp_port;

	while (!exit_flag)
	{
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			monitor.IncAcceptErr();
			continue;
		}
		InetNtopW(AF_INET, &clientaddr.sin_addr, temp_ip, _countof(temp_ip));
		temp_port = ntohs(clientaddr.sin_port);

		if (OnConnectionRequest(temp_ip, temp_port))
		{
			
			for (int idx = 0; idx < _max_client; idx++)
			{
				if (session_arr[idx].used == false)
				{
					session_arr[idx].used = true;
					Session* session = &session_arr[idx];

					session->sock = client_sock;
					wcscpy_s(session->ip, _countof(session->ip), temp_ip);
					session->port = ntohs(clientaddr.sin_port);
					session->io_count = 0;
					session->release_flag = 0;
					session->session_id = session_id++;
					session->send_flag = false;
					session->send_packet_cnt = 0;
					session->send_q.ClearBuffer();
					session->recv_q.ClearBuffer();

					tracer.trace(10, session, session->session_id); // accept

					//접속
					monitor.IncAccept();

					InterlockedIncrement((LONG*)&session_cnt);

					CreateIoCompletionPort((HANDLE)client_sock, hcp, (ULONG_PTR)session, 0);

					// RecvPost()
					if (RecvPost(session))
					{
						OnClientJoin(session->session_id);
					}
					break;
				}
			}
		}
	}
}

void CLanServer::RunIoThread()
{
	int ret_gqcp;
	DWORD thread_id = GetCurrentThreadId();
	CPacket packet;
	LARGE_INTEGER recv_start, recv_end;
	LARGE_INTEGER send_start, send_end;
	LARGE_INTEGER on_recv_st, on_recv_ed;
	DWORD error_code;

	wprintf(L"%d worker thread On...\n", thread_id);

	while (1)
	{
		DWORD cbTransferred;
		WSAOVERLAPPED* overlapped;
		Session* session;
		ret_gqcp = GetQueuedCompletionStatus(hcp, &cbTransferred, (PULONG_PTR)&session, (LPOVERLAPPED*)&overlapped, INFINITE); // overlapped가 null인지 확인 우선

		if (overlapped == NULL) // deque 실패 1. timeout 2. 잘못 호출(Invalid handle) 3. 임의로 queueing 한 것(PostQueue)
		{
			wprintf(L"%d exit worker thread [error : %d]\n", thread_id, WSAGetLastError());
			break;
		}

		if (ret_gqcp == 0)
		{
			//에러코드 로깅
			error_code = GetLastError();
			if (error_code != ERROR_NETNAME_DELETED)
				tracer.trace(00, NULL, error_code);
		}

		if (cbTransferred == 0) // Pending 후 I/O 처리 실패
		{
	
			SOCKET temp_sock = InterlockedExchange(&session->sock, INVALID_SOCKET);
			if (temp_sock != INVALID_SOCKET)
				closesocket(temp_sock);
		}
		else {
			OnWorkerThreadBegin();
			if (&session->recv_overlapped == overlapped) // recv 결과 처리
			{
				tracer.trace(21, session, session->session_id);
				QueryPerformanceCounter(&recv_start);


				session->recv_q.MoveRear(cbTransferred);
				// msg 확인

				while (true)
				{
					PacketHeader header;
					int q_size = session->recv_q.GetFillSize();

					session->recv_q.Peek((char*)&header, sizeof(header));

					if (header.len + sizeof(header) > q_size)
						break;

					session->recv_q.MoveFront(sizeof(header));

					packet.Clear();

					int ret_deq = session->recv_q.Dequeue(packet.GetBufferPtr(), header.len);

					packet.MoveWritePos(ret_deq);

					QueryPerformanceCounter(&on_recv_st);
					OnRecv(session->session_id, &packet);
					QueryPerformanceCounter(&on_recv_ed);
					monitor.AddOnRecvTime(&on_recv_st, &on_recv_ed);
				}
				QueryPerformanceCounter(&recv_end);
				monitor.AddRecvCompTime(&recv_start, &recv_end);
				monitor.IncRecv();

				tracer.trace(22, session, session->session_id);
				RecvPost(session);

			}
			else if (&session->send_overlapped == overlapped) // send 결과 처리
			{
				tracer.trace(31, session, session->session_id);
				QueryPerformanceCounter(&send_start);
				monitor.UpdateSendPacket(cbTransferred);

				CPacket* send_packet;
				int packet_cnt = session->send_packet_cnt;
				session->send_packet_cnt = 0;
				while (packet_cnt--)
				{
					session->send_q.Dequeue((char*)&send_packet, 8);
					delete send_packet;
				}
				session->send_flag = false;
				QueryPerformanceCounter(&send_end);
				monitor.AddSendCompTime(&send_start, &send_end);

				tracer.trace(32, session, session->session_id);
				if (session->send_q.GetFillSize() > 0)
					SendPost(session);

			}
		}
		UpdateIOCount(session);
		OnWorkerThreadEnd();
	}

	return;
}

bool CLanServer::SendPacket(unsigned int session_id, CPacket* packet)
{
	monitor.IncSendPacket();
	bool ret = false;
	Session* session = nullptr;
	bool check = false; // 누수 체크용

	for (int idx = 0; idx < _max_client; idx++)
	{
		if (session_arr[idx].used == false) continue;

		if (session_arr[idx].session_id == session_id)
		{
			session = &session_arr[idx];
			if (session->session_id != session_id)
			{
				tracer.trace(11, session, session_id);
				break;
			}
			InterlockedIncrement((LONG*)&session->io_count);
			if (session->release_flag == 0)
			{
				session->send_q.Enqueue((char*)&packet, 8);  // 64 bit 기준 8byte

				ret = SendPost(session);
				if (ret == false) // 실질적인 send post 호출 성공
					monitor.IncSendPostInRecv();

				check = true;
			}
			UpdateIOCount(session);
			break;
		}
	}


	if (!check)
	{
		monitor.IncNoSession();
	}



	return ret;
}

void CLanServer::DisconnectSession(unsigned int session_id)
{
	Session* session = nullptr;

	for (int idx = 0; idx < _max_client; idx++)
	{
		if (session_arr[idx].used == false) continue;

		if (session_arr[idx].session_id == session_id)
		{
			session = &session_arr[idx];
			InterlockedIncrement((LONG*)&session->io_count);
			if (session->release_flag == 0)
			{
				SOCKET temp_sock = InterlockedExchange(&session->sock, INVALID_SOCKET);
				if (temp_sock != INVALID_SOCKET)
					closesocket(temp_sock);

			}
			UpdateIOCount(session);
			break;
		}
	}

	return;
}

bool CLanServer::RecvPost(Session* session)
{
	DWORD recvbytes, flags = 0;

	ZeroMemory(&session->recv_overlapped, sizeof(session->recv_overlapped));

	int emptySize = session->recv_q.GetEmptySize();
	int size1 = session->recv_q.DirectEnqueSize();

	WSABUF wsabuf[2];
	int cnt = 1;
	wsabuf[0].buf = session->recv_q.GetRearPtr();
	wsabuf[0].len = size1;

	if (size1 < emptySize)
	{
		++cnt;
		wsabuf[1].buf = session->recv_q.GetBufPtr();
		wsabuf[1].len = emptySize - size1;
	}

	int temp = InterlockedIncrement((LONG*)&session->io_count);
	monitor.UpdateMaxIOCount(temp);


	DWORD error_code;
	int retval = WSARecv(session->sock, wsabuf, cnt, &recvbytes, &flags, &session->recv_overlapped, NULL);
	if (retval == SOCKET_ERROR)
	{
		if ((error_code = WSAGetLastError()) != ERROR_IO_PENDING)
		{ // 요청이 실패
			int io_temp = UpdateIOCount(session);
			tracer.trace(1, session, error_code);
		}
		else
		{
			// Pending
		}
	}
	else
	{
		//동기 recv
	}

	return true;
}

bool CLanServer::SendPost(Session* session)
{
	bool temp;
	LARGE_INTEGER start, end;

	if ((temp = InterlockedExchange((LONG*)&session->send_flag, true)) == false)
	{
		if (session->send_q.GetFillSize() == 0)
		{
			tracer.trace(20, 0);
			session->send_flag = false;
			return true;
		}

		int retval;

		ZeroMemory(&session->send_overlapped, sizeof(session->send_overlapped));

		// 개선 필요
		int currentSize = session->send_q.GetFillSize();

		char buf[1000];
		session->send_q.Peek(buf, currentSize);
		CPacket* packet;
		CPacket** ptr = (CPacket**)buf;
		WSABUF wsabuf[MAX_WSABUF];
		int buf_cnt = currentSize / sizeof(CPacket*);
		if (buf_cnt > MAX_WSABUF)
			buf_cnt = MAX_WSABUF;

		for (int cnt = 0; cnt < buf_cnt; cnt++)
		{
			packet = *ptr;
			wsabuf[cnt].buf = packet->GetBufferPtrWithHeader();
			wsabuf[cnt].len = packet->GetDataSizeWithHeader();
			++ptr;
		}
		session->send_packet_cnt = buf_cnt;
		DWORD sendbytes;
		int temp = InterlockedIncrement((LONG*)&session->io_count);
		monitor.UpdateMaxIOCount(temp);
		monitor.IncSend();

		tracer.trace(33, session, session->session_id);

		QueryPerformanceCounter(&start);
		retval = WSASend(session->sock, wsabuf, buf_cnt, &sendbytes, 0, &session->send_overlapped, NULL);
		QueryPerformanceCounter(&end);
		monitor.AddSendTime(&start, &end);

		DWORD error_code;
		if (retval == SOCKET_ERROR)
		{
			if ((error_code = WSAGetLastError()) != WSA_IO_PENDING) // 요청 자체가 실패
			{
				// 내가 release 시켜야하는 경우 Packet 해제 해줘야 함
				int io_temp = UpdateIOCount(session);
				tracer.trace(2, session, error_code);
			}
			else
			{
				// Pending
			}
		}
		else
		{
			//동기처리
		}

	}

	return temp;
}

int CLanServer::UpdateIOCount(Session* session)
{
	int temp;
	if ((temp = InterlockedDecrement((LONG*)&session->io_count)) == 0)
	{
		ReleaseSession(session->session_id);
	}

	return temp;
}

void CLanServer::ReleaseSession(unsigned int session_id)
{
	Session* session;
	unsigned long long flag;

	for (int idx = 0; idx < _max_client; idx++)
	{
		if (session_arr[idx].used == false) continue;

		if (session_arr[idx].session_id == session_id)
		{
			session = &session_arr[idx];

			flag = *((unsigned long long*)(&session->io_count));
			if (flag != 0) break;
			if (InterlockedCompareExchange64((LONG64*)&session->io_count, 0x100000000, flag) == flag) {
				session->session_id = -1;

				CPacket* packet;
				while (session->send_q.Dequeue((char*)&packet, 8))
				{
					delete packet;
				}
				if (session->sock != INVALID_SOCKET)
					closesocket(session->sock);  // 동시에 closesocket 진입 가능성 아직 없음. 생기면 수정 필요
				session->sock = INVALID_SOCKET;
				session->used = false;

				OnClientLeave();

				InterlockedDecrement((LONG*)&session_cnt);  // 없어도 동기화 문제 없음..??
			}
			break;
		}
	}

	

	return;
}

