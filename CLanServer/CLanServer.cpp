#include "CLanServer.h"




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

unsigned long _stdcall CLanServer::IoThread(void* param)
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
			server->monitor.IncAcceptErr();
			continue;
		}

		int idx = 0;
		for (; idx < MAX_SESSION; idx++)
		{
			if (g_session_arr[idx].used == false)
			{
				g_session_arr[idx].used = true;
				break;
			}
		}
		Session* session = &g_session_arr[idx];

		session->sock = client_sock;
		inet_ntop(AF_INET, &clientaddr.sin_addr, session->ip, sizeof(session->ip));
		session->port = ntohs(clientaddr.sin_port);
		session->io_count = 0;
		session->session_id = g_sessionid++;
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
		server->monitor.IncAccept();

		InterlockedIncrement((LONG*)&session_cnt);

		CreateIoCompletionPort((HANDLE)client_sock, hcp, (ULONG_PTR)session, 0);

		// RecvPost()
		RecvPost(session);

	}

	return 0;

}

unsigned long _stdcall CLanServer::WorkerThread(void* param)
{


}