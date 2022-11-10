#include "EchoServer.h"
#include "CPacket.h"
#include "Protocol.h"


bool EchoServer::OnConnectionRequest(wchar_t* ip, unsigned short port) // ȭ��Ʈ����Ʈ
{
	return true;
}

void EchoServer::OnClientJoin(unsigned long long session_id)
{
	__int64 data = 0x7fffffffffffffff;

#ifdef AUTO_PACKET
	PacketPtr packet = CPacket::Alloc();
	*(*packet) << data;
	SendPacket(session_id, packet);
#else
	CPacket* packet = CPacket::Alloc();
	*packet << data;
	SendPacket(session_id, packet);
	
	CPacket::Free(packet);
#endif
}

void EchoServer::OnClientLeave(unsigned long long session_id)
{

}

#ifdef AUTO_PACKET
void EchoServer::OnRecv(unsigned long long session_id, PacketPtr packet)
{
	__int64 echo;
	*(*packet) >> echo;

	PacketPtr send_packet = CPacket::Alloc();
	*(*send_packet) << echo;

	SendPacket(session_id, send_packet);

	return;
}
#else
void EchoServer::OnRecv(unsigned long long session_id, CPacket* packet)
{
	__int64 echo;
	*packet >> echo;

	CPacket* send_packet = CPacket::Alloc();
	*send_packet << echo;
	
	SendPacket(session_id, send_packet);

	CPacket::Free(send_packet);

	return;
}
#endif


void EchoServer::OnSend(unsigned long long session_id, int send_size)
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
