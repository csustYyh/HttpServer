#pragma once
#include <string>
#include <string.h>

class Buffer :public std::string
{ // Buffer类，扩展std::string 类，提供了额外的构造函数和类型转换操作符。通过这些操作符，可以方便地将 Buffer 对象转换为 C 风格的字符串指针
public:
	// 注意派生类构造时，派生类的构造函数负责初始化派生类自己的成员和基类的成员。因此，调用基类构造函数是构造派生类对象时必需的步骤
	Buffer() :std::string() {} // 默认构造 
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
	operator void* () { return (char*)c_str(); } // 重载char*类型转换
	operator char* ()  { return (char*)c_str(); } // 重载char*类型转换，适用const对象
	operator unsigned char* () { return (unsigned char*)c_str(); }
	operator char* () const { return (char*)c_str(); }
	operator const char* () const { return c_str(); } // 重载const char* 类型转换，不涉及类型强制转换
	operator const void* () const { return c_str(); }
	
};