#pragma once
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <fcntl.h>
#include "Public.h"

enum SockAttr { // 套接字属性
	SOCK_ISSERVER = 1,  // 是否为服务器，0-客户端 1-服务器					  0001
	SOCK_ISNOBLOCK = 2, // 是否阻塞	    0-阻塞   1-非阻塞				  0010
	SOCK_ISUDP = 4,     // 是否为UDP，   0-tcp   1-udp					  0100
	SOCK_ISIP = 8,      // 是否为IP协议  0-本地套接字 1-IP协议(网络套接字)   1000   如果这时候attr=1111(二进制)， 通过判断按位与(&)就知道是哪些属性的组合
	SOCK_ISREUSE = 16   // 是否重用地址
};

class CSockParam // 套接字初始化参数类
{
public:
	CSockParam() {
		bzero(&addr_in, sizeof(addr_in));
		bzero(&addr_un, sizeof(addr_un));
		port = -1;
		attr = 0; // 默认是 客户端、阻塞、tcp、本地套接字、不重用地址
	}
	CSockParam(const Buffer& ip, short port, int attr) { // 构造时传入ip和port，说明是网络套接字通信
		this->ip = ip;
		this->port = port;
		this->attr = attr;
		addr_in.sin_family = AF_INET;
		addr_in.sin_port = htons(port); // 主机字节序转网络字节序 `s`---short
		addr_in.sin_addr.s_addr = inet_addr(ip); // Buffer类中实现了const char*()const 重载
	}
	CSockParam(const sockaddr_in* addrin, int attr) {
		this->ip = ip;
		this->port = port;
		this->attr = attr;
		memcpy(&addr_in, addrin, sizeof(addr_in));
	}
	CSockParam(const Buffer& path, int attr) { // 构造时传入路径，说明是本地套接字通信
		ip = path;
		addr_un.sun_family = AF_UNIX;
		strcpy(addr_un.sun_path, path);
		this->attr = attr;
	}
	~CSockParam(){}
	CSockParam(const CSockParam& param) { // 拷贝构造
		ip = param.ip;
		port = param.port;
		attr = param.attr;
		memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
		memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
	}
	CSockParam& operator=(const CSockParam& param) { // 重载赋值运算符
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
	sockaddr* addrin() { return (sockaddr*)&addr_in; } // 返回网络套接字地址结构体，用于后续操作如填参
	sockaddr* addrun() { return (sockaddr*)&addr_un; } // 返回本地套接字地址结构体，用于后续操作如填参
public:
	sockaddr_in addr_in; // 网络套接字地址结构体
	sockaddr_un addr_un; // 本地套接字地址结构体
	Buffer ip; // ip
	short port; // 端口
	int attr; // 参考SockAttr 套接字属性
};

class CSocketBase // 套接字基类 是一个抽象类 被本地套接字类和网络套接字类继承
{ // 带有纯虚函数的类就是抽象类，抽象类无法实例化对象，其子类必须对所有纯虚函数做具体实现
public:
	CSocketBase() {
		m_socket = -1;
		m_status = 0; // 初始化未完成
	}
	virtual ~CSocketBase() {
		//Close();
	}
public:
	// 封装socket后抽象出的接口：Init() Link() Send() Recv() Close()
	// Init():服务器-套接字创建、bind、listen 客户端-套接字创建
	virtual int Init(const CSockParam& param) = 0; // 纯虚函数 子类必须实现
	// Link():服务器-accept 客户端-connect 对于udp，这里可以忽略
	// 二级指针(**)是为了能让一级指针(*)的值修改后能传出，这里是出参，又因为是个抽象类，没有实例，故不能通过引用传参，所以用二级指针
	virtual int Link(CSocketBase** pClient = NULL) = 0; // 对于服务端，Link时会接收到一个客户端， 客户端无需这个参数 故默认null
	// Send()
	virtual int Send(const Buffer& data) = 0;
	// Recv()
	virtual int Recv(Buffer& data) = 0;
	// Close()
	virtual int Close() { 
		m_status = 3; 
		if (m_socket != -1) {
			if ((m_param.attr & SOCK_ISSERVER) && // 服务器
				(m_param.attr & SOCK_ISIP) == 0)  // 非IP(即非网络套接字通信的服务器)
				unlink(m_param.ip); // 客户端结束 不能析构这个路径 否则别的客户端Link(connect)就没了
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

protected: // 只让子类继承 不让外部访问 故为protected
	int m_socket; // 套接字fd，默认-1（在服务器端是监听套接字）
	int m_status; // 套接字当前状态，0-初始化未完成 1-初始化完成 2-连接完成 3-已关闭
	CSockParam m_param; // 初始化套接字参数
};

// 本地套接字类
/* 
class CLocalSocket : public CSocketBase // 本地套接字类
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
	// Init():服务器-套接字创建、bind、listen 客户端-套接字创建
	virtual int Init(const CSockParam& param) {
		if (m_status != 0) return -1; // 当前套接字不在未初始化状态
		m_param = param;
		int type = (m_param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM; // 若参数属性为UDP，则设置对应的类型
		if (m_socket == -1) 
			m_socket = socket(PF_LOCAL, type, 0); // 创建本地套接字
		else
			m_status = 2;//accept来的套接字，已经处于连接状态
		if (m_socket == -1) return -2; // 套接字创建失败
		// 至此，完成了套接字的创建，接下来判断是否为服务器，是的话还需要 bind listen
		int ret = 0;
		if (m_param.attr & SOCK_ISSERVER) { // 如果是服务器
			ret = bind(m_socket, m_param.addrun(), sizeof(sockaddr_un)); // para1:要绑定的套接字fd para2:要绑定的地址和端口信息(一个地址结构体*)
			if (ret == -1) return -3; // bind失败
			ret = listen(m_socket, 32);
			if (ret == -1) return -4; // listen失败
		}
		if (m_param.attr & SOCK_ISNOBLOCK) { // 参数属性为非阻塞（即轮询）
			int option = fcntl(m_socket, F_GETFL); // 获取套接字状态标志
			if (option == -1) return -5; // 获取状态标志失败
			option |= O_NONBLOCK; // 位或操作，将 O_NONBLOCK 标志位设置为 1，而保留其他标志位不变，即设置非阻塞模式
			ret = fcntl(m_socket, F_SETFL, option); // 更新套接字状态标志
			if (ret == -1) return -6; // 设置非阻塞失败
		}
		if (m_status == 0)
			m_status = 1; // 初始化完成
		return 0;
	}
	// Link():服务器-accept 客户端-connect 对于udp，这里可以忽略
	virtual int Link(CSocketBase** pClient = NULL) { // 如果是服务器的话，通过出参(**)来返回接收到并初始化后的客户端socket(通信套接字)对象
		if ((m_socket == -1) || m_status <= 0) return -1; // 套接字未创建或初始化未完成
		int ret = 0;
		if (m_param.attr & SOCK_ISSERVER) { // 如果是服务器
			if (pClient == NULL) return -2; // 没有提供连接到的客户端的出参，连接失败
			CSockParam param; // 默认 客户端、阻塞、tcp 用来给连接上的客户端初始化
			socklen_t len = sizeof(sockaddr_un);
			int fd = accept(m_socket, param.addrun(), &len); // 返回通信套接字 同时将请求连接的客户端的地址信息填到param中
			if (fd == -1) return -3; // 连接失败
			*pClient = new CLocalSocket(fd); // 连上来的客户端套接字对象指针 
			if (*pClient == NULL) return -4;
			ret = (*pClient)->Init(param); // 客户端初始化 由于是二级指针传参，会作为出参返回连接上来的客户端对象
			if (ret != 0) { // 客户端初始化失败
				delete(*pClient);
				*pClient = NULL;
				return -5;
			}
		}
		else { // 如果是客户端
			ret = connect(m_socket, m_param.addrun(), sizeof(sockaddr_un)); // para2是服务器的地址信息，故客户端初始化的时候，给其的param其实是服务器的信息
			if (ret != 0) return -6; // Link()失败
		}	
		m_status = 2; // 连接已完成
		return 0;
	}
	// Send()
	virtual int Send(const Buffer& data) { // 这里的参数是指定要发送的数据 即发送缓冲区
		//printf("%s(%d):[%s]data=%s \n", __FILE__, __LINE__, __FUNCTION__, (char*)data);
		if ((m_socket == -1) || m_status < 2) return -1; // 套接字未创建或未初始化 为什么不是<=2,因为如果建立udp连接没有Link 状态还是1 
		ssize_t index = 0; // 已发送的数据
		while (index < (ssize_t)data.size()) {
			ssize_t len = write(m_socket, (char*)data + index, data.size() - index); // 返回写入buffer的长度
			if (len == 0) return -2;
			if (len < 0) return -3;
			index += len;
		}
		return 0;
	}
	// Recv()
	virtual int Recv(Buffer& data) { // 出参，读入的数据通过参数返回 这里的参数用来读数据 即接收缓冲区
		if ((m_socket == -1) || m_status < 2) return -1; // 套接字未创建或未初始化
		ssize_t len = read(m_socket, data, data.size());
		if (len > 0) { // 读了len个字节大小
			data.resize(len); // 通过出参只返回读入的数据
			return (int)len; // 返回收到的数据大小
		}
		if (len < 0) {
			if (errno == EINTR || errno == EAGAIN) { // read()返回是因为被中断，或非阻塞(即轮循EAGAIN，内核buffer没就绪会直接返回)，不能算接收失败
				data.clear(); 
				return 0; // 没有收到数据 但不算失败
			}
			return -2; // 发生错误
		}
		return -3; // 套接字被关闭
	}
	// 关闭连接
	virtual	int Close() {
		return CSocketBase::Close();
	}
};
*/

class CSocket : public CSocketBase // 兼容本地套接字与网络套接字类
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
	// Init():服务器-套接字创建、bind、listen 客户端-套接字创建
	virtual int Init(const CSockParam& param) {
		if (m_status != 0) return -1; // 当前套接字不在未初始化状态
		m_param = param; // 位掩码，利用二进制的独立性，使一个整数能同时表示多种状态或属性的组合
		int type = (m_param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM; // 若参数属性为UDP，则设置对应的类型
		if (m_socket == -1) {
			if (param.attr & SOCK_ISIP) // 如果是IP协议
				m_socket = socket(PF_INET, type, 0);  // 创建网络套接字
			else
				m_socket = socket(PF_LOCAL, type, 0); // 创建本地套接字
		} 
		else
			m_status = 2;//accept来的套接字，已经处于连接状态
		if (m_socket == -1) return -2; // 套接字创建失败
		// 至此，完成了套接字的创建，接下来判断是否为服务器，是的话还需要 bind listen
		int ret = 0;
		if (m_param.attr & SOCK_ISREUSE) {
			int option = 1;
			ret = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
			if (ret == -1) return -7;
		}
		if (m_param.attr & SOCK_ISSERVER) { // 如果是服务器
			if (param.attr & SOCK_ISIP) // 如果是网络套接字，那就绑定网络套接字的地址结构体
				ret = bind(m_socket, m_param.addrin(), sizeof(sockaddr_in)); // para1:要绑定的套接字fd para2:要绑定的地址和端口信息(一个地址结构体*)
			else
				ret = bind(m_socket, m_param.addrun(), sizeof(sockaddr_un)); // para1:要绑定的套接字fd para2:要绑定的地址和端口信息(一个地址结构体*)
			if (ret == -1) return -3; // bind失败
			ret = listen(m_socket, 32);
			if (ret == -1) return -4; // listen失败
		}
		if (m_param.attr & SOCK_ISNOBLOCK) { // 参数属性为非阻塞（即轮询）
			int option = fcntl(m_socket, F_GETFL); // 获取套接字状态标志
			if (option == -1) return -5; // 获取状态标志失败
			option |= O_NONBLOCK; // 位或操作，将 O_NONBLOCK 标志位设置为 1，而保留其他标志位不变，即设置非阻塞模式
			ret = fcntl(m_socket, F_SETFL, option); // 更新套接字状态标志
			if (ret == -1) return -6; // 设置非阻塞失败
		}
		if (m_status == 0)
			m_status = 1; // 初始化完成
		return 0;
	}
	// Link():服务器-accept 客户端-connect 对于udp，这里可以忽略
	virtual int Link(CSocketBase** pClient = NULL) { // 如果是服务器的话，通过出参(**)来返回接收到并初始化后的客户端socket(通信套接字)对象
		if ((m_socket == -1) || m_status <= 0) return -1; // 套接字未创建或初始化未完成
		int ret = 0;
		if (m_param.attr & SOCK_ISSERVER) { // 如果是服务器
			if (pClient == NULL) return -2; // 没有提供连接到的客户端的出参，连接失败
			CSockParam param; // 默认 客户端、阻塞、tcp 用来给连接上的客户端初始化
			int fd = -1;
			socklen_t len = 0;
			if (m_param.attr & SOCK_ISIP) {
				param.attr |= SOCK_ISIP; // 给服务器的通信套接字设置属性
				len = sizeof(sockaddr_in);
				fd = accept(m_socket, param.addrin(), &len); // 返回通信套接字 同时将请求连接的客户端的地址信息填到param中
			}
			else {
				len = sizeof(sockaddr_un);
				fd = accept(m_socket, param.addrun(), &len); // 返回通信套接字 同时将请求连接的客户端的地址信息填到param中
			}
			if (fd == -1) return -3; // 连接失败
			*pClient = new CSocket(fd); // 与客户端通信的套接字对象指针
			if (*pClient == NULL) return -4;
			ret = (*pClient)->Init(param); // 客户端套接字初始化 由于是二级指针传参，会作为出参返回 服务器那边就可以得到一个设置好的与客户端通信的套接字对象
			if (ret != 0) { // 客户端初始化失败
				delete(*pClient);
				*pClient = NULL;
				return -5;
			}
		}
		else { // 如果是客户端
			if (m_param.attr & SOCK_ISIP)
				ret = connect(m_socket, m_param.addrin(), sizeof(sockaddr_in)); // para2是服务器的地址信息，故客户端初始化的时候，给其的param其实是服务器的信息
			else 
				ret = connect(m_socket, m_param.addrun(), sizeof(sockaddr_un)); // para2是服务器的地址信息，故客户端初始化的时候，给其的param其实是服务器的信息
			if (ret != 0) return -6; // Link()失败
		}
		m_status = 2; // 连接已完成
		return 0;
	}
	// Send()
	virtual int Send(const Buffer& data) { // 这里的参数是指定要发送的数据 即发送缓冲区
		if ((m_socket == -1) || m_status < 2) return -1; // 套接字未创建或未初始化 为什么不是<=2,因为如果建立udp连接没有Link 状态还是1 
		ssize_t index = 0; // 已发送的数据
		while (index < (ssize_t)data.size()) {
			ssize_t len = write(m_socket, (char*)data + index, data.size() - index); // 返回写入buffer的长度
			if (len == 0) return -2;
			if (len < 0) return -3;
			index += len;
		}
		return 0;
	}
	// Recv()
	virtual int Recv(Buffer& data) { // 出参，读入的数据通过参数返回 这里的参数用来读数据 即接收缓冲区
		if ((m_socket == -1) || m_status < 2) return -1; // 套接字未创建或未初始化
		data.resize(1024 * 1024); // 给缓冲区大小
		ssize_t len = read(m_socket, data, data.size());
		if (len > 0) { // 读了len个字节大小
			data.resize(len); // 通过出参只返回读入的数据
			return (int)len; // 返回收到的数据大小
		}
		data.clear();
		if (len < 0) {
			if (errno == EINTR || errno == EAGAIN) { // read()返回是因为被中断，或非阻塞(即轮循EAGAIN 表示当前没有数据可读，内核buffer没就绪会直接返回)，不能算接收失败
				data.clear();
				return 0; // 没有收到数据 但不算失败
			}
			return -2; // 发生错误
		}
		return -3; // 套接字被关闭
	}
	// 关闭连接
	virtual	int Close() {
		return CSocketBase::Close();
	}
};