#pragma once
#include <string>
#include <string.h>

class Buffer :public std::string
{ // Buffer�࣬��չstd::string �࣬�ṩ�˶���Ĺ��캯��������ת����������ͨ����Щ�����������Է���ؽ� Buffer ����ת��Ϊ C �����ַ���ָ��
public:
	// ע�������๹��ʱ��������Ĺ��캯�������ʼ���������Լ��ĳ�Ա�ͻ���ĳ�Ա����ˣ����û��๹�캯���ǹ������������ʱ����Ĳ���
	Buffer() :std::string() {} // Ĭ�Ϲ��� 
	Buffer(size_t size) : std::string() { resize(size); }
	Buffer(const std::string& str) :std::string(str) {}
	Buffer(const char* str) :std::string(str) {}
	Buffer(const char* str, size_t length)
		:std::string() {
		resize(length);
		memcpy((char*)c_str(), str, length);
	}
	Buffer(const char* begin, const char* end) :std::string() {
		long int len = end - begin;
		if (len > 0) {
			resize(len);
			memcpy((char*)c_str(), begin, len);
		}
	}
	operator void* () { return (char*)c_str(); } // ����char*����ת��
	operator char* ()  { return (char*)c_str(); } // ����char*����ת��������const����
	operator unsigned char* () { return (unsigned char*)c_str(); }
	operator char* () const { return (char*)c_str(); }
	operator const char* () const { return c_str(); } // ����const char* ����ת�������漰����ǿ��ת��
	operator const void* () const { return c_str(); }
	
};