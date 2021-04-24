#ifndef _COUNTER_H_
#define _COUNTER_H_
#include <atomic>
#include <cassert>

unsigned n;
std::atomic<unsigned> bar = 0;
const size_t frag = 8;

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

template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) SOC_Chan
{
  volatile uint8_t values[T];
  volatile uint8_t c;
  size_t size;
  unsigned tid;
  SOC_Chan(size_t s, unsigned tid) : c(0), size(s),tid(tid) {assert(T >= s);}
  inline int push(uint8_t* val)
  {
    if(c) return 0;
    for(unsigned i = 0; i < size; i++)
    {
      values[i] = val[i];
    }
    __atomic_store_n(&c, 1, __ATOMIC_RELEASE);
    return 1;
  }
  inline int popo(uint8_t** val)
  {
    if(!c) return 0;
    for(size_t i = 0; i < size; i++)
    {
      (*val)[i] = values[i];
    }
    std::atomic_thread_fence(std::memory_order::memory_order_acquire);
    __atomic_store_n(&c, 0, __ATOMIC_RELAXED);
    return 1;
  }
};

template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) SOZ_Chan
{
  volatile uintptr_t values;
  size_t size;
  unsigned tid;
  SOZ_Chan(size_t s, unsigned tid) : values(0), size(s), tid(tid) {}
  inline int push(uint8_t* val)
  {
    if(values) return 0;
    __atomic_store_n(&values, (uintptr_t)val, __ATOMIC_RELEASE);
    return 1;
  }
  inline int popo(uint8_t** val)
  {
    if(!values) return 0;
    *val = (uint8_t*) values;
    std::atomic_thread_fence(std::memory_order::memory_order_acquire);
    __atomic_store_n(&values, 0, __ATOMIC_RELAXED);
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
  SDZ_Chan(size_t s, unsigned tid) : fd(mkvl()), pus(), pop(), size(s), tid(tid)
  {
    assert(fd >= 0);
    int err;
    err = open_twin_sd_vl_as_consumer(fd, &pop);
    assert(!err);
    err = open_twin_sd_vl_as_producer(fd, &pus);
    assert(!err);
  }
  inline int push(uint8_t* val)
  {
    return twin_sd_vl_push_non(&pus, (uint64_t) val);
  }
  inline int popo(uint8_t** val)
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
  vlendpt_t pus;
  vlendpt_t pop;
  size_t size;
  unsigned tid;
  VLZ_Chan(size_t s, unsigned tid) : fd(mkvl()), pus(), pop(), size(s), tid(tid)
  {
    assert(fd >= 0);
    int err;
    err = open_twin_vl_as_consumer(fd, &pop, 1);
    assert(!err);
    err = open_twin_vl_as_producer(fd, &pus, 1);
    assert(!err);
  }
  inline int push(uint8_t* val)
  {
    auto ret = twin_vl_push_non(&pus, (uint64_t) val);
    twin_vl_flush(&pus);
    return ret;
  }
  inline int popo(uint8_t** val)
  {
    bool ret;
    twin_vl_pop_non(&pop, (uint64_t*) val, &ret);
    return ret;
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
  VLC_Chan(size_t s, unsigned tid) : fd(mkvl()), pus(), pop(), size(s), tid(tid)
  {
    assert(fd >= 0);
    int err;
    err = open_twin_vl_as_consumer(fd, &pop, 1);
    assert(!err);
    err = open_twin_vl_as_producer(fd, &pus, 1);
    assert(!err);
  }
  inline int push(uint8_t* val)
  {
    for(size_t i = 0; i < size; i+= frag) twin_vl_push_strong(&pus, *(uint64_t*) (val + i), std::min(size - i, frag));
    twin_vl_flush(&pus);
    return 1;
  }
  inline int popo(uint8_t** val)
  {
    size_t i = 0;
    for(size_t i = 0; i < size; i += frag) twin_vl_pop_strong(&pop, (uint64_t*) ((*val)+i));
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
  uint8_t blah[T];
};

#include <boost/lockfree/queue.hpp>
template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) BMC_Chan
{
  boost::lockfree::queue<helper<T>, boost::lockfree::fixed_sized<true>> q;
  size_t size;
  unsigned tid;
  BMC_Chan(size_t s, unsigned tid) : q(4096/T), size(s), tid(tid) {}
  inline int push(uint8_t* val)
  {
    helper<T>& h = *(helper<T>*) val;
    return q.push(h);
  }
  inline int popo(uint8_t** val)
  {
    helper<T>& h = *(helper<T>*) (*val);
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
  inline int push(uint8_t* val)
  {
    return q.push((uintptr_t) val);
  }
  inline int popo(uint8_t** val)
  {
    uintptr_t& h = *(uintptr_t*) val;
    return q.pop(h);
  }
};

#include <boost/lockfree/spsc_queue.hpp>
template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) BSC_Chan
{
  boost::lockfree::spsc_queue<helper<T>,boost::lockfree::capacity<4096/T>> q;
  size_t size;
  unsigned tid;
  BSC_Chan(size_t s, unsigned tid) : q(), size(s), tid(tid) {}
  inline int push(uint8_t* val)
  {
    helper<T>& h = *(helper<T>*) val;
    return q.push(h);
  }
  inline int popo(uint8_t** val)
  {
    helper<T>& h = *(helper<T>*) *val;
    return q.pop(h);
  }
};

template<size_t T>
struct __attribute__((aligned(L1D_CACHE_LINE_SIZE))) BSZ_Chan
{
  boost::lockfree::spsc_queue<uintptr_t,boost::lockfree::capacity<4096/sizeof(uintptr_t)>> q;
  size_t size;
  unsigned tid;
  BSZ_Chan(size_t s, unsigned tid) : q(), size(s), tid(tid) {}
  inline int push(uint8_t* val)
  {
    return q.push((uintptr_t) val);
  }
  inline int popo(uint8_t** val)
  {
    uintptr_t& h = *((uintptr_t*) val);
    return q.pop(h);
  }
};

#endif //BOOST

template<typename T, bool touch>
void* run(void* args)
{
  uint8_t* random_sum = (uint8_t*) malloc(sizeof(uint8_t));
  auto channels = (T**) args;
  T* inp = channels[0];
  T* out = channels[1];
  assert(inp->size == out->size);
  unsigned count = inp->tid;
  uint8_t* old_s = (uint8_t*) aligned_alloc(L1D_CACHE_LINE_SIZE, inp->size);
  uint8_t* s = old_s;
  m5_reset_stats(0,0);
  bar.fetch_add(1, std::memory_order_relaxed);
  while(bar.load(std::memory_order_relaxed) != cores);
  while(n + cores > count)
  {
    while(!inp->popo(&s));
    for(size_t i = 0; i < inp->size; i += L1D_CACHE_LINE_SIZE)
    {
      if(touch) s[i] += 1;
      *random_sum += s[i];
    }
    while(!out->push(s));
    count += cores;
  }
  m5_dump_reset_stats(0,0);
  free(old_s);
  return (void*) random_sum;
}

template<typename T, size_t size, bool touch>
void setup(pthread_t* threads, pthread_attr_t* attr)
{
  T** out = new T*[cores + 1];
  uint8_t* s = (uint8_t*) aligned_alloc(L1D_CACHE_LINE_SIZE, size);
  for(size_t i = 0; i < size; i++) s[i] = (uint8_t) rando();
  Timing<true> t;
  for(unsigned i = 0; i < cores; i++)
  {
    out[i] = new T(size, i);
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

#endif //_COUNTER_H_
