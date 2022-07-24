
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <Windows.h>

#include <conio.h>
#include "EchoServer.h"




#define SERVERPORT 6000

EchoServer server;

int main()
{
	int a = 0;

	

	server.Start(L"0.0.0.0", SERVERPORT, 4, 4, 0, 200);

	ULONGLONG oldTick = GetTickCount64();
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

		ULONGLONG term = GetTickCount64() - oldTick;
		Sleep(1000 - term);
		oldTick = GetTickCount64();

		server.Show();

	}

	wprintf(L"Fine Closing\n");
	Sleep(1000);

	return 0;
}