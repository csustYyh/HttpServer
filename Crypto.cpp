#include "Crypto.h"
#include <openssl/md5.h>

Buffer Crypto::MD5(const Buffer& text) // ���������ı��� MD5 hashֵ
{
    Buffer result;
    Buffer data(16); // ���ڴ洢 MD5 �㷨���ɵ� 128 λ��16 �ֽڣ���ϣֵ
    MD5_CTX md5; // MD5 �㷨�������Ľṹ�����ڱ��� MD5 �㷨���м�״̬
    MD5_Init(&md5); // ��ʼ�� MD5 ������
    MD5_Update(&md5, text, text.size()); // ������ text ���µ� MD5 ��������
    MD5_Final(data, &md5); // �������յ� MD5 ��ϣֵ����������洢�� data ��
    char temp[3] = "";
    for (size_t i = 0; i < data.size(); i++)  // �� MD5 ��ϣֵת��Ϊ�ַ�����ʽ
    {   // ��ÿ���ֽڵ�ֵ��ʽ��Ϊ��λ��ʮ�������ַ���(Сд)�����洢�� result ��
        snprintf(temp, sizeof(temp), "%02x", data[i] & 0xFF);
        result += temp;
    }
    return result;
}
