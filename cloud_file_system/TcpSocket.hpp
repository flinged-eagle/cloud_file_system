#ifndef _M_TCPSOCK__
#define _M_TCPSOCK__ 
#include<iostream>
#include<netinet/in.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/types.h>
#include<string>

class TcpSocket{
public:
  TcpSocket()
    :_socketfd(-1)
  {}
//创建socket
  bool Socket()
  {
    _socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(_socketfd < 0)
    {
      std::cerr << "socket error\n";
      return false;
    }
    return true;
  }
//绑定端口
  bool Bind(int16_t port)
  {
    int opt = 1;
    //端口被占用就开启地址重用
    //套接口设置
    setsockopt(_socketfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(int));
    struct sockaddr_in addr;
    //inet_addr : 将一个点分十进制转化位一个（长整数）u_long
    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    int ret = bind(_socketfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
    if(ret < 0)
    {
      std::cerr<<"bind error\n";
      return false;
    }
    return true;
  }
//监听
  bool Listen(int backlog = 5)
  {
    //backlog 等待连接队列的最大长度
    int ret = listen(_socketfd, backlog);
    if(ret < 0)
    {
      std::cerr<<"listen error\n";
      return false;
    }
    return true;
  }
//接受socket
  bool Accept(TcpSocket& newsock)
  {
    //返回型参数
    struct sockaddr_in addr;
    socklen_t len = sizeof(struct sockaddr_in);
    int ret = accept(_socketfd, (struct sockaddr*)&addr, &len);
    //成功返回文件操作符，失败返回-1
    if(ret < 0)
    {
      std::cerr<<"accpet error\n";
      return false;
    }
    newsock._socketfd = ret;
    return true;
  }
//探测接受，不取出缓冲区的数据
  bool RecvPeek(std::string& tmp)
  {
    char buf[1024*10] = {0};
    //MSG_PEEK 探测接受
    int ret = recv(_socketfd, buf, sizeof(buf), MSG_PEEK);
    if(ret <= 0)
    {
      //非阻塞情况下，无数据可读
      if(errno == EAGAIN)
      {
        return true;
      }
      return false;
    }
    //替换ret个字节的buf中的内容到tmp中
    tmp.assign(buf, ret);
    return true;
  }
//设置非阻塞
  void SetNonBlock()
  {
    //获取文件的访问格式和访问状态
    //fcntl():针对文件描述符进行控制 F_GETFL:取得文件描述符的旗标
    int flag = fcntl(_socketfd, F_GETFL, 0);
    fcntl(_socketfd, F_SETFL, O_NONBLOCK | flag);
  }

//接受文件
bool Recv(std::string& buf, int length)
{
  buf.resize(length);
  //已经读取的字节数
  int rlen = 0;
  while(rlen < length)
  {
    int ret = recv(_socketfd, &buf[0]+rlen, length-rlen, 0);
    if(ret <= 0)
    {
      //缓冲区中读取完
      if(errno == EAGAIN)
      {
        usleep(1000);
        continue;
      }
      std::cerr<<"recv error\n";
      return false;
    }
    rlen += ret;
  }
  return true;
}
//发送文件
  bool Send(const std::string& buf)
  {
    //sock非阻塞，不能阻塞传输
    int64_t slen = 0;
    while(slen < buf.size())
    {
      int ret = send(_socketfd, &buf[0] + slen, buf.size() - slen, 0);
      if(ret < 0)
      {
        if(errno == EAGAIN)
        {
          usleep(1000);
          continue;
        }
        std::cerr<<"send error\n";
        return false;
      }
      slen += ret;
    }
    return true;
  }
  int GetFd()
  {  
    return _socketfd;
  }
  void SetFd(int fd)
  {
    _socketfd = fd;
  }
  void Close()
  {
    close(_socketfd);
  }
private:
  int _socketfd;
};

#endif
