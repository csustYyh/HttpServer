#include "HttpParser.h"


CHttpParser::CHttpParser()
{
	m_complete = false;
	memset(&m_parser, 0, sizeof(m_parser));
	m_parser.data = this; // ͨ������Ϊthisָ�뽫http_parser�еĻص�����(C���Եģ�ֻ���Ǳ�׼���û�̬������û��thisָ��)�����Ƿ�װ�ĳ�Ա�����ҹ�������̬������ͨ��this����Ա����
	http_parser_init(&m_parser, HTTP_REQUEST); // ����ˣ�ֻ���յ����Կͻ��˵�����(HTTP_REQUEST��,����Ҫ��������http������
	memset(&m_settings, 0, sizeof(m_settings)); // ��ŵ��ǻص�����ָ��
	m_settings.on_message_begin = &CHttpParser::OnMessageBegin; // ���ڻص���Ҫ�Ǿ�̬�������׼���ú�����C++�в����ñ�׼���ú�����Ϊ��ĳ�Ա��������ֻ��ͨ����̬����������
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
	if (m_complete == false) { // ��������ɹ��Ļ� m_complete == true (OnMessageComplete()���callback�л�����)
		m_parser.http_errno = 0x7F; // ����ʧ��
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
	m_url = Buffer(at, length); // �õ�Url
	return 0;
}

int CHttpParser::OnStatus(const char* at, size_t length)
{
	m_status = Buffer(at, length); // �õ�״̬
	return 0;
}

int CHttpParser::OnHeaderField(const char* at, size_t length)
{
	m_lastField = Buffer(at, length); // �õ�header�ļ�
	return 0;
}

int CHttpParser::OnHeaderValue(const char* at, size_t length)
{
	m_HeaderValues[m_lastField] = Buffer(at, length); // �õ�header��ֵ
	return 0;
}

int CHttpParser::OnHeadersComplete()
{
	return 0;
}

int CHttpParser::OnBody(const char* at, size_t length)
{
	m_body = Buffer(at, length); // �õ�body
	return 0;
}

int CHttpParser::OnMessageComplete()
{
	m_complete = true; // ���
	return 0;
}

UrlParser::UrlParser(const Buffer& url)
{
	m_url = url;
}

int UrlParser::Parser()
{   // url: https://www.example.com:8080/path/to/resource?query1=value1&query2=value2&lang=en
	//      �� www.example.com:8080 �� /path/to/resource ��Դ�Ͻ��в�ѯ����ѯ�������� query1��query2 �� lang��
	//      '://'���ڷָ�Э����������
	//      ':' ͨ������ָʾ�������ֺͶ˿ڲ���֮��ķָ�
	//	    '?' ���ڷָ�URL�����岿�ֺͲ�ѯ�������֡�ͨ����'?' ������URL�У���ʾURL�����岿�ֽ����������ǲ�ѯ��������
	//      '=' ͨ�����ڷָ���ѯ�����еļ�ֵ�ԡ��ڲ�ѯ�����У�'=' ����Ǽ����ұ�����Ӧ��ֵ
	//      '&' ͨ�����ڷָ���ͬ�Ĳ�ѯ��������һ��URL���ж����ѯ����ʱ������ʹ�� '&' �����Ƿָ�����
	   
	// ��������Э�顢�����Ͷ˿ڡ�uri����ֵ��
	// 1.����Э��
	const char* pos = m_url;
	const char* target = strstr(pos, "://");
	if (target == NULL)return -1;
	m_protocol = Buffer(pos, target); // �洢��������Э��
	// 2.���������Ͷ˿�
	pos = target + 3; // ����"://"
	target = strchr(pos, '/'); // �����������Ͷ˿ںź��'/'
	if (target == NULL) { // �Ҳ�����
		if (m_protocol.size() + 3 >= m_url.size())
			return -2;
		m_host = pos; // ��¼������
		return 0;
	}
	Buffer value = Buffer(pos, target); // ���������Ͷ˿ڴ���value��
	if (value.size() == 0)return -3;
	target = strchr(value, ':'); // ����:
	if (target != NULL) { // �ҵ��ˣ�˵�����ڶ˿ں�
		m_host = Buffer(value, target); // �洢������
		m_port = atoi(Buffer(target + 1, (char*)value + value.size())); // �洢�˿ں�(string->int)
	}
	else {
		m_host = value; // ֻ��������û�ж˿ں�
	}
	pos = strchr(pos, '/'); // ��uri����ʼλ��
	// 3.����uri
	target = strchr(pos, '?'); // ��'��'
	if (target == NULL) { // û��'?',˵��URI����û�в�ѯ����
		m_uri = pos + 1; // ��URI�洢�� m_uri ��
		return 0;
	}
	// 4.������ѯ����
	else { // ����ҵ����ʺţ���ʾURI�����в�ѯ����
		m_uri = Buffer(pos + 1, target); // ���ʺ�֮ǰ�Ĳ�����ΪURI�洢�� m_uri ��
		//����key��value
		pos = target + 1; // ָ���ѯ��������ʼλ��
		const char* t = NULL;
		do {
			target = strchr(pos, '&'); // ���Ҳ�ѯ�����е� "&" �ָ���
			if (target == NULL) { // ����Ҳ��� "&"����ʾ�����һ����ֵ��
				t = strchr(pos, '='); // ���Ҽ�ֵ���е� "="
				if (t == NULL)return -4;
				m_values[Buffer(pos, t)] = Buffer(t + 1); // ������ֵ�洢�� m_values ��
			}
			else { // ����ҵ��� "&"����ʾ������һ����ֵ��
				Buffer kv(pos, target); // ����ǰ��ֵ�Դ洢�� kv ��
				t = strchr(kv, '='); // �ڵ�ǰ��ֵ���в��� "="
				if (t == NULL)return -5;
				m_values[Buffer(kv, t)] = Buffer(t + 1, (char*)kv + kv.size()); // ������ֵ�洢�� m_values ��
				pos = target + 1;
			}
		} while (target != NULL); // ��¼���в�ѯ������ֵ�Ե� m_values[]
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
