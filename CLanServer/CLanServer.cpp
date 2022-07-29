#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <stdio.h>

#include "LockFreeQueue.h"
#include "CLanServer.h"
#include "Protocol.h"


long long packet_counter[101];
int log_arr[100];

bool CLanServer::Start(const wchar_t* ip, unsigned short port, int num_create_worker, int num_run_worker, bool nagle, int max_client)
{
	if (isRunning)
	{
		OnError(90, L"Duplicate Start Request\n");
	}
	_max_client = max_client;
	wcscpy_s(_ip, ip);
	_port = port;
	exit_flag = false;

	session_arr = new Session[_max_client];

#ifdef STACK_INDEX
	for (int i = 0; i < _max_client; i++)
		empty_session_stack.Push(i);
#endif
	
	
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


	isRunning = true;

	return true;
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
	
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET)
	{
		wprintf(L"exit socket fail\n");
	}

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	wchar_t ip[16] = L"127.0.0.1";  //loop back

	InetPtonW(AF_INET, ip, &serveraddr.sin_addr.s_addr);

	serveraddr.sin_port = htons(_port);

	exit_flag = true;
	int ret_con = connect(sock, (sockaddr*)&serveraddr, sizeof(serveraddr));
	if (ret_con == SOCKET_ERROR)
	{
		DWORD error_code = WSAGetLastError();
		wprintf(L"exit connect fail %d\n", error_code);
	}

	closesocket(sock);

	for (int i = 0; i < _max_client; i++)
	{
		if (!session_arr[i].release_flag)
		{
			DisconnectSession(*(unsigned long long*)&session_arr[i].session_id);
		}
	}

	wprintf(L"Disconnected all session\n");

	HANDLE* hExit = new HANDLE[num_of_worker + 1];

	for (int i = 0; i < num_of_worker; i++)
	{
		hExit[i] = hWorkerThread[i];
	}
	delete[] hWorkerThread;
	hExit[num_of_worker] = hAcceptThread;

	for (int i = 0; i < num_of_worker; i++)
		PostQueuedCompletionStatus(hcp, 0, 0, 0);


	WaitForMultipleObjects(num_of_worker + 1, hExit, TRUE, INFINITE);
	
	delete[] hExit;
	delete[] session_arr;

	WSACleanup();

	isRunning = false;
	

	return;
}

inline int CLanServer::GetSessionCount()
{
	return session_cnt;
}

unsigned long _stdcall CLanServer::AcceptThread(void* param)
{
	CLanServer* server = (CLanServer*)param;
	wprintf(L"%d Accept thread On...\n", GetCurrentThreadId());
	server->RunAcceptThread();
	// this call이나 server-> 콜이나 mov 2회지만 코딩 편의상
	// mov reg1 [this]/[server(지역주소)]
	// mov reg2 [reg1]
	wprintf(L"%d Accept thread end\n", GetCurrentThreadId());
	return 0;
}

unsigned long _stdcall CLanServer::IoThread(void* param)
{
	CLanServer* server = (CLanServer*)param;
	wprintf(L"%d worker thread On...\n", GetCurrentThreadId());
	server->RunIoThread();
	wprintf(L"%d IO thread end\n", GetCurrentThreadId());
	return 0;
}

inline void CLanServer::RunAcceptThread()
{
	int retval;

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

		tracer.trace(70, 0, client_sock);

		if (OnConnectionRequest(temp_ip, temp_port))
		{
			unsigned short index;
#ifdef STACK_INDEX
			while (!empty_session_stack.Pop(&index))
			{
			}
#else
			bool find = false;
			while (!find)
			{
				for (index = 0; index < _max_client; index++)
				{
					if (seesion_arr[index].used == false)
					{
						find = true;
						break;
					}
				}
			}
			session_arr[index].used = true;
#endif

			Session* session = &session_arr[index];

			session->session_id = m_sess_id++;
			if (m_sess_id == 0) m_sess_id++;
			*(unsigned*)&session->session_index = index;
			session->sock = client_sock;
			wcscpy_s(session->ip, _countof(session->ip), temp_ip);
			session->port = ntohs(clientaddr.sin_port);
			session->io_count = 0;
			session->send_flag = false;
			session->send_packet_cnt = 0;
			session->disconnect = false;
			//session->send_q.ClearBuffer(); 비어있어야 정상
			session->recv_q.ClearBuffer();

			CreateIoCompletionPort((HANDLE)client_sock, hcp, (ULONG_PTR)session, 0);

			session->release_flag = 0; // 준비 끝

			tracer.trace(10, session, session->session_id); // accept

			//접속
			monitor.IncAccept();

			InterlockedIncrement((LONG*)&session_cnt);


			// RecvPost()
			if (RecvPost(session))
			{
				OnClientJoin(*((unsigned long long*)&session->session_id));
			}
		}
		else // Connection Requeset 거부
		{
			closesocket(client_sock);
		}
	}
}
	

inline void CLanServer::RunIoThread()
{
	int ret_gqcp;
	DWORD thread_id = GetCurrentThreadId();
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
			wprintf(L"%d NULL overlapped [error : %d]\n", thread_id, WSAGetLastError());
			break;
		}

		if (ret_gqcp == 0)
		{
			//에러코드 로깅
			error_code = GetLastError();
			if (error_code != ERROR_NETNAME_DELETED)
				tracer.trace(00, session, error_code);
		}

		if (cbTransferred == 0 || session->disconnect) // Pending 후 I/O 처리 실패
		{
			tracer.trace(78, session, session->session_id);
			session->disconnect = true;
		
		}
		else {
			OnWorkerThreadBegin();
			if (&session->recv_overlapped == overlapped) // recv 결과 처리
			{
				if (session->recv_sock != session->sock)
					log_arr[2]++;
				tracer.trace(21, session, session->session_id);
				QueryPerformanceCounter(&recv_start);


				session->recv_q.MoveRear(cbTransferred);
				// msg 확인

				while (true)
				{
					LanPacketHeader header;

					if (session->recv_q.Peek((char*)&header, sizeof(header)) != sizeof(header))
						break;

					int q_size = session->recv_q.GetFillSize();
					if (header.len + sizeof(header) > q_size)
						break;
					session->recv_q.MoveFront(sizeof(header));

#ifdef AUTO_PACKET
					PacketPtr packet = CPacket::Alloc();
					int ret_deq = session->recv_q.Dequeue((*packet)->GetBufferPtr(), header.len);
					(*packet)->MoveWritePos(ret_deq);
					QueryPerformanceCounter(&on_recv_st);
					OnRecv(*(unsigned long long*)&session->session_id, packet);
					QueryPerformanceCounter(&on_recv_ed);
					monitor.AddOnRecvTime(&on_recv_st, &on_recv_ed);
#else
					CPacket* packet = CPacket::Alloc();
					int ret_deq = session->recv_q.Dequeue(packet->GetBufferPtr(), header.len);
					packet->MoveWritePos(ret_deq);
					QueryPerformanceCounter(&on_recv_st);
					OnRecv(*(unsigned long long*) &session->session_id, packet);
					QueryPerformanceCounter(&on_recv_ed);
					monitor.AddOnRecvTime(&on_recv_st, &on_recv_ed);

					CPacket::Free(packet);

#endif
				}
				QueryPerformanceCounter(&recv_end);
				monitor.AddRecvCompTime(&recv_start, &recv_end);
				monitor.IncRecv();

				tracer.trace(22, session, session->session_id);
				RecvPost(session);

			}
			else if (&session->send_overlapped == overlapped) // send 결과 처리
			{
				OnSend(*(unsigned long long*)&session->session_id, cbTransferred);
				if (session->send_sock != session->sock)
					log_arr[3]++;
				tracer.trace(31, session, session->session_id);
				
				QueryPerformanceCounter(&send_start);
				monitor.UpdateSendPacket(cbTransferred);

				int packet_cnt = session->send_packet_cnt;
				if (packet_cnt == 0)
				{
					log_arr[0]++;
				}                      
#ifdef AUTO_PACKET				
				while (packet_cnt > 0)
				{
					session->temp_packet[--packet_cnt].~PacketPtr();
				}


#else
				while (packet_cnt > 0)
				{
					session->temp_packet[--packet_cnt]->SubRef();
				}
#endif

				session->send_packet_cnt = 0;
				session->send_flag = false;

				QueryPerformanceCounter(&send_end);
				monitor.AddSendCompTime(&send_start, &send_end);

				tracer.trace(32, session, session->session_id);
				if (session->send_q.GetSize() > 0)
					SendPost(session);

			}
			else
			{
				log_arr[1]++;
			}
		}
		UpdateIOCount(session);
		OnWorkerThreadEnd();
	}

	return;
}

#ifdef AUTO_PACKET
bool CLanServer::SendPacket(unsigned long long session_id, PacketPtr packet)
{
	monitor.IncSendPacket();
	bool ret = false;

	unsigned short idx = session_id >> INDEX_BIT_SHIFT;
	unsigned int id = session_id & ID_MASK;
#ifndef STACK_INDEX

	for (idx = 0; idx < _max_client; idx++)
	{
		if (session_arr[idx].session_id == id)
			break;
	}

#endif

	Session* session = &session_arr[idx];

	InterlockedIncrement((LONG*)&session->io_count);
	if (session->release_flag == 0)
	{
		if (session->session_id == id)
		{
			session->send_q.Enqueue(packet);  // 64 bit 기준 8byte

			SendPost(session);

			ret = true;
		}
		else
		{
			log_arr[5]++;
		}
	}
	UpdateIOCount(session);




	if (!ret)
	{
		monitor.IncNoSession();
	}

	return ret;
}
#else
bool CLanServer::SendPacket(unsigned long long session_id, CPacket* packet)
{
	monitor.IncSendPacket();
	bool ret = false;

	unsigned short idx = session_id >> INDEX_BIT_SHIFT;
	unsigned int id = session_id & ID_MASK;

#ifndef STACK_INDEX

	for (idx = 0; idx < _max_client; idx++)
	{
		if (session_arr[idx].session_id == id)
			break;
	}

#endif

	Session* session = &session_arr[idx];

	InterlockedIncrement((LONG*)&session->io_count);
	if (session->release_flag == 0)
	{
		if (session->session_id == id)
		{
			packet->AddRef();

			session->send_q.Enqueue(packet);  // 64 bit 기준 8byte

			SendPost(session);

			ret = true;
		}
		else
		{
			log_arr[5]++;
		}
	}
	UpdateIOCount(session);




	if (!ret)
	{
		monitor.IncNoSession();
	}

	return ret;
}
#endif


inline void CLanServer::DisconnectSession(unsigned long long session_id)
{
	unsigned short idx = session_id >> INDEX_BIT_SHIFT;
	unsigned int id = session_id & ID_MASK;

#ifndef STACK_INDEX

	for (idx = 0; idx < _max_client; idx++)
	{
		if (session_arr[idx].session_id == id)
			break;
	}

#endif

	Session* session = &session_arr[idx];

	InterlockedIncrement((LONG*)&session->io_count);
	if (session->release_flag == 0)
	{
		if (session->session_id == id)
			session->disconnect = true;

	}
	UpdateIOCount(session);

	return;
}

inline bool CLanServer::RecvPost(Session* session)
{
	DWORD recvbytes, flags = 0;

	int temp = InterlockedIncrement((LONG*)&session->io_count);
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

	monitor.UpdateMaxIOCount(temp);

	SOCKET socket = session->sock;
	session->recv_sock = socket;
	DWORD error_code;

	int retval = WSARecv(socket, wsabuf, cnt, &recvbytes, &flags, &session->recv_overlapped, NULL);
	if (retval == SOCKET_ERROR)
	{
		if ((error_code = WSAGetLastError()) != ERROR_IO_PENDING)
		{ // 요청이 실패
			int io_temp = UpdateIOCount(session);
			tracer.trace(1, session, error_code, socket);
		}
		else
		{
			tracer.trace(73, session, socket);
			// Pending
		}
	}
	else
	{
		tracer.trace(73, session, socket);
		//동기 recv
	}

	return true;
}

inline bool CLanServer::SendPost(Session* session)
{
	bool temp;
	LARGE_INTEGER start, end;

	if ((temp = InterlockedExchange((LONG*)&session->send_flag, true)) == false)
	{
		long long buf_cnt = session->send_q.GetSize();
		if (buf_cnt <= 0)
		{
			log_arr[8]++;
			session->send_flag = false;
			return true;
		}
		int temp = InterlockedIncrement((LONG*)&session->io_count);

		int retval;

		ZeroMemory(&session->send_overlapped, sizeof(session->send_overlapped));

		// 개선 필요
		if (buf_cnt > MAX_WSABUF)
			buf_cnt = MAX_WSABUF;

		WSABUF wsabuf[MAX_WSABUF];
		ZeroMemory(wsabuf, sizeof(wsabuf));

#ifdef AUTO_PACKET
		PacketPtr packet;
		for (int cnt = 0; cnt < buf_cnt;)
		{
			if (session->send_q.Dequeue(&packet) == false) continue;
			

			wsabuf[cnt].buf = (*packet)->GetBufferPtrLan();
			wsabuf[cnt].len = (*packet)->GetDataSizeLan();
			session->temp_packet[cnt] = packet;

			++cnt;
		}
#else
		CPacket* packet;
		for (int cnt = 0; cnt < buf_cnt;)
		{
			if (!session->send_q.Dequeue(&packet)) continue;
			
			wsabuf[cnt].buf = packet->GetBufferPtrWithHeader();
			wsabuf[cnt].len = packet->GetDataSizeWithHeader();
			session->temp_packet[cnt] = packet;

			++cnt;
		}
#endif

		session->send_packet_cnt = buf_cnt;
		DWORD sendbytes;
		monitor.UpdateMaxIOCount(temp);
		monitor.IncSend();
;
		SOCKET socket = session->sock;
		session->send_sock = socket;
		QueryPerformanceCounter(&start);
		retval = WSASend(socket, wsabuf, buf_cnt, &sendbytes, 0, &session->send_overlapped, NULL);
		QueryPerformanceCounter(&end);
		monitor.AddSendTime(&start, &end);


		DWORD error_code;
		if (retval == SOCKET_ERROR)
		{
			if ((error_code = WSAGetLastError()) != WSA_IO_PENDING) // 요청 자체가 실패
			{
				// 내가 release 시켜야하는 경우 Packet 해제 해줘야 함
				int io_temp = UpdateIOCount(session);
				tracer.trace(2, session, error_code, socket);
			}
			else
			{
				tracer.trace(72, session, socket);
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

inline int CLanServer::UpdateIOCount(Session* session)
{
	int temp;
	tracer.trace(76, session, session->session_id);
	if ((temp = InterlockedDecrement((LONG*)&session->io_count)) == 0)
	{
		ReleaseSession(session);
	}
	if (temp < 0)
	{
		log_arr[8]++;
	}

	return temp;
}

inline void CLanServer::ReleaseSession(Session* session)
{

	unsigned long long flag = *((unsigned long long*)(&session->io_count));
	if (flag == 0)
	{
		if (InterlockedCompareExchange64((LONG64*)&session->io_count, 0x100000000, flag) == flag)
		{

			tracer.trace(75, session, session->session_id, session->sock);
			session->session_id = 0;


#ifdef AUTO_PACKET
			PacketPtr packet;
			while (session->send_q.Dequeue(&packet))
			{
			}
			int remain = session->send_packet_cnt;
			while (remain > 0)
			{
				session->temp_packet[--remain].~PacketPtr();
			}

#else
			CPacket* packet;
			while (session->send_q.Dequeue(&packet))
			{
				packet->SubRef();
			}

			int remain = session->send_packet_cnt;
			while (remain > 0)
			{
				session->temp_packet[--remain]->SubRef();
			}
#endif

			closesocket(session->sock);
			session->sock = INVALID_SOCKET;
			

			OnClientLeave(*(unsigned long long*)&session->session_id);

#ifdef STACK_INDEX
			empty_session_stack.Push(session->session_index);
#else
			session->used = false;
#endif

			InterlockedDecrement((LONG*)&session_cnt);
		}
	}

	return;
}

void CLanServer::Show()
{
	monitor.Show(session_cnt);
	return;
}
