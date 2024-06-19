#include "Logger.h"

LogInfo::LogInfo(
	const char* file, int line, const char* func,
	pid_t pid, pthread_t tid, int level,
	const char* fmt, ...
)
{

	const char sLevel[][8] = { // 静态二维字符数组，存储日志级别的字符串表示
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	}; // sLevel[0]->"INFO" sLevel[1]->"DEBUG"... 
	char* buf = NULL;
	bAuto = false;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ", // 将格式化的日志信息写到buf,文件名、行号、日志级别、
		file, line, sLevel[level],							  // 时间、进程id、线程id、函数名
		(char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0) { // 格式化成功 
		m_buf = buf; // m_buf:存储日志信息的字符串缓冲区
		free(buf);
	}
	else return;

	va_list ap; // 指向参数列表的指针，va_list用于存储可变参数的类型，在使用可变参数函数时需要声明一个 va_list 类型的变量来存储参数列表
	va_start(ap, fmt); // 将 ap 初始化为参数列表的起始位 fmt 是日志格式字符串，用于确定可变参数的位置。va_start 宏会将 ap 设置为参数列表中 fmt 后面的第一个可变参数的位置
	count = vasprintf(&buf, fmt, ap); // 将参数列表中的可变参数格式化为字符串，并将结果保存到 buf 中
	if (count > 0) {
		m_buf += buf;
		free(buf);
	}
	m_buf += "\n";
	va_end(ap); // 结束可变参数列表的访问
}

LogInfo::LogInfo(const char* file, int line, const char* func, pid_t pid, pthread_t tid, int level)
{//自己主动发的 流式的日志 
	bAuto = true; // LogInfo退出作用域时，调用析构函数时 检测m_bool 如果为true 则调用 CLoggerServer::Trace(*this)发送
	const char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = NULL;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ",
		file, line, sLevel[level],
		(char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0) {
		m_buf = buf;
		free(buf);
	}
}

LogInfo::LogInfo( // 根据给定的日志信息和二进制数据，格式化生成一条完整的日志消息
	const char* file, int line, const char* func,
	pid_t pid, pthread_t tid, int level,
	void* pData, size_t nSize
)
{
	const char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = NULL;
	bAuto = false;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s)\n",
		file, line, sLevel[level],
		(char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0) {
		m_buf = buf;
		free(buf);
	}
	else return;

	Buffer out;
	size_t i = 0;
	char* Data = (char*)pData;
	for (; i < nSize; i++)
	{
		char buf[16] = ""; // 使用循环遍历数据。在每次迭代中，将数据转换为十六进制格式，并追加到 m_buf 中
		snprintf(buf, sizeof(buf), "%02X ", Data[i] & 0xFF);
		m_buf += buf; 
		if (0 == ((i + 1) % 16)) { // 每行最多打印16个字节的数据，并在每16个字节后添加一个分隔符和对应的ASCII字符表示
			m_buf += "\t; ";
			char buf[17] = "";
			memcpy(buf, Data + i - 15, 16);
			for (int j = 0; j < 16; j++)
				if ((buf[j] < 32) && (buf[j] >= 0))buf[j] = '.';
			m_buf += buf;
			m_buf += "\n";
		}
	}
	//处理尾巴
	size_t k = i % 16; // 在循环结束后，可能存在不足16个字节的数据剩余，需要特殊处理，保证格式的一致性
	if (k != 0) {
		for (size_t j = 0; j < 16 - k; j++) m_buf += "   ";
		m_buf += "\t; ";
		for (size_t j = i - k; j <= i; j++) {
			if ((Data[i] & 0xFF) > 31 && ((Data[j] & 0xFF) < 0x7F)) {
				m_buf += Data[i];
			}
			else {
				m_buf += '.';
			}
		}
		m_buf += "\n";
	}
}

LogInfo::~LogInfo()
{
	if (bAuto) {
		m_buf += "\n";
		CLoggerServer::Trace(*this);
	}
}
