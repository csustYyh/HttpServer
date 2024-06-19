#pragma once
#include <unistd.h>
#include <sys/epoll.h>
#include <vector>
#include <errno.h>
#include <sys/signal.h>
#include <memory.h>

#define EVENT_SIZE 128
class EpollData
{
public:
	EpollData() { m_data.u64 = 0; } // �����壬��������Ǹ�����Ϊ0����������������ڴ�ռ䶼��0��
	EpollData(void* ptr) { m_data.ptr = ptr; }
	// explicit�ؼ������ڷ�ֹ���캯��������ת���б���ʽ���á��Թ��캯��ʹ��explicit�ؼ��ֵ�Ŀ���Ƿ�ֹ�������Զ�ִ�в���ȷ�����������ת�����Ӷ���ߴ���İ�ȫ�ԺͿɶ��ԡ�
	explicit EpollData(int fd) { m_data.fd = fd; } // ʹ�� explicit �ؼ��ֺ󣬱�����ʽ�ص��ù��캯��
	explicit EpollData(uint32_t u32) { m_data.u32 = u32; } // ��ֹ�������������������캯���Ĳ���
	explicit EpollData(uint64_t u64) { m_data.u64 = u64; }
	EpollData(const EpollData& data) { m_data.u64 = data.m_data.u64; } // ��ֵ���죬���ƴ����������������ڴ�ռ�
public:
	EpollData& operator=(const EpollData& data) {
		if (this != &data)
			m_data.u64 = data.m_data.u64;
		return *this;
	}
	EpollData& operator=(void* data) {
		m_data.ptr = data;
		return *this;
	}
	EpollData& operator=(int data) {
		m_data.fd = data;
		return *this;
	}
	EpollData& operator=(uint32_t data) {
		m_data.u32 = data;
		return *this;
	}
	EpollData& operator=(uint64_t data) {
		m_data.u64 = data;
		return *this;
	}
	operator epoll_data_t() { return m_data; } // ���� epoll_data_t ����ת��
	operator epoll_data_t() const { return m_data; } // ���� epoll_data_t ����ת��
	operator epoll_data_t* () { return &m_data;} // ���� epoll_data_t* ����ת��
	operator const epoll_data_t*() const { return &m_data; } // ���� const epoll_data_t* ����ת��
private:
	// epoll_data_t�����ĸ���Ա:void* ptr; int fd; uint32_t u32; uint64_t u64; ����洢��ͬ���͵����ݣ����κ�ʱ��ֻ�ܴ洢����һ��
	epoll_data_t m_data; // �����壬 ������ epoll �¼�֪ͨ�����д洢�û��Զ�������ݡ����� epoll_event �ṹ���һ����
};

// �ṹ��epoll_event����������Ա��uint32_t event; // Ҫ��ص��¼�(�ɶ�����д...)
// epoll_data_t data; �����壬������(ptr/fd/int)���ڴ洢�Զ������ݣ�һ���������������
using EPEvents = std::vector<epoll_event>; // ����epoll_event��̬����ı��� 

class CEpoll
{
public:
	CEpoll() {
		m_epoll = -1;
	}
	~CEpoll(){
		Close();
	}

public: // ��ֹ���и��ƹ���͸�ֵ���㣬��Ϊepoll�������ں˶����û�̬�޷����ƹ����ֵ ������CEpollʵ����ֻ��ͨ��Create()�ӿڴ���
	CEpoll(const CEpoll&) = delete; // ����ʵ�� û��Ҫд������
	CEpoll& operator=(const CEpoll&) = delete; // ����ʵ�֣�û��Ҫд������
public:
	operator int()const { return m_epoll;} // ���� int ����ת��
public:
	// ����epollר��fd(����)
	int Create(unsigned count) {
		if (m_epoll != -1) return -1;
		m_epoll = epoll_create1(EPOLL_CLOEXEC); // ����һ�� epoll ר�õ��ļ������� �Դ� linux 2.6.8 ֮��size �����Ǳ����Եģ�Ҳ����˵������ֻ�д��� 0 ������ֵ
		if (m_epoll == -1) return -2; // ����ʧ��
		return 0;
	}
	// �ȴ��¼��ķ��� <0->�������� =0->û�����鷢�� >0->�ɹ��õ��¼�
	ssize_t waitEvents(EPEvents& events, int timeout = 10) { // para1:��������epoll���صķ������¼� para2:��ʱʱ��
		if (m_epoll == -1) return -1; // û�д���epollר�õ�fd
		EPEvents evs(EVENT_SIZE); // evc��epoll��ص�fd�����¼�(epoll_event)�Ķ�̬���� ��ʼ��Ϊ128��
		// epoll_wait()���ȴ��¼��Ĳ������ռ��� epoll ��ص��¼����Ѿ����͵��¼��������� select() ����
		// para1:epoll ר�õ��ļ���������epoll_create()�ķ���ֵ
		// para2: epoll_event �ṹ�����飬epoll����ѷ������¼���ֵ��evs������ evs.data()��ʾָ�������̬�����ָ��
		// para3:��֪�ں˼�ص��¼��ж��ٸ� para4:��ʱʱ�䣬��λms��-1��ʾ����Ϊ����
		// ����ֵ���ɹ����򷵻���Ҫ������¼���Ŀ 0�����ѳ�ʱ -1������ʧ��
		int ret = epoll_wait(m_epoll, evs.data(), (int)evs.size(), timeout); // vector��,.data()����һ��ָ�������е�һ��Ԫ�ص�ָ��
		if (ret == -1) {
			if ((errno == EINTR) || (errno == EAGAIN)) // ����ϵͳ�жϺ���ѭ
				return 0;
			return -2;
		}
		if (ret > (int)events.size()) { // ��epoll���ط������¼���>events����������Գ�������
			events.resize(ret);
		}
		memcpy(events.data(), evs.data(), sizeof(epoll_event) * ret); // ͨ������events��epoll���صķ����¼�����
		return ret; // ������Ҫ������¼���
	}
	// ע��fd
	int Add(int fd, const EpollData& data = EpollData((void*)0), uint32_t events = EPOLLIN) {
		// para1:�����ص�fd para2���Զ������ݣ�һ���������ݶ������ para3:��Ҫ�������¼����ͣ�Ĭ��Ϊ�ɶ� EPOLLOUT-��д EPOLLET-����Ϊ��Ե����
		if (m_epoll == -1) return -1; // û�д���epollר�õ�fd
		epoll_event ev = { events, data };
		// epoll_ctl():epoll ���¼�ע�ắ��������ͬ�� select() ���ڼ����¼�ʱ�����ں�Ҫ����ʲô���͵��¼���������������ע��Ҫ�������¼�����
		// para1:epoll ר�õ��ļ���������epoll_create()�ķ���ֵ 
		// para2:��ʾ������������������ʾ:EPOLL_CTL_ADD ע���µ�fd��para1��, EPOLL_CTL_MOD �޸��Ѿ�ע���fd�ļ����¼� EPOLL_CTL_DEL ɾ��һ��fd
		// para3:��Ҫ������fd para4:�����ں�Ҫ����ʲô�¼� ��ev�е�events
		// ����ֵ��0��ʾ�ɹ� -1��ʾʧ��
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev);
		if (ret == -1) return -2;
		return 0;
	}
	// �޸��Ѿ�ע���fd�ļ����¼�
	int Modify(int fd, uint32_t events, const EpollData& data = EpollData((void*)0)){
		if (m_epoll == -1) return -1; // û�д���epollר�õ�fd
		epoll_event ev = { events, data };
		// 
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &ev);
		if (ret == -1) return -2;
		return 0;
	}
	// ɾ���Ѿ�ע���fd
	int Del(int fd) {
		if (m_epoll == -1) return -1; // û�д���epollר�õ�fd
		//
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, NULL);
		if (ret == -1) return -2;
		return 0;
	}
	// �ر�epoll
	void Close() {
		if (m_epoll != -1) {
			int fd = m_epoll;
			m_epoll = -1;
			close(fd);
		}
	}
private:
	int m_epoll; // epoll ר�õ��ļ�������(ע�⣬��������Ҫ����IO�¼���fd)
};

