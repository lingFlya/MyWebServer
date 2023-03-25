#include "httpData.h"
#include "util/util.h"
#include "timer.h"

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <cstring>

using namespace std;
// 对EAGAIN这样的错误尝试超过一定的次数就抛弃
const int AGAIN_MAX_TIMES = 5;
extern const uint64_t TIMEOUT = 30000; // 要设置和main.cpp中的一样
extern TimerManager timerQueue;    // 所有计时器

httpData::httpData()
    : httpData(-1, "/")
{}

// 初始化列表的顺序必须和class的变量申明顺序一致
httpData::httpData(int cfd, string resource)
        : againTime(0),clientFd(cfd),
          method(httpMethod::ERROR),h_major(-1), h_minor(-1),
          parseState(ParseRequest::PARSESTARTLINE),isKeepAlive(false),
          resPath(resource), timer(nullptr)
{}

httpData::~httpData()
{}

ParseResult httpData::parse_StartLine()
{
    string::size_type pos = content.find("\r\n");
    if(pos == string::npos)
        return ParseResult::AGAIN;
    // 得到请求行
    string request_line = content.substr(0 ,pos);
    content.erase(0, pos + 2);// \r\n可以删除了

    // 分析请求行
    // http method
    string md;
    size_t i = 0;
    while(i < request_line.size() && request_line[i] != ' ')
        md += request_line[i++];
    if(md == "GET")
        method = httpMethod::GET;
    else if(md == "POST")
        method = httpMethod::POST;
    else if(md == "HEAD")
        method = httpMethod::HEAD;
    else
        return ParseResult::ERROR;

    // url
    url = resPath;
    if(i == request_line.size())
        return ParseResult::ERROR;
    ++i;
    if(request_line[i] != '/')
        return ParseResult::ERROR;
    while(request_line[i] != ' ' && i < request_line.size())
        url.push_back(request_line[i++]);
    // 看下是否有查询字符串, 也就是额外的参数
    pos = url.find('?');
    if(pos != string::npos)
        url = url.substr(0, pos); // 这里选择忽略额外参数
    // 目录
    if(url.back() == '/')
        url += "index.html";

    // HTTP版本号
    string ver = request_line.substr(i + 1);
    if(ver.size() != 8)
        return ParseResult::ERROR;
    ver = ver.substr(5);
    if(ver.size() != 3)
        return ParseResult::ERROR;
    h_major = ver[0] - '0';
    if(ver[1] != '.')
        return ParseResult::ERROR;
    h_minor = ver[2] - '0';
    //  下一个状态, 解析头部字段
    return ParseResult::SUCCESS;
}

ParseResult httpData::parse_Headers()
{
    while(true)
    {
        string::size_type pos = content.find("\r\n");
        if(pos == string::npos)
            return ParseResult::AGAIN;// 数据不完整
        string tmp(content.substr(0, pos));
        content.erase(0, pos + 2);// 从content删掉这一行, \r\n也删掉
        // 碰到空行, 头部解析完成
        if(tmp.empty())
            return ParseResult::SUCCESS;
        else
        {
            pos = tmp.find(':');
            if(pos == string::npos)
                break;// 格式错误
            string key = tmp.substr(0, pos);
            string value = tmp.substr(pos+2);
            headerMap.insert(pair<string,string>(key, value));
        }
    }
    return ParseResult::ERROR;
}

ParseResult httpData::parse_Body()
{
    // 首先确定有没有body
    auto item = headerMap.find("Content-Length");
    if(item == headerMap.end())
        return ParseResult::ERROR;
    size_t len = stoi(item->second);
    if(content.size() < len)
        return ParseResult::AGAIN;
    // body内容都在content中了
    return ParseResult::SUCCESS;
}

SendResult httpData::sendResponse()
{
    char send_header[4096] = "HTTP/1.1 200 OK\r\n";
    // 长连接
    // keep-alive写成keep_alive导致设置长连接失败,注意格式
    if(headerMap.find("Connection") != headerMap.end() && headerMap["Connection"] == "keep-alive")
    {
        this->isKeepAlive = true;
        sprintf(send_header, "%sConnection: keep-alive\r\n", send_header);
        sprintf(send_header, "%sKeep-Alive: timeout=%lu\r\n", send_header, TIMEOUT/1000);
    }
    else
    {
        this->isKeepAlive = false;
        sprintf(send_header, "%sConnection: close\r\n", send_header);
    }

    // 处理GET和POST
    if(method == httpMethod::POST)
    {
        char send_content[4096] = "I have recv this!";
        sprintf(send_header, "%sContent-Type: text/plain\r\n", send_header);
        sprintf(send_header, "%sContent-Length: %zu\r\n", send_header, strlen(send_content));
        // 加上空行
        sprintf(send_header, "%s\r\n%s", send_header, send_content);
        // 发送头部
        int sendLen = util::writen(clientFd, send_header, strlen(send_header));
        if((size_t)sendLen != strlen(send_header))
            return SendResult::ERROR;

        // 发送body
        sendLen = util::writen(clientFd, send_content, strlen(send_content));
        if((size_t)sendLen != strlen(send_content))
            return SendResult::ERROR;
        printf("成功接收POST请求! 内容: %s\n", content.data());
    }
    else if(method == httpMethod::GET || method == httpMethod::HEAD)
    {
        size_t dot_pos = url.find('.');
        string fileType;
        if(dot_pos == string::npos)
            fileType = util::getMimeType("default");// 无后缀, 当作文本文件展示
        else
            fileType = util::getMimeType(url.substr(dot_pos));
        // 先判断文件是否存在
        if(access(url.data(), F_OK) == -1)
            return SendResult::NOTFOUND;
        // 获取文件大小
        struct stat statbuf;
        if(-1 == stat(url.data(), &statbuf))
            return SendResult::NOTFOUND;
        sprintf(send_header, "%sContent-Type: %s\r\n", send_header, fileType.data());
        sprintf(send_header, "%sContent-Length: %ld\r\n", send_header, statbuf.st_size);
        sprintf(send_header, "%s\r\n", send_header);// 空行, 头部结束

        // 发送头部
        size_t send_len = util::writen(clientFd, send_header, strlen(send_header));
        if(send_len != strlen(send_header))
            return SendResult::ERROR;
        // 如果是HEAD请求的话,只要发送头部
        if(method == httpMethod::GET)
        {// 发送body, 也就是发送文件内容
            int fd = open(url.data(), O_RDONLY);
            int ret = sendfile(clientFd, fd, nullptr, statbuf.st_size);
            if (ret != statbuf.st_size)
                return SendResult::ERROR;
        }
    }
    else
    {
        printf("get, post之外的http method, 没有实现\n");
        return SendResult::NOTIMPL;
    }
    return SendResult::SUCCESS;
}

// 处理http请求，一切的起点
ParseRequest httpData::handleRequest()
{
    char buf[4096];
    bool isError = false;
    while(parseState != ParseRequest::FINISH){
        // 读数据
        int readSum = util::readn(clientFd, buf, 4096);
        if(readSum < 0)
        {
            isError = true;
            break;
        }
        else if(readSum == 0)
        {
            // 有请求但是读不到数据，可能是Request Aborted, 或者是对方的数据还没有到达
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {// 到达一定次数, 就不再尝试
                if(againTime > AGAIN_MAX_TIMES)
                    isError = true;
                else
                    ++againTime;
            }
            else if(errno != 0)
            {
                isError = true;// 有错误发生
                break;
            }
            else
                break;// 对端关闭, 也会返回0
        }
        else
            // 将读到的数据添加到content成员变量中
            content += std::string(buf, buf + readSum);

        // 状态机解析
        if(this->parseState == ParseRequest::PARSESTARTLINE)
        {// 处于解析请求头的状态
            ParseResult flag = parse_StartLine();
            if(flag == ParseResult::AGAIN)
                continue; // 重新进行while循环, 再尝试一次readn
            else if(flag == ParseResult::ERROR)
            {
                isError = true;
                break;
            }
            else
                parseState = ParseRequest::PARSEHEADERS;
        }
        // 解析头部字段
        if(this->parseState == ParseRequest::PARSEHEADERS)
        {
            ParseResult flag = parse_Headers();
            if(flag == ParseResult::AGAIN)
                continue;
            else if(flag == ParseResult::ERROR)
            {
                isError = true;
                break;
            }
            else
            {// get请求也可以有body数据, 但是通常不建议
                if(method == httpMethod::POST)
                    parseState = ParseRequest::PARSEBODY;
                else
                    parseState = ParseRequest::SENDRESPONE;
            }
        }
        // 解析body
        if(this->parseState == ParseRequest::PARSEBODY)
        {
            ParseResult flag = parse_Body();
            if(flag == ParseResult::AGAIN)
                continue;
            else if(flag == ParseResult::ERROR)
            {
                isError = true;
                break;
            }
            else
                parseState = ParseRequest::SENDRESPONE;
        }
        // 分析请求
        if(this->parseState == ParseRequest::SENDRESPONE)
        {
            SendResult flag = sendResponse();
            switch (flag)
            {
                case SendResult::SUCCESS:
                    parseState = ParseRequest::FINISH;
                    break;
                case SendResult::NOTFOUND:
                    perror("sendResponse");
                    handleError(404, "Not Found!");
                    break;
                case SendResult::NOTIMPL:
                    handleError(501, "Not Implemented!");
                    break;
                case SendResult::ERROR:
                    perror("sendResponse");
                    isError = true;
                    break;
            }
            break;
        }
    }
    if(isError)
        return ParseRequest::ERROR;
    if(isKeepAlive)
        return ParseRequest::KEEPALIVE;
    return parseState;
}

void httpData::handleError(int statusCode, std::string short_msg)
{
    short_msg = " " + short_msg;
    string body =
            "<html>"
            "   <title>Error</title>"
            "   <body><h1>";
    body += to_string(statusCode) + short_msg+ "<h1/>";
    body += "       <hr><em> Linglong's Web Server</em>"
            "   </body>"
            "</html>";

    string header;
    header += "HTTP/1.1 " + to_string(statusCode) + short_msg + "\r\n";
    header += "Content-type: text/html\r\n";
    header += "Connection: close\r\n";
    header += "Content-length: " + to_string(body.size()) + "\r\n";
    header += "\r\n";

    util::writen(clientFd, header.data(), header.size());// 写入header
    util::writen(clientFd, body.data(), body.size());// 写入body
}

void httpData::reset()
{
    againTime = 0;
    content.clear();
    method = httpMethod::ERROR;
    parseState = ParseRequest::PARSESTARTLINE;
    url.clear();
    this->h_major = this->h_minor = -1;
    headerMap.clear();
}