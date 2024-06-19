#pragma once
#include "Socket.h"
#include "Epoll.h"
#include "ThreadPool.h"
#include "Process.h"

template<typename _FUNCTION_, typename... _ARGS_> // 连接回调函数模版类
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

template<typename _FUNCTION_, typename... _ARGS_> // 接收回调函数模版类
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

class CBusiness // 客户端处理的接口类
{
public:
	// 1.不做具体的业务实现，但是业务实现需要继承该类
	// 2.接口层最大的作用就是解耦！这里就通过接口层解耦了网络通信(主模块)和业务实现，在主模块中不需要写任何业务实现的具体逻辑
	//   具体的业务实现(客户端处理)通过继承该接口类去做实现
	CBusiness() 
		: m_connectedcallback(NULL), m_recvcallback(NULL) 
	{}
	virtual int BusinessProcess(CProcess* proc) = 0; // 纯虚函数，由派生出的业务处理模块去实现
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
	CFunctionBase* m_connectedcallback; // 连接完成后的回调函数
	CFunctionBase* m_recvcallback; // 收到网络用户请求的回调函数
};

class CServer // 服务器类 接收网络上请求连接的客户端
{
public:
	CServer();
	~CServer() { Close(); }
	CServer(const CServer&) = delete;
	CServer& operator=(const CServer&) = delete;
public:
	// 1.设置子进程(业务处理进程)入口函数 2.创建子进程 3.开启线程池 4.创建epoll 
	// 5.创建监听套接字 6.监听套接字挂树 7.给线程池派发任务
	int Init(CBusiness* business, const Buffer& ip = "0.0.0.0", short port = 9999);
	int Run(); // 只要监听套接字还在 就工作
	int Close();
private:
	// 1.多线程共享epoll树根 2.epoll_wait()，返回的事件只有accept()客户端的connect()
	// 3.accept()后得到与客户端的通信套接字对象 4.将通信套接字发送给子进程(业务处理) 
	int ThreadFunc(); // 主模块要干的事情
private:
	CThreadPool m_pool; // 线程池
	CSocketBase* m_server; // 主模块的网络监听套接字对象指针
	CEpoll m_epoll; // 主模块的epoll
	CProcess m_process; // 子进程
	CBusiness* m_business; // 业务模块 需要我们手动delete
	// 主模块中的通信双方：服务器->主模块  客户端->网络用户
	// 主模块的任务：1.创建客户端业务处理进程 2.接收网络客户端的连接请求(多线程+epoll) 3.将accept上来的通信套接字全部发送给业务处理进程(进程间通信->传递文件描述符)
	//	  对应需求： 1.进程类               2.线程池类 epoll类                     3.网络套接字类 进程类中封装的SendFD()
	// 主模块根本不处理客户端的收发，只负责连接客户端，得到通信套接字fd，然后交由子进程(业务)处理
};

