#pragma once
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <fcntl.h>
#include "Public.h"

enum SockAttr { // �׽�������
	SOCK_ISSERVER = 1,  // �Ƿ�Ϊ��������0-�ͻ��� 1-������					  0001
	SOCK_ISNOBLOCK = 2, // �Ƿ�����	    0-����   1-������				  0010
	SOCK_ISUDP = 4,     // �Ƿ�ΪUDP��   0-tcp   1-udp					  0100
	SOCK_ISIP = 8,      // �Ƿ�ΪIPЭ��  0-�����׽��� 1-IPЭ��(�����׽���)   1000   �����ʱ��attr=1111(������)�� ͨ���жϰ�λ��(&)��֪������Щ���Ե����
	SOCK_ISREUSE = 16   // �Ƿ����õ�ַ
};

class CSockParam // �׽��ֳ�ʼ��������
{
public:
	CSockParam() {
		bzero(&addr_in, sizeof(addr_in));
		bzero(&addr_un, sizeof(addr_un));
		port = -1;
		attr = 0; // Ĭ���� �ͻ��ˡ�������tcp�������׽��֡������õ�ַ
	}
	CSockParam(const Buffer& ip, short port, int attr) { // ����ʱ����ip��port��˵���������׽���ͨ��
		this->ip = ip;
		this->port = port;
		this->attr = attr;
		addr_in.sin_family = AF_INET;
		addr_in.sin_port = htons(port); // �����ֽ���ת�����ֽ��� `s`---short
		addr_in.sin_addr.s_addr = inet_addr(ip); // Buffer����ʵ����const char*()const ����
	}
	CSockParam(const sockaddr_in* addrin, int attr) {
		this->ip = ip;
		this->port = port;
		this->attr = attr;
		memcpy(&addr_in, addrin, sizeof(addr_in));
	}
	CSockParam(const Buffer& path, int attr) { // ����ʱ����·����˵���Ǳ����׽���ͨ��
		ip = path;
		addr_un.sun_family = AF_UNIX;
		strcpy(addr_un.sun_path, path);
		this->attr = attr;
	}
	~CSockParam(){}
	CSockParam(const CSockParam& param) { // ��������
		ip = param.ip;
		port = param.port;
		attr = param.attr;
		memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
		memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
	}
	CSockParam& operator=(const CSockParam& param) { // ���ظ�ֵ�����
		if (this != &param)
		{
			ip = param.ip;
			port = param.port;
			attr = param.attr;
			memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
			memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
		}
		return *this;
	}
	sockaddr* addrin() { return (sockaddr*)&addr_in; } // ���������׽��ֵ�ַ�ṹ�壬���ں������������
	sockaddr* addrun() { return (sockaddr*)&addr_un; } // ���ر����׽��ֵ�ַ�ṹ�壬���ں������������
public:
	sockaddr_in addr_in; // �����׽��ֵ�ַ�ṹ��
	sockaddr_un addr_un; // �����׽��ֵ�ַ�ṹ��
	Buffer ip; // ip
	short port; // �˿�
	int attr; // �ο�SockAttr �׽�������
};

class CSocketBase // �׽��ֻ��� ��һ�������� �������׽�����������׽�����̳�
{ // ���д��麯��������ǳ����࣬�������޷�ʵ���������������������д��麯��������ʵ��
public:
	CSocketBase() {
		m_socket = -1;
		m_status = 0; // ��ʼ��δ���
	}
	virtual ~CSocketBase() {
		//Close();
	}
public:
	// ��װsocket�������Ľӿڣ�Init() Link() Send() Recv() Close()
	// Init():������-�׽��ִ�����bind��listen �ͻ���-�׽��ִ���
	virtual int Init(const CSockParam& param) = 0; // ���麯�� �������ʵ��
	// Link():������-accept �ͻ���-connect ����udp��������Ժ���
	// ����ָ��(**)��Ϊ������һ��ָ��(*)��ֵ�޸ĺ��ܴ����������ǳ��Σ�����Ϊ�Ǹ������࣬û��ʵ�����ʲ���ͨ�����ô��Σ������ö���ָ��
	virtual int Link(CSocketBase** pClient = NULL) = 0; // ���ڷ���ˣ�Linkʱ����յ�һ���ͻ��ˣ� �ͻ�������������� ��Ĭ��null
	// Send()
	virtual int Send(const Buffer& data) = 0;
	// Recv()
	virtual int Recv(Buffer& data) = 0;
	// Close()
	virtual int Close() { 
		m_status = 3; 
		if (m_socket != -1) {
			if ((m_param.attr & SOCK_ISSERVER) && // ������
				(m_param.attr & SOCK_ISIP) == 0)  // ��IP(���������׽���ͨ�ŵķ�����)
				unlink(m_param.ip); // �ͻ��˽��� �����������·�� �����Ŀͻ���Link(connect)��û��
			int fd = m_socket;
			m_socket = -1;
			close(fd);
		}
		return 0;
	}
	virtual operator int() { return m_socket; }
	virtual operator int()const { return m_socket; }
	virtual operator const sockaddr_in* ()const { return &m_param.addr_in; }
	virtual operator sockaddr_in* () { return &m_param.addr_in; }

protected: // ֻ������̳� �����ⲿ���� ��Ϊprotected
	int m_socket; // �׽���fd��Ĭ��-1���ڷ��������Ǽ����׽��֣�
	int m_status; // �׽��ֵ�ǰ״̬��0-��ʼ��δ��� 1-��ʼ����� 2-������� 3-�ѹر�
	CSockParam m_param; // ��ʼ���׽��ֲ���
};

// �����׽�����
/* 
class CLocalSocket : public CSocketBase // �����׽�����
{
public:
	CLocalSocket() :CSocketBase() {}
	CLocalSocket(int sock) :CSocketBase() {
		m_socket = sock;
	}
	virtual ~CLocalSocket() {
		Close();
	}
public:
	// Init():������-�׽��ִ�����bind��listen �ͻ���-�׽��ִ���
	virtual int Init(const CSockParam& param) {
		if (m_status != 0) return -1; // ��ǰ�׽��ֲ���δ��ʼ��״̬
		m_param = param;
		int type = (m_param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM; // ����������ΪUDP�������ö�Ӧ������
		if (m_socket == -1) 
			m_socket = socket(PF_LOCAL, type, 0); // ���������׽���
		else
			m_status = 2;//accept�����׽��֣��Ѿ���������״̬
		if (m_socket == -1) return -2; // �׽��ִ���ʧ��
		// ���ˣ�������׽��ֵĴ������������ж��Ƿ�Ϊ���������ǵĻ�����Ҫ bind listen
		int ret = 0;
		if (m_param.attr & SOCK_ISSERVER) { // ����Ƿ�����
			ret = bind(m_socket, m_param.addrun(), sizeof(sockaddr_un)); // para1:Ҫ�󶨵��׽���fd para2:Ҫ�󶨵ĵ�ַ�Ͷ˿���Ϣ(һ����ַ�ṹ��*)
			if (ret == -1) return -3; // bindʧ��
			ret = listen(m_socket, 32);
			if (ret == -1) return -4; // listenʧ��
		}
		if (m_param.attr & SOCK_ISNOBLOCK) { // ��������Ϊ������������ѯ��
			int option = fcntl(m_socket, F_GETFL); // ��ȡ�׽���״̬��־
			if (option == -1) return -5; // ��ȡ״̬��־ʧ��
			option |= O_NONBLOCK; // λ��������� O_NONBLOCK ��־λ����Ϊ 1��������������־λ���䣬�����÷�����ģʽ
			ret = fcntl(m_socket, F_SETFL, option); // �����׽���״̬��־
			if (ret == -1) return -6; // ���÷�����ʧ��
		}
		if (m_status == 0)
			m_status = 1; // ��ʼ�����
		return 0;
	}
	// Link():������-accept �ͻ���-connect ����udp��������Ժ���
	virtual int Link(CSocketBase** pClient = NULL) { // ����Ƿ������Ļ���ͨ������(**)�����ؽ��յ�����ʼ����Ŀͻ���socket(ͨ���׽���)����
		if ((m_socket == -1) || m_status <= 0) return -1; // �׽���δ�������ʼ��δ���
		int ret = 0;
		if (m_param.attr & SOCK_ISSERVER) { // ����Ƿ�����
			if (pClient == NULL) return -2; // û���ṩ���ӵ��Ŀͻ��˵ĳ��Σ�����ʧ��
			CSockParam param; // Ĭ�� �ͻ��ˡ�������tcp �����������ϵĿͻ��˳�ʼ��
			socklen_t len = sizeof(sockaddr_un);
			int fd = accept(m_socket, param.addrun(), &len); // ����ͨ���׽��� ͬʱ���������ӵĿͻ��˵ĵ�ַ��Ϣ�param��
			if (fd == -1) return -3; // ����ʧ��
			*pClient = new CLocalSocket(fd); // �������Ŀͻ����׽��ֶ���ָ�� 
			if (*pClient == NULL) return -4;
			ret = (*pClient)->Init(param); // �ͻ��˳�ʼ�� �����Ƕ���ָ�봫�Σ�����Ϊ���η������������Ŀͻ��˶���
			if (ret != 0) { // �ͻ��˳�ʼ��ʧ��
				delete(*pClient);
				*pClient = NULL;
				return -5;
			}
		}
		else { // ����ǿͻ���
			ret = connect(m_socket, m_param.addrun(), sizeof(sockaddr_un)); // para2�Ƿ������ĵ�ַ��Ϣ���ʿͻ��˳�ʼ����ʱ�򣬸����param��ʵ�Ƿ���������Ϣ
			if (ret != 0) return -6; // Link()ʧ��
		}	
		m_status = 2; // ���������
		return 0;
	}
	// Send()
	virtual int Send(const Buffer& data) { // ����Ĳ�����ָ��Ҫ���͵����� �����ͻ�����
		//printf("%s(%d):[%s]data=%s \n", __FILE__, __LINE__, __FUNCTION__, (char*)data);
		if ((m_socket == -1) || m_status < 2) return -1; // �׽���δ������δ��ʼ�� Ϊʲô����<=2,��Ϊ�������udp����û��Link ״̬����1 
		ssize_t index = 0; // �ѷ��͵�����
		while (index < (ssize_t)data.size()) {
			ssize_t len = write(m_socket, (char*)data + index, data.size() - index); // ����д��buffer�ĳ���
			if (len == 0) return -2;
			if (len < 0) return -3;
			index += len;
		}
		return 0;
	}
	// Recv()
	virtual int Recv(Buffer& data) { // ���Σ����������ͨ���������� ����Ĳ������������� �����ջ�����
		if ((m_socket == -1) || m_status < 2) return -1; // �׽���δ������δ��ʼ��
		ssize_t len = read(m_socket, data, data.size());
		if (len > 0) { // ����len���ֽڴ�С
			data.resize(len); // ͨ������ֻ���ض��������
			return (int)len; // �����յ������ݴ�С
		}
		if (len < 0) {
			if (errno == EINTR || errno == EAGAIN) { // read()��������Ϊ���жϣ��������(����ѭEAGAIN���ں�bufferû������ֱ�ӷ���)�����������ʧ��
				data.clear(); 
				return 0; // û���յ����� ������ʧ��
			}
			return -2; // ��������
		}
		return -3; // �׽��ֱ��ر�
	}
	// �ر�����
	virtual	int Close() {
		return CSocketBase::Close();
	}
};
*/

class CSocket : public CSocketBase // ���ݱ����׽����������׽�����
{
public:
	CSocket() :CSocketBase() {}
	CSocket(int sock) :CSocketBase() {
		m_socket = sock;
	}
	virtual ~CSocket() {
		Close();
	}
public:
	// Init():������-�׽��ִ�����bind��listen �ͻ���-�׽��ִ���
	virtual int Init(const CSockParam& param) {
		if (m_status != 0) return -1; // ��ǰ�׽��ֲ���δ��ʼ��״̬
		m_param = param; // λ���룬���ö����ƵĶ����ԣ�ʹһ��������ͬʱ��ʾ����״̬�����Ե����
		int type = (m_param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM; // ����������ΪUDP�������ö�Ӧ������
		if (m_socket == -1) {
			if (param.attr & SOCK_ISIP) // �����IPЭ��
				m_socket = socket(PF_INET, type, 0);  // ���������׽���
			else
				m_socket = socket(PF_LOCAL, type, 0); // ���������׽���
		} 
		else
			m_status = 2;//accept�����׽��֣��Ѿ���������״̬
		if (m_socket == -1) return -2; // �׽��ִ���ʧ��
		// ���ˣ�������׽��ֵĴ������������ж��Ƿ�Ϊ���������ǵĻ�����Ҫ bind listen
		int ret = 0;
		if (m_param.attr & SOCK_ISREUSE) {
			int option = 1;
			ret = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
			if (ret == -1) return -7;
		}
		if (m_param.attr & SOCK_ISSERVER) { // ����Ƿ�����
			if (param.attr & SOCK_ISIP) // ����������׽��֣��ǾͰ������׽��ֵĵ�ַ�ṹ��
				ret = bind(m_socket, m_param.addrin(), sizeof(sockaddr_in)); // para1:Ҫ�󶨵��׽���fd para2:Ҫ�󶨵ĵ�ַ�Ͷ˿���Ϣ(һ����ַ�ṹ��*)
			else
				ret = bind(m_socket, m_param.addrun(), sizeof(sockaddr_un)); // para1:Ҫ�󶨵��׽���fd para2:Ҫ�󶨵ĵ�ַ�Ͷ˿���Ϣ(һ����ַ�ṹ��*)
			if (ret == -1) return -3; // bindʧ��
			ret = listen(m_socket, 32);
			if (ret == -1) return -4; // listenʧ��
		}
		if (m_param.attr & SOCK_ISNOBLOCK) { // ��������Ϊ������������ѯ��
			int option = fcntl(m_socket, F_GETFL); // ��ȡ�׽���״̬��־
			if (option == -1) return -5; // ��ȡ״̬��־ʧ��
			option |= O_NONBLOCK; // λ��������� O_NONBLOCK ��־λ����Ϊ 1��������������־λ���䣬�����÷�����ģʽ
			ret = fcntl(m_socket, F_SETFL, option); // �����׽���״̬��־
			if (ret == -1) return -6; // ���÷�����ʧ��
		}
		if (m_status == 0)
			m_status = 1; // ��ʼ�����
		return 0;
	}
	// Link():������-accept �ͻ���-connect ����udp��������Ժ���
	virtual int Link(CSocketBase** pClient = NULL) { // ����Ƿ������Ļ���ͨ������(**)�����ؽ��յ�����ʼ����Ŀͻ���socket(ͨ���׽���)����
		if ((m_socket == -1) || m_status <= 0) return -1; // �׽���δ�������ʼ��δ���
		int ret = 0;
		if (m_param.attr & SOCK_ISSERVER) { // ����Ƿ�����
			if (pClient == NULL) return -2; // û���ṩ���ӵ��Ŀͻ��˵ĳ��Σ�����ʧ��
			CSockParam param; // Ĭ�� �ͻ��ˡ�������tcp �����������ϵĿͻ��˳�ʼ��
			int fd = -1;
			socklen_t len = 0;
			if (m_param.attr & SOCK_ISIP) {
				param.attr |= SOCK_ISIP; // ����������ͨ���׽�����������
				len = sizeof(sockaddr_in);
				fd = accept(m_socket, param.addrin(), &len); // ����ͨ���׽��� ͬʱ���������ӵĿͻ��˵ĵ�ַ��Ϣ�param��
			}
			else {
				len = sizeof(sockaddr_un);
				fd = accept(m_socket, param.addrun(), &len); // ����ͨ���׽��� ͬʱ���������ӵĿͻ��˵ĵ�ַ��Ϣ�param��
			}
			if (fd == -1) return -3; // ����ʧ��
			*pClient = new CSocket(fd); // ��ͻ���ͨ�ŵ��׽��ֶ���ָ��
			if (*pClient == NULL) return -4;
			ret = (*pClient)->Init(param); // �ͻ����׽��ֳ�ʼ�� �����Ƕ���ָ�봫�Σ�����Ϊ���η��� �������Ǳ߾Ϳ��Եõ�һ�����úõ���ͻ���ͨ�ŵ��׽��ֶ���
			if (ret != 0) { // �ͻ��˳�ʼ��ʧ��
				delete(*pClient);
				*pClient = NULL;
				return -5;
			}
		}
		else { // ����ǿͻ���
			if (m_param.attr & SOCK_ISIP)
				ret = connect(m_socket, m_param.addrin(), sizeof(sockaddr_in)); // para2�Ƿ������ĵ�ַ��Ϣ���ʿͻ��˳�ʼ����ʱ�򣬸����param��ʵ�Ƿ���������Ϣ
			else 
				ret = connect(m_socket, m_param.addrun(), sizeof(sockaddr_un)); // para2�Ƿ������ĵ�ַ��Ϣ���ʿͻ��˳�ʼ����ʱ�򣬸����param��ʵ�Ƿ���������Ϣ
			if (ret != 0) return -6; // Link()ʧ��
		}
		m_status = 2; // ���������
		return 0;
	}
	// Send()
	virtual int Send(const Buffer& data) { // ����Ĳ�����ָ��Ҫ���͵����� �����ͻ�����
		if ((m_socket == -1) || m_status < 2) return -1; // �׽���δ������δ��ʼ�� Ϊʲô����<=2,��Ϊ�������udp����û��Link ״̬����1 
		ssize_t index = 0; // �ѷ��͵�����
		while (index < (ssize_t)data.size()) {
			ssize_t len = write(m_socket, (char*)data + index, data.size() - index); // ����д��buffer�ĳ���
			if (len == 0) return -2;
			if (len < 0) return -3;
			index += len;
		}
		return 0;
	}
	// Recv()
	virtual int Recv(Buffer& data) { // ���Σ����������ͨ���������� ����Ĳ������������� �����ջ�����
		if ((m_socket == -1) || m_status < 2) return -1; // �׽���δ������δ��ʼ��
		data.resize(1024 * 1024); // ����������С
		ssize_t len = read(m_socket, data, data.size());
		if (len > 0) { // ����len���ֽڴ�С
			data.resize(len); // ͨ������ֻ���ض��������
			return (int)len; // �����յ������ݴ�С
		}
		data.clear();
		if (len < 0) {
			if (errno == EINTR || errno == EAGAIN) { // read()��������Ϊ���жϣ��������(����ѭEAGAIN ��ʾ��ǰû�����ݿɶ����ں�bufferû������ֱ�ӷ���)�����������ʧ��
				data.clear();
				return 0; // û���յ����� ������ʧ��
			}
			return -2; // ��������
		}
		return -3; // �׽��ֱ��ر�
	}
	// �ر�����
	virtual	int Close() {
		return CSocketBase::Close();
	}
};