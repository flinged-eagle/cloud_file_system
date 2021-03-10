#ifndef _M_EPOLL__
#define _M_EPOLL__ 
#include<sys/epoll.h>
#include<vector>
#include<iostream>
#include<unistd.h>
#define MAX_EPOLL 1024

class Epoll{
  public:
    bool init()
    {
      _epfd = epoll_create(MAX_EPOLL);
      if(_epfd < 0)
      {
        std::cerr<<"epoll_create error\n";
        return false;
      }
      return true;
    }

    bool Add(int fd)
    {
      struct epoll_event ev;
      ev.events = EPOLLIN;
      ev.data.fd = fd;
      int ret = epoll_ctl(_epfd, EPOLL_CTL_ADD, fd, &ev);
      if(ret < 0)
      {
        std::cerr<<"epoll add error\n";
        return false;
      }
      return true;
    }

    bool Del(int fd)
    {
      int ret = epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
      if(ret < 0)
      {
        std::cerr<<"remove monitor error\n";
        return false;
      }
      return true;
    }
//等待事件的产生
    bool Wait(std::vector<int>& _list_fd, int timeout = 3000)
    {
        struct epoll_event eve[MAX_EPOLL];
        int ret = epoll_wait(_epfd, eve, MAX_EPOLL, timeout);
        if(ret < 0)
        {
          std::cerr<<"epoll_wait error\n";
          return false;
        }else if(ret == 0)
        {
          return false;
        }
        
        for(int i=0; i<ret; ++i)
        {
          _list_fd.push_back(eve[i].data.fd);
        }
        return true;
    }
  private:
    int _epfd;
};
#endif
