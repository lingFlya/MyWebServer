/**
 * @author  2mu
 * @date    2024/3/28
 * @brief   关于errno的实现
 */
#include <cstddef>


typedef struct {
    unsigned short len;
    char data[]; // 柔性数组
} __attribute__((__packed__)) error_msg_t;


/**
 * @brief 提前记录好所有errno的错误描述, 因为man手册说 strerror 不是线程安全的; strerror_r才是线程安全, 
 * 但是nginx源码说 strerror_r不是Async-signal-safe的, signal handler中用会不安全。
 * @return 0成功, 否则返回错误码
 */
int strerror_init();


/**
 * @brief 释放strerror相关的内存
 * @return 0表示成功
 */
int strerror_destroy();


/**
 * @brief 根据传入的错误码，将相关的错误描述字符串拷贝到err_msg中，并返回
 * @param err 错误码
 * @param err_msg 错误码对应的错误信息描述
 * @param size err_msg字符串长度, 长度不允许超过65535（也不可能超过）
 * @return err_msg
 */
char* my_strerror_cpy(int err_num, char *err_msg, size_t size);


/**
 * @brief 直接返回对应错误码的描述字符串, 返回的字符串只读，不允许修改, 多线程读应该是安全的
 * @param err_num 错误码
 * @return 错误码对应的字符串
 */
const char* my_strerror(int err_num);
