#ifndef _COUNTER_H_
#define _COUNTER_H_

inline void m5_reset_stats(unsigned a, unsigned b){}
inline void m5_dump_reset_stats(unsigned a, unsigned b){}

volatile uint64_t __attribute__((aligned(CACHE_LINE_SIZE))) counter = 1;
unsigned n;
void* scount(void* v)
{
  (void) v;
  while(n >= atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed));
  return nullptr;
}

struct Channel __attribute__((aligned(CACHE_LINE_SIZE)))
{
  volatile uint64_t counter;
  Channel() : counter(0) {}
  Channel(uint64_t c) : counter(c) {}
  inline void push(uint64_t c)
  {
    atomic_store_explicit(&counter, c, memory_order_relaxed);
  }
  inline uint64_t pop()
  {
    uint64_t blah;
    while(!(blah = atomic_exchange_explicit(&counter,0, memory_order_relaxed)));
    return blah;
  }
};

void* dcount(void* args)
{
  auto channels = (Channel**) args;
  Channel* in = channels[0];
  Channel* out = channels[1];

  while(true)
  {
    auto num = in->pop();
    if(num == n) return nullptr;
    out->push(num);
  }
}

#endif
