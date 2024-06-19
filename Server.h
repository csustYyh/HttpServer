#pragma once
#include "Socket.h"
#include "Epoll.h"
#include "ThreadPool.h"
#include "Process.h"

template<typename _FUNCTION_, typename... _ARGS_> // ���ӻص�����ģ����
class CConnectedFunction :public CFunctionBase
{
public:
	CConnectedFunction(_FUNCTION_ func, _ARGS_... args)
		:m_binder(std::bind(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...))
	{}
	virtual ~CConnectedFunction() {}
	virtual int operator()(CSocketBase* pClient) {
		return m_binder(pClient);
	}
	//typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;
	std::function<int(CSocketBase*)> m_binder;
};

template<typename _FUNCTION_, typename... _ARGS_> // ���ջص�����ģ����
class CReceivedFunction :public CFunctionBase
{
public:
	CReceivedFunction(_FUNCTION_ func, _ARGS_... args)
		:m_binder(std::bind(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...))
	{}
	virtual ~CReceivedFunction() {}
	virtual int operator()(CSocketBase* pClient, const Buffer& data) {
		return m_binder(pClient, data);
	}
	//typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;
	std::function<int(CSocketBase*, const Buffer&)> m_binder;
};

class CBusiness // �ͻ��˴���Ľӿ���
{
public:
	// 1.���������ҵ��ʵ�֣�����ҵ��ʵ����Ҫ�̳и���
	// 2.�ӿڲ��������þ��ǽ�������ͨ���ӿڲ����������ͨ��(��ģ��)��ҵ��ʵ�֣�����ģ���в���Ҫд�κ�ҵ��ʵ�ֵľ����߼�
	//   �����ҵ��ʵ��(�ͻ��˴���)ͨ���̳иýӿ���ȥ��ʵ��
	CBusiness() 
		: m_connectedcallback(NULL), m_recvcallback(NULL) 
	{}
	virtual int BusinessProcess(CProcess* proc) = 0; // ���麯��������������ҵ����ģ��ȥʵ��
	template<typename _FUNCTION_, typename... _ARGS_>
	int setConnectedCallback(_FUNCTION_ func, _ARGS_... args) {	
		m_connectedcallback = new CConnectedFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_connectedcallback == NULL) return -1;
		return 0;
	}
	template<typename _FUNCTION_, typename... _ARGS_>
	int setRecvCallback(_FUNCTION_ func, _ARGS_... args) {
		m_recvcallback = new CReceivedFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_recvcallback == NULL) return -1;
		return 0;
	}
protected:
	CFunctionBase* m_connectedcallback; // ������ɺ�Ļص�����
	CFunctionBase* m_recvcallback; // �յ������û�����Ļص�����
};

class CServer // �������� �����������������ӵĿͻ���
{
public:
	CServer();
	~CServer() { Close(); }
	CServer(const CServer&) = delete;
	CServer& operator=(const CServer&) = delete;
public:
	// 1.�����ӽ���(ҵ�������)��ں��� 2.�����ӽ��� 3.�����̳߳� 4.����epoll 
	// 5.���������׽��� 6.�����׽��ֹ��� 7.���̳߳��ɷ�����
	int Init(CBusiness* business, const Buffer& ip = "0.0.0.0", short port = 9999);
	int Run(); // ֻҪ�����׽��ֻ��� �͹���
	int Close();
private:
	// 1.���̹߳���epoll���� 2.epoll_wait()�����ص��¼�ֻ��accept()�ͻ��˵�connect()
	// 3.accept()��õ���ͻ��˵�ͨ���׽��ֶ��� 4.��ͨ���׽��ַ��͸��ӽ���(ҵ����) 
	int ThreadFunc(); // ��ģ��Ҫ�ɵ�����
private:
	CThreadPool m_pool; // �̳߳�
	CSocketBase* m_server; // ��ģ�����������׽��ֶ���ָ��
	CEpoll m_epoll; // ��ģ���epoll
	CProcess m_process; // �ӽ���
	CBusiness* m_business; // ҵ��ģ�� ��Ҫ�����ֶ�delete
	// ��ģ���е�ͨ��˫����������->��ģ��  �ͻ���->�����û�
	// ��ģ�������1.�����ͻ���ҵ������� 2.��������ͻ��˵���������(���߳�+epoll) 3.��accept������ͨ���׽���ȫ�����͸�ҵ�������(���̼�ͨ��->�����ļ�������)
	//	  ��Ӧ���� 1.������               2.�̳߳��� epoll��                     3.�����׽����� �������з�װ��SendFD()
	// ��ģ�����������ͻ��˵��շ���ֻ�������ӿͻ��ˣ��õ�ͨ���׽���fd��Ȼ�����ӽ���(ҵ��)����
};

