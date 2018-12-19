// port of https://www.cs.nmsu.edu/~jcook/Tools/pthreads/pc.c

#include "pthreads.hpp"
#include <unistd.h>
#include <cstdio>
#include <array>
#include <mutex>
#include <thread>

using namespace std;
constexpr auto loop =20;

using T =int;

struct queue{
  static constexpr auto queuesize=10;
  long head=0;
  long tail=0;
  bool full=false;
  bool empty=true;
  pthreads::mutex mut={};
  pthreads::cond not_full={};
  pthreads::cond not_empty={};
  array<T,queuesize> buf={};

  void add(T in)
  {
    buf[tail++] = in;
    if(tail==queuesize) tail = 0;
    if(tail==head) full = true;
    empty=0;
  }
  T del()
  {
    const auto old_head = head++;
    if(head==queuesize) head=0;
    if(head==tail) empty=true;
    full=false;
    return move(buf[old_head]);
  }
};

int main(){
  auto fifo = queue{};
  pthreads::thread pro =
    {[&](){
       auto run =
         [&](auto time){
           for(int i=0; i<loop;++i){
             {auto lock = lock_guard{fifo.mut};
               while(fifo.full){
                 printf("producer: queue full.\n");
                 fifo.not_full.wait(fifo.mut);}
             }
             fifo.add(i);
             fifo.not_empty.signal();
             this_thread::sleep_for(time);}};
       run(0.1s);
       run(0.2s);
     }};
  pthreads::thread con =
    {[&](){
       auto run =
         [&](auto time){
           for(int i=0; i<loop;++i){
             {auto lock = lock_guard{fifo.mut};
               while(fifo.empty){
                 printf("consumer: queue empty.\n");
                 fifo.not_empty.wait(fifo.mut);}
             }
             auto d = fifo.del();
             fifo.not_full.signal();
             printf("consumer: recieved %d.\n", d);
               this_thread::sleep_for(time);}};
       run(0.2s);
       run(0.5s);
     }};
  pro.join();
  con.join();
  return 0;
}
