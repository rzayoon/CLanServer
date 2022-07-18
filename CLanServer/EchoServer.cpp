#include "EchoServer.h"

bool EchoServer::OnConnectionRequest(wchar_t* ip, unsigned short port) // 화이트리스트
{
	return true;
}

void EchoServer::OnClientJoin(unsigned int session_id)
{

}

void EchoServer::OnClientLeave()
{

}

void EchoServer::OnRecv(unsigned int session_id, CPacket* packet)
{
	__int64 echo;
	*packet >> echo;

	CPacket* send_packet = new CPacket;
	*send_packet << echo;
	
	SendPacket(session_id, send_packet);

	return;
}

void EchoServer::OnSend(unsigned int session_id, int send_size)
{

}

void EchoServer::OnWorkerThreadBegin()
{
}

void EchoServer::OnWorkerThreadEnd()
{
}

void EchoServer::OnError(int errorcode, const wchar_t* msg)
{

}
