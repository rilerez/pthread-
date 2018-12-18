#include <iostream>
#include <functional>
#include <type_traits>
#include <utility>
#include <chrono>
#include <unistd.h>
#include <pthread.h>
#include <csignal>
#include <ctime>

namespace pthreads{
  using namespace std;

  struct pthread_error {
    const int erno;
    pthread_error(int erno): erno(erno) {}
  };

  void throw_if(int error_code){
    if (error_code!=0) throw pthread_error(error_code);
  }

  template<typename F,typename ret_t>
  struct thread_helper_base{
    // a function we can take a pointer to that calls its argument
    // this way we can pass closures to PTHREAD_CREATE
    static auto call_closure(void* _f){
      auto f =((F*)_f);
      ret_t out = (*f)();
      auto out_ptr = new decltype(out){out};
      return (void*)out_ptr;
    }
    static auto join(const pthread_t& t) {
      ret_t* out;
      auto res = pthread_join(t,(void**)&out);
      auto ret = move(*out); // is this safe?
      delete out;
      throw_if(res);
      return ret;
    }
  };

  template<typename F>
  struct thread_helper_base<F,void>{
    static auto call_closure(void* _f){
      auto f =((F*)_f);
      (*f)();
      return (void*)0;
    }

    static void join(const pthread_t& t){
      throw_if(pthread_join(t,nullptr));
    }
  };

  template<typename F>
  struct thread_helper
    : thread_helper_base<F,typename result_of<F()>::type>
  {};


  template<typename F>
  struct thread{
    F myf;
    pthread_t mythread;

    thread(F&& f, const pthread_attr_t *attr = nullptr): myf(forward<F>(f)){
      throw_if(pthread_create(mythread,attr,
                              &thread_helper<F>::call_closure,
                              &_f));
    }

    // do you want to copy threads?
    thread(const F& f, pthread_attr_t*) = delete;

    auto join(){return thread_helper<F>::join(mythread);}
    void kill(int sig){throw_if(pthread_kill(t,sig));}
    void cancel(){throw_if(pthread_cancel(mythread));}
  };

  struct mutex{
    pthread_mutex_t mymutex;
    mutex(const pthread_mutexattr_t *attr = nullptr)
    {
      throw_if(pthread_mutex_init(&mymutex,attr));
    }
    void lock(){throw_if(pthread_mutex_lock(&mymutex));}
    void trylock(){throw_if(pthread_mutex_trylock(&mymutex));}
    // pthread_mutex_unlock can error but basic lockable requires this function
    // is noexcept
    void unlock()noexcept {pthread_mutex_unlock(&mymutex);}
    ~mutex(){pthread_mutex_destroy(&mymutex);};
  };

  template<class Duration>
  timespec to_timespec(Duration dur){
    auto sec = chrono::duration_cast<chrono::seconds>(dur);
    return
      {sec.count(),
       chrono::duration_cast<chrono::nanoseconds>(dur-sec.count())};
  }

  struct cond{
    pthread_cond_t mycond;
    cond(pthread_condattr_t *attr=nullptr){
      throw_if(pthread_cond_init(&mycond,attr));
    }
    void wait(mutex& mut){throw_if(pthread_cond_wait(&mycond,&mut.mymutex));}
    void signal(){throw_if(pthread_cond_signal(&mycond));}
    void broadcast(){throw_if(pthread_cond_broadcast(&mycond));}


    template<class Timepoint_or_dur>
    void timedwait(mutex& mut,Timepoint_or_dur&& timept_or_dur){
      auto sys_time = chrono::time_point<chrono::system_clock>
        (forward<Timepoint_or_dur>(timept_or_dur));
      const auto dur = sys_time.time_since_epoch();
      const auto sec = chrono::duration_cast<chrono::seconds>(dur);

      auto tspec = timespec{sec.count(),
                            chrono::duration_cast<chrono::nanoseconds>
                            (dur - sec).count()};

      throw_if(pthread_cond_timedwait(&mycond,&mut.mymutex,&tspec));
    }
    //https://stackoverflow.com/questions/17402657/how-to-convert-stdchronosystem-clockduration-into-struct-timeval


    ~cond(){pthread_cond_destroy(&mycond);}
  };
}
