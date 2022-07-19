#pragma once







class Tracer
{
	struct DebugNode
	{
		unsigned int id;
		unsigned long long seq;
		char act;
		PVOID session;
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
	void trace(char code, PVOID session, long long value = 0);

	void Crash();
private:

	DebugNode buf[65536];
	alignas(64) LONG64 pos;
	alignas(64) LONG b;

	static const unsigned int mask;

};


