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
		clock_gettime(CLOCK_REALTIME, &tp); // 获取系统当前时间 
		char* buf = NULL;  // tp.tv_sec->自 Epoch（1970年1月1日）以来的秒数 tv_nsec->当前秒中的纳秒数
		asprintf(&buf, "%d.%d.sock", tp.tv_sec % 100000, tp.tv_nsec % 1000000); // 生成路径名
		if (buf != NULL) { // 生成的路径名赋值给 path
			m_path = buf;
			free(buf);
		} // 有问题的话，在start接口里面判断m_path来解决问题
		usleep(1);
	}
	~CThreadPool() {
		Close();
	}
	CThreadPool(const CThreadPool&) = delete;
	CThreadPool operator=(const CThreadPool&) = delete;
	// 1.创建并初始化本地监听套接字对象 2.创建epoll树根(eventpoll结构 wq-等待队列 rbr-红黑树 rdllist-就绪的fd链表)
	// 3 监听套接字挂树 4.创建线程并指定线程函数TaskDispatch() 4.启动线程
	int Start(unsigned count) { // 线程池中线程的个数需要在线程池构造时就指定
		int ret = 0;
		if (m_server != NULL) return -1; // 已经初始化了
		if (m_path.size() == 0) return -2; // 构造函数失败
		m_server = new CSocket(); // 本地套接字
		if (m_server == NULL) return -3;
		ret = m_server->Init(CSockParam(m_path, SOCK_ISSERVER)); // 监听套接字初始化 socket()->bind()->listen()
		if (ret != 0) return -4;
		ret = m_epoll.Create(count); // 创建epoll树根
		if (ret != 0) return -5;
		// *m_server->取到套接字对象->Add()第一个参数会有一个int的隐式类型转换，拿到的就是fd
		ret = m_epoll.Add(*m_server, EpollData((void*)m_server)); // 监听套接字挂树
		if (ret != 0) return -6;
		m_threads.resize(count); // 根据参数修改线程vector数量
		for (unsigned i = 0; i < count; i++) {
			m_threads[i] = new CThread(&CThreadPool::TaskDispatch, this); // 创建线程时指定线程函数，传递this指针
			if (m_threads[i] == NULL) return -7;
			ret = m_threads[i]->Start(); // 启动线程
			if (ret != 0) return -8;
		}
		return 0;
	}
	// 1.关闭epoll 2.关闭监听套接字 3.关闭线程池中所有线程 4.删除文件路径(本地套接字通信中服务器ip其实就是文件)
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
	// 主进程中调用该函数: 1.建立one thread one client 的连接(与线程池服务器) 2.通过本地套接字将任务发送给线程池
	int AddTask(_FUNCTION_ func, _ARGS_... args) {
		static thread_local CSocket client; // 每个线程拥有一个本地套接字对象(线程作为客户端)
		int ret = 0; // 静态的但每个线程(static thread_local)调这个函数都会构造一个新的 client 即一个线程对应一个客户端
		if (client == -1) { // 当前调用AddTask()的线程还没有初始化
			ret = client.Init(CSockParam(m_path, 0)); // 初始化客户端套接字 此时服务器的地址已经在m_path中了
			if (ret != 0) return -1;
			ret = client.Link(); // 客户端的Link() 即 connect() 建立客户端(想要给线程池添加任务的进程/线程)与线程池的连接
			if (ret != 0) return -2;
		}
		CFunctionBase* base = new CFunction<_FUNCTION_, _ARGS_...>(func, args...); // 任务函数
		if (base == NULL) return -3;
		Buffer data(sizeof(base));
		memcpy(data, &base, sizeof(base));
		ret = client.Send(data); // 将任务函数对象指针发给服务器
		if (ret != 0) {
			delete base;
			return -4;
		}
		return 0;
	}
	size_t Size()const { return m_threads.size(); } // 返回线程池中的线程个数
private:
	// 共享epoll树根 -> 一起wait(都在epoll的等待队列中) -> 谁抢到(竞争到)事件(m_server上的读->连接客户端 pClient上的读->执行任务函数)谁就干(被唤醒)
	int TaskDispatch() {
		while (m_epoll != -1) { // 创建了epoll树根
			EPEvents events; // epoll_event 结构体（{event, data}event->希望监听的事件 data->自定义参数）的vector 用来接收epoll_wait()返回
			int ret = 0;
			//esize=发生事件数量，events=出参存储发生的事件
			ssize_t esize = m_epoll.waitEvents(events); //epoll_event {event, data} event->注册时希望监听的事件，所以这里wait回来的就是发生的事件 data->自定义参数
			if (esize > 0) {
				for (ssize_t i = 0; i < esize; i++) {
					if (events[i].events & EPOLLIN) { // 读事件
						CSocketBase* pClient = NULL;
						if (events[i].data.ptr == m_server) { // 监听套接字上的读事件，说明有客户端请求(调用AddTask()的进程/线程即客户端)
							ret = m_server->Link(&pClient); // 通过二级指针返回连接后的通信套接字(服务器：监听+通信)对象指针
							if (ret != 0) continue;
							ret = m_epoll.Add(*pClient, EpollData((void*)pClient)); // 通信套接字挂树(.Add()para3默认为可读事件)
							if (ret != 0) {
								delete pClient;
								continue;
							}
						}
						else { // 客户端套接字上的读事件(AddTask)
							pClient = (CSocketBase*)events[i].data.ptr;
							if (pClient) {
								CFunctionBase* base = NULL;
								Buffer data(sizeof(base));
								ret = pClient->Recv(data);
								if (ret <= 0) {
									m_epoll.Del(*pClient); // 将通信套接字从树上摘下
									delete pClient;
									continue;
								}
								memcpy(&base, (char*)data, sizeof(base));
								if (base != NULL) {
									(*base)(); // 调用任务函数 base->函数对象指针 *base->可调用对象
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
	CEpoll m_epoll; // epoll监听I/O事件
	std::vector<CThread*> m_threads; // 线程池中的线程指针vector
	// std::vector<std::shared_ptr<CThread>> m_threads; // 线程池中的线程指针vector
	CSocketBase* m_server; // 线程池本地监听套接字
	Buffer m_path; // 服务器本地套接字通信地址(其本身也是个本地套接字)
	// 1.在线程池中的通信双方：服务器->线程池，客户端->想要给线程分配任务的其他进程/线程
	//   服务器->处理客户端发来的任务 客户端->将要执行的任务(我们封装的函数类对象指针)发送给服务器
	// 2.线程池中各个线程干了什么：TaskDispatch()
	//   共享epoll树根 -> 一起wait -> 谁抢到(竞争到)事件(m_server上的读->连接客户端 pClient上的读->执行任务函数)谁就干
	// 3.为什么要用epoll + 本地套接字通信：
	//   不用epoll+套接字通信->在服务器生成一个待处理事件队列->由于是多线程执行->会涉及到多线程同时读写->要加锁->麻烦且效率低
	//   用epoll+套接字通信->利用网络编程特性->由网卡和系统驱动拿到的数据->支持高并发
};	
