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
	//  ����BusinessProcess�����һ����Ա������Ҫ����һ��������thisָ�룬�������������麯������ʲô������Ķ���ͻ�ִ����������߼������ﻹ�Ƕ�̬
	ret = m_process.SetEntryFunction(&CBusiness::BusinessProcess, m_business, &m_process); // �����ӽ��̵Ľ�����ں���
	if (ret != 0) return -2;
	ret = m_process.CreateSubProcess(); // �����ӽ���
	if (ret != 0) return -3;
	ret = m_pool.Start(2); // �����̿����̳߳�
	if (ret != 0) return -4;
	ret = m_epoll.Create(2); // �����̵�epoll
	if (ret != 0) return -5;
	m_server = new CSocket(); // �����̵ļ����׽���(����)
	if (m_server == NULL) return -6;
	ret = m_server->Init(CSockParam(ip, port, SOCK_ISSERVER | SOCK_ISIP | SOCK_ISREUSE)); // �����̼����׽��ֳ�ʼ��[(socket() bind() listen()]
	if (ret != 0) return -7;
	ret = m_epoll.Add(*m_server, (EpollData)((void*)m_server)); // �����̼����׽��ֹ�epoll��
	if (ret != 0) return -8;
	for (size_t i = 0; i < m_pool.Size(); i++) { // �����̸��̳߳ط�������
		ret = m_pool.AddTask(&CServer::ThreadFunc, this); 
		if (ret != 0) return -9;
	}
	return 0;
}

int CServer::Run()
{
	while (m_server != NULL) { // ֻҪ�����׽��ֻ��� �͹���
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
				} // ��ģ�����������ͻ��˵��շ���ֻ�������ӿͻ��ˣ��õ�ͨ���׽���fd��Ȼ�����ӽ��̴���
				else if (events[i].events & EPOLLIN) { // �����ɶ��¼� EPOLLOUT->��д EPOLLRDHUP->�Զ˹ر����� EPOLLET->��Ե����
					if (m_server) { // ��ģ������׽��ֻ����ڣ����пɶ��¼���˵���пͻ�������������
						CSocketBase* pClient = NULL; // ����ͨ���׽��ֶ���
						ret = m_server->Link(&pClient); // accept ͬʱ��ʼ����ͨ���׽��ֶ���(����ָ�����)
						if (ret != 0) continue;
						ret = m_process.SendSocket(*pClient, *pClient); // ��ͨ���׽��ּ����ַ���͸��ӽ��� ��ģ��ֻ���������ӣ�������ͨ��һ�Ų���
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
	TRACEI("��������ֹ");
	return 0;
}