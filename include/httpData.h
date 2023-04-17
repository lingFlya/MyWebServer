/*
 *@author RedDragon
 *@date 2020/8/26
 *@brief 解析http请求的相关函数和类
 * http状态机
 * while就是为了状态的转移
 **/
#ifndef WEBSERVER_HTTPDATA_H
#define WEBSERVER_HTTPDATA_H
#include <string>
#include <unordered_map>
using std::string;
using std::unordered_map;

// 解析http request报文的状态
enum class ParseRequest{
    PARSESTARTLINE,// 首行(请求行)
    PARSEHEADERS,  // 头部字段
    PARSEBODY,     // 主体数据
    SENDRESPONE,   // 发送响应
    KEEPALIVE,     // 长连接
    FINISH,        // 完成
    ERROR
};

// 解析结果(通用, 无论是解析请求行还是头部字段)
enum class ParseResult{
    AGAIN,
    ERROR,
    SUCCESS
};

enum class SendResult{
    SUCCESS,
    NOTFOUND,
    NOTIMPL,
    ERROR
};

enum class httpMethod{
    // http1.0
    GET,
    POST,
    HEAD,
    // http1.1新增
    OPTIONS,
    DELETE,
    PUT,
    TRACE,
    PATCH,
    CONNECT,
    ERROR,
};

class Timer;// 前向声明(不要再带上#include "Timer.h", 反而会错)

class httpData
{
private:
    int againTime;          // 重复尝试readn的次数
    int clientFd;           // 客户端fd
    string content;         // readn()读到的内容(也就是请求报文的所有内容)
    httpMethod method;      // 此次请求的方法
    // http版本
    int h_major;              // 主版本号
    int h_minor;              // 次版本号
    ParseRequest parseState;  // 当前解析request报文的状态
    bool isKeepAlive;         // 长连接
    string url;             // 请求url
    string resPath;         // 资源文件夹(长连接用得到)
    unordered_map<std::string, std::string> headerMap;// 所有头部字段

    // 解析请求行（第一行）
    ParseResult parse_StartLine();
    // 解析头部字段，并保存到headers
    ParseResult parse_Headers();
    // 解析主体内容
    ParseResult parse_Body();
    // 处理请求, 简单实现了GET和POST
    SendResult sendResponse();
    // 发送响应失败的处理方式
    void handleError(int statusCode, std::string short_msg);

public:
    Timer* timer;

    httpData();
    httpData(int cfd, string resPath);
    ~httpData();
    int getFd()const {return clientFd;}
    ParseRequest handleRequest();// 解析http请求的 起点
    void reset();
};

#endif
