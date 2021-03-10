#include<iostream>
#include<boost/filesystem.hpp>
#include<fstream>
#include<sstream>
#include<string>
#include<vector>
#include<stdlib.h>

#define _ROOT "./root"

//记录body中的数据分布
class Boundary
{
  public:
    std::string _name;
    std::string _filename;
    int64_t _start_end;
    int64_t _data_len;
};

bool headerParse(std::string& header, Boundary &file)
{
  std::string name = "name=\"";
  std::string end_name = "\"";
  size_t pos = header.find(name);

  if(pos == std::string::npos)
  {
    return false;
  }

  size_t next_pos = header.find(end_name, pos+name.size());
  if(next_pos == std::string::npos)
  {
    return false;
  }
  file._name = header.substr(pos+name.size(), next_pos-pos-name.size());
  if(file._name == "fileupload")
  {
    pos = header.find("filename=\"");
    if(pos == std::string::npos)
    {
      return false;
    }
    next_pos = header.find("\"", pos+sizeof("filename=\"")-1);
    if(next_pos == std::string::npos)
    {
      return false;
    }
    file._filename = header.substr(pos+sizeof("filename=\"")-1, next_pos - pos - sizeof("filename=\"")+1);
  }
    return true;
}

bool GetHeader(const std::string& key, std::string& val)
{
  std::string body;
  char* ptr = getenv(key.c_str());
  if(ptr == NULL)
  {
    return false;
  }
  val = ptr;
  return true;
}

//根据boundary解析数据
bool BoundaryParse(std::string& body, std::vector<Boundary>& list)
{
  std::string tmp;
  if(!GetHeader("Content-Type",tmp))
  {
    return false;
  }
  std::string boundary = tmp.substr(tmp.find("boundary=")+sizeof("boundary=")-1);
  //数据由boundary分割，first boundary 和 last bodundary 只出现一次
  //中间由middle boundary 间隔
  std::string fir_boundary = "--"+boundary;
  std::string mid_boundary = "\r\n"+fir_boundary;
  std::string lst_boundary = mid_boundary + "--\r\n";
  std::string dash = "--";
  std::string craf = "\r\n";
  std::string tail = "\r\n\r\n";
  
  size_t pos, next_pos;
  pos = body.find(fir_boundary);
  if(pos == std::string::npos)
  {
    std::cerr<<"first boundary error\n";
    return false;
  }
  pos += fir_boundary.size();
  
  while(pos < body.size())
  {
    next_pos = body.find(tail, pos);
    if(next_pos == std::string::npos)
    {
      return false;
    }

    std::string header = body.substr(pos, next_pos-pos);
    pos = next_pos + tail.size();
    next_pos = body.find(mid_boundary, pos);
    if(next_pos == std::string::npos)
    {
      return false;
    }
    int64_t offset = pos;
    int64_t len = next_pos - pos;
    pos = next_pos + mid_boundary.size();
    next_pos = body.find(craf, pos);
    if(next_pos == std::string::npos)
    {
      return false;
    }
    pos = next_pos + craf.size();
    Boundary file;
    file._data_len = len;
    file._start_end = offset;
    headerParse(header, file);
    list.push_back(file);
  }
  return true;
}

bool StorageFile(std::string& body, std::string rpath, std::vector<Boundary>& list)
{
  for(auto& i : list)
  {
    if(i._name != "fileupload")
    {
      continue;
    }
    std::string realpath = _ROOT+rpath+"/"+i._filename;
    std::ofstream file(realpath);
    if(!file.is_open())
    {
      std::cerr<<"open file:"<<realpath<<"failed\n";
      return false;
    }
    file.write(&body[i._start_end], i._data_len);
    if(!file.good())
    {
      std::cerr<<"write file error\n";
      return false;
    }
    file.close();
  }
  return true;
}

int main()
{
  //通过环境变量进行数据交互
  size_t len = atoi(getenv("Content-Length"));
  std::string _body;
  _body.resize(len);

  size_t rlen = 0;
  while(rlen < len)
  {
    int ret = read(0,&_body[rlen], len-rlen);
    if(ret < 0)
    {
      std::cerr<<"read pipe error\n";
      exit(-1);
    }else if(ret == 0)
    {
      //管道已经为空
      break;
    }
    rlen += ret;
  }

  std::vector<Boundary> list;
  std::string ori = getenv("Origin");
  std::string refer = getenv("Referer");
  std::string rpath = refer.substr(refer.find(ori)+ori.size());
  std::stringstream ss;

  BoundaryParse(_body, list);
  StorageFile(_body, rpath, list);
  ss<<"<html lang=\"zh-cn\"><head><title>success</title></head><body>";
  ss<<"<a href='"<<rpath<<"'>return menu</a></body></html>";
  std::string str = ss.str();
  rlen = 0;
  len = str.size();
  if(rlen < len)
  {
    while(rlen < len)
    {
      int ret = write(1, &str[rlen], len-rlen);
      if(ret < 0)
      {
        std::cerr<<"write error\n";
        exit(-1);
      }else if(ret == 0)
      {
        break;
      }
      rlen += ret;
    }
  }
  close(1);
  close(0);
  return 0;
}
