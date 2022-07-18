#pragma once







class Tracer
{
	struct DebugNode
	{
		unsigned int id;
		unsigned long long seq;
		char act;
		unsigned int err_code;
		PVOID ptr;
		long long info;
	};
public:
	Tracer()
	{
		pos = 0;
		memset(buf, 0x00, sizeof(buf));
	}
	~Tracer()
	{


	}
	void trace(char code, unsigned int err = 0, long long value = 0, PVOID ptr = 0);

	void Crash();
private:
	alignas(64) LONG64 pos;

	DebugNode buf[65536];
	static const unsigned int mask;

};


