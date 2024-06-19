#pragma once
#include "Logger.h"
#include "Server.h"
#include "HttpParser.h"
#include "Crypto.h"
#include "MysqlClient.h"
#include "jsoncpp/json.h"
#include <map>
#include <fstream>
#include <iomanip>

DECLARE_TABLE_CLASS(edoyunLogin_user_mysql, _mysql_table_)
DECLARE_MYSQL_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")  //QQ号
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_phone, DEFAULT, "VARCHAR", "(11)", "", "")  //手机
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_name, NOT_NULL, "TEXT", "", "", "")    //姓名
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_nick, NOT_NULL, "TEXT", "", "", "")    //昵称
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat, DEFAULT, "TEXT", "", "NULL", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat_id, DEFAULT, "TEXT", "", "NULL", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_address, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_province, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_country, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_age, DEFAULT | CHECK, "INTEGER", "", "18", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_male, DEFAULT, "BOOL", "", "1", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_flags, DEFAULT, "TEXT", "", "0", "")
DECLARE_MYSQL_FIELD(TYPE_REAL, user_experience, DEFAULT, "REAL", "", "0.0", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_level, DEFAULT | CHECK, "INTEGER", "", "0", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_class_priority, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_REAL, user_time_per_viewer, DEFAULT, "REAL", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_career, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_password, NOT_NULL, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_birthday, NONE, "DATETIME", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_describe, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_education, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_register_time, DEFAULT, "DATETIME", "", "", "")
DECLARE_TABLE_CLASS_EDN()

/*
* 1. 客户端的地址问题   --- 主模块服务器通过sockpair()发送通信套接字时，不光发送fd还要发送其地址 SendSocket()+RecvSocket()
* 2. 连接回调的参数问题 ---
* 3. 接收回调的参数问题 --- 
*/
#define ERR_RETURN(ret, err) if(ret!=0){TRACEE("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));return err;}

#define WARN_CONTINUE(ret) if(ret!=0){TRACEW("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));continue;}

class CEdoyunPlayerServer :
	public CBusiness // 继承自业务抽象接口类
{
public:
	CEdoyunPlayerServer(unsigned count) :CBusiness() {
		m_count = count;
	}
	~CEdoyunPlayerServer() {
		if (m_db) {
			CDatabaseClient* db = m_db;
			m_db = NULL;
			db->Close();
			delete db;
		}
		m_epoll.Close();
		m_pool.Close();
		for (auto it : m_mapClients) {
			if (it.second) {
				delete it.second;
			}
		}
		m_mapClients.clear();
	}
	// 客户端处理模块的主线程
	// 1.设置连接回调函数&接收回调函数 2.创建epoll树根 3.启动线程池 4.给线程池中的所有子线程分配任务ThreadFunc
	// 5.将主模块发来的通信套接字挂树(自己的epoll树) 6.调用连接回调函数
	virtual int BusinessProcess(CProcess* proc) { 
		using namespace std::placeholders;
		int ret = 0;    
		m_db = new CMysqlClient();
		if (m_db == NULL) {
			TRACEE("no more memory!");
			return -1;
		}
		KeyValue args;
		args["host"] = "10.170.141.3";
		args["user"] = "Yyh";
		args["password"] = "123456";
		args["port"] = 3306;
		args["db"] = "Yyh";
		ret = m_db->Connect(args);
		ERR_RETURN(ret, -2);
		edoyunLogin_user_mysql user;
		ret = m_db->Exec(user.Create());
		ERR_RETURN(ret, -3);
		ret = setConnectedCallback(&CEdoyunPlayerServer::Connected, this, _1); // 设置连接回调函数(传this，因为Connected()是一个成员函数 _thiscall)
		ERR_RETURN(ret, -4); // 若出错 写日志
		ret = setRecvCallback(&CEdoyunPlayerServer::Received, this, _1, _2); // 设置接收回调函数 std::placeholders_1/_2 都是占位符 告诉可变参数的模版函数，我还没有确定参数是什么，但是位置先占好
		ERR_RETURN(ret, -5);
		ret = m_epoll.Create(m_count); // 创建epoll树根(eventpoll)
		ERR_RETURN(ret, -6);
		ret = m_pool.Start(m_count); // 启动线程池
		ERR_RETURN(ret, -7);
		for (unsigned i = 0; i < m_count; i++) { // 给线程池所有线程分派任务
			ret = m_pool.AddTask(&CEdoyunPlayerServer::ThreadFunc, this);
			ERR_RETURN(ret, -8);
		}
		int sock = 0;
		sockaddr_in addrin; // 用于存放主模块发来的客户端地址
		while (m_epoll != -1) { // 将主模块发来的通信套接字挂树
			ret = proc->RecvSocket(sock, &addrin); // 接收主模块发过来的与网络用户的通信套接字及其地址
			TRACEI("RecvSocket ret=%d", ret);
			if (ret < 0 || (sock == 0)) break;
			CSocketBase* pClient = new CSocket(sock); // 根据通信套接字新建套接字对象
			if (pClient == NULL) continue;
			ret = pClient->Init(CSockParam(&addrin, SOCK_ISIP));
			WARN_CONTINUE(ret);
			ret = m_epoll.Add(sock, (EpollData)(void*)pClient); // 通信套接字挂树，注意是挂在客户端处理模块的epoll树上
			if (m_connectedcallback) {
				(*m_connectedcallback)(pClient); // 回调 打印主进程(服务器)发给我们的连上来的用户的ip和端口
			}
			WARN_CONTINUE(ret); // 写WARNNING日志
		}
		return 0;
	}
private:
	// 连接回调函数
	int Connected(CSocketBase* pClient) {
		//TODO:客户端连接处理 简单打印一下客户端信息
		sockaddr_in* paddr = (sockaddr_in*)*pClient;
		TRACEI("client connected addr %s port:%d", inet_ntoa(paddr->sin_addr), paddr->sin_port);
		return 0;
	}
	// 接收回调函数 
	int Received(CSocketBase* pClient, const Buffer& data) {
		TRACEI("接收到数据！");
		//TODO:主要业务，在此处理 即客户端处理进程中，线程池(子线程)干的事
		//HTTP 解析
		int ret = 0;
		Buffer response = "";
		ret = HttpParser(data); // 1.解析Http请求 2.处理登录 3.查询数据库检查登录请求
		TRACEI("HttpParser ret=%d", ret);
		//验证结果的反馈
		if (ret != 0) {//验证失败
			TRACEE("http parser failed!%d", ret);
		}
		response = MakeResponse(ret); // 响应包
		ret = pClient->Send(response);
		if (ret != 0) {
			TRACEE("http response failed!%d [%s]", ret, (char*)response);
		}
		else {
			TRACEI("http response success!%d", ret);
		}

		return 0;
	}
	int HttpParser(const Buffer& data) {
		CHttpParser parser;
		size_t size = parser.Parser(data);
		if (size == 0 || (parser.Errno() != 0)) {
			TRACEE("size %llu errno:%u", size, parser.Errno());
			return -1;
		}
		TRACEE("parser.Method() =  %d ", parser.Method());
		if (parser.Method() == HTTP_GET) { // 如果是 GET 请求
			//get 处理
			UrlParser url("https://10.170.141.3" + parser.Url());
			int ret = url.Parser();
			if (ret != 0) {
				TRACEE("ret = %d url[%s]", ret, "https://10.170.141.3" + parser.Url());
				return -2;
			}
			const Buffer uri = url.Uri();
			TRACEI("**** uri = %s", (char*)uri);
			if (uri == "login") { // 且 URI 为 "login
				//处理登录 提取 URL 中的参数：时间、盐值、用户名和签名
				Buffer time = url["time"]; 
				Buffer salt = url["salt"];
				Buffer user = url["user"];
				Buffer sign = url["sign"];
				TRACEI("time %s salt %s user %s sign %s", (char*)time, (char*)salt, (char*)user, (char*)sign);
				//数据库的查询
				edoyunLogin_user_mysql dbuser;
				Result result;
				Buffer sql = dbuser.Query("user_name=\"" + user + "\""); // 构造 SQL 查询语句，查询数据库中对应的用户名信息
				ret = m_db->Exec(sql, result, dbuser);
				if (ret != 0) { // 查询失败
					TRACEE("sql=%s ret=%d", (char*)sql, ret);
					return -3;
				}
				if (result.size() == 0) { // 没有查询结果
					TRACEE("no result sql=%s ret=%d", (char*)sql, ret);
					return -4;
				}
				if (result.size() != 1) { // 查询结果不唯一
					TRACEE("more than one sql=%s ret=%d", (char*)sql, ret);
					return -5;
				}
				auto user1 = result.front(); // 结果集中的第一个列对象是 用户id
				Buffer pwd = *user1->Fields["user_password"]->Value.String; // 根据用户id查数据库中的用户密码
				TRACEI("password = %s", (char*)pwd);
				//登录请求的验证
				const char* MD5_KEY = "*&^%$#@b.v+h-b*g/h@n!h#n$d^ssx,.kl<kl";
				Buffer md5str = time + MD5_KEY + pwd + salt; // 使用预定义的 MD5_KEY 和从 URL 中提取的时间、盐值和数据库查到的用户密码构造一个字符串
				Buffer md5 = Crypto::MD5(md5str); // 计算MD5哈希值
				TRACEI("md5 = %s", (char*)md5);
				if (md5 == sign) { // 如果计算出的 MD5 哈希值与 URL 中的签名相匹配，则表示登录成功
					return 0;
				}
				return -6; // 登录失败
			}
		}
		else if (parser.Method() == HTTP_POST) {
			//post 处理
		}
		return -7;
	}
	Buffer MakeResponse(int ret) { // 生成http响应包
		Json::Value root;
		root["status"] = ret;
		if (ret != 0) {
			root["message"] = "登录失败，可能是用户名或者密码错误！";
		}
		else {
			root["message"] = "success";
		}
		root["hello"] = "niwudile, dycha";
		Buffer json = root.toStyledString();
		Buffer result = "HTTP/1.1 200 OK\r\n"; // 状态行
		// 获取当前时间
		time_t t;
		time(&t);
		tm* ptm = localtime(&t);
		char temp[64] = "";
		strftime(temp, sizeof(temp), "%a, %d %b %G %T GMT\r\n", ptm);
		// 创建 HTTP 头部
		Buffer Date = Buffer("Date: ") + temp;
		Buffer Server = "Server: Edoyun/1.0\r\nContent-Type: text/html; charset=utf-8\r\nX-Frame-Options: DENY\r\n";
		snprintf(temp, sizeof(temp), "%d", json.size());
		Buffer Length = Buffer("Content-Length: ") + temp + "\r\n";
		Buffer Stub = "X-Content-Type-Options: nosniff\r\nReferrer-Policy: same-origin\r\n\r\n";
		// 拼接响应
		result += Date + Server + Length + Stub + json;
		TRACEI("response: %s", (char*)result);
		return result;
	}



private:
	// 1.子进程共享epoll树根 2.等待epoll上事件(网络用户套接字的可读事件)发生 3.调用接收回调函数
	int ThreadFunc() { // 等待epoll上事件(网络用户套接字的可读事件)发生，回调处理
		int ret = 0;
		EPEvents events;
		while (m_epoll != -1) {
			ssize_t  size = m_epoll.waitEvents(events);
			if (size < 0) break;
			if (size > 0) {
				for (ssize_t i = 0; i < size; i++)
				{
					if (events[i].events & EPOLLERR) {
						break;
					}
					else if (events[i].events & EPOLLIN) { // 通信套接字上有可读事件
						CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
						if (pClient) {
							Buffer data;
							ret = pClient->Recv(data); // 读内核接收缓冲区
							TRACEI("recv data size %d", ret);
							if (ret <= 0) {
								TRACEW("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));
								m_epoll.Del(*pClient);
								continue;
							}
							if (m_recvcallback) {
								(*m_recvcallback)(pClient, data); // 回调
							}
						}
					}
				}
			}
		}
		return 0;
	}
private:
	CEpoll m_epoll; // 客户端处理模块的epoll 监听通信套接字
	std::map<int, CSocketBase*> m_mapClients; // 套接字与套接字对象映射表 就是管理客户端处理模块的通信套接字呗 
	CThreadPool m_pool; // 线程池，用于处理网络用户的I/O请求
	unsigned m_count; // 客户端处理模块的线程个数
	CDatabaseClient* m_db; // 数据库
};
// 客户端处理模块：
// 通信双方：服务器->客户端处理模块 客户端->主模块(发送通信套接字) + 网络用户(发送服务请求)
// 任务：1.主线程接收主模块发送过来的与网络用户的通信套接字，然后挂epoll树 2.客户端处理线程等待这些套接字上的I/O事件发生，然后处理
// 需求：  线程池 + epoll + 管理通信套接字的数据结构


