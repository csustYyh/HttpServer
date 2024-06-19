#include "Crypto.h"
#include <openssl/md5.h>

Buffer Crypto::MD5(const Buffer& text) // 计算输入文本的 MD5 hash值
{
    Buffer result;
    Buffer data(16); // 用于存储 MD5 算法生成的 128 位（16 字节）哈希值
    MD5_CTX md5; // MD5 算法的上下文结构，用于保存 MD5 算法的中间状态
    MD5_Init(&md5); // 初始化 MD5 上下文
    MD5_Update(&md5, text, text.size()); // 将数据 text 更新到 MD5 上下文中
    MD5_Final(data, &md5); // 计算最终的 MD5 哈希值，并将结果存储在 data 中
    char temp[3] = "";
    for (size_t i = 0; i < data.size(); i++)  // 将 MD5 哈希值转换为字符串形式
    {   // 将每个字节的值格式化为两位的十六进制字符串(小写)，并存储在 result 中
        snprintf(temp, sizeof(temp), "%02x", data[i] & 0xFF);
        result += temp;
    }
    return result;
}
