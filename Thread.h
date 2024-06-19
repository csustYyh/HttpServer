#pragma once
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <map>
#include <thread>
#include "Function.h"

//class CThread
//{
//public:
//	CThread() {
//		m_function = NULL;
//		m_thread = 0;
//		m_bpaused = false;
//	}
//	
//	// 这个构造函数是个模版函数，根据不同的输入参数实例化出一个构造函数
//	// 在实例化的构造函数中，通过给模版类CFunction传参，实例化了一个函数类，然后再用这个函数类 new了一个实例，赋值给m_function
//	template<typename _FUNCTION_, typename..._ARGS_> // 构造时即可指定线程函数
//	CThread(_FUNCTION_ func, _ARGS_... args) :m_function(new CFunction<_FUNCTION_, _ARGS_...>(func, args...)) {
//		m_thread = 0; 
//		m_bpaused = false;
//	}
//
//	~CThread(){}
//
//public: // 禁止进行复制构造和赋值运算，因为thread本质是内核对象，用户态无法复制构造或赋值
//	CThread(const CThread&) = delete; //  不会实现 没必要写形参名
//	CThread operator=(const CThread&) = delete;
//
//public:
//	template<typename _FUNCTION_, typename..._ARGS_>
//	int SetThreadFunc(_FUNCTION_ func, _ARGS_... args) { // 设置线程函数
//		m_function = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
//		if (m_function == NULL) return -1;
//		return 0;
//	}
//	int Start() {
//		pthread_attr_t attr; // 线程属性对象。线程属性对象包含创建线程时可以指定的各种属性，如堆栈大小、调度策略、继承调度属性等
//		int ret = 0;
//		ret = pthread_attr_init(&attr); // 初始化线程属性对象
//		if (ret != 0) return -1;
//		// 设置线程为 joinable 状态意味着线程在结束时不会自动释放资源，需要调用 pthread_join 来清理资源
//		ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); 
//		if (ret != 0) return -2;
//		// 设置线程的调度竞争范围为进程范围（PTHREAD_SCOPE_PROCESS），不会和其他进程中的线程竞争，不设置也是默认
//		// ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);
//		// if (ret != 0) return -3;
//		// 创建线程, para1：出参，存放新线程id para2:线程属性对象 para3:新线程执行的函数(要求全局可见) para4:传递给新线程函数的参数 
//		ret = pthread_create(&m_thread, &attr, &CThread::ThreadEntry, this); 
//		if (ret != 0) return -4;
//		m_mapThread[m_thread] = this; // hash_map保存线程id和当前线程类对象的映射关系
//		ret = pthread_attr_destroy(&attr); // 销毁线程属性对象，释放相关资源
//		if (ret != 0) return -5;
//		return 0;
//	}
//	int Pause() { // 暂停/恢复
//		if (m_thread == 0) return -1; // 线程无效
//		if (m_bpaused) { // 如果当前处于暂停，修改状态标志为运行
//			m_bpaused = false; // 这样子暂停那边的信号处理函数循环就会跳出，不再暂停
//			return 0;
//		}
//		m_bpaused = true; // 当前在运行中，修改状态标志为暂停
//		int ret = pthread_kill(m_thread, SIGUSR1); // 发送暂停信号SIGUSR1
//		if (ret != 0) { // 发送信号失败
//			m_bpaused = false;
//			return -2;
//		}
//		return 0;
//	}
//	int Stop() {
//		if (m_thread != 0) { // 线程id为0说明不用再停止
//			pthread_t thread = m_thread;
//			m_thread = 0;
//			timespec ts; // 设置等待时间 创建一个 timespec 结构体
//			ts.tv_sec = 0; // 秒=0
//			ts.tv_nsec = 100 * 1000000; // 100ms 纳秒=10*1000000，即100ms
//			// 因为下面要在其他某个线程中尝试等待thread是否结束，故线程创建时属性为：PTHREAD_CREATE_JOINABLE
//			int ret = pthread_timedjoin_np(thread, NULL, &ts); // 尝试在 ts 时间内等待线程结束
//			if (ret == ETIMEDOUT) { // 在指定的等待时间内，线程没有结束
//				pthread_detach(thread); // 将线程分离，以确保当线程最终结束时其资源会被自动释放
//				pthread_kill(thread, SIGUSR2); // 向线程发送 SIGUSR2 信号 
//			}
//		}
//		return 0;
//	}
//	bool isValid()const { return m_thread != 0; }
//private: // __stdcall 无需具体实例对象来调用
//	static void* ThreadEntry(void* arg) { // 线程入口函数，即创建线程时的para3 传入的参数是某个类实例对象的this指针
//		CThread* thiz = (CThread*)arg; // 拿到this指针才可访问类的非静态方法&成员
//		struct sigaction act = { 0 }; // 用于设置信号处理函数
//		sigemptyset(&act.sa_mask); // 清空信号屏蔽集，确保信号处理函数不会被其他信号打断
//		act.sa_flags = SA_SIGINFO; // 设置信号处理函数的标志为 SA_SIGINFO，表示使用 sa_sigaction 字段指定的函数作为信号处理函数
//		act.sa_sigaction = &CThread::Sigaction; // 指定信号处理函数
//		sigaction(SIGUSR1, &act, NULL); // 注册 SIGUSR1 信号的处理函数为 CThread::Sigaction
//		sigaction(SIGUSR2, &act, NULL); // 注册 SIGUSR2 信号的处理函数为 CThread::Sigaction
//
//		thiz->EnterThread(); // 用传进来的this指针调用实际的线程工作函数
//
//		if (thiz->m_thread) thiz->m_thread = 0; // 线程工作结束
//		pthread_t thread = pthread_self(); // 不是冗余，有可能stop()把m_thread清零了
//		auto it = m_mapThread.find(thread);
//		if (it != m_mapThread.end())
//			m_mapThread[thread] = NULL;
//		pthread_detach(thread); // 将当前线程设置为可分离状态，使得线程结束后其资源会自动回收
//		pthread_exit(NULL); // 结束线程
//	}
//	// 信号处理函数，para1:接收到的信号编号 para2:信号的附加信息 para3:信号处理时的上下文信息
//	static void Sigaction(int signo, siginfo_t* info, void* context) { 
//		if (signo == SIGUSR1) { // 暂停/恢复
//			pthread_t thread = pthread_self(); // 获取当前的线程id
//			auto it = m_mapThread.find(thread); // hash_map中查找线程id及其对应的线程类实例对象
//			if (it != m_mapThread.end()) { // 找到
//				if (it->second) { // 线程id对应的线程类对象
//					while (it->second->m_bpaused) { // 当前线程处于暂停状态
//						usleep(1000); // 休眠1ms后继续循环
//					}
//				}
//			}
//		}
//		else if (signo == SIGUSR2) { // 线程退出
//			pthread_exit(NULL);
//		}
//	}
//	void EnterThread() { // 线程工作函数  __thiscall 需要传隐含的this指针，即需要某个具体实例来调用
//		if (m_function != NULL) {
//			int ret = (*m_function)(); // 在 Function.h 中重载了CFunctionBase类对象的函数调用运算符，这里就是在执行真正的线程函数
//			if (ret != 0) {
//				printf("%s(%d):[%s]ret = %d\n", __FILE__, __LINE__, __FUNCTION__);
//			}
//		}
//	}
//private:
//	CFunctionBase* m_function; // 线程函数指针
//	pthread_t m_thread; // 线程id
//	bool m_bpaused; // true-暂停 false-运行中
//	static std::map<pthread_t, CThread*> m_mapThread; // 线程id与线程对象指针的hash_map
//};
//

#pragma once
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <map>
#include "Function.h"

class CThread
{
public:
	CThread() {
		m_function = NULL;
		m_thread = 0;
		m_bpaused = false;
	}

	// 这个构造函数是个模版函数，根据不同的输入参数实例化出一个构造函数
	// 在实例化的构造函数中，通过给模版类CFunction传参，实例化了一个函数类，然后再用这个函数类 new了一个实例，赋值给m_function
	template<typename _FUNCTION_, typename..._ARGS_> // 构造时即可指定线程函数
	CThread(_FUNCTION_ func, _ARGS_... args) :m_function(new CFunction<_FUNCTION_, _ARGS_...>(func, args...)) {
		m_thread = 0;
		m_bpaused = false;
	}

	~CThread() {}

public: // 禁止进行复制构造和赋值运算，因为thread本质是内核对象，用户态无法复制构造或赋值
	CThread(const CThread&) = delete; //  不会实现 没必要写形参名
	CThread operator=(const CThread&) = delete;

public:
	template<typename _FUNCTION_, typename..._ARGS_>
	int SetThreadFunc(_FUNCTION_ func, _ARGS_... args) { // 设置线程函数
		m_function = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_function == NULL) return -1;
		return 0;
	}
	int Start() {
		m_thread = new std::thread(&CThread::ThreadEntry, this);
		m_mapThread[m_thread->native_handle()] = this; // hash_map保存线程id和当前线程类对象的映射关系
		return 0;
	}
	int Pause() { // 暂停/恢复
		if (m_thread == nullptr) return -1; // 线程无效
		if (m_bpaused) { // 如果当前处于暂停，修改状态标志为运行
			m_bpaused = false; // 这样子暂停那边的信号处理函数循环就会跳出，不再暂停
			return 0;
		}
		m_bpaused = true; // 当前在运行中，修改状态标志为暂停
		int ret = pthread_kill(m_thread->native_handle(), SIGUSR1); // 发送暂停信号SIGUSR1
		if (ret != 0) { // 发送信号失败
			m_bpaused = false;
			return -2;
		}
		return 0;
	}
	int Stop() {
		if (m_thread != nullptr && m_thread->joinable()) {
			m_thread->join();
			auto a = m_thread;
			m_thread = nullptr;
			delete a;
		}
		return 0;
	}
	bool isValid()const { return m_thread != nullptr; }

private: // __stdcall 无需具体实例对象来调用
	static void* ThreadEntry(void* arg) { // 线程入口函数，即创建线程时的para3 传入的参数是某个类实例对象的this指针
		CThread* thiz = (CThread*)arg; // 拿到this指针才可访问类的非静态方法&成员
		struct sigaction act = { 0 }; // 用于设置信号处理函数
		sigemptyset(&act.sa_mask); // 清空信号屏蔽集，确保信号处理函数不会被其他信号打断
		act.sa_flags = SA_SIGINFO; // 设置信号处理函数的标志为 SA_SIGINFO，表示使用 sa_sigaction 字段指定的函数作为信号处理函数
		act.sa_sigaction = &CThread::Sigaction; // 指定信号处理函数
		sigaction(SIGUSR1, &act, NULL); // 注册 SIGUSR1 信号的处理函数为 CThread::Sigaction
		sigaction(SIGUSR2, &act, NULL); // 注册 SIGUSR2 信号的处理函数为 CThread::Sigaction

		thiz->EnterThread(); // 用传进来的this指针调用实际的线程工作函数

		if (thiz->m_thread) thiz->m_thread = nullptr; // 线程工作结束
		pthread_t thread = thiz->m_thread->native_handle(); // 不是冗余，有可能stop()把m_thread清零了
		auto it = m_mapThread.find(thread);
		if (it != m_mapThread.end())
			m_mapThread[thread] = NULL;
		thiz->m_thread->join();
	}
	// 信号处理函数，para1:接收到的信号编号 para2:信号的附加信息 para3:信号处理时的上下文信息
	static void Sigaction(int signo, siginfo_t* info, void* context) {
		if (signo == SIGUSR1) { // 暂停/恢复
			pthread_t thread = pthread_self(); // 获取当前的线程id
			auto it = m_mapThread.find(thread); // hash_map中查找线程id及其对应的线程类实例对象
			if (it != m_mapThread.end()) { // 找到
				if (it->second) { // 线程id对应的线程类对象
					while (it->second->m_bpaused) { // 当前线程处于暂停状态
						usleep(1000); // 休眠1ms后继续循环
					}
				}
			}
		}
		else if (signo == SIGUSR2) { // 线程退出
			pthread_exit(NULL);
		}
	}
	void EnterThread() { // 线程工作函数  __thiscall 需要传隐含的this指针，即需要某个具体实例来调用
		if (m_function != NULL) {
			int ret = (*m_function)(); // 在 Function.h 中重载了CFunctionBase类对象的函数调用运算符，这里就是在执行真正的线程函数
			if (ret != 0) {
				printf("%s(%d):[%s]ret = %d\n", __FILE__, __LINE__, __FUNCTION__);
			}
		}
	}
private:
	std::unique_ptr<CFunctionBase> m_function;
	//CFunctionBase* m_function; // 线程函数指针
	//pthread_t m_thread; // 线程id
	//std::unique_ptr<std::thread> m_thread;
	std::thread* m_thread; // 线程对象指针
	bool m_bpaused; // true-暂停 false-运行中
	static std::map<pthread_t, CThread*> m_mapThread; // 线程id与线程对象指针的hash_map
};



