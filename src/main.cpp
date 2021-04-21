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

inline size_t compute_size(size_t size) const
{
  switch (size)
  {
    case 8:
      return 8;
    case 16:
      return 16;
    case 32:
      return 32;
    case 64:
      return 64;
    case 128:
      return 128;
    case 256:
      return 256;
    case 512:
      return 512;
    default:
      return 0;
  }
}

void usage(char* arg0)
{
  std::cerr << "Usage:\t" << arg0 << "[-s -v -b -n -p[2-CORES] -q[Direct DATASIZE] -i[Indirect DATASIZE]] n" << std::endl;
  std::cerr << "-s: Single Occupancy" << std::endl;
  std::cerr << "-v: VirtualLink" << std::endl;
  std::cerr << "-b: Boost MPMC" << std::endl;
  std::cerr << "-n: Boost SPSC" << std::endl;
}

enum QUEUE_TYPE
{
 SINGOC,
 VTLINK,
 BOOSTM,
 BOOSTS
};

//may return 0 when not able to detect
const auto proc_count = std::thread::hardware_concurrency();

void parse_args(int argc, char** argv,
    unsigned& n, //Args
    QUEUE_TYPE& at, unsigned& cores, unsigned& datasize, bool& direct, bool& touch) //Flags
{
  int opt;
  char* arg0 = argv[0];
  auto us = [arg0] () {usage(arg0);};
  int helper;
  while((opt = getopt(argc, argv, "sdvtp:q:i:")) != -1)
  {
    std::ostringstream num_hwpar;
    switch(opt)
    {
      case 't':
        touch = true;
        break;
      case 'v' :
        *at = VTLINK;
        break;
      case 's' :
        *at = SINGOC;
        break;
      case 'p' :
        helper = atoi(optarg);
        if(2 > helper || helper > proc_count)
        {
          std::cerr << "You failed to properly specify the number of threads" << std::endl;
          us();
          exit(-4);
        }
        cores = (unsigned) helper;
        break;
      case 'i' :
        helper = atoi(optarg);
        if (1 > helper)
        {
          std::cerr << "You did not provide a proper size input" << std::endl;
          us();
          exit(-7);
        }
        direct = false;
        datasize = (unsigned) helper
        break;
      case 'q' :
        helper = atoi(optarg);
        if (1 > helper)
        {
          std::cerr << "You did not provide a proper size input" << std::endl;
          us();
          exit(-7);
        }
        datasize = (unsigned) helper
        direct = true;
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
  QUEUE_TYPE at = SINGOC;
  unsigned datasize = 8;
  bool direct = true;
  bool touch = false;
  parse_args(argc, argv, &n, &at, &cores, &datasize, &direct, &touch);

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
  const s = compute_size(size);
  if(s == 0)
  {
    std::cerr << "Invalid Size" << std::endl;
  }
  else
  {
    switch(at)
    {
      case SINGOC:
        if(direct)
        {
          setup<SOC_Chan,s,touch>(threads, attr);
        }
        else
        {
          setup<SOZ_Chan,s,touch>(threads, attr);
        }
        break;
      case VTLINK:
        if(direct)
        {
          setup<VLC_Chan,s,touch>(threads, attr);
        }
        else
        {
          setup<SVL_Chan,s,touch>(threads, attr);
        }
        break;
      case BOOSTM:
        if(direct)
        {
          setup<BMC_Chan,s,touch>(threads, attr);
        }
        else
        {
          setup<BMZ_Chan,s,touch>(threads, attr);
        }
        break;
      case BOOSTS:
        if(direct)
        {
          setup<BSC_Chan,s,touch>(threads, attr);
        }
        else
        {
          setup<BSZ_Chan,s,touch>(threads, attr);
        }
        break;
      default:
        std::cerr << "Invalid Buffer type" << std::endl;
    }
  }
  delete[] threads;
  delete[] attr;


  return 0;
}
