#include "HttpParser.h"


CHttpParser::CHttpParser()
{
	m_complete = false;
	memset(&m_parser, 0, sizeof(m_parser));
	m_parser.data = this; // 通过设置为this指针将http_parser中的回调函数(C语言的，只能是标准调用或静态函数，没有this指针)与我们封装的成员函数挂钩，即静态方法中通过this调成员方法
	http_parser_init(&m_parser, HTTP_REQUEST); // 服务端，只会收到来自客户端的请求(HTTP_REQUEST）,即主要解析的是http请求报文
	memset(&m_settings, 0, sizeof(m_settings)); // 存放的是回调函数指针
	m_settings.on_message_begin = &CHttpParser::OnMessageBegin; // 由于回调需要是静态函数或标准调用函数，C++中不能用标准调用函数作为类的成员函数，故只能通过静态函数来设置
	m_settings.on_url = &CHttpParser::OnUrl;
	m_settings.on_status = &CHttpParser::OnStatus;
	m_settings.on_header_field = &CHttpParser::OnHeaderField;
	m_settings.on_header_value = &CHttpParser::OnHeaderValue;
	m_settings.on_headers_complete = &CHttpParser::OnHeadersComplete;
	m_settings.on_body = &CHttpParser::OnBody;
	m_settings.on_message_complete = &CHttpParser::OnMessageComplete;
}

CHttpParser::~CHttpParser()
{}

CHttpParser::CHttpParser(const CHttpParser& http)
{
	memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
	m_parser.data = this;
	memcpy(&m_settings, &http.m_settings, sizeof(m_settings));
	m_status = http.m_status;
	m_url = http.m_url;
	m_body = http.m_body;
	m_complete = http.m_complete;
	m_lastField = http.m_lastField;
}

CHttpParser& CHttpParser::operator=(const CHttpParser& http)
{
	if (this != &http){
		memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
		m_parser.data = this;
		memcpy(&m_settings, &http.m_settings, sizeof(m_settings));
		m_status = http.m_status;
		m_url = http.m_url;
		m_body = http.m_body;
		m_complete = http.m_complete;
		m_lastField = http.m_lastField;
	}
	return *this;
}

size_t CHttpParser::Parser(const Buffer& data) 
{
	m_complete = false;
	size_t ret = http_parser_execute(
		&m_parser, &m_settings, data, data.size());
	if (m_complete == false) { // 如果解析成功的话 m_complete == true (OnMessageComplete()这个callback中会设置)
		m_parser.http_errno = 0x7F; // 解析失败
		return 0;
	}
	return ret;
}

int CHttpParser::OnMessageBegin(http_parser* parser) 
{
	return ((CHttpParser*)parser->data)->OnMessageBegin();
}

int CHttpParser::OnUrl(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnUrl(at, length);
}

int CHttpParser::OnStatus(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnStatus(at, length);
}

int CHttpParser::OnHeaderField(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnHeaderField(at, length);
}

int CHttpParser::OnHeaderValue(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnHeaderValue(at, length);
}

int CHttpParser::OnHeadersComplete(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnHeadersComplete();
}

int CHttpParser::OnBody(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnBody(at, length);
}

int CHttpParser::OnMessageComplete(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnMessageComplete();
}

int CHttpParser::OnMessageBegin()
{
	return 0;
}

int CHttpParser::OnUrl(const char* at, size_t length)
{
	m_url = Buffer(at, length); // 得到Url
	return 0;
}

int CHttpParser::OnStatus(const char* at, size_t length)
{
	m_status = Buffer(at, length); // 得到状态
	return 0;
}

int CHttpParser::OnHeaderField(const char* at, size_t length)
{
	m_lastField = Buffer(at, length); // 得到header的键
	return 0;
}

int CHttpParser::OnHeaderValue(const char* at, size_t length)
{
	m_HeaderValues[m_lastField] = Buffer(at, length); // 得到header的值
	return 0;
}

int CHttpParser::OnHeadersComplete()
{
	return 0;
}

int CHttpParser::OnBody(const char* at, size_t length)
{
	m_body = Buffer(at, length); // 得到body
	return 0;
}

int CHttpParser::OnMessageComplete()
{
	m_complete = true; // 完成
	return 0;
}

UrlParser::UrlParser(const Buffer& url)
{
	m_url = url;
}

int UrlParser::Parser()
{   // url: https://www.example.com:8080/path/to/resource?query1=value1&query2=value2&lang=en
	//      在 www.example.com:8080 的 /path/to/resource 资源上进行查询，查询参数包括 query1、query2 和 lang。
	//      '://'用于分割协议与主机名
	//      ':' 通常用于指示主机部分和端口部分之间的分隔
	//	    '?' 用于分隔URL的主体部分和查询参数部分。通常，'?' 出现在URL中，表示URL的主体部分结束，后面是查询参数部分
	//      '=' 通常用于分隔查询参数中的键值对。在查询参数中，'=' 左边是键，右边是相应的值
	//      '&' 通常用于分隔不同的查询参数。当一个URL中有多个查询参数时，可以使用 '&' 将它们分隔开来
	   
	// 分三步：协议、域名和端口、uri、键值对
	// 1.解析协议
	const char* pos = m_url;
	const char* target = strstr(pos, "://");
	if (target == NULL)return -1;
	m_protocol = Buffer(pos, target); // 存储解析出的协议
	// 2.解析域名和端口
	pos = target + 3; // 跳过"://"
	target = strchr(pos, '/'); // 查找主机名和端口号后的'/'
	if (target == NULL) { // 找不到到
		if (m_protocol.size() + 3 >= m_url.size())
			return -2;
		m_host = pos; // 记录主机名
		return 0;
	}
	Buffer value = Buffer(pos, target); // 将主机名和端口存在value中
	if (value.size() == 0)return -3;
	target = strchr(value, ':'); // 查找:
	if (target != NULL) { // 找到了：说明存在端口号
		m_host = Buffer(value, target); // 存储主机名
		m_port = atoi(Buffer(target + 1, (char*)value + value.size())); // 存储端口号(string->int)
	}
	else {
		m_host = value; // 只有主机名没有端口号
	}
	pos = strchr(pos, '/'); // 找uri的起始位置
	// 3.解析uri
	target = strchr(pos, '?'); // 找'？'
	if (target == NULL) { // 没有'?',说明URI后面没有查询参数
		m_uri = pos + 1; // 将URI存储在 m_uri 中
		return 0;
	}
	// 4.解析查询参数
	else { // 如果找到了问号，表示URI后面有查询参数
		m_uri = Buffer(pos + 1, target); // 将问号之前的部分作为URI存储在 m_uri 中
		//解析key和value
		pos = target + 1; // 指向查询参数的起始位置
		const char* t = NULL;
		do {
			target = strchr(pos, '&'); // 查找查询参数中的 "&" 分隔符
			if (target == NULL) { // 如果找不到 "&"，表示是最后一个键值对
				t = strchr(pos, '='); // 查找键值对中的 "="
				if (t == NULL)return -4;
				m_values[Buffer(pos, t)] = Buffer(t + 1); // 将键和值存储在 m_values 中
			}
			else { // 如果找到了 "&"，表示还有下一个键值对
				Buffer kv(pos, target); // 将当前键值对存储在 kv 中
				t = strchr(kv, '='); // 在当前键值对中查找 "="
				if (t == NULL)return -5;
				m_values[Buffer(kv, t)] = Buffer(t + 1, (char*)kv + kv.size()); // 将键和值存储在 m_values 中
				pos = target + 1;
			}
		} while (target != NULL); // 记录所有查询参数键值对到 m_values[]
	}

	return 0;
}

Buffer UrlParser::operator[](const Buffer& name) const
{
	auto it = m_values.find(name);
	if (it == m_values.end()) return Buffer();
	return it->second;
}

void UrlParser::SetUrl(const Buffer& url)
{
	m_url = url;
	m_protocol = "";
	m_host = "";
	m_port = 80;
	m_values.clear();
}
