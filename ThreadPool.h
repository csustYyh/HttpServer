#pragma once
#include "Epoll.h"
#include "Thread.h"
#include "Function.h"
#include "Socket.h"
#include <memory>

class CThreadPool
{
public:
	CThreadPool() {
		m_server = NULL;
		timespec tp = { 0, 0 };
		clock_gettime(CLOCK_REALTIME, &tp); // ��ȡϵͳ��ǰʱ�� 
		char* buf = NULL;  // tp.tv_sec->�� Epoch��1970��1��1�գ����������� tv_nsec->��ǰ���е�������
		asprintf(&buf, "%d.%d.sock", tp.tv_sec % 100000, tp.tv_nsec % 1000000); // ����·����
		if (buf != NULL) { // ���ɵ�·������ֵ�� path
			m_path = buf;
			free(buf);
		} // ������Ļ�����start�ӿ������ж�m_path���������
		usleep(1);
	}
	~CThreadPool() {
		Close();
	}
	CThreadPool(const CThreadPool&) = delete;
	CThreadPool operator=(const CThreadPool&) = delete;
	// 1.��������ʼ�����ؼ����׽��ֶ��� 2.����epoll����(eventpoll�ṹ wq-�ȴ����� rbr-����� rdllist-������fd����)
	// 3 �����׽��ֹ��� 4.�����̲߳�ָ���̺߳���TaskDispatch() 4.�����߳�
	int Start(unsigned count) { // �̳߳����̵߳ĸ�����Ҫ���̳߳ع���ʱ��ָ��
		int ret = 0;
		if (m_server != NULL) return -1; // �Ѿ���ʼ����
		if (m_path.size() == 0) return -2; // ���캯��ʧ��
		m_server = new CSocket(); // �����׽���
		if (m_server == NULL) return -3;
		ret = m_server->Init(CSockParam(m_path, SOCK_ISSERVER)); // �����׽��ֳ�ʼ�� socket()->bind()->listen()
		if (ret != 0) return -4;
		ret = m_epoll.Create(count); // ����epoll����
		if (ret != 0) return -5;
		// *m_server->ȡ���׽��ֶ���->Add()��һ����������һ��int����ʽ����ת�����õ��ľ���fd
		ret = m_epoll.Add(*m_server, EpollData((void*)m_server)); // �����׽��ֹ���
		if (ret != 0) return -6;
		m_threads.resize(count); // ���ݲ����޸��߳�vector����
		for (unsigned i = 0; i < count; i++) {
			m_threads[i] = new CThread(&CThreadPool::TaskDispatch, this); // �����߳�ʱָ���̺߳���������thisָ��
			if (m_threads[i] == NULL) return -7;
			ret = m_threads[i]->Start(); // �����߳�
			if (ret != 0) return -8;
		}
		return 0;
	}
	// 1.�ر�epoll 2.�رռ����׽��� 3.�ر��̳߳��������߳� 4.ɾ���ļ�·��(�����׽���ͨ���з�����ip��ʵ�����ļ�)
	void Close() {
		m_epoll.Close();
		if (m_server) {
			CSocketBase* p = m_server;
			m_server = NULL;
			delete p;
		}
		for (auto thread : m_threads)
		{
			if (thread) delete thread;
		}
		m_threads.clear();
		unlink(m_path);
	}
	template<typename _FUNCTION_, typename... _ARGS_>
	// �������е��øú���: 1.����one thread one client ������(���̳߳ط�����) 2.ͨ�������׽��ֽ������͸��̳߳�
	int AddTask(_FUNCTION_ func, _ARGS_... args) {
		static thread_local CSocket client; // ÿ���߳�ӵ��һ�������׽��ֶ���(�߳���Ϊ�ͻ���)
		int ret = 0; // ��̬�ĵ�ÿ���߳�(static thread_local)������������ṹ��һ���µ� client ��һ���̶߳�Ӧһ���ͻ���
		if (client == -1) { // ��ǰ����AddTask()���̻߳�û�г�ʼ��
			ret = client.Init(CSockParam(m_path, 0)); // ��ʼ���ͻ����׽��� ��ʱ�������ĵ�ַ�Ѿ���m_path����
			if (ret != 0) return -1;
			ret = client.Link(); // �ͻ��˵�Link() �� connect() �����ͻ���(��Ҫ���̳߳��������Ľ���/�߳�)���̳߳ص�����
			if (ret != 0) return -2;
		}
		CFunctionBase* base = new CFunction<_FUNCTION_, _ARGS_...>(func, args...); // ������
		if (base == NULL) return -3;
		Buffer data(sizeof(base));
		memcpy(data, &base, sizeof(base));
		ret = client.Send(data); // ������������ָ�뷢��������
		if (ret != 0) {
			delete base;
			return -4;
		}
		return 0;
	}
	size_t Size()const { return m_threads.size(); } // �����̳߳��е��̸߳���
private:
	// ����epoll���� -> һ��wait(����epoll�ĵȴ�������) -> ˭����(������)�¼�(m_server�ϵĶ�->���ӿͻ��� pClient�ϵĶ�->ִ��������)˭�͸�(������)
	int TaskDispatch() {
		while (m_epoll != -1) { // ������epoll����
			EPEvents events; // epoll_event �ṹ�壨{event, data}event->ϣ���������¼� data->�Զ����������vector ��������epoll_wait()����
			int ret = 0;
			//esize=�����¼�������events=���δ洢�������¼�
			ssize_t esize = m_epoll.waitEvents(events); //epoll_event {event, data} event->ע��ʱϣ���������¼�����������wait�����ľ��Ƿ������¼� data->�Զ������
			if (esize > 0) {
				for (ssize_t i = 0; i < esize; i++) {
					if (events[i].events & EPOLLIN) { // ���¼�
						CSocketBase* pClient = NULL;
						if (events[i].data.ptr == m_server) { // �����׽����ϵĶ��¼���˵���пͻ�������(����AddTask()�Ľ���/�̼߳��ͻ���)
							ret = m_server->Link(&pClient); // ͨ������ָ�뷵�����Ӻ��ͨ���׽���(������������+ͨ��)����ָ��
							if (ret != 0) continue;
							ret = m_epoll.Add(*pClient, EpollData((void*)pClient)); // ͨ���׽��ֹ���(.Add()para3Ĭ��Ϊ�ɶ��¼�)
							if (ret != 0) {
								delete pClient;
								continue;
							}
						}
						else { // �ͻ����׽����ϵĶ��¼�(AddTask)
							pClient = (CSocketBase*)events[i].data.ptr;
							if (pClient) {
								CFunctionBase* base = NULL;
								Buffer data(sizeof(base));
								ret = pClient->Recv(data);
								if (ret <= 0) {
									m_epoll.Del(*pClient); // ��ͨ���׽��ִ�����ժ��
									delete pClient;
									continue;
								}
								memcpy(&base, (char*)data, sizeof(base));
								if (base != NULL) {
									(*base)(); // ���������� base->��������ָ�� *base->�ɵ��ö���
									delete base;
								}
							}
						}
					}
				}
			}
		}
		return 0;
	}
private:
	CEpoll m_epoll; // epoll����I/O�¼�
	std::vector<CThread*> m_threads; // �̳߳��е��߳�ָ��vector
	// std::vector<std::shared_ptr<CThread>> m_threads; // �̳߳��е��߳�ָ��vector
	CSocketBase* m_server; // �̳߳ر��ؼ����׽���
	Buffer m_path; // �����������׽���ͨ�ŵ�ַ(�䱾��Ҳ�Ǹ������׽���)
	// 1.���̳߳��е�ͨ��˫����������->�̳߳أ��ͻ���->��Ҫ���̷߳����������������/�߳�
	//   ������->����ͻ��˷��������� �ͻ���->��Ҫִ�е�����(���Ƿ�װ�ĺ��������ָ��)���͸�������
	// 2.�̳߳��и����̸߳���ʲô��TaskDispatch()
	//   ����epoll���� -> һ��wait -> ˭����(������)�¼�(m_server�ϵĶ�->���ӿͻ��� pClient�ϵĶ�->ִ��������)˭�͸�
	// 3.ΪʲôҪ��epoll + �����׽���ͨ�ţ�
	//   ����epoll+�׽���ͨ��->�ڷ���������һ���������¼�����->�����Ƕ��߳�ִ��->���漰�����߳�ͬʱ��д->Ҫ����->�鷳��Ч�ʵ�
	//   ��epoll+�׽���ͨ��->��������������->��������ϵͳ�����õ�������->֧�ָ߲���
};	
