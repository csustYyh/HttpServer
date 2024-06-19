#include "Server.h"
#include "Logger.h"

CServer::CServer()
{
	m_server = NULL;
	m_business = NULL;
}

int CServer::Init(CBusiness* business, const Buffer& ip, short port)
{
	int ret = 0;
	if (business == NULL) return -1;
	m_business = business;
	//  由于BusinessProcess是类的一个成员函数，要给他一个类对象的this指针，又由于这里是虚函数，传什么派生类的对象就会执行派生类的逻辑，这里还是多态
	ret = m_process.SetEntryFunction(&CBusiness::BusinessProcess, m_business, &m_process); // 设置子进程的进程入口函数
	if (ret != 0) return -2;
	ret = m_process.CreateSubProcess(); // 创建子进程
	if (ret != 0) return -3;
	ret = m_pool.Start(2); // 主进程开启线程池
	if (ret != 0) return -4;
	ret = m_epoll.Create(2); // 主进程的epoll
	if (ret != 0) return -5;
	m_server = new CSocket(); // 主进程的监听套接字(网络)
	if (m_server == NULL) return -6;
	ret = m_server->Init(CSockParam(ip, port, SOCK_ISSERVER | SOCK_ISIP | SOCK_ISREUSE)); // 主进程监听套接字初始化[(socket() bind() listen()]
	if (ret != 0) return -7;
	ret = m_epoll.Add(*m_server, (EpollData)((void*)m_server)); // 主进程监听套接字挂epoll树
	if (ret != 0) return -8;
	for (size_t i = 0; i < m_pool.Size(); i++) { // 主进程给线程池发送任务
		ret = m_pool.AddTask(&CServer::ThreadFunc, this); 
		if (ret != 0) return -9;
	}
	return 0;
}

int CServer::Run()
{
	while (m_server != NULL) { // 只要监听套接字还在 就工作
		usleep(10);
	}
	return 0;
}

int CServer::Close()
{
	if (m_server) {
		CSocketBase* sock = m_server;
		m_server = NULL;
		m_epoll.Del(*sock);
		delete sock;
	}
	m_epoll.Close();
	m_process.SendFD(-1);
	m_pool.Close();
	return 0;
}

int CServer::ThreadFunc()	
{
	TRACEI("epoll %d server %p", (int)m_epoll, m_server);
	int ret = 0;
	EPEvents events;
	while ((m_epoll != -1) && (m_server != NULL)) {
		ssize_t size = m_epoll.waitEvents(events, 500);
		if (size < 0) break;
		if (size > 0) {
			TRACEI("size=%d event %08X", size, events[0].events);
			for (ssize_t i = 0; i < size; i++)
			{
				if (events[i].events & EPOLLERR) {
					break;
				} // 主模块根本不处理客户端的收发，只负责连接客户端，得到通信套接字fd，然后交由子进程处理
				else if (events[i].events & EPOLLIN) { // 发生可读事件 EPOLLOUT->可写 EPOLLRDHUP->对端关闭连接 EPOLLET->边缘触发
					if (m_server) { // 主模块监听套接字还存在，且有可读事件，说明有客户端连接上来了
						CSocketBase* pClient = NULL; // 创建通信套接字对象
						ret = m_server->Link(&pClient); // accept 同时初始化了通信套接字对象(二级指针出参)
						if (ret != 0) continue;
						ret = m_process.SendSocket(*pClient, *pClient); // 将通信套接字及其地址发送给子进程 主模块只负责处理连接，后续的通信一概不管
						TRACEI("SendSocket %d", ret);
						int s = *pClient;
						delete pClient;
						if (ret != 0) {
							TRACEE("send client %d failed!", s);
							continue;
						}
					}
				}
			}
		}
	}
	TRACEI("服务器终止");
	return 0;
}