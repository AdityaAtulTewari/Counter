#include <iostream>
#include <sstream>
#include "timing.h"
#include <getopt.h>
#include <pthread.h>
#include <thread>
#include <sched.h>
#include <unistd.h>

#define ARGS 1
#ifndef L1D_CACHE_LINE_SIZE
#define L1D_CACHE_LINE_SIZE 64
#endif

unsigned cores = 2;
#include "counter.h"

void usage(char* arg0)
{
  std::cerr << "Usage:\t" << arg0 << "[-v -s -p[1-CORES]] n" << std::endl;
}

enum ALLOC_TYPE
{
 STD_ALLOC,
 DYN_ALLOC,
 VTL_ALLOC
};

//may return 0 when not able to detect
const auto proc_count = std::thread::hardware_concurrency();

void parse_args(int argc, char** argv,
    unsigned* n,
    ALLOC_TYPE* at, bool* sched, unsigned* cores)
{
  int opt;
  char* arg0 = argv[0];
  auto us = [arg0] () {usage(arg0);};
  int helper;
  while((opt = getopt(argc, argv, "svdp:")) != -1)
  {
    std::ostringstream num_hwpar;
    switch(opt)
    {
      case 'v' :
        *at = VTL_ALLOC;
        break;
      case 'd' :
        *at = DYN_ALLOC;
        break;
      case 's' :
        *at = STD_ALLOC;
        break;
      case 'p' :
        helper = atoi(optarg);
        if(2 > helper || helper > proc_count)
        {
          std::cerr << "You failed to properly specify the number of threads" << std::endl;
          us();
          exit(-4);
        }
        *cores = (unsigned) helper;
        break;
    }
  }
  std::istringstream sarr[ARGS];
  unsigned darr[ARGS];
  unsigned i;
  for(i = 0; i < ARGS; i++)
  {
    if(optind + i == argc)
    {
      std::cerr << "You have too few unsigned int arguments: " << i << " out of " << ARGS << std::endl;
      us();
      exit(-3);
    }
    sarr[i] = std::istringstream(argv[optind + i]);
    if(!(sarr[i] >> darr[i]))
    {
      std::cerr << "Your argument at " << optind + i << " was malformed." << std::endl;
      std::cerr << "It should have been an unsigned int" << std::endl;
      us();
      exit(-2);
    }
  }
  if(i + optind != argc)
  {
    std::cerr << "You have too many arguments." << std::endl;
    us();
    exit(-1);
  }

  *n = darr[0] + 1;
}

int main(int argc, char** argv)
{
  bool sched = false;
  ALLOC_TYPE at = STD_ALLOC;
  parse_args(argc, argv, &n, &at, &sched, &cores);

  Timing<true> t;
  pthread_t* threads = new pthread_t[cores];
  pthread_attr_t* attr = new pthread_attr_t[cores];
  for(unsigned i = 0; i < cores; i++)
  {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(i,&mask);
    pthread_attr_setaffinity_np(&attr[i], sizeof(mask), &mask);
    if(i == 0)
    {
      pthread_t current_thread = pthread_self();
      pthread_setaffinity_np(current_thread, sizeof(mask), &mask);
    }
  }

  if(at == STD_ALLOC)
  {
    t.s();
    for(unsigned i = 1; i < cores; i++)
    {
      pthread_create(&threads[i], &attr[i], scount, nullptr);
    }
    scount(nullptr);
    t.e();
    for(unsigned i = 1; i < cores; i++)
    {
      pthread_join(threads[i], nullptr);
    }

    t.p("ATOMIC");
  }
  else if(at == DYN_ALLOC)
  {
    DChannel** out = new DChannel*[cores + 1];
    for(unsigned i = 0; i < cores; i++)
    {
      out[i] = new DChannel();
    }
    out[cores] = out[0];
    for(unsigned i = 1; i < cores; i++)
    {
      pthread_create(&threads[i], &attr[i], dcount, (void*) &out[i]);
    }
    out[1]->counter.store(1, std::memory_order_relaxed);
    t.s();
    dcount((void*) &out[0]);
    t.e();
    for(unsigned i = 1; i < cores; i++)
    {
      pthread_join(threads[i], nullptr);
    }
    for(unsigned i = 0; i < cores; i++)
    {
      delete out[i];
    }
    delete[] out;

    t.p("BUFFER");
  }
  else if(at == VTL_ALLOC)
  {
    int* fds = new int[cores];
    for(unsigned i = 0; i < cores; i++)
    {
      auto fd = mkvl();
      if(fd < 0)
      {
        std::cerr << "Unable to allocate a vl" << std::endl;
        exit(-5);
      }
      fds[i] = fd;
    }
    VArgs** vargs = new VArgs*[cores];
    for(unsigned i = 0; i < cores - 1; i++)
    {
      vargs[i] = new VArgs(fds[i], fds[i+1]);
    }
    vargs[cores - 1] = new VArgs(fds[cores - 1], fds[0]);
    for(unsigned i = 1; i < cores; i++)
    {
      pthread_create(&threads[i], &attr[i], vcount, (void*) vargs[i]);
    }
    vargs[0]->push(1);
    t.s();
    vcount((void*) vargs[0]);
    t.e();
    for(unsigned i = 1; i < cores; i++)
    {
      pthread_join(threads[i], nullptr);
    }
    for(unsigned i = 0; i < cores; i++)
    {
      delete vargs[i];
    }
    delete[] vargs;
    delete[] fds;

    t.p("VTLINK");
  }
  else
  {
    std::cerr << "Invalid Buffer type" << std::endl;
  }
  delete[] threads;
  delete[] attr;


  return 0;
}
