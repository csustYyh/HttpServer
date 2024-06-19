#pragma once
#include "Thread.h"
#include "Epoll.h"
#include "Socket.h"
#include <list>
#include <sys/timeb.h>
#include <stdarg.h>
#include <sstream>
#include <sys/stat.h>

enum LogLevel {
	LOG_INFO,
	LOG_DEBUG, 
	LOG_WARNING, 
	LOG_ERROR,
	LOG_FATAL
};

class LogInfo {
public:
	LogInfo(const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level,
		const char* fmt, ...);
	LogInfo(const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level);
	LogInfo(const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level,
		void* pData, size_t nSize);
	~LogInfo();
	operator Buffer()const {
		return m_buf;
	}
	template<typename T> // 模版函数 重载输出操作符<< 以支持流式日志
	LogInfo& operator<<(const T& data) { // 要输出的数据
		std::stringstream stream; // 用于将各种数据转换为字符串
		stream << data; // 利用 << 操作符将 data 写入流中
		m_buf += stream.str().c_str(); // 将流中数据转为字符串，并追加到 m_buf 
		return *this; // 返回对象，以支持链式调用：a << b << c
	}
private:
	bool bAuto; // 默认是false 流式日志，则为true
	Buffer m_buf; // 存储日志信息的字符串缓冲区
};

class CLoggerServer
{
public:
	CLoggerServer() :
		m_thread(&CLoggerServer::ThreadFunc, this) // 成员初始化列表的方式初始化了子线程(用于epoll_wait()->连接客户端->写日志)
	{
		m_server = NULL;
		char curpath[256] = "";
		getcwd(curpath, sizeof(curpath));
		m_path = curpath;
		m_path += "/log/" + GetTimeStr() + ".log";
		printf("%s(%d):[%s]path=%s\n", __FILE__, __LINE__, __FUNCTION__, (char*)m_path);
	}
	~CLoggerServer() {
		Close();
	}
public: // 禁止复制构造与赋值重载 = delete 是 C++11 中的一个特性，用于删除类中的特定函数
	CLoggerServer(const CLoggerServer&) = delete;  // 通过将某个函数声明为= delete，可以阻止该函数在类中被调用或者编译器生成
	CLoggerServer& operator=(const CLoggerServer&) = delete; // 这个特性通常用于禁用默认的成员函数或者操作符，也可以用于阻止某些不希望被调用的函数
public:
	int Start() {// 日志服务器的启动：打开日志文件->创建epoll->创建监听本地套接字对象并对其初始化->开启线程
		if (m_server != NULL) return -1; // 已经有了本地套接字
		if (access("log", W_OK | R_OK) != 0) { // 
			mkdir("log", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
		} // 检查当前进程对文件 "log" 的读写权限。如果当前进程对 "log" 文件没有读写权限，则创建一个名为 "log" 的目录，并设置相应的权限
		 // 以读写模式打开日志，如果不存在则创建，如果已经存在则打开文件并清空
		if (m_file == NULL) return -2;
		int ret = m_epoll.Create(1); // 创建epoll m_epoll---epoll专用fd
		//printf("%s(%d):[%s]ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
		if (ret != 0) return -3;
		m_server = new CSocket(); // 创建本地套接字对象(用以监听) 
		if (m_server == NULL) {
			Close();
			return -4;
		}
		ret = m_server->Init(CSockParam("./log/server.sock", (int)SOCK_ISSERVER | SOCK_ISREUSE)); // 监听套接字对象初始化
		if (ret != 0) {
			Close();
			return -5;
		}
		ret = m_epoll.Add(*m_server, EpollData((void*)m_server), EPOLLIN | EPOLLERR);
		if (ret != 0) {
			Close();
			return -6;
		}
		ret = m_thread.Start(); // 开启线程来使用epoll_wait()等待事件产生
		if (ret != 0) {
			printf("%s(%d):<%s> pid=%d errno:%d msg:%s ret:%d\n",
				__FILE__, __LINE__, __FUNCTION__, getpid(), errno, strerror(errno), ret);
			Close();
			return -7;
		}
		return 0;
	}
	
	int Close() { // 释放本地监听套接字对象
		if (m_server != NULL) {
			CSocketBase* p = m_server;
			m_server = NULL;
			delete p;
		}
		m_epoll.Close();
		m_thread.Stop();
		return 0;
	}
	// 给其他非日志进程的进程和线程使用的 可以通过类名+作用域符来调用  CLoggerServer::Trace()
	static void Trace(const LogInfo& info) {
		int ret = 0;
		static thread_local CSocket client; // 客户端套接字对象 one thread one connection
		if (client == -1) { // 静态的但每个线程(static thread_local)调这个函数都会构造一个新的 client 即一个线程对应一个客户端
			ret = client.Init(CSockParam("./log/server.sock", 0)); // 指定本地日志服务器套接字路径 <Socket.h line176>
			if (ret != 0) {
#ifdef _DEBUG
				printf("%s(%d):[%s]ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
#endif
				return;
			}
			printf("%s(%d):[%s]ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
			ret = client.Link(); // 客户端Link 即accept
			printf("%s(%d):[%s]ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
			if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
		}
		ret = client.Send(info);
		printf("%s(%d):[%s]ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
	}
	static Buffer GetTimeStr() { // 获取当前时间的字符串表示
		Buffer result(128);
		timeb tmb; // 时间结构体
		ftime(&tmb); // 获取当前系统时间
		tm* pTm = localtime(&tmb.time); // 将 tmb.time 转换为本地时间
		int nSize = snprintf(result, result.size(),
			"%04d-%02d-%02d_%02d-%02d-%02d.%03d",
			pTm->tm_year + 1900, pTm->tm_mon + 1, pTm->tm_mday,
			pTm->tm_hour, pTm->tm_min, pTm->tm_sec,
			tmb.millitm
		); // 将时间写入 result 并返回写入的字节数
		result.resize(nSize);
		return result;
	}
private:
	int ThreadFunc() { // 线程函数 主要是执行 epoll_wait 同时对其返回的IO事件进行处理(处理连接、写日志)
		printf("%s(%d):[%s] %d\n", __FILE__, __LINE__, __FUNCTION__, m_thread.isValid());
		printf("%s(%d):[%s] %d\n", __FILE__, __LINE__, __FUNCTION__, (int)m_epoll);
		printf("%s(%d):[%s] %p\n", __FILE__, __LINE__, __FUNCTION__, m_server);
		EPEvents events; // epoll_event结构体的动态数组 用来接收epoll_wait()返回的发生事件
		std::map<int, CSocketBase*> mapClients; // 本地通信对象的套接字及对象指针的map
		while (m_thread.isValid() && (m_epoll != -1) && (m_server != NULL)) { // 日志服务器线程OK epoll OK 本地监听套接字对象OK
			ssize_t ret = m_epoll.waitEvents(events, 1000); // 等待事件发生，ret=数量，events=出参存储发生的事件(epoll_event)，超时时间=1ms
			//printf("%s(%d):[%s] %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
			if (ret < 0) break;
			if (ret > 0) { // 有IO事件返回，需要处理 ret=事件个数
				printf("%s(%d):[%s]ret=%d \n", __FILE__, __LINE__, __FUNCTION__, ret);
				ssize_t i = 0;
				for (; i < ret; i++) {
					if (events[i].events & EPOLLERR) { // 第i个事件是错误事件
						printf("%s(%d):[%s]errno=%d \n", __FILE__, __LINE__, __FUNCTION__, 123);
						break;
					}
					else if (events[i].events & EPOLLIN) { // 可读
						if (events[i].data.ptr == m_server) { // 监听套接字对象上发生可读事件 说明有客户端需要连接
							CSocketBase* pClient = NULL; // 通信套接字对象指针
							int r = m_server->Link(&pClient); // 通过二级指针返回连接后的通信套接字对象指针
							printf("%s(%d):[%s]ret=%d \n", __FILE__, __LINE__, __FUNCTION__, r);
							if (r < 0) continue;  // 注意 pClient 是在 Link()中new出来的，我们在这里要释放避免内存泄漏
							// 将通信套接字对象加入监控，监听可读事件(说明客户端发来了日志)
							r = m_epoll.Add(*pClient, EpollData((void*)pClient), EPOLLIN | EPOLLERR); // 将通信套接字对象指针存入epoll_event中data.ptr
							printf("%s(%d):[%s]ret=%d \n", __FILE__, __LINE__, __FUNCTION__, r);
							if (r < 0) { // 加入监控失败  
								delete pClient;
								continue;
							}
							auto it = mapClients.find(*pClient);
							//if (it->second != NULL) {
							//	//delete it->second; // 删除旧套接字对象指针
							//}
							if (it != mapClients.end()) {
								if (it->second) delete it->second;
							}
							mapClients[*pClient] = pClient; // 更新map
						}
						else { // 通信套接字上发生可读事件 则写日志
							printf("%s(%d):[%s]ptr=%p \n", __FILE__, __LINE__, __FUNCTION__, events[i].data.ptr);
							CSocketBase* pClient = (CSocketBase*)events[i].data.ptr; // 拿到发生可读事件的通信套接字对象指针	
							if (pClient != NULL) {
								Buffer data(1024 * 1024);
								int r = pClient->Recv(data); // 接收
								printf("%s(%d):[%s]ret=%d \n", __FILE__, __LINE__, __FUNCTION__, r);
								if (r <= 0) { // 未收到数据
									mapClients[*pClient] = NULL;
									delete pClient;
								}
								else { // 收到数据 写日志
									printf("%s(%d):[%s]data=%s \n", __FILE__, __LINE__, __FUNCTION__, (char*)data);
									WriteLog(data);
								}
								printf("%s(%d):[%s]ret=%d \n", __FILE__, __LINE__, __FUNCTION__, r);
							}
						}
					}
				}
				if (i != ret) { // 事件没处理完就提前结束了
					break;
				}
			}
		} // 释放资源
		for (auto it = mapClients.begin(); it != mapClients.end(); it++) {
			if (it->second) {
				delete it->second;
			}
		}
		mapClients.clear();
		return 0;
	}
	void WriteLog(const Buffer& data) {
		if (m_file != NULL) { // 文件指针不为空
			FILE* pFile = m_file; // 在后续的操作中使用临时指针 pFile，以避免直接操作成员变量
			fwrite((char*)data, 1, data.size(), pFile); // para1:写入数据的起始地址 para2:每次写入数据大小 para3:数据长度 para4:文件指针
			fflush(pFile); // 刷新文件缓冲区，确保数据被立即写入到文件中，而不是暂时存储在缓冲区中等待写入
#ifdef _DEBUG
			printf("%s", (char*)data);
#endif
		}
	}
private:
	CThread m_thread; // 日志服务器的主线程 只有一个线程负责处理连接和写日志，故不用考虑线程间的互斥同步
	CEpoll m_epoll; // epoll
	CSocketBase* m_server; // 日志服务器的监听套接字
	Buffer m_path; // 日志文件存放路径
	FILE* m_file; // 日志文件指针 
};


#ifndef TRACE
#define TRACEI(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO, __VA_ARGS__))
#define TRACED(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, __VA_ARGS__))
#define TRACEW(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, __VA_ARGS__))
#define TRACEE(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, __VA_ARGS__))
#define TRACEF(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, __VA_ARGS__))

//LOGI<<"hello" << 10 <<"how are you"; // 流失日志在日志对象析构时通过判断 m_bool == true 来调Trace(）
#define LOGI LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO)
#define LOGD LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG)
#define LOGW LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING)
#define LOGE LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR)
#define LOGF LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL)

//内存导出
//00 01 02 65……  ; ...a……
#define DUMPI(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO, data, size))
#define DUMPD(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, data, size))
#define DUMPW(data, size) CLoggerServer::TraceLogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, data, size))
#define DUMPE(data, size) CLoggerServer::TraceLogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, data, size))
#define DUMPF(data, size) CLoggerServer::TraceLogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, data, size))
#endif