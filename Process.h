#pragma once
#include <memory.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include "Function.h"

class CProcess // ������
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
    //  ����ÿ�����̺�������һ������������ʹ�÷��ͱ��˼�룬ʹ��ģ�溯�� ʹ���ܽ����κ����͵ĺ��������������Ĳ��� ��ߴ��븴���������ֲ��
    template <typename _FUNCTION_, typename... _ARGS_> // ģ�������1.��������(_FUNCTION_) 2.������Ҫ�Ĳ�������(���̶� _ARGS_)
    int SetEntryFunction(_FUNCTION_ func, _ARGS_ ... args) { // �������ý�����ں��� para1:����Ľ�����ں��� para2...:��Ӧ�Ĳ���
        m_func = new CFunction<_FUNCTION_, _ARGS_...>(func, args...); // ����(����)��ģ����CFunction ����ʵ����һ�������࣬��ʹ�����������ʵ����(new)һ����������
        return 0;
    }
    int CreateSubProcess() { // �����ӽ���
        if (m_func == NULL) return -1; // û�����ý�����ں���������ʧ��
        // ����һ���໥���ӵ��׽��֣�sockets�������ǿ�������˫��ͨ�š�
        // �� pipe ������ͬ���ǣ�socketpair ��������ȫ˫����full-duplex��ͨ��ͨ��������ζ�����ݿ���������������ͬʱ���䡣pipe ͨ���ǰ�˫���ģ�ֻ�ܵ�������
        // para1:ָ���׽��ֽ��ڱ��ؽ���ͨ�ţ�������ͨ�����硣������ͬһ�����ϵĽ��̼�ͨ�� 
        // para2:����һ�����׽��֣�stream socket�������ṩ�������ӵġ��ɿ����ֽ�������������TCP�������ڱ���ͨ�� para3:ʹ��Ĭ��Э��
        // para4(����)���������飬����Ϊ2�����ڴ洢�����������ļ�����������һ���໥���ӵ��׽��֣����������ļ������������ڽ��̼���̼߳����ͨ��
        int ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, pipes); // ͨ���Ĵ���������fork()֮ǰ
        if (ret == -1) return -2; // ����ʧ��
        pid_t pid = fork(); // fork()�ڸ������з���һ��,�������ɹ������ӽ�����Ҳ�᷵��һ�� 
        if (pid == -1) return -3;
        if (pid == 0) { // �ӽ��� 
            close(pipes[1]); // �ر�д �����ļ��������ǽ��̹���ģ��ʶ��Ľ���Ҫ�ر�д������ 
            pipes[1] = 0;
            // �����ӽ��̺������ڱ���Ŀ�м� CreateLogServer() & CreateClientServer()
            ret = (*m_func)(); // m_func->ָ��  *m_func->��ָ������õõ����� (*m_func)()->�ö���ʵ���˺���������������� 
            exit(0);
        }
        // ������
        close(pipes[0]); // д�Ľ��̹رն������� socketpair()���ڸ��ӽ���ʱ��һ��Ṧ�ܷ��룬һ��������������һ����������д
        pipes[0] = 0;
        m_pid = pid;
        return 0;
    }

    int SendFD(int fd) { // �����̷���fd���ӽ���
        // ׼����Ϣͷ��msg�������ݻ�������iov��
        struct msghdr msg;
        iovec iov[2];  // ������һ��I/O�����д����������������ݿ�
        char buf[2][10] = { "edoyun", "jueding" };
        iov[0].iov_base = buf[0];
        iov[0].iov_len = sizeof(buf[0]);
        iov[1].iov_base = buf[1];
        iov[1].iov_len = sizeof(buf[1]);
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;

        // ׼��������Ϣ��cmsghdr��--- ʹ������ṹ������fd
        // para1:Ҫ�����Ԫ������ para2:ÿ��Ԫ�صĴ�С ���أ�ָ������ڴ���ʼλ�õ�ָ�� ��malloc()ֻ����һ������
        // calloc() ������ڴ�ᱻ��ʼ��Ϊ�� malloc() ������ڴ治�ᱻ��ʼ������������δ����ģ����ܰ�������ֵ
        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
        if (cmsg == NULL) return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int)); // ������궨�壬��ʵ����һ�� cmsghdr �Ĵ�С+���������Ĵ�С �������ﴫfd��������һ��int��С
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        *(int*)CMSG_DATA(cmsg) = fd; // ������궨�壬��ʵ���ǽ���ֵ(fd)���� cmsg->cmsg_data�����������Ա��void*������ǿת�ٽ����ø��丳ֵ
        msg.msg_control = cmsg;
        msg.msg_controllen = cmsg->cmsg_len;

        ssize_t ret = sendmsg(pipes[1], &msg, 0); // ������Ϣ���ӽ���
        free(cmsg);
        if (ret == -1) {
            return -2;
        }
        return 0;
    }

    int RecvFD(int& fd) { // �ӽ��̽��������̷�����fd 
        struct msghdr msg; // ��Ϊ���Ƿ���ֵֻ��״̬(�ɹ���ʧ�ܵĴ���)�� �ʲ��ó��εķ�ʽ����fd 
        iovec iov[2]; // �������̷��͵� "edoyun" "jueding" 
        char buf[][10] = { "", "" }; // ʵ��û�� ����������Ҫ��ʵ��fd�ķ��������
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

        ssize_t ret = recvmsg(pipes[0], &msg, 0); // �յ������̴�������Ϣ
        if (ret == -1) {
            free(cmsg); // ����֮ǰ�ͷ� �����ڴ�й©
            return -2;
        }
        fd = *(int*)CMSG_DATA(cmsg); // �����õ�fd
        free(cmsg); // ����֮ǰ�ͷ� �����ڴ�й©
        return 0;
    }

    int SendSocket(int fd, const sockaddr_in* addrin) {//��������� �����������������(��)��connect()�����������û������׽��ּ����ַ�����ͻ��˴������(��)
        struct msghdr msg;
        iovec iov;
        char buf[20] = "";
        bzero(&msg, sizeof(msg));
        memcpy(buf, addrin, sizeof(sockaddr_in));
        iov.iov_base = buf;
        iov.iov_len = sizeof(buf);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        // ��������ݣ�����������Ҫ���ݵġ�
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

    int RecvSocket(int& fd, sockaddr_in* addrin) //���ڿͻ��˴�����̽�����ģ����̷������������û�ͨ���׽��ּ����ַ
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

    static int SwitchDeamon() { // ������תΪ�ػ�״̬
        pid_t ret = fork();
        if (ret == -1) return -1; // �ӽ��̴���ʧ��
        if (ret > 0) exit(0); // ����������
        // �ӽ�����������
        ret = setsid(); // �����»Ự�����ӽ��̳�Ϊ�Ự�׽��̺��µĽ������鳤��ʵ����֮ǰ���նˡ��Ự�Ϳ��ƶ�����
        if (ret == -1) return -2;
        ret = fork(); // ���������
        if (ret == -1) return -3; // ���������ʧ��
        if (ret > 0) exit(0); // �ӽ��̵���Ϊֹ
        // ������������£������ػ�״̬
        for (int i = 0; i < 3; i++) close(i); // �رձ�׼�������
        umask(0); // ����ļ�����������
        signal(SIGCHLD, SIG_IGN); // �����ӽ��̵��˳��ź� ��ʾ�����̶��ӽ��̽��������ģ����ں˻���
        return 0;

    }

private:
    CFunctionBase* m_func; // ������ں���ָ�� �������Ͳ�����CFunction ��Ϊ�Ǹ��Ǹ�ģ���� ʹ������������ҲҪŪ��ģ����(ģ���ഫȾ) ��ʹ���������Ϊ m_func ��������������Ⱦ
    pid_t m_pid; // ����id
    int pipes[2]; // �洢socketpair()����������fd ����Ŀ�У�pipes[0]�����ӽ��̶���pipes[1]���ڸ�����д(���ܷ��룬���Ҫ���ӽ��̻���������Ҫ����pipes[2])
};
