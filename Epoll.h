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
	EpollData() { m_data.u64 = 0; } // 联合体，让其最长的那个变量为0，则整个联合体的内存空间都置0了
	EpollData(void* ptr) { m_data.ptr = ptr; }
	// explicit关键字用于防止构造函数在类型转换中被隐式调用。对构造函数使用explicit关键字的目的是防止编译器自动执行不明确或意外的类型转换，从而提高代码的安全性和可读性。
	explicit EpollData(int fd) { m_data.fd = fd; } // 使用 explicit 关键字后，必须显式地调用构造函数
	explicit EpollData(uint32_t u32) { m_data.u32 = u32; } // 防止编译器混淆这三个构造函数的参数
	explicit EpollData(uint64_t u64) { m_data.u64 = u64; }
	EpollData(const EpollData& data) { m_data.u64 = data.m_data.u64; } // 赋值构造，复制传入的整个联合体的内存空间
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
	operator epoll_data_t() { return m_data; } // 重载 epoll_data_t 类型转换
	operator epoll_data_t() const { return m_data; } // 重载 epoll_data_t 类型转换
	operator epoll_data_t* () { return &m_data;} // 重载 epoll_data_t* 类型转换
	operator const epoll_data_t*() const { return &m_data; } // 重载 const epoll_data_t* 类型转换
private:
	// epoll_data_t包括四个成员:void* ptr; int fd; uint32_t u32; uint64_t u64; 允许存储不同类型的数据，但任何时候只能存储其中一种
	epoll_data_t m_data; // 联合体， 用于在 epoll 事件通知机制中存储用户自定义的数据。它是 epoll_event 结构体的一部分
};

// 结构体epoll_event包括两个成员：uint32_t event; // 要监控的事件(可读、可写...)
// epoll_data_t data; 联合体，可以是(ptr/fd/int)用于存储自定义数据，一般用来传额外参数
using EPEvents = std::vector<epoll_event>; // 创建epoll_event动态数组的别名 

class CEpoll
{
public:
	CEpoll() {
		m_epoll = -1;
	}
	~CEpoll(){
		Close();
	}

public: // 禁止进行复制构造和赋值运算，因为epoll本质是内核对象，用户态无法复制构造或赋值 构造完CEpoll实例后，只能通过Create()接口创建
	CEpoll(const CEpoll&) = delete; // 不会实现 没必要写参数名
	CEpoll& operator=(const CEpoll&) = delete; // 不会实现，没必要写参数名
public:
	operator int()const { return m_epoll;} // 重载 int 类型转换
public:
	// 创建epoll专用fd(树根)
	int Create(unsigned count) {
		if (m_epoll != -1) return -1;
		m_epoll = epoll_create1(EPOLL_CLOEXEC); // 返回一个 epoll 专用的文件描述符 自从 linux 2.6.8 之后，size 参数是被忽略的，也就是说可以填只有大于 0 的任意值
		if (m_epoll == -1) return -2; // 创建失败
		return 0;
	}
	// 等待事件的发生 <0->发生错误 =0->没有事情发生 >0->成功拿到事件
	ssize_t waitEvents(EPEvents& events, int timeout = 10) { // para1:用来接收epoll返回的发生的事件 para2:超时时间
		if (m_epoll == -1) return -1; // 没有创建epoll专用的fd
		EPEvents evs(EVENT_SIZE); // evc：epoll监控的fd及其事件(epoll_event)的动态数组 初始化为128个
		// epoll_wait()：等待事件的产生，收集在 epoll 监控的事件中已经发送的事件，类似于 select() 调用
		// para1:epoll 专用的文件描述符，epoll_create()的返回值
		// para2: epoll_event 结构体数组，epoll将会把发生的事件赋值到evs数组中 evs.data()表示指向这个动态数组的指针
		// para3:告知内核监控的事件有多少个 para4:超时时间，单位ms，-1表示函数为阻塞
		// 返回值：成功，则返回需要处理的事件数目 0，则已超时 -1，则已失败
		int ret = epoll_wait(m_epoll, evs.data(), (int)evs.size(), timeout); // vector中,.data()返回一个指向容器中第一个元素的指针
		if (ret == -1) {
			if ((errno == EINTR) || (errno == EAGAIN)) // 忽略系统中断和轮循
				return 0;
			return -2;
		}
		if (ret > (int)events.size()) { // 若epoll返回发生的事件数>events的容量，则对出参扩容
			events.resize(ret);
		}
		memcpy(events.data(), evs.data(), sizeof(epoll_event) * ret); // 通过出参events将epoll返回的发生事件传出
		return ret; // 返回需要处理的事件数
	}
	// 注册fd
	int Add(int fd, const EpollData& data = EpollData((void*)0), uint32_t events = EPOLLIN) {
		// para1:加入监控的fd para2：自定义数据，一般用来传递额外参数 para3:需要监听的事件类型，默认为可读 EPOLLOUT-可写 EPOLLET-设置为边缘触发
		if (m_epoll == -1) return -1; // 没有创建epoll专用的fd
		epoll_event ev = { events, data };
		// epoll_ctl():epoll 的事件注册函数，它不同于 select() 是在监听事件时告诉内核要监听什么类型的事件，而是在这里先注册要监听的事件类型
		// para1:epoll 专用的文件描述符，epoll_create()的返回值 
		// para2:表示动作，用三个宏来表示:EPOLL_CTL_ADD 注册新的fd到para1中, EPOLL_CTL_MOD 修改已经注册的fd的监听事件 EPOLL_CTL_DEL 删除一个fd
		// para3:需要监听的fd para4:告诉内核要监听什么事件 即ev中的events
		// 返回值：0表示成功 -1表示失败
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev);
		if (ret == -1) return -2;
		return 0;
	}
	// 修改已经注册的fd的监听事件
	int Modify(int fd, uint32_t events, const EpollData& data = EpollData((void*)0)){
		if (m_epoll == -1) return -1; // 没有创建epoll专用的fd
		epoll_event ev = { events, data };
		// 
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &ev);
		if (ret == -1) return -2;
		return 0;
	}
	// 删除已经注册的fd
	int Del(int fd) {
		if (m_epoll == -1) return -1; // 没有创建epoll专用的fd
		//
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, NULL);
		if (ret == -1) return -2;
		return 0;
	}
	// 关闭epoll
	void Close() {
		if (m_epoll != -1) {
			int fd = m_epoll;
			m_epoll = -1;
			close(fd);
		}
	}
private:
	int m_epoll; // epoll 专用的文件描述符(注意，并不是需要监听IO事件的fd)
};

