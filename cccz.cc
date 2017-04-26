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
#include <mutex>

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

class SpinLock {
    std::atomic_flag locked = ATOMIC_FLAG_INIT ;
public:
    void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) { ; }
    }
    void unlock() {
        locked.clear(std::memory_order_release);
    }
};




void inc(char* p) __attribute__((noinline));
void dec(char* p) __attribute__((noinline));
char peek(char* p) __attribute__((noinline));

void inc(char* p){(*p)++;};
void dec(char* p){(*p)--;};
char peek(char* p){return (*p);};

void set(char* p,char a){(*p)=a;};


template <size_t write_count, size_t write_spread>
int inc(char* p) __attribute__((noinline));
template <size_t write_count, size_t write_spread>
int inc(char* p){
  (*p)++;
  return write_count*200+write_spread*2;
};

template <size_t write_count, size_t write_spread, bool shared>
int atominc(std::atomic<int8_t>* p) __attribute__((noinline));
template <size_t write_count, size_t write_spread, bool shared>
int atominc(std::atomic<int8_t>* p){
  (*p)++;
  return write_count*200+write_spread*2+(int)shared;
};

template <size_t write_count, size_t write_spread>
int atomxxxx(std::atomic<int8_t>* p) __attribute__((noinline));
template <size_t write_count, size_t write_spread>
int atomxxxx(std::atomic<int8_t>* p){
  (*p)++;
  return 100000+write_count*200+write_spread*2;
};




template <size_t write_count, size_t write_spread, bool share>
uint64_t generic_test(
    size_t test_area_size,
    char* test_area_in,
    std::vector<int>& cpus)
{
  test_area_size = test_area_size/2;
  alignas(cache_line_size) SpinLock m1;
  alignas(cache_line_size) SpinLock m2;

  std::atomic<uint64_t> time_a(0);


  std::vector<std::thread> threads;
  int cpu_count = cpus.size();
  m2.lock();
  for (int c=0; c<cpu_count; c++)
  {
    threads.push_back( std::thread([&,c]() {
      set_affinity(cpus[c]);
      char* test_area = test_area_in + (share?0:1)*c*test_area_size;
      uint64_t time = 0;
      for (size_t k=0; k<hard_repeats; k++)
      {
        time -= now_usec();
        size_t r=0;
        for (size_t i=0; i<64*256; i++)
        {
          if (c==0)
            m1.lock();
          else
            m2.lock();
          //time -= now_usec();
          char a=0;
          r = 0;
          for(size_t wc=0; wc<write_count; wc++)
          {
            inc<write_count, write_spread>(&test_area[r]);
            a+=peek(&test_area[r]);
            r=r+write_spread;
            if(r>test_area_size) r-=test_area_size;
          }
          set(&test_area[r],a);

          //time+= now_usec();

          if (c==0)
            m2.unlock();
          else
            m1.unlock();
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




typedef uint64_t (*test_f)(
    size_t test_area_size,
    char* test_area,
    std::vector<int>& cpus);

struct test_t {
  size_t write_count;
  size_t write_spread;
  bool share;
  test_f test;
};

#define TEST_T(a,b,c) {a*15,b,c,generic_test<a*15,b,c>}

std::vector<test_t> regular_separate{
  TEST_T(0, 4, false),
  TEST_T(5, 4, false),
  TEST_T(10, 4, false),
  TEST_T(15, 4, false),
  TEST_T(20, 4, false),
  TEST_T(25, 4, false),
  TEST_T(30, 4, false),

  TEST_T(0, 14, false),
  TEST_T(5, 14, false),
  TEST_T(10, 14, false),
  TEST_T(15, 14, false),
  TEST_T(20, 14, false),
  TEST_T(25, 14, false),
  TEST_T(30, 14, false),

  TEST_T(0, 14, false),
  TEST_T(5, 14, false),
  TEST_T(10, 14, false),
  TEST_T(15, 14, false),
  TEST_T(20, 14, false),
  TEST_T(25, 14, false),
  TEST_T(30, 14, false),

  TEST_T(0, 24, false),
  TEST_T(5, 24, false),
  TEST_T(10, 24, false),
  TEST_T(15, 24, false),
  TEST_T(20, 24, false),
  TEST_T(25, 24, false),
  TEST_T(30, 24, false),

  TEST_T(0, 34, false),
  TEST_T(5, 34, false),
  TEST_T(10, 34, false),
  TEST_T(15, 34, false),
  TEST_T(20, 34, false),
  TEST_T(25, 34, false),
  TEST_T(30, 34, false),

  TEST_T(0, 64, false),
  TEST_T(5, 64, false),
  TEST_T(10, 64, false),
  TEST_T(15, 64, false),
  TEST_T(20, 64, false),
  TEST_T(25, 64, false),
  TEST_T(30, 64, false),

  TEST_T(0, 4, true),
  TEST_T(5, 4, true),
  TEST_T(10, 4, true),
  TEST_T(15, 4, true),
  TEST_T(20, 4, true),
  TEST_T(25, 4, true),
  TEST_T(30, 4, true),

  TEST_T(0, 14, true),
  TEST_T(5, 14, true),
  TEST_T(10, 14, true),
  TEST_T(15, 14, true),
  TEST_T(20, 14, true),
  TEST_T(25, 14, true),
  TEST_T(30, 14, true),

  TEST_T(0, 24, true),
  TEST_T(5, 24, true),
  TEST_T(10, 24, true),
  TEST_T(15, 24, true),
  TEST_T(20, 24, true),
  TEST_T(25, 24, true),
  TEST_T(30, 24, true),

  TEST_T(0, 34, true),
  TEST_T(5, 34, true),
  TEST_T(10, 34, true),
  TEST_T(15, 34, true),
  TEST_T(20, 34, true),
  TEST_T(25, 34, true),
  TEST_T(30, 34, true),

  TEST_T(0, 64, true),
  TEST_T(5, 64, true),
  TEST_T(10, 64, true),
  TEST_T(15, 64, true),
  TEST_T(20, 64, true),
  TEST_T(25, 64, true),
  TEST_T(30, 64, true),

};

int main(int argc, char** argv)
{
  size_t test_area_size = 128*1024;
  char* test_area = (char*)malloc(test_area_size);

  std::vector<int> vec{0,1};
  for (int i=1; i<2; i++)
    {
      //vec.push_back(i);
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
        printf("%3lu %3lu %s %9lu\n", e.write_count, e.write_spread, e.share?"shared  ":"separate", time);
      }
    }
}
