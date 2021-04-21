#ifndef _COUNTER_H_
#define _COUNTER_H_
#include <atomic>
#include <cassert>

unsigned n;
std::atomic<unsigned> bar = 0;

#ifndef NOGEM5
#include <gem5/m5ops.h>
#else
inline void m5_reset_stats(unsigned a, unsigned b){}
inline void m5_dump_reset_stats(unsigned a, unsigned b){}
#endif

unsigned x = 1, y = 4, z = 7, w = 13;
unsigned rando()
{
  unsigned t = x;
  t ^= t << 11;
  t ^= t >> 8;
  x = y;
  y = z;
  z = w;
  w ^= w >> 19;
  w ^= t;
  return w;
}

template<typename T, size_t size, bool touch>
void setup(pthread_t* threads, pthread_attr_t* attr)
{
  char* s = aligned_alloc(L1D_CACHE_LINE_SIZE, size);
  for(size_t i = 0; i < size; i++) s[i] = (char) rando();
  Timing<true> t;
  for(unsigned i = 0; i < cores; i++)
  {
    out[i] = new T<size>(size, i);
  }
  out[cores] = out[0];
  for(unsigned i = 1; i < cores; i++)
  {
    pthread_create(&threads[i], &attr[i], run<T,touch>, (void*) &out[i]);
  }
  while(!out[0]->push(s));
  t.s();
  run<T,touch>(&out[0]);
  t.e();
  for(unsigned i = 1; i < cores; i++)
  {
    pthread_join(threads[i], nullptr);
  }
  free(s);
  t.p(typeid(T).name());
}

template<typename T, bool touch>
void* run(void* args)
{
  auto channels = (T**) args;
  T* inp = channels[0];
  T* out = channels[1];
  assert(inp->size == out->size);
  unsigned count = tid;
  char* old_s = (char*) aligned_alloc(L1D_CACHE_LINE_SIZE, inp->size);
  char* s = old_s;
  m5_reset_stats(0,0);
  bar.fetch_add(1, std::memory_order_relaxed);
  while(bar.load(std::memory_order_relaxed) != cores);
  while(n + cores > count)
  {
    while(!inp->pop(&s));
    if(touch) for(size_t i = 0; i < inp->size; i += L1D_CACHE_LINE_SIZE) s[i] += 1;
    while(!out->push(s));
    count += cores;
  }
  m5_dump_reset_stats(0,0);
  free(old_s);
  return nullptr;
}

template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) SOC_Chan
{
  volatile char values[T];
  char c;
  size_t size;
  unsigned tid;
  SOC_Chan(size_t s, unsigned tid) : c(0), size(s),tid(tid) {assert(T >= s);}
  inline int push(char* val)
  {
    if(c) return 0;
    for(unsigned i = 0; i < size; i++)
    {
      values[i] = val[i];
    }
    __atomic_store_n(&c, 1, __ATOMIC_RELEASE);
    return 1;
  }
  inline void pop(char** val)
  {
    if(!c) return 0;
    for(size_t i = 0; i < size; i++)
    {
      (*val)[i] = values[i];
    }
    __atomic_store_n(&c, 0, __ATOMIC_ACQUIRE);
    return 1;
  }
};

template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) SOZ_Chan
{
  volatile char* values;
  size_t size;
  unsigned tid;
  SOZ_Chan(size_t s, unsigned tid) : values(nullptr), size(s), tid(tid) {}
  inline int push(char* val)
  {
    if(values) return 0;
    values = val;
    __atomic_store_n(&values, val, __ATOMIC_RELEASE);
    return 1;
  }
  inline void pop(char** val)
  {
    if(!values) return 0;
    *val = values;
    __atomic_store_n(&values, 0, __ATOMIC_ACQUIRE);
    return 1;
  }
};

#ifdef VL
#include <vl/vl.h>
template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) SDZ_Chan
{
  int fd;
  sd_vlendpt_t pus;
  sd_vlendpt_t pop;
  size_t size;
  unsigned tid;
  SDZ_Chan(size_t s, unsigned tid) : fd(mkvl()), pus(0),pop(0), size(s), tid(tid)
  {
    assert(fd >= 0);
    int err;
    err = open_twin_sd_vl_as_consumer(fd, &pop);
    assert(!err);
    err = open_twin_sd_vl_as_producer(fd, &pus);
    assert(!err);
  }
  inline int push(char* val)
  {
    return twin_sd_vl_push_non(&pus, (uint64_t) val);
  }
  inline int pop(char** val)
  {
    return twin_sd_vl_pop_non(&pop, (uint64_t*) val);
  }
  ~SDZ_Chan()
  {
    close_twin_sd_vl_as_consumer(pus);
    close_twin_sd_vl_as_consumer(pop);
  }
};

template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) VLZ_Chan
{
  int fd;
  sd_vlendpt_t pus;
  sd_vlendpt_t pop;
  size_t size;
  unsigned tid;
  VLZ_Chan(size_t s, unsigned tid) : fd(mkvl()), pus(0), pop(0), size(s), tid(tid)
  {
    assert(fd >= 0);
    int err;
    err = open_twin_vl_as_consumer(fd, &pop);
    assert(!err);
    err = open_twin_vl_as_producer(fd, &pus);
    assert(!err);
  }
  inline int push(char* val)
  {
    return twin_vl_push_non(&pus, (uint64_t) val);
  }
  inline int pop(char** val)
  {
    return twin_vl_pop_non(&pop, (uint64_t*) val);
  }
  ~VLZ_Chan()
  {
    close_twin_vl_as_producer(pus);
    close_twin_vl_as_consumer(pop);
  }
};

template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) VLC_Chan
{
  int fd;
  vlendpt_t pus;
  vlendpt_t pop;
  size_t size;
  unsigned tid;
  VLC_Chan(size_t s, unsigned tid) : fd(mkvl()), pus(0), pop(0), size(s), tid(tid)
  {
    assert(fd >= 0);
    int err;
    err = open_byte_vl_as_consumer(fd, &pop);
    assert(!err);
    err = open_byte_vl_as_producer(fd, &pus);
    assert(!err);
  }
  inline int push(char* val)
  {
    for(size_t i = 0; i < size; i+= 62) line_vl_push_strong(&pus, val + i, min(size - i, 62));
    return 1;
  }
  inline int pop(char** val)
  {
    size_t i = 0;
    while(i < size)
    {
      size_t pcnt = min(size - i, 62);
      line_vl_pop_strong(&pop, val+i, &pcnt);
      i += pcnt;
    }

    return 1;
  }
  ~VLC_Chan()
  {
    close_twin_vl_as_producer(pus);
    close_twin_vl_as_consumer(pop);
  }
};
#endif //VL

#ifdef BOOST
//correct boost queue
template<size_t T>
struct helper
{
  char blah[T];
};

#include <boost/lockfree/queue.hpp>
template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) BMC_Chan
{
  boost::lockfree::queue<helper<T>, boost::lockfree::fixed_sized<true>> q;
  size_t size;
  unsigned tid;
  BMC_Chan(size_t s, unsigned tid) : q(4096/size), size(s), tid(tid) {}
  inline int push(char* val)
  {
    helper& h = &(*(helper*) val);
    return q.push(helper);
  }
  inline int pop(char** val)
  {
    helper& h = &(*(helper*) *val);
    return q.pop(h);
  }
};

template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) BMZ_Chan
{
  boost::lockfree::queue<uintptr_t, boost::lockfree::fixed_sized<true>> q;
  size_t size;
  unsigned tid;
  BMZ_Chan(size_t s, uint64_t tid) : q(4096/sizeof(uintptr_t)), size(s), tid(tid) {}
  inline int push(char* val)
  {
    return q.push((uintptr_t) val);
  }
  inline int pop(char** val)
  {
    uintptr_t& h = &(*(uintptr_t*) val);
    return q.pop(h);
  }
};

#include <boost/lockfree/spsc_queue.hpp>
template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) BSC_Chan
{
  boost::lockfree::queue_spsc<helper<T>, boost::lockfree::fixed_sized<true>> q;
  size_t size;
  unsigned tid;
  BSZ_Chan(size_t s, unsigned tid) : q(4096/sizeof(uintptr_t)), size(s), tid(tid) {}
  inline int push(char* val)
  {
    helper& h = &(*(helper*) val);
    return q.push(helper);
  }
  inline int pop(char** val)
  {
    helper& h = &(*(helper*) *val);
    return q.pop(h);
  }
};

template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) BSZ_Chan
{
  boost::lockfree::queue_spsc<uintptr_t, boost::lockfree::fixed_sized<true>> q;
  size_t size;
  unsigned tid;
  BSZ_Chan(size_t s, unsigned tid) : q(4096/sizeof(uintptr_t)), size(s), tid(tid) {}
  inline int push(char* val)
  {
    return q.push((uintptr_t) val);
  }
  inline int pop(char** val)
  {
    uintptr_t& h = &(*(uintptr_t*) val);
    return q.pop(h);
  }
};

#endif //BOOST


#endif //_COUNTER_H_
