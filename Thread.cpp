#include "Thread.h"

std::map<pthread_t, CThread*> CThread::m_mapThread; // static¹Ø¼ü×Ö¿ÉÊ¡ÂÔ