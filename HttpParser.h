#pragma once
#include "Public.h"
#include "http_parser.h"
#include <map>

// ʹ��http������ http_parser �����̣�
// 1.���ûص�(callback)�� http_parser_settings settings; settings.on_url = on_url; ...
// 2.���岢��ʼ����������http_parser parser; http_parser_init(&parser, HTTP_REQUEST);
// 3.������ http_parser_execute(&parser, &settings, data, strlen(data)); 
// para1:http������ʵ����ָ�� para2��http_parser_settings�ṹ���ָ�룬�����˽����������úͻص�����
// para3:�����������ݵ�ָ�� para4:�����������ݵĳ��� return���ɹ����������ݳ���

// ��������ν�����C���Ե�http_parser��װ����������:
// 1.CHttpParser���а�����Ա���� http_parser ��ʼ��ʱ��http_parser.data ����Ϊ��ǰ��ʵ����thisָ��
// 2.http_parser_settings�еĻص���������Ϊ��̬������ͬʱ������Ӧ�ĳ�Ա����
// (���ڻص���Ҫ�Ǿ�̬�������׼���ú�����C++�в����ñ�׼���ú�����Ϊ��ĳ�Ա��������ֻ��ͨ����̬����������)
// 3.�ص�������(����������һ��http_parser*)��ͨ��http_parser*->data �õ�thisָ��
// 4.����thisָ����ö�Ӧ�ĳ�Ա����,������ֵ������ĳ�Ա����
class CHttpParser // ��װ��http������
{
private:
	http_parser m_parser; // http���Ľ�����  https://github.com/nodejs/http-parser 
	http_parser_settings m_settings; // �ṹ�壬������һЩ����ʱ�Ļص�����(cb)ָ�루�ο� http_parser.h �е�������
	std::map<Buffer, Buffer> m_HeaderValues; // ʵ������һ����ֵ�ԣ�HTTP �������Ӧ��ͷ����header�����ݸ�ʽ���Ǽ�ֵ�ԣ�key-value pairs��
	Buffer m_status; // http��Ӧ������״̬�е�״̬��+״̬��Ϣ
	Buffer m_url; // http�������������е� url
	Buffer m_body; // http������body����
	bool m_complete; // ������ɵı�־
	Buffer m_lastField; // header�е�key
public:
	CHttpParser();
	~CHttpParser();
	CHttpParser(const CHttpParser& http);
	CHttpParser& operator=(const CHttpParser& http);
public:
	size_t Parser(const Buffer& data); // ��������������http_parser_execute, ����http����(data)
	// GET POST ... �ο�http_parser.h HTTP_METHOD_MAP ��
	unsigned Method() const { return m_parser.method; } // ��ȡhttp�еĿͻ��������ĵ�һ��(������)�е����󷽷���GET POST... ע�⣬ֻ�������Ĳ��У� 
	const std::map<Buffer, Buffer>& Headers() { return m_HeaderValues; } // ͷ��(header)���Ǽ�ֵ��
	const Buffer& Status() const { return m_status; } // ��ȡhttp�еķ�������Ӧ���ĵ�һ��(״̬��)�е�״̬��+��Ϣ��200 OK 302 FOUND... ע�⣬ֻ����Ӧ���Ĳ��У� 
	const Buffer& Url() const { return m_url; } // ��ȡ�����ĵ�һ��(������)�е�Url(Uniform Resource Locator) ��ָ����������Դ�ľ����ַ�����������󷽷�֮��
	// HTTP �������Ӧ�е� body�����壩����������ʵ�ʵ��������ݡ�body ����ͨ�����ڷ��ͻ�������ݣ�������ύ�����ݡ��ļ���JSON �����
	const Buffer& Body() const { return m_body; } // header�е��ֶ� Content-Type �� Content-Length �������� body �����ͺͳ���
	unsigned Errno() const { return m_parser.http_errno; } // ������
protected:
	static int OnMessageBegin(http_parser* parser); // http ������ʼʱ���¼�
	// para1:ָ��ǰHTTP������ʵ����ָ�� para2:ָ�����URL���ַ�����ָ�롣���ָ��ָ��ԭʼ�������ݵ�ĳ��λ�� para3:��ʾURL�ַ����ĳ���
	// para2��para3�ǽ������Լ��õ��ģ�����������⵽�������е�URLʱ���������on_url�ص������������ݵ�ǰ������״̬��URL����
	static int OnUrl(http_parser* parser, const char* at, size_t length); 
	static int OnStatus(http_parser* parser, const char* at, size_t length); // para2��״̬��Ϣ��ʼλ�� para3��״̬��Ϣ����
	static int OnHeaderField(http_parser* parser, const char* at, size_t length); // para2:ͷ���ֶ���(��Key "Connection:")����ʼλ�� para3:ͷ���ֶ�������
	static int OnHeaderValue(http_parser* parser, const char* at, size_t length); // para2:ͷ���ֶ�ֵ(��Value "Keep-Alive")����ʼλ�� para3:ͷ���ֶ�ֵ����
	static int OnHeadersComplete(http_parser* parser); // ͷ��(header)�������ʱ���¼�
	static int OnBody(http_parser* parser, const char* at, size_t length); // para2:body��ʼλ�� para3:body����
	static int OnMessageComplete(http_parser* parser); // http �������ʱ���¼�
	int OnMessageBegin();
	int OnUrl(const char* at, size_t length);
	int OnStatus(const char* at, size_t length);
	int OnHeaderField(const char* at, size_t length);
	int OnHeaderValue(const char* at, size_t length);
	int OnHeadersComplete();
	int OnBody(const char* at, size_t length);
	int OnMessageComplete();
};
 
class UrlParser // url������
{
public:
	UrlParser(const Buffer& url); 
	~UrlParser() {}
	int Parser(); // ����
	Buffer operator[](const Buffer& name)const; // ���ز�ѯ������value
	Buffer Protocol()const { return m_protocol; } // ���ش���Э��
	Buffer Host()const { return m_host;} // ��������
	int Port()const { return m_port; } // Ĭ�Ϸ���80
	void SetUrl(const Buffer& url); // ����url
	const Buffer Uri()const { return m_uri; } // ���� m_uri
private:
	Buffer m_url; // url ͳһ��Դ��λ�� URL �� URI ���Ӽ�����������ʶ��Դ�����ṩ����Դ�Ķ�λ��Ϣ�������߿ͻ�����η�����Դ�����磬https://www.example.com/index.html ��һ�� URL����ָʾ��һ��������Դ����ҳ�������ṩ�˷��ʸ���Դ��Э�飨HTTPS����λ��
	Buffer m_protocol; // Э��
	Buffer m_host; // ����
	// uri ͳһ��Դ��ʶ�� �����������ڻ�������Դ��URI ��һ���ַ����У�����Ψһ��ʶ��Դ��������������Ҫ�������URL �� URN
	Buffer m_uri; // ����Ŀ��ָ ����֮�� '?'֮ǰ�Ĳ��� �磺https://www.example.com/index.html?query1=value1&query2=value2 �е�"/index.html"
	int m_port; // ͨ�Ŷ˿�
	std::map<Buffer, Buffer> m_values; // ��ѯ������ֵ��
};