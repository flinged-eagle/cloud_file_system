#ifndef _M_SRV__
#define _M_SRV__ 
#include<stdlib.h>
#include<fstream>
#include<sys/wait.h>
#include"TcpSocket.hpp"
#include"Epoll.hpp"
#include"ThreadPool.hpp"
#include"http.hpp"

#define _ROOT "./root"

class Server{
  public:
    Server(){}
    static bool HttpProcess(HttpRequest& req, HttpResponse& rsp)
    {
      //根据请求进行划分
      //GET：
      //POST:
      std::string realpath = _ROOT+req._path;
      if(!boost::filesystem::exists(realpath))
      {
        std::cerr<<"exist error\n";
        std::cerr<<realpath<<std::endl;
        rsp._status = 404;
        return false;
      }
      if((req._method == "GET" && req._param.size() != 0) || req._method == "POST")
      {
        CGI(req,rsp);
      }else
      {
        //文件下载/目录列表请求
        if(boost::filesystem::is_directory(realpath))
        {
          if(!ListShow(realpath, rsp._body))
          {
            rsp._status = 400;
            return false;
          }
          rsp._status = 200;
          rsp.SetHeader("Content-Type","text/html");
        }else 
        {
          RangeDownload(req,rsp);
          return true;
        }
      }
      return true;
    }

    static bool RangeDownload(HttpRequest& req, HttpResponse& rsp)
    {
      std::string realpath = _ROOT + req._path;
      int64_t data_len = boost::filesystem::file_size(realpath);
      int64_t last_time = boost::filesystem::last_write_time(realpath);
      //生成Etag,判断是否为同一文件
      std::cout<<"realpath: "<<realpath<<"datalen: "<< data_len<<"pthraed: "<<pthread_self()<<std::endl;
      std::string etag = std::to_string(data_len) + std::to_string(last_time);
      rsp.SetHeader("Accept-Ranges","bytes");
      rsp.SetHeader("Content-Type","application/octet-stream");
      rsp.SetHeader("Connection","close");
      auto it = req._headers.find("Range");
      if(it == req._headers.end())
      {
        Download(realpath, 0, data_len, rsp._body);
        rsp._status = 200;
      }else 
      {
        std::string range = it->second;
        std::string unit = "bytes=";
        size_t pos = range.find(unit);
        if(pos == std::string::npos)
        {
          return false;
        }
        pos += unit.size();
        size_t pos2 = range.find("-",pos);
        if(pos2 == std::string::npos)
        {
          return false;
        }

        std::string start = range.substr(pos,pos2-pos);
        std::string end = range.substr(pos2+1);
        int64_t dig_start, dig_end;
        dig_start = str_to_digit(start);
        if(end.size() == 0)
        {
          dig_end = data_len -1;
        }else 
        {
          dig_end = str_to_digit(end);
        }

        int64_t range_len = dig_end - dig_start +1;
        Download(realpath, dig_start, range_len, rsp._body);
        std::stringstream tmp;
        tmp<<"bytes "<<dig_start<<"-"<<dig_end<<"/"<<data_len;

        rsp.SetHeader("Content-Range",tmp.str());
        rsp._status = 206;
      }
      rsp.SetHeader("Etag",etag);
      return true;
    }

    static int str_to_digit(std::string& str)
    {
      return atoi(str.c_str());
    }

    static bool Download(std::string& path, int64_t start, int64_t len, std::string& body)
    {
      body.resize(len);
      //二进制流方式打开
      std::ifstream file(path, std::ios::binary);
      if(!file.is_open())
      {
        std::cerr<<"open file error\n";
        return false;
      }
      // ios::beg 从文件头开始计算偏移量
      // seek get 
      file.seekg(start, std::ios::beg);
      file.read(&body[0], len);
      if(!file.good())
      {
        std::cerr<<"read file data error\n";
        return false;
      }
      file.close();
      return true;
    }

    static bool CGI(HttpRequest& req, HttpResponse& rsp)
    {
      int pipe_in[2];
      int pipe_out[2];
      if(pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
      {
        std::cerr<<"pipe error\n";
        return false;
      }
      //利用环境变量进行头部信息的传输
      for(auto& e : req._headers)
      {
        int ret = setenv(e.first.c_str(), e.second.c_str(), 1);
        std::cout<<e.first.c_str() <<" = "<<e.second.c_str()<<std::endl;
        if(ret < 0)
        {
          std::cerr<<"setenv error\n";
        }
      }
      //利用管道进行正文信息的传输
      //利用进程进行数据传输，稳定性高，意外退出，不会导致主进程宕机
      pid_t pid = fork();
      if(pid < 0)
      {
        std::cerr<<"fork error\n";
        return false;
      }else if(pid == 0)
      {
        //子进程
        close(pipe_in[0]);  //子进程 关闭读端  写
        close(pipe_out[1]); //子进程 关闭写端  读
        dup2(pipe_in[1],1);
        dup2(pipe_out[0],0);
        std::string realpath = _ROOT + req._path;
        int ret = execl(realpath.c_str(), realpath.c_str(), NULL);
        if(ret < 0)
        {
          std::cerr<<"execl error\n";
          return false;
        }
        exit(0);
      }
      //主进程
      close(pipe_in[1]);  //父进程  关闭写段 读
      close(pipe_out[0]); //父进程  关闭读端 写
      int rlen = 0;
      int len = req._body.size();
      while(rlen < len)
      {
        int ret = write(pipe_out[1], &req._body[rlen], len-rlen);
        if(ret < 0)
        {
          std::cerr<<"write error\n";
          return false;
        }
        //所有读端关闭
        if(ret == 0)
        {
          break;
        }
        rlen += ret;
      }
      //读数据
      while(1)
      {
        char buf[1024] = {0};
        int ret = read(pipe_in[0], buf, 1024);
        if(ret < 0)
        {
          std::cerr<<"read error\n";
          rsp._status = 500;
          return false;
        }
        if(ret == 0)
        {
          break;
        }
        buf[ret] = '\0';
        rsp._body += buf;
      }
      rsp._status = 200;
      //关闭管道符
      close(pipe_out[1]);
      close(pipe_in[0]);

      wait(NULL);
      return true;
    }
  
  static bool ListShow(std::string& path, std::string& _body)
  {
    if(path.find(_ROOT) == std::string::npos)
    {
      return false;
    }
    std::string www = _ROOT;
    std::string root_path = path.substr(path.find(www)+www.size());
    std::string retpath = root_path.substr(0,root_path.rfind("/"));
    if(retpath.empty())
    {
      retpath = "/";
    }
    std::stringstream tmp;
    tmp<<"<html>";
    tmp<<"<head><title>shared_directory</title></head><br>\n";
    tmp<<"<form action='/upload' method=\"POST\" enctype=\"multipart/form-data\">\n";
    tmp<<"<div>\n";
    tmp<<"<input type='file' name='fileupload'>\n";
    tmp<<"<input type='submit' name='submit'>\n";
    tmp<<"</div></form>\n";
    tmp<<"<ol><strong><a href='"<<retpath<<"'>parent directory</a></strong></ol>\n";
    //组织文件节点信息
    boost::filesystem::directory_iterator iter_begin(path);
    boost::filesystem::directory_iterator iter_end;

    for(; iter_begin != iter_end; ++iter_begin)
    {
      std::string pathname = iter_begin->path().string();
      std::string name = iter_begin->path().filename().string();
      std::string url;
      if(root_path != "/")
      {
        url = root_path + '/'+name;
      }else 
      {
        url = root_path + name;
      }
      //判断是否为目录/文件
      if(boost::filesystem::is_directory(pathname))
      {
        tmp<<"<p>\n";
        tmp<<"<ol><strong><a href='";
        tmp<<url<<"'>"<<name<<"</a></strong><br>\n";
        tmp<<"<small>file_type:directory</small><br>\n";
        tmp<<"</ol></p>\n";
      }else 
      {
        int64_t ssize = boost::filesystem::file_size(pathname);
        int64_t last_write = boost::filesystem::last_write_time(pathname);
        tmp<<"<p>\n";
        tmp<<"<ol><strong><a href = '";
        tmp<<url<<"'>";
        tmp<<name<<"</a></strong><br>\n";
        tmp<<"<small>ssize = "<<ssize/1024<<"kb"<<"</small><br>\n";
        tmp<<"<small>last_write_time = "<<last_write<<"</small><br>\n";
        tmp<<"</ol>\n";
        tmp<<"</p>\n";
      }
    }
    tmp<<"</body>\n";
    tmp<<"</html>";
    _body = tmp.str();

    return true;
  }

  //线程处理函数
  static void ThreadHandler(int socketfd)
  {
    TcpSocket sock;
    sock.SetFd(socketfd);
    HttpRequest req;
    HttpResponse rsp;
    int status = req.RequestParse(sock);
    if(status != 200)
    {
      rsp._status = status;
      rsp.ErrorProcess(sock);
      sock.Close();
      return ;
    }
    HttpProcess(req,rsp);
    rsp.NormalProcess(sock);
    sock.Close();
  }

  bool start(uint16_t port = 9000)
  {
    _lst_sock.Socket();
    _lst_sock.Bind(port);
    _lst_sock.Listen();
    _lst_sock.SetNonBlock();

    e.init();
    e.Add(_lst_sock.GetFd());
    pool.ThreadInit();

    while(1)
    {
      std::vector<int> list;
      if(e.Wait(list) == false)
      {
        sleep(1);
        continue;
      }
      for(auto&i : list)
      {
        if(_lst_sock.GetFd() == i)
        {
          std::cout<<i<<std::endl;
          TcpSocket newsock;
          if(_lst_sock.Accept(newsock) == false)
          {
            continue;
          }
          newsock.SetNonBlock();
          e.Add(newsock.GetFd());
        }else 
        {
          e.Del(i);
          ThreadTask tt(ThreadHandler, i);
          pool.ThreadAdd(tt);
        }
      }
    }
  }

  private:
    Epoll e;
    ThreadPool pool;
    TcpSocket _lst_sock;
};
#endif 
