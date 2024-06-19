#pragma once
#include "Public.h"
#include "http_parser.h"
#include <map>

// 使用http解析器 http_parser 的流程：
// 1.设置回调(callback)： http_parser_settings settings; settings.on_url = on_url; ...
// 2.定义并初始化解释器：http_parser parser; http_parser_init(&parser, HTTP_REQUEST);
// 3.解析： http_parser_execute(&parser, &settings, data, strlen(data)); 
// para1:http解析器实例的指针 para2：http_parser_settings结构体的指针，包含了解析器的配置和回调函数
// para3:待解析的数据的指针 para4:待解析的数据的长度 return：成功解析的数据长度

// 我们是如何将基于C语言的http_parser封装成面向对象的:
// 1.CHttpParser类中包含成员对象 http_parser 初始化时将http_parser.data 设置为当前类实例的this指针
// 2.http_parser_settings中的回调函数声明为静态函数，同时声明对应的成员函数
// (由于回调需要是静态函数或标准调用函数，C++中不能用标准调用函数作为类的成员函数，故只能通过静态函数来设置)
// 3.回调函数内(参数都会有一个http_parser*)，通过http_parser*->data 得到this指针
// 4.再用this指针调用对应的成员函数,将解析值赋给类的成员变量
class CHttpParser // 封装的http解析器
{
private:
	http_parser m_parser; // http报文解析器  https://github.com/nodejs/http-parser 
	http_parser_settings m_settings; // 结构体，里面是一些解析时的回调函数(cb)指针（参考 http_parser.h 中的声明）
	std::map<Buffer, Buffer> m_HeaderValues; // 实际上是一个键值对，HTTP 请求和响应的头部（header）数据格式都是键值对（key-value pairs）
	Buffer m_status; // http响应报文中状态行的状态码+状态信息
	Buffer m_url; // http请求报文中请求行的 url
	Buffer m_body; // http报文中body部分
	bool m_complete; // 解析完成的标志
	Buffer m_lastField; // header中的key
public:
	CHttpParser();
	~CHttpParser();
	CHttpParser(const CHttpParser& http);
	CHttpParser& operator=(const CHttpParser& http);
public:
	size_t Parser(const Buffer& data); // 解析函数，调用http_parser_execute, 解析http请求(data)
	// GET POST ... 参考http_parser.h HTTP_METHOD_MAP 宏
	unsigned Method() const { return m_parser.method; } // 获取http中的客户端请求报文第一行(请求行)中的请求方法：GET POST... 注意，只有请求报文才有！ 
	const std::map<Buffer, Buffer>& Headers() { return m_HeaderValues; } // 头部(header)都是键值对
	const Buffer& Status() const { return m_status; } // 获取http中的服务器响应报文第一行(状态行)中的状态码+信息：200 OK 302 FOUND... 注意，只有响应报文才有！ 
	const Buffer& Url() const { return m_url; } // 获取请求报文第一行(请求行)中的Url(Uniform Resource Locator) 是指向所请求资源的具体地址，紧跟在请求方法之后
	// HTTP 请求和响应中的 body（主体）是用来传输实际的数据内容。body 部分通常用于发送或接收数据，比如表单提交的数据、文件、JSON 对象等
	const Buffer& Body() const { return m_body; } // header中的字段 Content-Type 和 Content-Length 用于描述 body 的类型和长度
	unsigned Errno() const { return m_parser.http_errno; } // 错误码
protected:
	static int OnMessageBegin(http_parser* parser); // http 解析开始时的事件
	// para1:指向当前HTTP解析器实例的指针 para2:指向包含URL的字符串的指针。这个指针指向原始输入数据的某个位置 para3:表示URL字符串的长度
	// para2和para3是解析器自己得到的，当解析器检测到请求行中的URL时，它会调用on_url回调函数，并传递当前解析器状态和URL数据
	static int OnUrl(http_parser* parser, const char* at, size_t length); 
	static int OnStatus(http_parser* parser, const char* at, size_t length); // para2：状态信息起始位置 para3：状态信息长度
	static int OnHeaderField(http_parser* parser, const char* at, size_t length); // para2:头部字段名(即Key "Connection:")的起始位置 para3:头部字段名长度
	static int OnHeaderValue(http_parser* parser, const char* at, size_t length); // para2:头部字段值(即Value "Keep-Alive")的起始位置 para3:头部字段值长度
	static int OnHeadersComplete(http_parser* parser); // 头部(header)解析完成时的事件
	static int OnBody(http_parser* parser, const char* at, size_t length); // para2:body起始位置 para3:body长度
	static int OnMessageComplete(http_parser* parser); // http 解析完成时的事件
	int OnMessageBegin();
	int OnUrl(const char* at, size_t length);
	int OnStatus(const char* at, size_t length);
	int OnHeaderField(const char* at, size_t length);
	int OnHeaderValue(const char* at, size_t length);
	int OnHeadersComplete();
	int OnBody(const char* at, size_t length);
	int OnMessageComplete();
};
 
class UrlParser // url解析器
{
public:
	UrlParser(const Buffer& url); 
	~UrlParser() {}
	int Parser(); // 解析
	Buffer operator[](const Buffer& name)const; // 返回查询参数的value
	Buffer Protocol()const { return m_protocol; } // 返回传输协议
	Buffer Host()const { return m_host;} // 返回域名
	int Port()const { return m_port; } // 默认返回80
	void SetUrl(const Buffer& url); // 设置url
	const Buffer Uri()const { return m_uri; } // 返回 m_uri
private:
	Buffer m_url; // url 统一资源定位符 URL 是 URI 的子集，它不仅标识资源，还提供了资源的定位信息，即告诉客户端如何访问资源。例如，https://www.example.com/index.html 是一个 URL，它指示了一个网络资源（网页），并提供了访问该资源的协议（HTTPS）和位置
	Buffer m_protocol; // 协议
	Buffer m_host; // 域名
	// uri 统一资源标识符 包括但不限于互联网资源。URI 是一个字符序列，用于唯一标识资源。它包含两个主要的子类别：URL 和 URN
	Buffer m_uri; // 本项目中指 域名之后 '?'之前的部分 如：https://www.example.com/index.html?query1=value1&query2=value2 中的"/index.html"
	int m_port; // 通信端口
	std::map<Buffer, Buffer> m_values; // 查询参数键值对
};