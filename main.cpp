#include "EdoyunPlayerServer.h"
#include "HttpParser.h"

//  日志服务器入口函数
int CreateLogServer(CProcess* proc)
{
    //printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid()); // __FILE__：当前源文件的名称 __LINE__：在源文件中的行号 __FUNCTION__：当前函数的名称
    CLoggerServer server;
    int ret = server.Start(); // 启动日志服务器
    if (ret != 0) {
        printf("%s(%d):<%s> pid=%d errno:%d msg:%s ret:%d\n",
            __FILE__, __LINE__, __FUNCTION__, getpid(), errno, strerror(errno), ret);
    }
    int fd = 0;
    while (true) {
        ret = proc->RecvFD(fd);
        printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
        if (fd <= 0)break; // 主进程发送 '-1/0' 这个特殊的fd给子进程，通知其结束
    }
    ret = server.Close();
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    return 0;
}

//  客户端处理服务器入口函数
int CreateClientServer(CProcess* proc)
{
    //printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
    int fd = -1;
    int ret = proc->RecvFD(fd); // 客户端处理进程中接收主进程发来的文件描述符
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd); // 打印收到的fd 与父进程打印出的fd并不会一样，但是确实是同一个文件
    sleep(1);
    char buf[10] = "";
    lseek(fd, 0, SEEK_SET); // 将fd的文件偏移量设置为文件开头 para2:相对于para3的偏移量 para3:开头
    read(fd, buf, sizeof(buf)); // 
    //printf("%s(%d):<%s> buf=%s\n", __FILE__, __LINE__, __FUNCTION__, buf);
    close(fd);
    return 0;
}

int LogTest()
{
    char buffer[] = "hello edoyun! 冯老师";
    usleep(1000 * 100); // 等待子进程启动
    TRACEI("here is log %d %c %f %g %s 哈哈 嘻嘻 易道云", 10, 'A', 1.0f, 2.0, buffer);
    DUMPD((void*)buffer, (size_t)sizeof(buffer));
    LOGE << 100 << " " << 'S' << " " << 0.12345f << " " << 1.23456789 << " " << buffer << " 易道云编程";
    return 0;
}

// 客户端处理模块之前的测试
int oldTest() 
{ 
    //CProcess::SwitchDeamon(); // 转为守护进程
    CProcess proclog, procclient; // 日志服务器进程 客户端处理进程
    printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid()); // 创建日志服务器进程入口函数之前
    proclog.SetEntryFunction(CreateLogServer, &proclog); // 设置日志服务器进程入口函数及参数
    int ret = proclog.CreateSubProcess(); // 启动日志服务器进程
    if (ret != 0) {
        printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid()); // 创建失败
        return -1;
    }
    LogTest(); // 测试日志模块
    printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid()); // 创建客户端处理服务器进程入口函数之前
    CThread thread(LogTest);
    thread.Start();
    procclient.SetEntryFunction(CreateClientServer, &procclient); // 设置客户端处理服务器进程入口函数及参数
    ret = procclient.CreateSubProcess(); // 启动客户端处理服务器进程
    if (ret != 0) {
        printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid()); // 创建失败
        return -2;
    }
    printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid()); // 创建成功

    // 创建子进程后，在主进程打开一个fd，发送给子进程，用以测试fd传递功能
    usleep(100 * 1000);
    int fd = open("./test.txt", O_RDWR | O_CREAT | O_APPEND); // 可读可写 | 不存在则创建 | 每次写操作会将数据追加到文件末尾而不是覆盖
    printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd); // 打印创建的fd 与子进程打印出的fd并不会一样，但是确实是同一个文件
    if (fd == -1) return -3;
    ret = procclient.SendFD(fd); // 将fd发送给客户端处理进程
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    if (ret != 0) printf("errno:%d msg:%s\n", errno, strerror(errno));
    write(fd, "edoyun", 6);
    close(fd);

    CThreadPool pool;
    ret = pool.Start(4); // 启动线程池
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
    ret = pool.AddTask(LogTest); // 给线程池发任务
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
    ret = pool.AddTask(LogTest); // 给线程池发任务
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
    ret = pool.AddTask(LogTest); // 给线程池发任务
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
    ret = pool.AddTask(LogTest); // 给线程池发任务
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
    (void)getchar();
    pool.Close();
    proclog.SendFD(-1);
    (void)getchar();

    return 0;
}

// 客户端处理模块的测试
int Main()
{
    int ret = 0;
    CProcess proclog; // 日志服务器(子进程)
    ret = proclog.SetEntryFunction(CreateLogServer, &proclog);
    ERR_RETURN(ret, -1);
    ret = proclog.CreateSubProcess();
    ERR_RETURN(ret, -2);

    CServer server; // 主模块服务器(主进程)
    CEdoyunPlayerServer business(2); // 客户端业务处理服务器 (主模块服务器的子进程)
    ret = server.Init(&business);
    ERR_RETURN(ret, -3);
    ret = server.Run();
    ERR_RETURN(ret, -4);
    return 0;
}

// http解析器的测试
int http_test()
{
    Buffer str = "GET /favicon.ico HTTP/1.1\r\n" // 请求行 包含了请求方法 (GET)、请求的资源路径 (/favicon.ico) 和 HTTP 协议的版本 (HTTP/1.1)
        "Host: 0.0.0.0=5000\r\n" // 请求头部 主机名
        "User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0\r\n" // 用户代理
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*; q = 0.8\r\n" // 接受的内容类型
        "Accept-Language: en-us,en;q=0.5\r\n" // 语言
        "Accept-Encoding: gzip,deflate\r\n" // 客户端可以接受的压缩格式
        "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n" // 客户端可以接受的字符集编码
        "Keep-Alive: 300\r\n" // 客户端在与服务器之间的连接中保持活动状态的时间（单位：s）
        "Connection: keep-alive\r\n" // 连接
        "\r\n"; // 空行标志着请求头部的结束，它是请求头部和请求体之间的分隔符

    CHttpParser parser; // 解析器
    size_t size = parser.Parser(str); // 解析
    if (parser.Errno() != 0) { // 出错
        printf("errno %d\n", parser.Errno());
        return -1;
    }
    if (size != 368) { // 解析完成的字数不对
        printf("size error:%lld  %lld\n", size, str.size());
        return -2;
    }
    printf("method %d url %s\n", parser.Method(), (char*)parser.Url()); // 打印输出 请求方法 + Url

    str = "GET /favicon.ico HTTP/1.1\r\n" // 请求报文（不完整的 错误的）
        "Host: 0.0.0.0=5000\r\n"
        "User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
    size = parser.Parser(str); // 解析
    printf("errno %d size %lld\n", parser.Errno(), size);
    if (parser.Errno() != 0x7F) { // 解析失败
        return -3;
    }
    if (size != 0) { // 解析了一部分
        return -4;
    }

    UrlParser url1("https://www.baidu.com/s?ie=utf8&oe=utf8&wd=httplib&tn=98010089_dg&ch=3"); // url解析器 m_url = 初始化给的参数，即待解析的url
    int ret = url1.Parser(); // 解析url
    if (ret != 0) { // 解析失败
        printf("urlparser1 failed:%d\n", ret);
        return -5;
    }
    printf("ie = %s except:utf8\n", (char*)url1["ie"]); // 打印查询参数的 key + value
    printf("oe = %s except:utf8\n", (char*)url1["oe"]); // 打印查询参数的 key + value
    printf("wd = %s except:httplib\n", (char*)url1["wd"]); // 打印查询参数的 key + value
    printf("tn = %s except:98010089_dg\n", (char*)url1["tn"]); // 打印查询参数的 key + value
    printf("ch = %s except:3\n", (char*)url1["ch"]); // 打印查询参数的 key + value

    UrlParser url2("http://127.0.0.1:19811/?time=144000&salt=9527&user=test&sign=1234567890abcdef"); // 第二个url解析器
    ret = url2.Parser(); // 解析url
    if (ret != 0) { // 解析失败
        printf("urlparser2 failed:%d\n", ret);
        return -6;
    }
    printf("time = %s except:144000\n", (char*)url2["time"]); // 打印查询参数的 key + value
    printf("salt = %s except:9527\n", (char*)url2["salt"]); // 打印查询参数的 key + value
    printf("user = %s except:test\n", (char*)url2["user"]); // 打印查询参数的 key + value
    printf("sign = %s except:1234567890abcdef\n", (char*)url2["sign"]); // 打印查询参数的 key + value
    printf("host:%s port:%d\n", (char*)url2.Host(), url2.Port()); // 打印查询参数的 key + value
    return 0;
}
/*
#include "Sqlite3Client.h"
DECLARE_TABLE_CLASS(user_test, _sqlite3_table_)
DECLARE_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")
DECLARE_FIELD(TYPE_VARCHAR, user_phone, NOT_NULL | DEFAULT, "VARCHAR", "(12)", "18888888888", "")
DECLARE_FIELD(TYPE_TEXT, user_name, 0, "TEXT", "", "", "")
DECLARE_TABLE_CLASS_EDN()
*/
// 用上面的宏定义来定义了一个 user_test表对象 字段有：user_id user_qq user_phone user_name
// 用下面的类定义来定义了一个 user_test表对象  显然宏定义更方便快捷
/*class user_test :public _sqlite3_table_
{
public:
    virtual PTable Copy() const {
        return PTable(new user_test(*this));
    }
    user_test() :_sqlite3_table_() {
        Name = "user_test";
        {
            PField field(new _sqlite3_field_(TYPE_INT, "user_id", NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INT", "", "", ""));
            FieldDefine.push_back(field);
            Fields["user_id"] = field;
        }
        {
            PField field(new _sqlite3_field_(TYPE_VARCHAR, "user_qq", NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "VARCHAR", "(15)", "", ""));
            FieldDefine.push_back(field);
            Fields["user_qq"] = field;
        }
    }
};*/
/*
int sql_test()
{
    user_test test, value;
    printf("create:%s\n", (char*)test.Create());
    printf("Delete:%s\n", (char*)test.Delete(test));
    value.Fields["user_qq"]->LoadFromStr("1817619619"); // 设置字段(列)值
    value.Fields["user_qq"]->Condition = SQL_INSERT; // 设置字段(列)属性为可插入
    printf("Insert:%s\n", (char*)test.Insert(value)); // 输出插入语句
    value.Fields["user_qq"]->LoadFromStr("123456789"); // 设置字段值
    value.Fields["user_qq"]->Condition = SQL_MODIFY; // 设置字段属性为可修改
    printf("Modify:%s\n", (char*)test.Modify(value)); // 输出修改语句
    printf("Query:%s\n", (char*)test.Query()); // 输出查询语句
    printf("Drop:%s\n", (char*)test.Drop()); // 输出删除语句
    getchar();
    int ret = 0;
    CDatabaseClient* pClient = new CSqlite3Client(); // sqlite3数据库对象
    KeyValue args; // 连接时的参数
    args["host"] = "test.db"; // 主机名为 test.db 这个数据库
    ret = pClient->Connect(args); // 连接数据库
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Exec(test.Create()); // 执行创建表的语句
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Exec(test.Delete(value)); // 执行在test表中删除表value的语句
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    value.Fields["user_qq"]->LoadFromStr("1817619619"); // 设置value表的字段user_qq值
    value.Fields["user_qq"]->Condition = SQL_INSERT; // 设置属性为可插入
    ret = pClient->Exec(test.Insert(value)); // 执行在test表中插入value的语句
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    value.Fields["user_qq"]->LoadFromStr("123456789"); // 设置value表的字段user_qq值
    value.Fields["user_qq"]->Condition = SQL_MODIFY; // 设置属性为可修改
    ret = pClient->Exec(test.Modify(value)); // 执行在test表中修改value的语句
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    Result result; // 结果集
    ret = pClient->Exec(test.Query(), result, test); // 查询test表 结果存放在result中
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Exec(test.Drop()); // 删除test表
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Close(); // 关闭数据库连接
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //getchar();
    return 0;
}
*/
/*
#include "MysqlClient.h"
DECLARE_TABLE_CLASS(user_test_mysql, _mysql_table_)
DECLARE_MYSQL_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_phone, NOT_NULL | DEFAULT, "VARCHAR", "(12)", "18888888888", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_name, 0, "TEXT", "", "", "")
DECLARE_TABLE_CLASS_EDN()

int mysql_test()
{
    user_test_mysql test, value;
    printf("create:%s\n", (char*)test.Create());
    printf("Delete:%s\n", (char*)test.Delete(test));
    value.Fields["user_qq"]->LoadFromStr("1817619619");
    value.Fields["user_qq"]->Condition = SQL_INSERT;
    printf("Insert:%s\n", (char*)test.Insert(value));
    value.Fields["user_qq"]->LoadFromStr("123456789");
    value.Fields["user_qq"]->Condition = SQL_MODIFY;
    printf("Modify:%s\n", (char*)test.Modify(value));
    printf("Query:%s\n", (char*)test.Query());
    printf("Drop:%s\n", (char*)test.Drop());
    getchar();
    int ret = 0;
    CDatabaseClient* pClient = new CMysqlClient();
    KeyValue args;
    args["host"] = "10.170.141.3";
    args["user"] = "Yyh";
    args["password"] = "123456";
    args["port"] = 3306;
    args["db"] = "Yyh";
    ret = pClient->Connect(args);
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Exec(test.Create());
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Exec(test.Delete(value));
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    value.Fields["user_qq"]->LoadFromStr("1817619619");
    value.Fields["user_qq"]->Condition = SQL_INSERT;
    ret = pClient->Exec(test.Insert(value));
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    value.Fields["user_qq"]->LoadFromStr("123456789");
    value.Fields["user_qq"]->Condition = SQL_MODIFY;
    ret = pClient->Exec(test.Modify(value));
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    Result result;
    ret = pClient->Exec(test.Query(), result, test);
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Exec(test.Drop());
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Close();
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //getchar();
    return 0;
}

#include "Crypto.h"
int crypto_test()
{
    Buffer data = "abcdef";
    data = Crypto::MD5(data);
    printf("except E80B5017098950FC58AAD83C8C14978E %s\n", (char*)data);
    return 0;
}*/

int main()
{
    //CProcess::SwitchDeamon(); // 转为守护进程
    int ret = 0;
    //int ret = http_test();
    //ret = sql_test();
    //ret = mysql_test();
    //ret = crypto_test();
    ret = Main();
    printf("main:ret = %d\n", ret);
    return ret;
}