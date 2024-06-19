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
//	// ������캯���Ǹ�ģ�溯�������ݲ�ͬ���������ʵ������һ�����캯��
//	// ��ʵ�����Ĺ��캯���У�ͨ����ģ����CFunction���Σ�ʵ������һ�������࣬Ȼ��������������� new��һ��ʵ������ֵ��m_function
//	template<typename _FUNCTION_, typename..._ARGS_> // ����ʱ����ָ���̺߳���
//	CThread(_FUNCTION_ func, _ARGS_... args) :m_function(new CFunction<_FUNCTION_, _ARGS_...>(func, args...)) {
//		m_thread = 0; 
//		m_bpaused = false;
//	}
//
//	~CThread(){}
//
//public: // ��ֹ���и��ƹ���͸�ֵ���㣬��Ϊthread�������ں˶����û�̬�޷����ƹ����ֵ
//	CThread(const CThread&) = delete; //  ����ʵ�� û��Ҫд�β���
//	CThread operator=(const CThread&) = delete;
//
//public:
//	template<typename _FUNCTION_, typename..._ARGS_>
//	int SetThreadFunc(_FUNCTION_ func, _ARGS_... args) { // �����̺߳���
//		m_function = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
//		if (m_function == NULL) return -1;
//		return 0;
//	}
//	int Start() {
//		pthread_attr_t attr; // �߳����Զ����߳����Զ�����������߳�ʱ����ָ���ĸ������ԣ����ջ��С�����Ȳ��ԡ��̳е������Ե�
//		int ret = 0;
//		ret = pthread_attr_init(&attr); // ��ʼ���߳����Զ���
//		if (ret != 0) return -1;
//		// �����߳�Ϊ joinable ״̬��ζ���߳��ڽ���ʱ�����Զ��ͷ���Դ����Ҫ���� pthread_join ��������Դ
//		ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); 
//		if (ret != 0) return -2;
//		// �����̵߳ĵ��Ⱦ�����ΧΪ���̷�Χ��PTHREAD_SCOPE_PROCESS������������������е��߳̾�����������Ҳ��Ĭ��
//		// ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);
//		// if (ret != 0) return -3;
//		// �����߳�, para1�����Σ�������߳�id para2:�߳����Զ��� para3:���߳�ִ�еĺ���(Ҫ��ȫ�ֿɼ�) para4:���ݸ����̺߳����Ĳ��� 
//		ret = pthread_create(&m_thread, &attr, &CThread::ThreadEntry, this); 
//		if (ret != 0) return -4;
//		m_mapThread[m_thread] = this; // hash_map�����߳�id�͵�ǰ�߳�������ӳ���ϵ
//		ret = pthread_attr_destroy(&attr); // �����߳����Զ����ͷ������Դ
//		if (ret != 0) return -5;
//		return 0;
//	}
//	int Pause() { // ��ͣ/�ָ�
//		if (m_thread == 0) return -1; // �߳���Ч
//		if (m_bpaused) { // �����ǰ������ͣ���޸�״̬��־Ϊ����
//			m_bpaused = false; // ��������ͣ�Ǳߵ��źŴ�����ѭ���ͻ�������������ͣ
//			return 0;
//		}
//		m_bpaused = true; // ��ǰ�������У��޸�״̬��־Ϊ��ͣ
//		int ret = pthread_kill(m_thread, SIGUSR1); // ������ͣ�ź�SIGUSR1
//		if (ret != 0) { // �����ź�ʧ��
//			m_bpaused = false;
//			return -2;
//		}
//		return 0;
//	}
//	int Stop() {
//		if (m_thread != 0) { // �߳�idΪ0˵��������ֹͣ
//			pthread_t thread = m_thread;
//			m_thread = 0;
//			timespec ts; // ���õȴ�ʱ�� ����һ�� timespec �ṹ��
//			ts.tv_sec = 0; // ��=0
//			ts.tv_nsec = 100 * 1000000; // 100ms ����=10*1000000����100ms
//			// ��Ϊ����Ҫ������ĳ���߳��г��Եȴ�thread�Ƿ���������̴߳���ʱ����Ϊ��PTHREAD_CREATE_JOINABLE
//			int ret = pthread_timedjoin_np(thread, NULL, &ts); // ������ ts ʱ���ڵȴ��߳̽���
//			if (ret == ETIMEDOUT) { // ��ָ���ĵȴ�ʱ���ڣ��߳�û�н���
//				pthread_detach(thread); // ���̷߳��룬��ȷ�����߳����ս���ʱ����Դ�ᱻ�Զ��ͷ�
//				pthread_kill(thread, SIGUSR2); // ���̷߳��� SIGUSR2 �ź� 
//			}
//		}
//		return 0;
//	}
//	bool isValid()const { return m_thread != 0; }
//private: // __stdcall �������ʵ������������
//	static void* ThreadEntry(void* arg) { // �߳���ں������������߳�ʱ��para3 ����Ĳ�����ĳ����ʵ�������thisָ��
//		CThread* thiz = (CThread*)arg; // �õ�thisָ��ſɷ�����ķǾ�̬����&��Ա
//		struct sigaction act = { 0 }; // ���������źŴ�����
//		sigemptyset(&act.sa_mask); // ����ź����μ���ȷ���źŴ��������ᱻ�����źŴ��
//		act.sa_flags = SA_SIGINFO; // �����źŴ������ı�־Ϊ SA_SIGINFO����ʾʹ�� sa_sigaction �ֶ�ָ���ĺ�����Ϊ�źŴ�����
//		act.sa_sigaction = &CThread::Sigaction; // ָ���źŴ�����
//		sigaction(SIGUSR1, &act, NULL); // ע�� SIGUSR1 �źŵĴ�����Ϊ CThread::Sigaction
//		sigaction(SIGUSR2, &act, NULL); // ע�� SIGUSR2 �źŵĴ�����Ϊ CThread::Sigaction
//
//		thiz->EnterThread(); // �ô�������thisָ�����ʵ�ʵ��̹߳�������
//
//		if (thiz->m_thread) thiz->m_thread = 0; // �̹߳�������
//		pthread_t thread = pthread_self(); // �������࣬�п���stop()��m_thread������
//		auto it = m_mapThread.find(thread);
//		if (it != m_mapThread.end())
//			m_mapThread[thread] = NULL;
//		pthread_detach(thread); // ����ǰ�߳�����Ϊ�ɷ���״̬��ʹ���߳̽���������Դ���Զ�����
//		pthread_exit(NULL); // �����߳�
//	}
//	// �źŴ�������para1:���յ����źű�� para2:�źŵĸ�����Ϣ para3:�źŴ���ʱ����������Ϣ
//	static void Sigaction(int signo, siginfo_t* info, void* context) { 
//		if (signo == SIGUSR1) { // ��ͣ/�ָ�
//			pthread_t thread = pthread_self(); // ��ȡ��ǰ���߳�id
//			auto it = m_mapThread.find(thread); // hash_map�в����߳�id�����Ӧ���߳���ʵ������
//			if (it != m_mapThread.end()) { // �ҵ�
//				if (it->second) { // �߳�id��Ӧ���߳������
//					while (it->second->m_bpaused) { // ��ǰ�̴߳�����ͣ״̬
//						usleep(1000); // ����1ms�����ѭ��
//					}
//				}
//			}
//		}
//		else if (signo == SIGUSR2) { // �߳��˳�
//			pthread_exit(NULL);
//		}
//	}
//	void EnterThread() { // �̹߳�������  __thiscall ��Ҫ��������thisָ�룬����Ҫĳ������ʵ��������
//		if (m_function != NULL) {
//			int ret = (*m_function)(); // �� Function.h ��������CFunctionBase�����ĺ�����������������������ִ���������̺߳���
//			if (ret != 0) {
//				printf("%s(%d):[%s]ret = %d\n", __FILE__, __LINE__, __FUNCTION__);
//			}
//		}
//	}
//private:
//	CFunctionBase* m_function; // �̺߳���ָ��
//	pthread_t m_thread; // �߳�id
//	bool m_bpaused; // true-��ͣ false-������
//	static std::map<pthread_t, CThread*> m_mapThread; // �߳�id���̶߳���ָ���hash_map
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

	// ������캯���Ǹ�ģ�溯�������ݲ�ͬ���������ʵ������һ�����캯��
	// ��ʵ�����Ĺ��캯���У�ͨ����ģ����CFunction���Σ�ʵ������һ�������࣬Ȼ��������������� new��һ��ʵ������ֵ��m_function
	template<typename _FUNCTION_, typename..._ARGS_> // ����ʱ����ָ���̺߳���
	CThread(_FUNCTION_ func, _ARGS_... args) :m_function(new CFunction<_FUNCTION_, _ARGS_...>(func, args...)) {
		m_thread = 0;
		m_bpaused = false;
	}

	~CThread() {}

public: // ��ֹ���и��ƹ���͸�ֵ���㣬��Ϊthread�������ں˶����û�̬�޷����ƹ����ֵ
	CThread(const CThread&) = delete; //  ����ʵ�� û��Ҫд�β���
	CThread operator=(const CThread&) = delete;

public:
	template<typename _FUNCTION_, typename..._ARGS_>
	int SetThreadFunc(_FUNCTION_ func, _ARGS_... args) { // �����̺߳���
		m_function = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_function == NULL) return -1;
		return 0;
	}
	int Start() {
		m_thread = new std::thread(&CThread::ThreadEntry, this);
		m_mapThread[m_thread->native_handle()] = this; // hash_map�����߳�id�͵�ǰ�߳�������ӳ���ϵ
		return 0;
	}
	int Pause() { // ��ͣ/�ָ�
		if (m_thread == nullptr) return -1; // �߳���Ч
		if (m_bpaused) { // �����ǰ������ͣ���޸�״̬��־Ϊ����
			m_bpaused = false; // ��������ͣ�Ǳߵ��źŴ�����ѭ���ͻ�������������ͣ
			return 0;
		}
		m_bpaused = true; // ��ǰ�������У��޸�״̬��־Ϊ��ͣ
		int ret = pthread_kill(m_thread->native_handle(), SIGUSR1); // ������ͣ�ź�SIGUSR1
		if (ret != 0) { // �����ź�ʧ��
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

private: // __stdcall �������ʵ������������
	static void* ThreadEntry(void* arg) { // �߳���ں������������߳�ʱ��para3 ����Ĳ�����ĳ����ʵ�������thisָ��
		CThread* thiz = (CThread*)arg; // �õ�thisָ��ſɷ�����ķǾ�̬����&��Ա
		struct sigaction act = { 0 }; // ���������źŴ�����
		sigemptyset(&act.sa_mask); // ����ź����μ���ȷ���źŴ��������ᱻ�����źŴ��
		act.sa_flags = SA_SIGINFO; // �����źŴ������ı�־Ϊ SA_SIGINFO����ʾʹ�� sa_sigaction �ֶ�ָ���ĺ�����Ϊ�źŴ�����
		act.sa_sigaction = &CThread::Sigaction; // ָ���źŴ�����
		sigaction(SIGUSR1, &act, NULL); // ע�� SIGUSR1 �źŵĴ�����Ϊ CThread::Sigaction
		sigaction(SIGUSR2, &act, NULL); // ע�� SIGUSR2 �źŵĴ�����Ϊ CThread::Sigaction

		thiz->EnterThread(); // �ô�������thisָ�����ʵ�ʵ��̹߳�������

		if (thiz->m_thread) thiz->m_thread = nullptr; // �̹߳�������
		pthread_t thread = thiz->m_thread->native_handle(); // �������࣬�п���stop()��m_thread������
		auto it = m_mapThread.find(thread);
		if (it != m_mapThread.end())
			m_mapThread[thread] = NULL;
		thiz->m_thread->join();
	}
	// �źŴ�������para1:���յ����źű�� para2:�źŵĸ�����Ϣ para3:�źŴ���ʱ����������Ϣ
	static void Sigaction(int signo, siginfo_t* info, void* context) {
		if (signo == SIGUSR1) { // ��ͣ/�ָ�
			pthread_t thread = pthread_self(); // ��ȡ��ǰ���߳�id
			auto it = m_mapThread.find(thread); // hash_map�в����߳�id�����Ӧ���߳���ʵ������
			if (it != m_mapThread.end()) { // �ҵ�
				if (it->second) { // �߳�id��Ӧ���߳������
					while (it->second->m_bpaused) { // ��ǰ�̴߳�����ͣ״̬
						usleep(1000); // ����1ms�����ѭ��
					}
				}
			}
		}
		else if (signo == SIGUSR2) { // �߳��˳�
			pthread_exit(NULL);
		}
	}
	void EnterThread() { // �̹߳�������  __thiscall ��Ҫ��������thisָ�룬����Ҫĳ������ʵ��������
		if (m_function != NULL) {
			int ret = (*m_function)(); // �� Function.h ��������CFunctionBase�����ĺ�����������������������ִ���������̺߳���
			if (ret != 0) {
				printf("%s(%d):[%s]ret = %d\n", __FILE__, __LINE__, __FUNCTION__);
			}
		}
	}
private:
	std::unique_ptr<CFunctionBase> m_function;
	//CFunctionBase* m_function; // �̺߳���ָ��
	//pthread_t m_thread; // �߳�id
	//std::unique_ptr<std::thread> m_thread;
	std::thread* m_thread; // �̶߳���ָ��
	bool m_bpaused; // true-��ͣ false-������
	static std::map<pthread_t, CThread*> m_mapThread; // �߳�id���̶߳���ָ���hash_map
};



