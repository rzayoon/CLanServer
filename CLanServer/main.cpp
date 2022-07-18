#include "EchoServer.h"



#define SERVERPORT 6000

EchoServer server;

int main()
{
	int a = 0;

	

	server.Start(L"0.0.0.0", SERVERPORT, 4, 4, 0, 100);

	Sleep(INFINITE);



	return 0;
}