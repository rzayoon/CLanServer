
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <Windows.h>

#include <conio.h>
#include "EchoServer.h"
#include "TextParser.h"



#define SERVERPORT 6000

EchoServer server;

int main()
{
	
	timeBeginPeriod(1);

	char ip[16];
	short port;
	int worker;
	int max_worker;
	int max_user;
	int nagle;

	TextParser parser;
	if (!parser.LoadFile("Config.ini")) return 1;

	parser.GetStringValue("ServerBindIP", ip, 16);
	parser.GetValue("ServerBindPort", (int*)&port);
	parser.GetValue("WorkerThread", &worker);
	parser.GetValue("MaxWorkerThread", &max_worker);
	parser.GetValue("MaxUser", &max_user);
	parser.GetValue("Nagle", &nagle);

	wchar_t wip[16];

	MultiByteToWideChar(CP_ACP, 0, ip, 16, wip, 16);

	server.Start(wip, port, worker, max_worker, nagle, max_user);

	DWORD oldTick = timeGetTime();
	while (1)
	{
		if (_kbhit())
		{
			wchar_t input;
			input = _getwch();

			if (input == L'q' || input == L'Q')
			{

				server.Stop();

				break;
			}
		}

		DWORD term = timeGetTime() - oldTick;
		Sleep(1000 - term);
		oldTick = timeGetTime();

		server.Show();

	}

	timeEndPeriod(1);

	wprintf(L"Fine Closing\n");
	Sleep(5000);


	return 0;
}