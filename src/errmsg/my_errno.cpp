#include "errmsg/my_errno.h"

#include <cstring>
#include <cstdlib>
#include <errno.h>

#include <vector>


static int max_errno = 200;
static error_msg_t* unknown_err_msg = NULL;
static std::vector<error_msg_t*> error_msg_array;


int strerror_init()
{
    const char* unknown_err = "Unknown error";
    int len = strlen(unknown_err);
    unknown_err_msg = (error_msg_t*)malloc(sizeof(error_msg_t) + len + 1);
    unknown_err_msg->len = len;
    memcpy(unknown_err_msg->data, unknown_err, len);
    unknown_err_msg->data[len] = '\0';


    // 错误码的数量在不同系统上不同, 这里假设最多 max_errno 个错误码; Linux通过 _sys_nerr查看是135个;
    // 当碰到非法错误码n, strerror调用返回字符串 Unknown error n, 也直接存起来;
    error_msg_array.reserve(max_errno);
    for(int my_errno = 0; my_errno < max_errno; my_errno++)
    {
        char* msg = strerror(my_errno);

        int len = strlen(msg);
        error_msg_t* sys_err = (error_msg_t*)malloc(sizeof(error_msg_t) + len + 1);
        sys_err->len = len;
        memcpy(sys_err->data, msg, len);
        sys_err->data[len] = '\0';

        error_msg_array.push_back(sys_err);
    }
    return 0;
}


int strerror_destroy()
{
    for(error_msg_t* err_msg : error_msg_array)
    {
        free(err_msg);
    }
    error_msg_array.clear();

    return 0;
}


char* my_strerror_cpy(int err_num, char* err_msg, size_t size)
{
    const error_msg_t* msg;

    if(err_num >= max_errno)
    {
        msg  = unknown_err_msg;
    }
    else
    {
        msg = error_msg_array[err_num];
    }
    size = std::min((size_t)msg->len, size);
    memcpy(err_msg, msg->data, size);
    return err_msg;
}


const char* my_strerror(int err_num)
{
    const error_msg_t* msg;

    if(err_num >= max_errno)
    {
        msg = unknown_err_msg;
    }
    else
    {
        msg = error_msg_array[err_num];
    }
    return msg->data;
}
