#pragma once

#include <Windows.h>

class CPacket
{
public:
	enum
	{
		eBUFFER_DEFAULT = 2000
	};

	friend class CLanServer;
	
	CPacket(int size = eBUFFER_DEFAULT);
	
	CPacket(CPacket& src);

	virtual ~CPacket();

	/// <summary>
	/// ��Ŷ �ı�
	/// </summary>
	/// <param name=""></param>
	inline void Release(void);

	/// <summary>
	/// ��Ŷ û��
	/// </summary>
	/// <param name=""></param>
	inline void Clear(void);

	/// <summary>
	/// ���� ������
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	int GetBufferSize(void) { return buffer_size; }

	/// <summary>
	/// ������� ������
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	int GetDataSize(void) { return data_size; }

	/// <summary>
	/// ���� ������
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	char* GetBufferPtr(void) { return buffer; };

	char* GetReadPtr(void) { return buffer + read_pos; }

	/// <summary>
	/// ���� ���� pos �̵�
	/// �ܺο��� ������ ���� ���� ������ ���
	/// </summary>
	/// <param name="size">�̵� ������</param>
	/// <returns>�̵��� ������</returns>
	int MoveWritePos(int size);

	/// <summary>
	/// ���� �б� pos �̵�
	/// �ܺο��� ������ ���� ���� ������ ���
	/// </summary>
	/// <param name="size">�̵� ������</param>
	/// <returns>�̵��� ������</returns>
	int MoveReadPos(int size);

	
	CPacket& operator=(CPacket& src);

	CPacket& operator<<(unsigned char value);
	CPacket& operator<<(char value);

	CPacket& operator<<(short value);
	CPacket& operator<<(unsigned short value);

	CPacket& operator<<(int value);
	CPacket& operator<<(unsigned int value);

	CPacket& operator<<(long value);
	CPacket& operator<<(unsigned long);

	CPacket& operator<<(float value);

	CPacket& operator<<(__int64 value);
	CPacket& operator<<(double value);

	CPacket& operator>>(unsigned char& value);
	CPacket& operator>>(char& value);


	CPacket& operator>>(short& value);
	CPacket& operator>>(unsigned short& value);

	CPacket& operator>>(int& value);
	CPacket& operator>>(unsigned int& value);
	CPacket& operator>>(long& value);
	CPacket& operator>>(unsigned long& value);
	CPacket& operator>>(float& value);

	CPacket& operator>>(__int64& value);
	CPacket& operator>>(double& value);

	int GetData(char* dest, int size);
	int PutData(char* src, int size);


protected:

	char* GetBufferPtrWithHeader(void);
	int GetDataSizeWithHeader(void);

	char* buffer;
	char* hidden_buf;
	int write_pos;
	int read_pos;


	int buffer_size;
	int data_size;
};

