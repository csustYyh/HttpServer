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
	template<typename T> // ģ�溯�� �������������<< ��֧����ʽ��־
	LogInfo& operator<<(const T& data) { // Ҫ���������
		std::stringstream stream; // ���ڽ���������ת��Ϊ�ַ���
		stream << data; // ���� << �������� data д������
		m_buf += stream.str().c_str(); // ����������תΪ�ַ�������׷�ӵ� m_buf 
		return *this; // ���ض�����֧����ʽ���ã�a << b << c
	}
private:
	bool bAuto; // Ĭ����false ��ʽ��־����Ϊtrue
	Buffer m_buf; // �洢��־��Ϣ���ַ���������
};

class CLoggerServer
{
public:
	CLoggerServer() :
		m_thread(&CLoggerServer::ThreadFunc, this) // ��Ա��ʼ���б�ķ�ʽ��ʼ�������߳�(����epoll_wait()->���ӿͻ���->д��־)
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
public: // ��ֹ���ƹ����븳ֵ���� = delete �� C++11 �е�һ�����ԣ�����ɾ�����е��ض�����
	CLoggerServer(const CLoggerServer&) = delete;  // ͨ����ĳ����������Ϊ= delete��������ֹ�ú��������б����û��߱���������
	CLoggerServer& operator=(const CLoggerServer&) = delete; // �������ͨ�����ڽ���Ĭ�ϵĳ�Ա�������߲�������Ҳ����������ֹĳЩ��ϣ�������õĺ���
public:
	int Start() {// ��־������������������־�ļ�->����epoll->�������������׽��ֶ��󲢶����ʼ��->�����߳�
		if (m_server != NULL) return -1; // �Ѿ����˱����׽���
		if (access("log", W_OK | R_OK) != 0) { // 
			mkdir("log", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
		} // ��鵱ǰ���̶��ļ� "log" �Ķ�дȨ�ޡ������ǰ���̶� "log" �ļ�û�ж�дȨ�ޣ��򴴽�һ����Ϊ "log" ��Ŀ¼����������Ӧ��Ȩ��
		 // �Զ�дģʽ����־������������򴴽�������Ѿ���������ļ������
		if (m_file == NULL) return -2;
		int ret = m_epoll.Create(1); // ����epoll m_epoll---epollר��fd
		//printf("%s(%d):[%s]ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
		if (ret != 0) return -3;
		m_server = new CSocket(); // ���������׽��ֶ���(���Լ���) 
		if (m_server == NULL) {
			Close();
			return -4;
		}
		ret = m_server->Init(CSockParam("./log/server.sock", (int)SOCK_ISSERVER | SOCK_ISREUSE)); // �����׽��ֶ����ʼ��
		if (ret != 0) {
			Close();
			return -5;
		}
		ret = m_epoll.Add(*m_server, EpollData((void*)m_server), EPOLLIN | EPOLLERR);
		if (ret != 0) {
			Close();
			return -6;
		}
		ret = m_thread.Start(); // �����߳���ʹ��epoll_wait()�ȴ��¼�����
		if (ret != 0) {
			printf("%s(%d):<%s> pid=%d errno:%d msg:%s ret:%d\n",
				__FILE__, __LINE__, __FUNCTION__, getpid(), errno, strerror(errno), ret);
			Close();
			return -7;
		}
		return 0;
	}
	
	int Close() { // �ͷű��ؼ����׽��ֶ���
		if (m_server != NULL) {
			CSocketBase* p = m_server;
			m_server = NULL;
			delete p;
		}
		m_epoll.Close();
		m_thread.Stop();
		return 0;
	}
	// ����������־���̵Ľ��̺��߳�ʹ�õ� ����ͨ������+�������������  CLoggerServer::Trace()
	static void Trace(const LogInfo& info) {
		int ret = 0;
		static thread_local CSocket client; // �ͻ����׽��ֶ��� one thread one connection
		if (client == -1) { // ��̬�ĵ�ÿ���߳�(static thread_local)������������ṹ��һ���µ� client ��һ���̶߳�Ӧһ���ͻ���
			ret = client.Init(CSockParam("./log/server.sock", 0)); // ָ��������־�������׽���·�� <Socket.h line176>
			if (ret != 0) {
#ifdef _DEBUG
				printf("%s(%d):[%s]ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
#endif
				return;
			}
			printf("%s(%d):[%s]ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
			ret = client.Link(); // �ͻ���Link ��accept
			printf("%s(%d):[%s]ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
			if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
		}
		ret = client.Send(info);
		printf("%s(%d):[%s]ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
	}
	static Buffer GetTimeStr() { // ��ȡ��ǰʱ����ַ�����ʾ
		Buffer result(128);
		timeb tmb; // ʱ��ṹ��
		ftime(&tmb); // ��ȡ��ǰϵͳʱ��
		tm* pTm = localtime(&tmb.time); // �� tmb.time ת��Ϊ����ʱ��
		int nSize = snprintf(result, result.size(),
			"%04d-%02d-%02d_%02d-%02d-%02d.%03d",
			pTm->tm_year + 1900, pTm->tm_mon + 1, pTm->tm_mday,
			pTm->tm_hour, pTm->tm_min, pTm->tm_sec,
			tmb.millitm
		); // ��ʱ��д�� result ������д����ֽ���
		result.resize(nSize);
		return result;
	}
private:
	int ThreadFunc() { // �̺߳��� ��Ҫ��ִ�� epoll_wait ͬʱ���䷵�ص�IO�¼����д���(�������ӡ�д��־)
		printf("%s(%d):[%s] %d\n", __FILE__, __LINE__, __FUNCTION__, m_thread.isValid());
		printf("%s(%d):[%s] %d\n", __FILE__, __LINE__, __FUNCTION__, (int)m_epoll);
		printf("%s(%d):[%s] %p\n", __FILE__, __LINE__, __FUNCTION__, m_server);
		EPEvents events; // epoll_event�ṹ��Ķ�̬���� ��������epoll_wait()���صķ����¼�
		std::map<int, CSocketBase*> mapClients; // ����ͨ�Ŷ�����׽��ּ�����ָ���map
		while (m_thread.isValid() && (m_epoll != -1) && (m_server != NULL)) { // ��־�������߳�OK epoll OK ���ؼ����׽��ֶ���OK
			ssize_t ret = m_epoll.waitEvents(events, 1000); // �ȴ��¼�������ret=������events=���δ洢�������¼�(epoll_event)����ʱʱ��=1ms
			//printf("%s(%d):[%s] %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
			if (ret < 0) break;
			if (ret > 0) { // ��IO�¼����أ���Ҫ���� ret=�¼�����
				printf("%s(%d):[%s]ret=%d \n", __FILE__, __LINE__, __FUNCTION__, ret);
				ssize_t i = 0;
				for (; i < ret; i++) {
					if (events[i].events & EPOLLERR) { // ��i���¼��Ǵ����¼�
						printf("%s(%d):[%s]errno=%d \n", __FILE__, __LINE__, __FUNCTION__, 123);
						break;
					}
					else if (events[i].events & EPOLLIN) { // �ɶ�
						if (events[i].data.ptr == m_server) { // �����׽��ֶ����Ϸ����ɶ��¼� ˵���пͻ�����Ҫ����
							CSocketBase* pClient = NULL; // ͨ���׽��ֶ���ָ��
							int r = m_server->Link(&pClient); // ͨ������ָ�뷵�����Ӻ��ͨ���׽��ֶ���ָ��
							printf("%s(%d):[%s]ret=%d \n", __FILE__, __LINE__, __FUNCTION__, r);
							if (r < 0) continue;  // ע�� pClient ���� Link()��new�����ģ�����������Ҫ�ͷű����ڴ�й©
							// ��ͨ���׽��ֶ�������أ������ɶ��¼�(˵���ͻ��˷�������־)
							r = m_epoll.Add(*pClient, EpollData((void*)pClient), EPOLLIN | EPOLLERR); // ��ͨ���׽��ֶ���ָ�����epoll_event��data.ptr
							printf("%s(%d):[%s]ret=%d \n", __FILE__, __LINE__, __FUNCTION__, r);
							if (r < 0) { // ������ʧ��  
								delete pClient;
								continue;
							}
							auto it = mapClients.find(*pClient);
							//if (it->second != NULL) {
							//	//delete it->second; // ɾ�����׽��ֶ���ָ��
							//}
							if (it != mapClients.end()) {
								if (it->second) delete it->second;
							}
							mapClients[*pClient] = pClient; // ����map
						}
						else { // ͨ���׽����Ϸ����ɶ��¼� ��д��־
							printf("%s(%d):[%s]ptr=%p \n", __FILE__, __LINE__, __FUNCTION__, events[i].data.ptr);
							CSocketBase* pClient = (CSocketBase*)events[i].data.ptr; // �õ������ɶ��¼���ͨ���׽��ֶ���ָ��	
							if (pClient != NULL) {
								Buffer data(1024 * 1024);
								int r = pClient->Recv(data); // ����
								printf("%s(%d):[%s]ret=%d \n", __FILE__, __LINE__, __FUNCTION__, r);
								if (r <= 0) { // δ�յ�����
									mapClients[*pClient] = NULL;
									delete pClient;
								}
								else { // �յ����� д��־
									printf("%s(%d):[%s]data=%s \n", __FILE__, __LINE__, __FUNCTION__, (char*)data);
									WriteLog(data);
								}
								printf("%s(%d):[%s]ret=%d \n", __FILE__, __LINE__, __FUNCTION__, r);
							}
						}
					}
				}
				if (i != ret) { // �¼�û���������ǰ������
					break;
				}
			}
		} // �ͷ���Դ
		for (auto it = mapClients.begin(); it != mapClients.end(); it++) {
			if (it->second) {
				delete it->second;
			}
		}
		mapClients.clear();
		return 0;
	}
	void WriteLog(const Buffer& data) {
		if (m_file != NULL) { // �ļ�ָ�벻Ϊ��
			FILE* pFile = m_file; // �ں����Ĳ�����ʹ����ʱָ�� pFile���Ա���ֱ�Ӳ�����Ա����
			fwrite((char*)data, 1, data.size(), pFile); // para1:д�����ݵ���ʼ��ַ para2:ÿ��д�����ݴ�С para3:���ݳ��� para4:�ļ�ָ��
			fflush(pFile); // ˢ���ļ���������ȷ�����ݱ�����д�뵽�ļ��У���������ʱ�洢�ڻ������еȴ�д��
#ifdef _DEBUG
			printf("%s", (char*)data);
#endif
		}
	}
private:
	CThread m_thread; // ��־�����������߳� ֻ��һ���̸߳��������Ӻ�д��־���ʲ��ÿ����̼߳�Ļ���ͬ��
	CEpoll m_epoll; // epoll
	CSocketBase* m_server; // ��־�������ļ����׽���
	Buffer m_path; // ��־�ļ����·��
	FILE* m_file; // ��־�ļ�ָ�� 
};


#ifndef TRACE
#define TRACEI(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO, __VA_ARGS__))
#define TRACED(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, __VA_ARGS__))
#define TRACEW(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, __VA_ARGS__))
#define TRACEE(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, __VA_ARGS__))
#define TRACEF(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, __VA_ARGS__))

//LOGI<<"hello" << 10 <<"how are you"; // ��ʧ��־����־��������ʱͨ���ж� m_bool == true ����Trace(��
#define LOGI LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO)
#define LOGD LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG)
#define LOGW LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING)
#define LOGE LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR)
#define LOGF LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL)

//�ڴ浼��
//00 01 02 65����  ; ...a����
#define DUMPI(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO, data, size))
#define DUMPD(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, data, size))
#define DUMPW(data, size) CLoggerServer::TraceLogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, data, size))
#define DUMPE(data, size) CLoggerServer::TraceLogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, data, size))
#define DUMPF(data, size) CLoggerServer::TraceLogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, data, size))
#endif