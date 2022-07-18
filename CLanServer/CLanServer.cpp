
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <stdio.h>

#include "CLanServer.h"
#include "Protocol.h"

Monitor monitor;
Tracer tracer;

bool CLanServer::Start(char* ip, unsigned short port, int num_create_worker, int num_run_worker, bool nagle, int max_client)
{
	if (isRunning)
	{
		OnError(90, L"Duplicate Start Request\n");
	}

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

	WSACleanup();

	return;
}

int CLanServer::GetSessionCount()
{
	return session_cnt;
}

unsigned long _stdcall CLanServer::AcceptThread(void* param)
{
	int retval;
	CLanServer* server = (CLanServer*)param;

	HANDLE hcp = server->hcp;

	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) {
		server->OnError(4, L"Listen socket()\n");
		return 0;
	}

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(server->port);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) return 1;

	int size = 0;
	setsockopt(listen_sock, SOL_SOCKET, SO_SNDBUF, (char*)&size, sizeof(size));


	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) return 1;

	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	
	while (!server->exit_flag)
	{
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			monitor.IncAcceptErr();
			continue;
		}

		int idx = 0;
		for (; idx < MAX_SESSION; idx++)
		{
			if (server->session_arr[idx].used == false)
			{
				server->session_arr[idx].used = true;
				break;
			}
		}
		Session* session = &server->session_arr[idx];

		session->sock = client_sock;
		inet_ntop(AF_INET, &clientaddr.sin_addr, session->ip, sizeof(session->ip));
		session->port = ntohs(clientaddr.sin_port);
		session->io_count = 0;
		session->session_id = server->session_id++;
		session->send_flag = false;
		session->send_packet_cnt = 0;
		session->send_q.ClearBuffer();
		session->recv_q.ClearBuffer();
		session->thread_run = 0;

		//debugging
		session->lastRecv = 0;
		session->lastSend = 0;
		session->lastSendError = 0;
		session->lastTransferredZero = 0;

		//접속
		monitor.IncAccept();

		InterlockedIncrement((LONG*)&server->session_cnt);

		CreateIoCompletionPort((HANDLE)client_sock, hcp, (ULONG_PTR)session, 0);

		// RecvPost()
		server->RecvPost(session);

	}

	return 0;

}

unsigned long _stdcall CLanServer::IoThread(void* param)
{
	int ret_gqcp, ret_deq;
	CLanServer* server = (CLanServer*)param;
	HANDLE hcp = server->hcp;
	DWORD thread_id = GetCurrentThreadId();
	CPacket packet;
	LARGE_INTEGER recv_start, recv_end;
	LARGE_INTEGER send_start, send_end;
	LARGE_INTEGER on_recv_st, on_recv_ed;
	DWORD error_code;

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
				tracer.trace(00, error_code);
		}

		//session->Lock();

		if (cbTransferred == 0) // Pending 후 I/O 처리 실패
		{
			tracer.trace(53, session->session_id, session->recv_q.GetFillSize(), (PVOID)session->io_count);
			SOCKET temp_sock = InterlockedExchange(&session->sock, INVALID_SOCKET);
			if (temp_sock != INVALID_SOCKET)
				closesocket(temp_sock);
		}
		else {
			if (&session->recv_overlapped == overlapped) // recv 결과 처리
			{

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
					server->OnRecv(session->session_id, packet);
					QueryPerformanceCounter(&on_recv_ed);
					monitor.AddOnRecvTime(&on_recv_st, &on_recv_ed);
				}

				tracer.trace(51, GetCurrentThreadId(), session->session_id);
				server->RecvPost(session);

				QueryPerformanceCounter(&recv_end);
				monitor.AddRecvCompTime(&recv_start, &recv_end);
				monitor.IncRecv();
			}
			else if (&session->send_overlapped == overlapped) // send 결과 처리
			{
				QueryPerformanceCounter(&send_start);
				monitor.UpdateSendPacket(cbTransferred);

				CPacket* send_packet; // 수정
				int packet_cnt = InterlockedExchange((LONG*)&session->send_packet_cnt, 0);
				while (packet_cnt--)
				{
					session->send_q.Dequeue((char*)&send_packet, 8);
					delete send_packet;
				}
				InterlockedExchange((LONG*)&session->send_flag, false);

				tracer.trace(52, GetCurrentThreadId(), session->session_id);
				if (session->send_q.GetFillSize() > 0)
					server->SendPost(session);

				QueryPerformanceCounter(&send_end);
				monitor.AddSendCompTime(&send_start, &send_end);
			}
		}
		server->UpdateIOCount(session);
		//session->Unlock();
	}

	return 0;

}

bool CLanServer::RecvPost(Session* session)
{
	return false;
}

bool CLanServer::SendPost(Session* session)
{
	return false;
}

int CLanServer::UpdateIOCount(Session* session)
{
	return 0;
}

void CLanServer::ReleaseSession(unsigned int session_id)
{
	Session* session;
	unsigned long long flag;

	for (int idx = 0; idx < MAX_SESSION; idx++)
	{
		if (session_arr[idx].session_id == session_id)
		{
			if (session_arr[idx].used == false)
			{
				tracer.trace(10, 0);
			}

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

				InterlockedDecrement((LONG*)&session_cnt);  // 없어도 동기화 문제 없음..??
			}
			break;
		}
	}
	return;
}

