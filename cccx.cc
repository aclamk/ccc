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

template <size_t write_count, size_t write_spread, bool shared>
int inc(char* p) __attribute__((noinline));
template <size_t write_count, size_t write_spread, bool shared>
int inc(char* p){
  (*p)++;
  return write_count*200+write_spread*2+(int)shared;
};

template <size_t write_count, size_t write_spread, bool shared>
int atominc(std::atomic<int8_t>* p) __attribute__((noinline));
template <size_t write_count, size_t write_spread, bool shared>
int atominc(std::atomic<int8_t>* p){
  (*p)++;
  return write_count*200+write_spread*2+(int)shared;
};





template <size_t write_count, size_t write_spread, bool share, bool do_atomic>
uint64_t generic_test(
    size_t test_area_size,
    char* test_area,
    std::vector<int>& cpus)
{
  std::atomic<uint32_t> first_done[cpus.size()];
  for(auto& a:first_done)
    a.store(0);
  std::atomic<uint64_t> time_a(0);

  alignas(cache_line_size) char buffer[cache_line_size*sizeof(cpus)]={0,};
  memset(buffer, 0, cache_line_size);
  alignas(cache_line_size) std::atomic<int8_t> atomiki[cache_line_size*cpus.size()];
  for(auto& a:atomiki)
    a.store(0);

  std::vector<std::thread> threads;
  int cpu_count = cpus.size();
  size_t range = cache_line_size/cpu_count*cpu_count;
  for (int c=0; c<cpu_count; c++)
  {
    threads.push_back( std::thread([&,c]() {
      set_affinity(cpus[c]);

      char* buffer_in=buffer;
      if (!share)
        buffer_in = &buffer[cache_line_size*c];
      std::atomic<int8_t>* atomiki_in=atomiki;
        if (!share)
          atomiki_in = &atomiki[cache_line_size*c];

      uint64_t time = 0;
      for (size_t k=0; k<hard_repeats; k++)
      {
        first_done[(c+1)%cpu_count]++;
        while(first_done[c].load() == 0);

        time -= now_usec();
        size_t j=c;
        size_t r=0;
        for (size_t i=0; i<64*256*100; i+=cpu_count)
        {
          for(size_t wc=0; wc<write_count; wc++)
          {
            test_area[r] = 1;
            r=r+write_spread;
            if(r>test_area_size) r-=test_area_size;
          }
          if (do_atomic)
            atominc<write_count, write_spread, share>(&atomiki_in[j]);
          else
            inc<write_count, write_spread, share>(&buffer_in[j]);
          j+=cpu_count;
          if (j>=range) j-=range;
        }
        time+= now_usec();
        first_done[c]--;
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




typedef uint64_t (*test_f)(
    size_t test_area_size,
    char* test_area,
    std::vector<int>& cpus);

struct test_t {
  size_t write_count;
  size_t write_spread;
  bool share;
  bool do_atomic;
  test_f test;
};

#define TEST_T(a,b,c,d) {a,b,c,d,generic_test<a,b,c,d>}

std::vector<test_t> regular_separate{
  TEST_T(0, 4, false, false),
  TEST_T(5, 4, false, false),
  TEST_T(10, 4, false, false),
  TEST_T(15, 4, false, false),
  TEST_T(20, 4, false, false),
  TEST_T(25, 4, false, false),
  TEST_T(30, 4, false, false),

  TEST_T(0, 4, true, false),
  TEST_T(5, 4, true, false),
  TEST_T(10, 4, true, false),
  TEST_T(15, 4, true, false),
  TEST_T(20, 4, true, false),
  TEST_T(25, 4, true, false),
  TEST_T(30, 4, true, false),

  TEST_T(0, 14, false, false),
  TEST_T(5, 14, false, false),
  TEST_T(10, 14, false, false),
  TEST_T(15, 14, false, false),
  TEST_T(20, 14, false, false),
  TEST_T(25, 14, false, false),
  TEST_T(30, 14, false, false),

  TEST_T(0, 14, true, false),
  TEST_T(5, 14, true, false),
  TEST_T(10, 14, true, false),
  TEST_T(15, 14, true, false),
  TEST_T(20, 14, true, false),
  TEST_T(25, 14, true, false),
  TEST_T(30, 14, true, false),

  TEST_T(0, 64, false, false),
  TEST_T(5, 64, false, false),
  TEST_T(10, 64, false, false),
  TEST_T(15, 64, false, false),
  TEST_T(20, 64, false, false),
  TEST_T(25, 64, false, false),
  TEST_T(30, 64, false, false),

  TEST_T(0, 64, true, false),
  TEST_T(5, 64, true, false),
  TEST_T(10, 64, true, false),
  TEST_T(15, 64, true, false),
  TEST_T(20, 64, true, false),
  TEST_T(25, 64, true, false),
  TEST_T(30, 64, true, false),

  TEST_T(0, 4, false, true),
  TEST_T(5, 4, false, true),
  TEST_T(10, 4, false, true),
  TEST_T(15, 4, false, true),
  TEST_T(20, 4, false, true),
  TEST_T(25, 4, false, true),
  TEST_T(30, 4, false, true),

  TEST_T(0, 4, true, true),
  TEST_T(5, 4, true, true),
  TEST_T(10, 4, true, true),
  TEST_T(15, 4, true, true),
  TEST_T(20, 4, true, true),
  TEST_T(25, 4, true, true),
  TEST_T(30, 4, true, true),

  TEST_T(0, 14, false, true),
  TEST_T(5, 14, false, true),
  TEST_T(10, 14, false, true),
  TEST_T(15, 14, false, true),
  TEST_T(20, 14, false, true),
  TEST_T(25, 14, false, true),
  TEST_T(30, 14, false, true),

  TEST_T(0, 14, true, true),
  TEST_T(5, 14, true, true),
  TEST_T(10, 14, true, true),
  TEST_T(15, 14, true, true),
  TEST_T(20, 14, true, true),
  TEST_T(25, 14, true, true),
  TEST_T(30, 14, true, true),

  TEST_T(0, 64, false, true),
  TEST_T(5, 64, false, true),
  TEST_T(10, 64, false, true),
  TEST_T(15, 64, false, true),
  TEST_T(20, 64, false, true),
  TEST_T(25, 64, false, true),
  TEST_T(30, 64, false, true),

  TEST_T(0, 64, true, true),
  TEST_T(5, 64, true, true),
  TEST_T(10, 64, true, true),
  TEST_T(15, 64, true, true),
  TEST_T(20, 64, true, true),
  TEST_T(25, 64, true, true),
  TEST_T(30, 64, true, true),
};

int main(int argc, char** argv)
{
  size_t test_area_size = 2*1024*1024;
  char* test_area = (char*)malloc(test_area_size);

  std::vector<int> vec;
  for (int i=0; i<4; i++)
    {
      vec.push_back(i);
      uint64_t time;
      printf("CPUs=");
      for (auto i:vec)
	{
	  printf("%2d ",i);
	}
      printf("\n");
      for (auto &e : regular_separate)
      {
        time = e.test(test_area_size, test_area, vec);
        printf("%2lu %2lu %s %s %9lu\n", e.write_count, e.write_spread, e.share?"shared  ":"separate", e.do_atomic?"atomic":"normal", time);
      }
    }
}
