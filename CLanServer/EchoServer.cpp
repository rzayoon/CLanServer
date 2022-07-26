#include "EchoServer.h"
#include "CPacket.h"
#include "Protocol.h"

bool EchoServer::OnConnectionRequest(wchar_t* ip, unsigned short port) // 화이트리스트
{
	return true;
}

void EchoServer::OnClientJoin(unsigned int session_id)
{
	__int64 data = 0x7fffffffffffffff;

#ifdef AUTO_PACKET
	PacketPtr packet = CPacket::Alloc();
	*(*packet) << data;
#else
	CPacket* packet = new CPacket;
	*packet << data;
#endif

	SendPacket(session_id, packet);
}

void EchoServer::OnClientLeave()
{

}

#ifdef AUTO_PACKET
void EchoServer::OnRecv(unsigned int session_id, PacketPtr packet)
{
	__int64 echo;
	*(*packet) >> echo;

	PacketPtr send_packet = CPacket::Alloc();
	*(*send_packet) << echo;

	SendPacket(session_id, send_packet);

	return;
}
#else
void EchoServer::OnRecv(unsigned int session_id, CPacket* packet)
{
	__int64 echo;
	*packet >> echo;

	CPacket* send_packet = new CPacket;
	*send_packet << echo;
	
	SendPacket(session_id, send_packet);

	return;
}
#endif


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
