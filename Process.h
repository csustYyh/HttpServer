#pragma once
#include <memory.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include "Function.h"

class CProcess // 进程类
{
public:
    CProcess() {
        m_func = NULL;
        memset(pipes, 0, sizeof(pipes));
    }
    ~CProcess() {
        if (m_func != NULL) {
            delete m_func;
            m_func = NULL;
        }
    }
    //  由于每个进程函数各不一样，我们这里使用泛型编程思想，使用模版函数 使其能接受任何类型的函数及任意数量的参数 提高代码复用性与可移植性
    template <typename _FUNCTION_, typename... _ARGS_> // 模版参数：1.函数类型(_FUNCTION_) 2.函数需要的参数类型(不固定 _ARGS_)
    int SetEntryFunction(_FUNCTION_ func, _ARGS_ ... args) { // 用以设置进程入口函数 para1:具体的进程入口函数 para2...:对应的参数
        m_func = new CFunction<_FUNCTION_, _ARGS_...>(func, args...); // 传参(类型)给模版类CFunction 用以实例化一个函数类，再使用这个函数类实例化(new)一个函数对象
        return 0;
    }
    int CreateSubProcess() { // 创建子进程
        if (m_func == NULL) return -1; // 没有设置进程入口函数，创建失败
        // 创建一对相互连接的套接字（sockets），它们可以用于双向通信。
        // 与 pipe 函数不同的是，socketpair 创建的是全双工（full-duplex）通信通道，这意味着数据可以在两个方向上同时传输。pipe 通常是半双工的，只能单方向传输
        // para1:指定套接字将在本地进行通信，而不是通过网络。适用于同一主机上的进程间通信 
        // para2:创建一个流套接字（stream socket），它提供面向连接的、可靠的字节流服务。类似于TCP，但用于本地通信 para3:使用默认协议
        // para4(出参)：整型数组，长度为2，用于存储创建的两个文件描述符（即一对相互连接的套接字）。这两个文件描述符可以在进程间或线程间进行通信
        int ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, pipes); // 通道的创建必须在fork()之前
        if (ret == -1) return -2; // 创建失败
        pid_t pid = fork(); // fork()在父进程中返回一次,若创建成功，在子进程中也会返回一次 
        if (pid == -1) return -3;
        if (pid == 0) { // 子进程 
            close(pipes[1]); // 关闭写 由于文件描述符是进程共享的，故读的进程要关闭写描述符 
            pipes[1] = 0;
            // 调用子进程函数，在本项目中即 CreateLogServer() & CreateClientServer()
            ret = (*m_func)(); // m_func->指针  *m_func->对指针解引用得到对象 (*m_func)()->该对象实现了函数调用运算符重载 
            exit(0);
        }
        // 主进程
        close(pipes[0]); // 写的进程关闭读描述符 socketpair()用于父子进程时，一般会功能分离，一个进程用来读，一个进程用来写
        pipes[0] = 0;
        m_pid = pid;
        return 0;
    }

    int SendFD(int fd) { // 主进程发送fd给子进程
        // 准备消息头（msg）和数据缓冲区（iov）
        struct msghdr msg;
        iovec iov[2];  // 可以在一次I/O操作中传输多个缓冲区或数据块
        char buf[2][10] = { "edoyun", "jueding" };
        iov[0].iov_base = buf[0];
        iov[0].iov_len = sizeof(buf[0]);
        iov[1].iov_base = buf[1];
        iov[1].iov_len = sizeof(buf[1]);
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;

        // 准备控制消息（cmsghdr）--- 使用这个结构来传递fd
        // para1:要分配的元素数量 para2:每个元素的大小 返回：指向分配内存起始位置的指针 而malloc()只接受一个参数
        // calloc() 分配的内存会被初始化为零 malloc() 分配的内存不会被初始化，其内容是未定义的，可能包含任意值
        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
        if (cmsg == NULL) return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int)); // 看这个宏定义，其实就是一个 cmsghdr 的大小+给定参数的大小 我们这里传fd，所以是一个int大小
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        *(int*)CMSG_DATA(cmsg) = fd; // 看这个宏定义，其实就是将右值(fd)赋给 cmsg->cmsg_data，由于这个成员是void*，故先强转再解引用给其赋值
        msg.msg_control = cmsg;
        msg.msg_controllen = cmsg->cmsg_len;

        ssize_t ret = sendmsg(pipes[1], &msg, 0); // 发送消息给子进程
        free(cmsg);
        if (ret == -1) {
            return -2;
        }
        return 0;
    }

    int RecvFD(int& fd) { // 子进程接收主进程发来的fd 
        struct msghdr msg; // 因为我们返回值只表状态(成功、失败的代号)， 故采用出参的方式传回fd 
        iovec iov[2]; // 接主进程发送的 "edoyun" "jueding" 
        char buf[][10] = { "", "" }; // 实际没用 我们这里主要是实现fd的发送与接收
        iov[0].iov_base = buf[0];
        iov[0].iov_len = sizeof(buf[0]);
        iov[1].iov_base = buf[1];
        iov[1].iov_len = sizeof(buf[0]);
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;

        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
        if (cmsg == NULL) return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        msg.msg_control = cmsg;
        msg.msg_controllen = cmsg->cmsg_len;

        ssize_t ret = recvmsg(pipes[0], &msg, 0); // 收到父进程传来的消息
        if (ret == -1) {
            free(cmsg); // 返回之前释放 避免内存泄漏
            return -2;
        }
        fd = *(int*)CMSG_DATA(cmsg); // 解析得到fd
        free(cmsg); // 返回之前释放 避免内存泄漏
        return 0;
    }

    int SendSocket(int fd, const sockaddr_in* addrin) {//主进程完成 用于网络服务器进程(主)将connect()上来的网络用户连接套接字及其地址传给客户端处理进程(子)
        struct msghdr msg;
        iovec iov;
        char buf[20] = "";
        bzero(&msg, sizeof(msg));
        memcpy(buf, addrin, sizeof(sockaddr_in));
        iov.iov_base = buf;
        iov.iov_len = sizeof(buf);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        // 下面的数据，才是我们需要传递的。
        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
        if (cmsg == NULL)return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        *(int*)CMSG_DATA(cmsg) = fd;
        msg.msg_control = cmsg;
        msg.msg_controllen = cmsg->cmsg_len;

        ssize_t ret = sendmsg(pipes[1], &msg, 0);
        free(cmsg);
        if (ret == -1) {
            printf("********errno %d msg:%s\n", errno, strerror(errno));
            return -2;
        }
        return 0;
    }

    int RecvSocket(int& fd, sockaddr_in* addrin) //用于客户端处理进程接收主模块进程发过来的网络用户通信套接字及其地址
    {
        msghdr msg;
        iovec iov;
        char buf[20] = "";
        bzero(&msg, sizeof(msg));
        iov.iov_base = buf;
        iov.iov_len = sizeof(buf);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
        if (cmsg == NULL)return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        msg.msg_control = cmsg;
        msg.msg_controllen = CMSG_LEN(sizeof(int));
        ssize_t ret = recvmsg(pipes[0], &msg, 0);
        if (ret == -1) {
            free(cmsg);
            return -2;
        }
        memcpy(addrin, buf, sizeof(sockaddr_in));
        fd = *(int*)CMSG_DATA(cmsg);
        free(cmsg);
        return 0;
    }

    static int SwitchDeamon() { // 将进程转为守护状态
        pid_t ret = fork();
        if (ret == -1) return -1; // 子进程创建失败
        if (ret > 0) exit(0); // 结束主进程
        // 子进程内容如下
        ret = setsid(); // 创建新会话，让子进程成为会话首进程和新的进程组组长，实现与之前的终端、会话和控制端脱离
        if (ret == -1) return -2;
        ret = fork(); // 创建孙进程
        if (ret == -1) return -3; // 创建孙进程失败
        if (ret > 0) exit(0); // 子进程到此为止
        // 孙进程内容如下，进入守护状态
        for (int i = 0; i < 3; i++) close(i); // 关闭标准输入输出
        umask(0); // 清除文件创建屏蔽字
        signal(SIGCHLD, SIG_IGN); // 忽略子进程的退出信号 表示父进程对子进程结束不关心，由内核回收
        return 0;

    }

private:
    CFunctionBase* m_func; // 进程入口函数指针 这里类型不能用CFunction 因为那个是个模版类 使用了那我这里也要弄成模版类(模版类传染) 故使用其基类作为 m_func 的类型来隔绝传染
    pid_t m_pid; // 进程id
    int pipes[2]; // 存储socketpair()创建的两个fd 本项目中，pipes[0]用于子进程读，pipes[1]用于父进程写(功能分离，如果要父子进程互传，则需要两个pipes[2])
};
