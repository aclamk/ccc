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
static constexpr size_t cache_line_size = 64;
constexpr size_t cache_size = 8192*1024;

constexpr size_t entries = cache_size / cache_line_size;





struct working_entry 
{
  working_entry* next;
  char content[cache_line_size - sizeof(working_entry*)];
};

static_assert(sizeof(working_entry) == cache_line_size, "how come??");


uint64_t now_usec()
{
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec*1000000 + tv.tv_usec;
}



void generate_random_permutation(size_t len, uint16_t* perm)
{
  assert(len < 1<<16);
  for (size_t i=0; i<len ; i++)
    perm[i] = i;
  for (size_t i=0; i<len ; i++)
    {
      size_t j = rand() % len;
      uint16_t tmp = perm[i];
      perm[i] = perm[j];
      perm[j] = tmp;
    }
}

void generate_random_data(working_entry* w)
{
  for (size_t i = 0 ; i < sizeof(working_entry::content) ; i++)
      w->content[i]=rand();
}


void generate_sequences(const uint16_t* perm, size_t perm_size, 
			//size_t chain_depth,
			char* raw_data)
{
  size_t curr;
  size_t next;
  struct working_entry* curr_ptr;
  struct working_entry* next_ptr;
  size_t block_size = cache_line_size * perm_size;
  size_t chain_depth = entries / perm_size; 
  for (size_t i=0; i<perm_size; i++)
    {
      curr = i;
      curr_ptr = reinterpret_cast<struct working_entry*>
	(raw_data + block_size * 0 + cache_line_size * curr);
      
      for (size_t j=0; j<chain_depth ; j++)
	{
	  //do fill with data;
	  generate_random_data(curr_ptr);
	  next = perm[curr];
	  next_ptr = reinterpret_cast<struct working_entry*>
	    (raw_data + block_size * (j + 1) + cache_line_size * next);
	  if (j==chain_depth - 1 ) 
	    next_ptr = nullptr;
	  curr_ptr -> next = next_ptr;
	 
	  curr = next;
	  curr_ptr = next_ptr;
	}
    }
} 


void generate_sequences(const uint16_t* perm, size_t perm_size, 
			size_t chain_depth,
			char* raw_data)
{
  size_t curr;
  size_t next;
  struct working_entry* curr_ptr;
  struct working_entry* next_ptr;
  size_t block_size = cache_line_size * perm_size;
  //size_t chain_depth = entries / perm_size; 
  for (size_t i=0; i<perm_size; i++)
    {
      curr = i;
      curr_ptr = reinterpret_cast<struct working_entry*>
	(raw_data + block_size * 0 + cache_line_size * curr);
      
      for (size_t j=0; j<chain_depth ; j++)
	{
	  //do fill with data;
	  generate_random_data(curr_ptr);
	  next = perm[curr];
	  next_ptr = reinterpret_cast<struct working_entry*>
	    (raw_data + block_size * (j + 1) + cache_line_size * next);
	  if (j==chain_depth - 1 ) 
	    next_ptr = nullptr;
	  curr_ptr -> next = next_ptr;
	 
	  curr = next;
	  curr_ptr = next_ptr;
	}
    }
} 

template <int cnt>
void add_front(working_entry* w)
{
  static_assert(cnt<=sizeof(working_entry::content), "write range exceeds buffer");
  while (w != nullptr) 
    {
      for (size_t i = 0 ; i < cnt ; i++)
	{
	  w->content[i] += (i+1);
	}
      w = w->next;
    }
}

template <int cnt>
void just_read(working_entry* w)
{
  char m=0;
  static_assert(cnt<=sizeof(working_entry::content), "write range exceeds buffer");
  while (w != nullptr) 
    {
      for (size_t i = 0 ; i < cnt ; i++)
	{
	  m += w->content[i];
	}
      if(w->next == nullptr)
	break;
      w = w->next;
    }
  w->content[0]=m;
}


template <int cnt>
void add_end(working_entry* w)
{
  static_assert(cnt<=sizeof(working_entry::content), "write range exceeds buffer");
  while (w != nullptr) 
    {
      for (size_t i = 0 ; i < cnt ; i++)
	{
	  w->content[sizeof(working_entry::content)-1-i] += (i+1);
	}
      w = w->next;
    }
}




void add(working_entry* w)
{
  while (w != nullptr) 
    {
      for (size_t i = 0 ; i < 5 /*sizeof(working_entry::content)*/ ; i++)
	{
	  w->content[i]+=(i+1);
	}
      w = w->next;
    }
}


typedef void (*func_t)(working_entry* w);

void set_affinity(int cpu)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  int s;
  s = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
  assert(s == 0);
}




working_entry* chain_begin(char* data, size_t i)
{
  return reinterpret_cast<struct working_entry*>
		(data + cache_line_size * i);
}




std::pair<uint64_t, uint64_t> multi_thread_cowork(char* data, size_t no_chains, int cpu, int load, func_t func)
{
  sem_t first_done;
  sem_t second_done;

  sem_init(&first_done, 0, 0);
  sem_init(&second_done, 0, 0);

  std::atomic<uint64_t> time_a(0);
  std::atomic<uint64_t> time_b(0);

  std::thread first_action = 
    std::thread([&]()
      {
	set_affinity(0);
	uint64_t time = 0;
	for (size_t k=0; k<10; k++)
	  {
	    for (size_t i=0; i<no_chains; i++)
	      {
		time -= now_usec();
		for(int l=0;l<load;l++)
		  func(chain_begin(data, i));
		time += now_usec();
		sem_post(&first_done);
		sem_wait(&second_done);
	      }
	  }
	time_a += time;
      }
  );

  std::thread second_action = 
    std::thread([&]()
      {
	set_affinity(cpu);
	uint64_t time = 0;
	for (size_t k=0; k<10; k++)
	  {
	    for (size_t i=0; i<no_chains; i++)
	      {
		time -= now_usec();
		for(int l=0;l<load;l++)
		  func(chain_begin(data, (i&~1) + (1 - (i&1)) ));
		time += now_usec();
		sem_post(&second_done);
		sem_wait(&first_done);
	      }
	  }
	time_b += time;
      }
  );
  first_action.join();
  second_action.join();
  return std::make_pair(time_a.load(), time_b.load());
  printf("COWORK    total_time = %ld %ld\n", time_a.load(), time_b.load());
}



std::pair<uint64_t, uint64_t> multi_thread_fully_separated(char* data, size_t no_chains, int cpu, int load, func_t func)
{
  sem_t first_done;
  sem_t second_done;

  sem_init(&first_done, 0, 0);
  sem_init(&second_done, 0, 0);

  std::atomic<uint64_t> time_a(0);
  std::atomic<uint64_t> time_b(0);

  std::thread first_action = 
    std::thread([&]()
      {
	set_affinity(0);
	uint64_t time = 0;
	for (size_t k=0; k<10; k++)
	  {
	    for (size_t i=0; i<no_chains; i+=2)
	      {
		time -= now_usec();
		for(int l=0;l<load;l++) {
		  func(chain_begin(data, i));
		  func(chain_begin(data, i));
		}
		time += now_usec();
		sem_post(&first_done);
		sem_wait(&second_done);
	      }
	  }
	time_a += time;
      }
  );

  std::thread second_action = 
    std::thread([&]()
      {
	set_affinity(cpu);
	uint64_t time = 0;
	for (size_t k=0; k<10; k++)
	  {
	    for (size_t i=0; i<no_chains; i+=2)
	      {
		time -= now_usec();
		for(int l=0;l<load;l++) {
		  func(chain_begin(data, i+1));
		  func(chain_begin(data, i+1));
		}
		time += now_usec();
		sem_post(&second_done);
		sem_wait(&first_done);
	      }
	  }
	time_b += time;
      }
  );
  first_action.join();
  second_action.join();
  return std::make_pair(time_a.load(), time_b.load());

  printf("SEPARATED total_time = %ld %ld\n", time_a.load(), time_b.load() );
}



std::pair<uint64_t, uint64_t> multi_thread_solowork(char* data, size_t no_chains, int cpu, int load, func_t func)
{
  sem_t first_done;
  sem_t second_done;

  sem_init(&first_done, 0, 0);
  sem_init(&second_done, 0, 0);
  std::atomic<uint64_t> time_a(0);
  std::atomic<uint64_t> time_b(0);

  std::thread first_action = 
    std::thread([&]()
      {
	set_affinity(0);
	uint64_t time = 0;
	for (size_t k=0; k<10; k++)
	  {
	    for (size_t i=0; i<no_chains; i++)
	      {
		time -= now_usec();
		for(int l=0;l<load;l++) {
		  func(chain_begin(data, i));
		  func(chain_begin(data, i));
		}
		time += now_usec();
		sem_post(&first_done);
		sem_wait(&second_done);
	      }
	  }
	time_a += time;
      }
  );

  std::thread second_action = 
    std::thread([&]()
      {
	set_affinity(cpu);
	uint64_t time = 0;
	for (size_t k=0; k<10; k++)
	  {
	    for (size_t i=0; i<no_chains; i++)
	      {
		time -= now_usec();
		time += now_usec();
		sem_post(&second_done);
		sem_wait(&first_done);
	      }
	  }
	time_b += time;
      }
  );
  first_action.join();
  second_action.join();
  return std::make_pair(time_a.load(), time_b.load());
  printf("SOLOWORK  total_time = %ld %ld\n", time_a.load(), time_b.load());
}








std::pair<char*,size_t> generate(size_t size, size_t depth)
{
  size_t entries = size / sizeof(working_entry);
  size_t no_chains = entries / depth; 
  
  char* data = (char*) memalign(cache_line_size, size);
  uint16_t* perm = new uint16_t[no_chains];

  generate_random_permutation(no_chains, perm);
  generate_sequences(perm, no_chains, depth, data);

  return std::make_pair(data, no_chains);
}


typedef std::pair<uint64_t, uint64_t> (*test_func_t)(char* data, size_t no_chains, int cpu, int load, func_t func);


int main(int argc, char** argv)
{
  for (int x=1; x<=4; x*=2)
    for (int chain_len=32; chain_len<=1024; chain_len*=2)
  {
    constexpr int proc_sizes[5]=
      {
	1,4,16,32,sizeof(working_entry::content)
      };
    constexpr func_t write_functions[5]={
      add_front<proc_sizes[0]>,
      add_front<proc_sizes[1]>,
      add_front<proc_sizes[2]>,
      add_front<proc_sizes[3]>,
      add_front<proc_sizes[4]>};
    constexpr func_t read_functions[5]={
      just_read<proc_sizes[0]>,
      just_read<proc_sizes[1]>,
      just_read<proc_sizes[2]>,
      just_read<proc_sizes[3]>,
      just_read<proc_sizes[4]>};

    constexpr test_func_t test_functions[3]={
      multi_thread_solowork,
      multi_thread_cowork,
      multi_thread_fully_separated
    };
    const char* test_names[3]={
      "solowork ",
      "cowork   ",
      "separated"
    };

    for (int mode=0; mode<=1; mode++)
      for (int f=0; f<5; f++)
	{
	  for(int cpu=1; cpu<=2; cpu++)
	    {
	      for(int load=1; load<=4; load++)
		{
		  std::pair<char*,size_t> p;
		  p = generate(cache_size * x, chain_len);
		  /*
		  printf("A cache_size_multiplier=%d chain_depth=%d no_chains = %ld\n", x,y,p.second);
		  printf("mode=%s processing_size = %d cpu=%d load=%d\n", 
			 mode==0?"write":"read ",
			 proc_sizes[f], cpu,load);
		  */
		  func_t func = mode==0?write_functions[f]:read_functions[f];

		  std::pair<uint64_t, uint64_t> time;
		  for(int test=0; test<=2; test++)
		    {
		      time = test_functions[test](p.first, p.second, cpu, load, func);
		      printf("ch_len=%4d no_chains=%5ld, %s %s cpu.aff=%d load=%dx time_a=%8ld time_b=%8ld\n",
			     chain_len, p.second, 
			     test_names[test], mode==0?"write":"read ", 
			     cpu, load, 
			     time.first, time.second);
			     
		    }
		  free(p.first);
		}
	    }
	}
  }

      
  

}
