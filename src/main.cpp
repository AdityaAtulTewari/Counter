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
  std::cerr << "Usage:\t" << arg0 << "[-s -v -b -n -t -p[2-CORES] -q[Direct DATASIZE] -i[Indirect DATASIZE]] n" << std::endl;
  std::cerr << "-s: Single Occupancy" << std::endl;
  std::cerr << "-v: VirtualLink" << std::endl;
  std::cerr << "-b: Boost MPMC" << std::endl;
  std::cerr << "-n: Boost SPSC" << std::endl;
  std::cerr << "-t: Touch values" << std::endl;
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
    QUEUE_TYPE& at, unsigned& cores, unsigned& datasize, bool& direct, bool& touch) //Flags
{
  int opt;
  char* arg0 = argv[0];
  auto us = [arg0] () {usage(arg0);};
  int helper;
  while((opt = getopt(argc, argv, "sbnvtp:q:i:")) != -1)
  {
    std::ostringstream num_hwpar;
    switch(opt)
    {
      case 's' :
        at = SINGOC;
        break;
      case 'v' :
        at = VTLINK;
        break;
      case 'b' :
        at = BOOSTM;
        break;
      case 'n' :
        at = BOOSTS;
        break;
      case 't':
        touch = true;
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
        datasize = (unsigned) helper;
        break;
      case 'q' :
        helper = atoi(optarg);
        if (1 > helper)
        {
          std::cerr << "You did not provide a proper size input" << std::endl;
          us();
          exit(-7);
        }
        datasize = (unsigned) helper;
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

  n = darr[0];
}

int main(int argc, char** argv)
{
  QUEUE_TYPE at = SINGOC;
  unsigned datasize = 8;
  bool direct = true;
  bool touch = false;
  parse_args(argc, argv, at, cores, datasize, direct, touch);

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
  if(at == SINGOC && direct && touch)
  {
    switch (datasize)
    {
      case 8:
        setup<SOC_Chan<  8>,  8, true>(threads, attr);
        break;
      case 16:
        setup<SOC_Chan< 16>, 16, true>(threads, attr);
        break;
      case 32:
        setup<SOC_Chan< 32>, 32, true>(threads, attr);
        break;
      case 64:
        setup<SOC_Chan< 64>, 64, true>(threads, attr);
        break;
      case 128:
        setup<SOC_Chan<128>,128, true>(threads, attr);
        break;
      case 256:
        setup<SOC_Chan<256>,256, true>(threads, attr);
        break;
      case 512:
        setup<SOC_Chan<512>,512, true>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
  else if(at == SINGOC && direct && !touch)
  {
    switch (datasize)
    {
      case 8:
        setup<SOC_Chan<  8>,  8, false>(threads, attr);
        break;
      case 16:
        setup<SOC_Chan< 16>, 16, false>(threads, attr);
        break;
      case 32:
        setup<SOC_Chan< 32>, 32, false>(threads, attr);
        break;
      case 64:
        setup<SOC_Chan< 64>, 64, false>(threads, attr);
        break;
      case 128:
        setup<SOC_Chan<128>,128, false>(threads, attr);
        break;
      case 256:
        setup<SOC_Chan<256>,256, false>(threads, attr);
        break;
      case 512:
        setup<SOC_Chan<512>,512, false>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
  else if(at == SINGOC && !direct && touch)
  {
    switch (datasize)
    {
      case 8:
        setup<SOZ_Chan<  8>,  8, true>(threads, attr);
        break;
      case 16:
        setup<SOZ_Chan< 16>, 16, true>(threads, attr);
        break;
      case 32:
        setup<SOZ_Chan< 32>, 32, true>(threads, attr);
        break;
      case 64:
        setup<SOZ_Chan< 64>, 64, true>(threads, attr);
        break;
      case 128:
        setup<SOZ_Chan<128>,128, true>(threads, attr);
        break;
      case 256:
        setup<SOZ_Chan<256>,256, true>(threads, attr);
        break;
      case 512:
        setup<SOZ_Chan<512>,512, true>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
  else if(at == SINGOC && !direct && !touch)
  {
    switch (datasize)
    {
      case 8:
        setup<SOZ_Chan<  8>,  8, false>(threads, attr);
        break;
      case 16:
        setup<SOZ_Chan< 16>, 16, false>(threads, attr);
        break;
      case 32:
        setup<SOZ_Chan< 32>, 32, false>(threads, attr);
        break;
      case 64:
        setup<SOZ_Chan< 64>, 64, false>(threads, attr);
        break;
      case 128:
        setup<SOZ_Chan<128>,128, false>(threads, attr);
        break;
      case 256:
        setup<SOZ_Chan<256>,256, false>(threads, attr);
        break;
      case 512:
        setup<SOZ_Chan<512>,512, false>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
#ifdef VL
  else if(at == VTLINK && direct && touch)
  {
    switch (datasize)
    {
      case 8:
        setup<VLC_Chan<  8>,  8, true>(threads, attr);
        break;
      case 16:
        setup<VLC_Chan< 16>, 16, true>(threads, attr);
        break;
      case 32:
        setup<VLC_Chan< 32>, 32, true>(threads, attr);
        break;
      case 64:
        setup<VLC_Chan< 64>, 64, true>(threads, attr);
        break;
      case 128:
        setup<VLC_Chan<128>,128, true>(threads, attr);
        break;
      case 256:
        setup<VLC_Chan<256>,256, true>(threads, attr);
        break;
      case 512:
        setup<VLC_Chan<512>,512, true>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
  else if(at == VTLINK && direct && !touch)
  {
    switch (datasize)
    {
      case 8:
        setup<VLC_Chan<  8>,  8, false>(threads, attr);
        break;
      case 16:
        setup<VLC_Chan< 16>, 16, false>(threads, attr);
        break;
      case 32:
        setup<VLC_Chan< 32>, 32, false>(threads, attr);
        break;
      case 64:
        setup<VLC_Chan< 64>, 64, false>(threads, attr);
        break;
      case 128:
        setup<VLC_Chan<128>,128, false>(threads, attr);
        break;
      case 256:
        setup<VLC_Chan<256>,256, false>(threads, attr);
        break;
      case 512:
        setup<VLC_Chan<512>,512, false>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
  else if(at == VTLINK && !direct && touch)
  {
    switch (datasize)
    {
      case 8:
        setup<VLZ_Chan<  8>,  8, true>(threads, attr);
        break;
      case 16:
        setup<VLZ_Chan< 16>, 16, true>(threads, attr);
        break;
      case 32:
        setup<VLZ_Chan< 32>, 32, true>(threads, attr);
        break;
      case 64:
        setup<VLZ_Chan< 64>, 64, true>(threads, attr);
        break;
      case 128:
        setup<VLZ_Chan<128>,128, true>(threads, attr);
        break;
      case 256:
        setup<VLZ_Chan<256>,256, true>(threads, attr);
        break;
      case 512:
        setup<VLZ_Chan<512>,512, true>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
  else if(at == VTLINK && !direct && !touch)
  {
    switch (datasize)
    {
      case 8:
        setup<VLZ_Chan<  8>,  8, false>(threads, attr);
        break;
      case 16:
        setup<VLZ_Chan< 16>, 16, false>(threads, attr);
        break;
      case 32:
        setup<VLZ_Chan< 32>, 32, false>(threads, attr);
        break;
      case 64:
        setup<VLZ_Chan< 64>, 64, false>(threads, attr);
        break;
      case 128:
        setup<VLZ_Chan<128>,128, false>(threads, attr);
        break;
      case 256:
        setup<VLZ_Chan<256>,256, false>(threads, attr);
        break;
      case 512:
        setup<VLZ_Chan<512>,512, false>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
#endif //VL
#ifdef BOOST
  else if(at == BOOSTM && direct && touch)
  {
    switch (datasize)
    {
      case 8:
        setup<BMC_Chan<  8>,  8, true>(threads, attr);
        break;
      case 16:
        setup<BMC_Chan< 16>, 16, true>(threads, attr);
        break;
      case 32:
        setup<BMC_Chan< 32>, 32, true>(threads, attr);
        break;
      case 64:
        setup<BMC_Chan< 64>, 64, true>(threads, attr);
        break;
      case 128:
        setup<BMC_Chan<128>,128, true>(threads, attr);
        break;
      case 256:
        setup<BMC_Chan<256>,256, true>(threads, attr);
        break;
      case 512:
        setup<BMC_Chan<512>,512, true>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
  else if(at == BOOSTM && direct && !touch)
  {
    switch (datasize)
    {
      case 8:
        setup<BMC_Chan<  8>,  8, false>(threads, attr);
        break;
      case 16:
        setup<BMC_Chan< 16>, 16, false>(threads, attr);
        break;
      case 32:
        setup<BMC_Chan< 32>, 32, false>(threads, attr);
        break;
      case 64:
        setup<BMC_Chan< 64>, 64, false>(threads, attr);
        break;
      case 128:
        setup<BMC_Chan<128>,128, false>(threads, attr);
        break;
      case 256:
        setup<BMC_Chan<256>,256, false>(threads, attr);
        break;
      case 512:
        setup<BMC_Chan<512>,512, false>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
  else if(at == BOOSTM && !direct && touch)
  {
    switch (datasize)
    {
      case 8:
        setup<BMZ_Chan<  8>,  8, true>(threads, attr);
        break;
      case 16:
        setup<BMZ_Chan< 16>, 16, true>(threads, attr);
        break;
      case 32:
        setup<BMZ_Chan< 32>, 32, true>(threads, attr);
        break;
      case 64:
        setup<BMZ_Chan< 64>, 64, true>(threads, attr);
        break;
      case 128:
        setup<BMZ_Chan<128>,128, true>(threads, attr);
        break;
      case 256:
        setup<BMZ_Chan<256>,256, true>(threads, attr);
        break;
      case 512:
        setup<BMZ_Chan<512>,512, true>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
  else if(at == BOOSTM && !direct && !touch)
  {
    switch (datasize)
    {
      case 8:
        setup<BMZ_Chan<  8>,  8, false>(threads, attr);
        break;
      case 16:
        setup<BMZ_Chan< 16>, 16, false>(threads, attr);
        break;
      case 32:
        setup<BMZ_Chan< 32>, 32, false>(threads, attr);
        break;
      case 64:
        setup<BMZ_Chan< 64>, 64, false>(threads, attr);
        break;
      case 128:
        setup<BMZ_Chan<128>,128, false>(threads, attr);
        break;
      case 256:
        setup<BMZ_Chan<256>,256, false>(threads, attr);
        break;
      case 512:
        setup<BMZ_Chan<512>,512, false>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
  else if(at == BOOSTS && direct && touch)
  {
    switch (datasize)
    {
      case 8:
        setup<BSC_Chan<  8>,  8, true>(threads, attr);
        break;
      case 16:
        setup<BSC_Chan< 16>, 16, true>(threads, attr);
        break;
      case 32:
        setup<BSC_Chan< 32>, 32, true>(threads, attr);
        break;
      case 64:
        setup<BSC_Chan< 64>, 64, true>(threads, attr);
        break;
      case 128:
        setup<BSC_Chan<128>,128, true>(threads, attr);
        break;
      case 256:
        setup<BSC_Chan<256>,256, true>(threads, attr);
        break;
      case 512:
        setup<BSC_Chan<512>,512, true>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
  else if(at == BOOSTS && direct && !touch)
  {
    switch (datasize)
    {
      case 8:
        setup<BSC_Chan<  8>,  8, false>(threads, attr);
        break;
      case 16:
        setup<BSC_Chan< 16>, 16, false>(threads, attr);
        break;
      case 32:
        setup<BSC_Chan< 32>, 32, false>(threads, attr);
        break;
      case 64:
        setup<BSC_Chan< 64>, 64, false>(threads, attr);
        break;
      case 128:
        setup<BSC_Chan<128>,128, false>(threads, attr);
        break;
      case 256:
        setup<BSC_Chan<256>,256, false>(threads, attr);
        break;
      case 512:
        setup<BSC_Chan<512>,512, false>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
  else if(at == BOOSTS && !direct && touch)
  {
    switch (datasize)
    {
      case 8:
        setup<BSZ_Chan<  8>,  8, true>(threads, attr);
        break;
      case 16:
        setup<BSZ_Chan< 16>, 16, true>(threads, attr);
        break;
      case 32:
        setup<BSZ_Chan< 32>, 32, true>(threads, attr);
        break;
      case 64:
        setup<BSZ_Chan< 64>, 64, true>(threads, attr);
        break;
      case 128:
        setup<BSZ_Chan<128>,128, true>(threads, attr);
        break;
      case 256:
        setup<BSZ_Chan<256>,256, true>(threads, attr);
        break;
      case 512:
        setup<BSZ_Chan<512>,512, true>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
  else if(at == BOOSTS && !direct && !touch)
  {
    switch (datasize)
    {
      case 8:
        setup<BSZ_Chan<  8>,  8, false>(threads, attr);
        break;
      case 16:
        setup<BSZ_Chan< 16>, 16, false>(threads, attr);
        break;
      case 32:
        setup<BSZ_Chan< 32>, 32, false>(threads, attr);
        break;
      case 64:
        setup<BSZ_Chan< 64>, 64, false>(threads, attr);
        break;
      case 128:
        setup<BSZ_Chan<128>,128, false>(threads, attr);
        break;
      case 256:
        setup<BSZ_Chan<256>,256, false>(threads, attr);
        break;
      case 512:
        setup<BSZ_Chan<512>,512, false>(threads, attr);
        break;
      default:
      std::cerr << "Invalid Size" << std::endl;
    }
  }
#endif //BOOST
  else
  {
    std::cerr << "Invalid Buffer type" << std::endl;
  }
  delete[] threads;
  delete[] attr;


  return 0;
}
