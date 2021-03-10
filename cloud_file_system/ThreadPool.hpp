#include<thread>
#include<iostream>
#include<vector>
#include<queue>
#include<pthread.h>

//函数指针
typedef void(*handler_t)(int);

class ThreadTask{
public:
  ThreadTask(handler_t handler, int data)
   :_handler(handler)
     ,_data(data)
  {}
  void ThreadRun()
  {
    _handler(_data);
  }
private:
  handler_t _handler;
  int _data;
};

class ThreadPool{
public:
  ThreadPool(int thr = 5)
    :max_thr(thr)
  {
    pthread_cond_init(&_cond_con, NULL);
    pthread_mutex_init(&_mutex, NULL);
  }
  bool ThreadInit()
  {
    for(int i=0; i< max_thr; ++i)
    {
      std::thread thr = std::thread(&ThreadPool::thr_start, this);
      thr.detach();   //不用等待回收线程资源
    }
    return true;
  }
  bool ThreadAdd(ThreadTask& task)
  {
    std::thread thr = std::thread(&ThreadPool::thr_start, this);
    thr.detach();
    pthread_mutex_lock(&_mutex);
    _queue.push(task);
    pthread_mutex_unlock(&_mutex);
    //唤醒线程
    pthread_cond_signal(&_cond_con);
    return true;
  }
private:
  void thr_start()
  {
    while(1)
    {
      pthread_mutex_lock(&_mutex);
      while(_queue.empty())
      {
        //线程睡眠
        pthread_cond_wait(&_cond_con, &_mutex);
      }
      ThreadTask tt = _queue.front();
      _queue.pop();
      pthread_mutex_unlock(&_mutex);
      tt.ThreadRun();
    }
  }


  std::queue<ThreadTask> _queue;
  //最大的线程数
  int max_thr;
  //互斥锁
  pthread_mutex_t _mutex;
  //条件信号量
  pthread_cond_t _cond_con;
};
