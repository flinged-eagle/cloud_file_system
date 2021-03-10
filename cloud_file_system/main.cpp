#include"server.hpp"
#include<signal.h>

void thr_sig(int date)
{
  std::cout<<"SIGPIPE quit\n";
}

int main()
{
  signal(SIGPIPE, thr_sig);
  Server srv;
  srv.start();
  return 0;
}
