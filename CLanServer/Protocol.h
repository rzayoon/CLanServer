#pragma once


#define FIX_KEY 0xa9
#define PACKET_CODE 0x00

#pragma pack(push, 1)

struct LanPacketHeader
{
	unsigned short len;
};


struct NetPacketHeader
{
	unsigned char code;
	unsigned short len;
	unsigned char rand_key;
	unsigned char check_sum;

};

#pragma pack(pop)

