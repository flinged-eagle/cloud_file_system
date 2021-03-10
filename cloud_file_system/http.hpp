#ifndef _M_HTTP__
#define _M_HTTP__ 
#include<string>
#include<boost/filesystem.hpp>
#include<boost/algorithm/string.hpp>
#include<unordered_map>
#include"TcpSocket.hpp"
#include<sstream>

class HttpRequest{
  public:
    HttpRequest(){};
    int RequestParse(TcpSocket& sock)
    {
      //接受Http头部
      std::string header;
      if(RecvHeader(sock,header) == false)
      {
        return 400;
      }
      //分割头部
      if(ParseHeader(header) == false)
      {
        return 400;
      }
      //校验请求信息
      //获取body
      auto it = _headers.find("Content-Length");
      if(it != _headers.end())
      {
        std::stringstream tmp;
        tmp << it->second;
        int64_t file_len;
        tmp >> file_len;
        sock.Recv(_body, file_len);
      }
      return 200;
    }
  private:
    //接受头部
    bool RecvHeader(TcpSocket& sock, std::string& header)
    {
      //探测行接收
      std::string tmp;
      if(sock.RecvPeek(tmp) == false)
      {
        return false;
      }

      //判断是否包含整个头部
      auto pos = tmp.find("\r\n\r\n");
      //头部与数据之间有两个行间隔
      
      if(pos == std::string::npos && tmp.size() == 10240)
      {
        return false;
      }else if( pos != std::string::npos)
      {
        header.assign(tmp.c_str(), pos);
        sock.Recv(tmp, pos+4);
        return true;
      }
      return false;
    }
    //分析头部
    bool ParseHeader(std::string& header)
    {
      //存储分割后的字符串
      std::vector<std::string> part;
      //按照\r\n进行分割，存储
      boost::split(part, header, boost::is_any_of("\r\n"), boost::token_compress_on);
      //首行解析  
      if(FirstLineParse(part[0]) == false)
      {
        return false;
      }
      for(unsigned long i = 1; i<part.size(); ++i)
      {
        auto pos = part[i].find(": ");
        std::string key = part[i].substr(0,pos);
        std::string val = part[i].substr(pos+2);
        _headers[key] = val;
      }
      return true;
    }

    //首行解析
    bool FirstLineParse(std::string& line)
    {
      std::vector<std::string> tmp;
      boost::split(tmp, line, boost::is_any_of(" "), boost::token_compress_on);
      if(tmp.size() != 3)
      {
        //http首行 请求方式 相对路径 请求状态符
        return false;
      }
      _method = tmp[0];
      unsigned long pos = tmp[1].find("?");
      //相对路径
      _path = tmp[1].substr(0, pos);
      if(pos == std::string::npos)
      {
        return true;
      }
      //获取参数
      std::string param = tmp[1].substr(pos+1, tmp[1].find("?"));
      tmp.clear();
      boost::split(tmp, param, boost::is_any_of("&"), boost::token_compress_on);
      for(auto&e : tmp)
      {
        int pos = e.find("=");
        std::string key = e.substr(0,pos);
        std::string val = e.substr(pos+1);
        _param[key] = val;
      }
      return true;
    }

    bool PathLegal()
    {
      return true;
    }

  public:
    std::string _method;
    std::string _path;
    std::string _query_str;
    std::unordered_map<std::string, std::string> _param;
    std::unordered_map<std::string, std::string> _headers;
    std::string _body;
};

class HttpResponse{
  public:
    bool SetHeader(const std::string& key, const std::string& val)
    {
      _headers[key] = val;
      return true;
    }

    bool NormalProcess(TcpSocket& sock)
    {
      std::string line;
      std::string header;
      std::stringstream tmp;
      tmp<<"HTTP/1.1"<<" "<<_status<<" "<<Getdesc()<<"\r\n";
      //保证数据中存在content-length
      if(_headers.find("Content-Length") == _headers.end())
      {
        _headers["Content-Length"] = std::to_string(_body.size());
      }

      for(auto& it : _headers)
      {
        tmp<<it.first<<": "<<it.second<<"\r\n";
      }
      tmp<<"\r\n";
      sock.Send(tmp.str());
      sock.Send(_body);
      
      return true;
    }
    bool ErrorProcess(TcpSocket& sock)
    {
      std::stringstream tmp;
      tmp<<"HTTP/1.1 "<<_status<<" "<<Getdesc()<<"\r\n";
      sock.Send(tmp.str());
      return false;
    }
  private:
    std::string Getdesc()
    {
      switch(_status)
      {
        case 400: return "BAD REQUEST";
        case 404: return "NOT FIND";
        case 206: return "Partial Content";
        case 200: return "OK";
        case 500: return "SERVICE ERROR";
      }

      return "UNKNOW";
    }

  public:
    int _status;
    std::unordered_map<std::string, std::string> _headers;
    std::string _body;
};
#endif 
