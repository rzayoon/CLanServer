#include <Windows.h>
#include <stdio.h>
#include "Tracer.h"




const unsigned int Tracer::mask = 0xFFFF;



void Tracer::trace(char code, unsigned int err, long long value, PVOID ptr)
{
	unsigned long long seq = InterlockedIncrement64(&pos);
	unsigned int _pos = seq & mask;

	buf[_pos].id = GetCurrentThreadId();
	buf[_pos].seq = seq;
	buf[_pos].act = code;
	buf[_pos].err_code = err;
	buf[_pos].ptr = ptr;
	buf[_pos].info = value;
}

void Tracer::Crash()
{
	// �̻��
	trace(99, NULL, NULL); // 99 �α� �����δ� ����
	
	int a = 0;
	// suspend other threads, and run to Write file

	FILE* fp;

	fopen_s(&fp, "debug.csv", "w");
	if (fp == nullptr)
		return;
	fprintf_s(fp, "index,thread,seq,act,l_node,r_node,cnt\n");

	fclose(fp);

	a = 1;
}