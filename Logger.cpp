#include "Logger.h"

LogInfo::LogInfo(
	const char* file, int line, const char* func,
	pid_t pid, pthread_t tid, int level,
	const char* fmt, ...
)
{

	const char sLevel[][8] = { // ��̬��ά�ַ����飬�洢��־������ַ�����ʾ
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	}; // sLevel[0]->"INFO" sLevel[1]->"DEBUG"... 
	char* buf = NULL;
	bAuto = false;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ", // ����ʽ������־��Ϣд��buf,�ļ������кš���־����
		file, line, sLevel[level],							  // ʱ�䡢����id���߳�id��������
		(char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0) { // ��ʽ���ɹ� 
		m_buf = buf; // m_buf:�洢��־��Ϣ���ַ���������
		free(buf);
	}
	else return;

	va_list ap; // ָ������б��ָ�룬va_list���ڴ洢�ɱ���������ͣ���ʹ�ÿɱ��������ʱ��Ҫ����һ�� va_list ���͵ı������洢�����б�
	va_start(ap, fmt); // �� ap ��ʼ��Ϊ�����б����ʼλ fmt ����־��ʽ�ַ���������ȷ���ɱ������λ�á�va_start ��Ὣ ap ����Ϊ�����б��� fmt ����ĵ�һ���ɱ������λ��
	count = vasprintf(&buf, fmt, ap); // �������б��еĿɱ������ʽ��Ϊ�ַ���������������浽 buf ��
	if (count > 0) {
		m_buf += buf;
		free(buf);
	}
	m_buf += "\n";
	va_end(ap); // �����ɱ�����б�ķ���
}

LogInfo::LogInfo(const char* file, int line, const char* func, pid_t pid, pthread_t tid, int level)
{//�Լ��������� ��ʽ����־ 
	bAuto = true; // LogInfo�˳�������ʱ��������������ʱ ���m_bool ���Ϊtrue ����� CLoggerServer::Trace(*this)����
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

LogInfo::LogInfo( // ���ݸ�������־��Ϣ�Ͷ��������ݣ���ʽ������һ����������־��Ϣ
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
		char buf[16] = ""; // ʹ��ѭ���������ݡ���ÿ�ε����У�������ת��Ϊʮ�����Ƹ�ʽ����׷�ӵ� m_buf ��
		snprintf(buf, sizeof(buf), "%02X ", Data[i] & 0xFF);
		m_buf += buf; 
		if (0 == ((i + 1) % 16)) { // ÿ������ӡ16���ֽڵ����ݣ�����ÿ16���ֽں����һ���ָ����Ͷ�Ӧ��ASCII�ַ���ʾ
			m_buf += "\t; ";
			char buf[17] = "";
			memcpy(buf, Data + i - 15, 16);
			for (int j = 0; j < 16; j++)
				if ((buf[j] < 32) && (buf[j] >= 0))buf[j] = '.';
			m_buf += buf;
			m_buf += "\n";
		}
	}
	//����β��
	size_t k = i % 16; // ��ѭ�������󣬿��ܴ��ڲ���16���ֽڵ�����ʣ�࣬��Ҫ���⴦����֤��ʽ��һ����
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
