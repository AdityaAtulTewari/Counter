#ifndef _COUNTER_H_
#define _COUNTER_H_
#include <atomic>

inline void m5_reset_stats(unsigned a, unsigned b){}
inline void m5_dump_reset_stats(unsigned a, unsigned b){}

std::atomic<uint64_t> __attribute__((aligned(L1D_CACHE_LINE_SIZE))) counter = 1;
unsigned n;
std::atomic<unsigned> bar = 0;

void* scount(void* v)
{
  (void) v;
  bar.fetch_add(1, std::memory_order_relaxed);
  while(bar.load(std::memory_order_relaxed) != cores);

  while(n > counter.fetch_add(1, std::memory_order_relaxed));
  return nullptr;
}

struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) DChannel
{
  std::atomic<uint64_t> counter;
  DChannel() : counter(0) {}
  DChannel(uint64_t c) : counter(c) {}
  inline void push(uint64_t c)
  {
    counter.store(c, std::memory_order_relaxed);
  }
  inline uint64_t pop()
  {
    uint64_t blah;
    while(!(blah = counter.exchange(0, std::memory_order_relaxed)));
    return blah;
  }
};

void* dcount(void* args)
{
  auto channels = (DChannel**) args;
  DChannel* in = channels[0];
  DChannel* out = channels[1];
  uint64_t num = 0;

  bar.fetch_add(1, std::memory_order_relaxed);
  while(bar.load(std::memory_order_relaxed) != cores);

  while(n > num)
  {
    num = in->pop();
    out->push(num + 1);
  }
  return nullptr;
}

#ifdef VL
#include "vl/vl.h"
struct VPush
{
  vlendpt_t end;
  VPush(int fd) :
  {
    open_twin_vl_as_producer(fd, &end, 1);
  }
  inline void push(uint64_t c)
  {
    twin_vl_push_strong(&end, c);
  }
  ~VPush()
  {
    close_twin_vl_as_producer(end);
  }
};

struct VPop
{
  vlendpt_t end;
  VPop(int fd)
  {
    open_twin_vl_as_consumer(fd, &end, 1);
  }
  inline uint64_t pop()
  {
    uint64_t blah;
    twin_vl_pop_strong(&end, &blah);
    return blah;
  }
  ~VPop()
  {
    close_twin_vl_as_consumer(end);
  }
};

struct VArgs
{
  VPop pop;
  VPush push;
  VArgs(int fdC, int fdP) : pop(VPop(fdC)), push(VPush(fdP)) {}
  inline uint64_t pop()
  {
    return pop.pop();
  }
  inline void push(uint64_t c)
  {
    push.push(c);
  }
};

void* vcount(void* args)
{
  auto cont = (VArgs*) args;
  uint64_t num = 0;

  bar.fetch_add(1, std::memory_order_relaxed);
  while(bar.load(std::memory_order_relaxed) != cores);

  while(n > num)
  {
    num = in->pop();
    out-> push(num+1);
  }
  return nullptr;
}
#endif /* VL */

#endif
