#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <sys/time.h>
#include <semaphore.h>
#include <thread>
#include <atomic>
#include <vector>

static constexpr size_t cache_line_size = 64;
constexpr size_t cache_size = 8192*1024/4;

constexpr size_t entries = cache_size / cache_line_size;

constexpr size_t hard_repeats = 10;



uint64_t now_usec()
{
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec*1000000 + tv.tv_usec;
}

void set_affinity(int cpu)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  int s;
  s = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
  assert(s == 0);
}





void inc(char* p) __attribute__((noinline));
void dec(char* p) __attribute__((noinline));
char peek(char* p) __attribute__((noinline));

void inc(char* p){(*p)++;};
void dec(char* p){(*p)--;};
char peek(char* p){return (*p);};





uint64_t multi_thread_regular(
    std::vector<int>& cpus)
{
  std::atomic<uint32_t> first_done[cpus.size()];
  for(auto& a:first_done)
    a.store(0);
  std::atomic<uint64_t> time_a(0);

  alignas(cache_line_size) char buffer[cache_line_size]={0,};
  memset(buffer,0, cache_line_size);

  std::vector<std::thread> threads;
  int cpu_count = cpus.size();
  size_t range = cache_line_size/cpu_count*cpu_count;
  for (int c=0; c<cpu_count; c++)
    {
      threads.push_back(
	std::thread([&,c]()
      {
	set_affinity(cpus[c]);
	uint64_t time = 0;
	for (size_t k=0; k<hard_repeats; k++)
	  {
	    first_done[(c+1)%cpu_count]++;
	    while(first_done[c].load() == 0);
	    first_done[c]--;
	    
	    time -= now_usec();	
	    size_t j=c;
	    for (size_t i=0; i<64*256*1000; i+=cpu_count)
	      {
		inc(&buffer[j]);
		j+=cpu_count;
		if (j>=range) j-=range;
	      }
	    time+= now_usec();
	  }
	time_a += time;
      }
    )
  );
    };
  for(auto& t:threads)
    t.join();
  return time_a.load();
}


uint64_t multi_thread_atomic(
    std::vector<int>& cpus)
{
  std::atomic<uint32_t> first_done[cpus.size()];
  for(auto& a:first_done)
    a.store(0);

  std::atomic<uint64_t> time_a(0);

  alignas(cache_line_size) std::atomic<int8_t> atomiki[cache_line_size];
  for(auto& a:atomiki)
    a.store(0);

  std::vector<std::thread> threads;
  int cpu_count = cpus.size();
  size_t range = cache_line_size/cpu_count*cpu_count;
  for (int c=0; c<cpu_count; c++)
    {
  threads.push_back(
    std::thread([&,c]()
      {
	set_affinity(cpus[c]);
	uint64_t time = 0;
	for (size_t k=0; k<hard_repeats; k++)
	  {
	    first_done[(c+1)%cpu_count]++;
	    while(first_done[c].load() == 0);
	    first_done[c]--;
	    
	    time -= now_usec();	
	    size_t j=c;
	    for (size_t i=0; i<64*256*1000; i+=cpu_count)
	      {
		atomiki[j]++;
		j+=cpu_count;
		if (j>=range) j-=range;
	      }
	    time+= now_usec();
	  }
	time_a += time;
      }
    )
  );
    };
  for(auto& t:threads)
    t.join();
  return time_a.load();
}

int main(int argc, char** argv)
{
  {
  std::vector<std::vector<int>> cpu_sets={{0}, {0,1}, {0,2}, {0,1,2} , {0,1,2,3}};
  for (auto& vec:cpu_sets)
    {
      uint64_t time;
      printf("CPUs=");
      for (auto i:vec)
	{
	  printf("%2d ",i);
	}
      printf("\n");
      time = multi_thread_regular(vec);
      printf("regular (us)=%9lu\n",time);
      time = multi_thread_atomic(vec);
      printf("atomic (us) =%9lu\n",time);
    }
  } 
}
