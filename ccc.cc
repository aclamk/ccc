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
  alignas(64) static char xxx[cnt];
  static_assert(cnt<=sizeof(working_entry::content), "write range exceeds buffer");
  //int len=0;
  while (w != nullptr) 
    {
      //for (size_t i = 0 ; i < cnt ; i++)
	{
	  //w->content[i] += (i+1);
	  memcpy(w->content, xxx, sizeof(xxx));
	}
      //len++;
      w = w->next;
    }
  //printf("%d\n",len);
}

template <int cnt>
void just_read(working_entry* w)
{
  static char xxx[cnt];
  static_assert(cnt<=sizeof(working_entry::content), "write range exceeds buffer");
  while (w != nullptr) 
    {
      //for (size_t i = 0 ; i < cnt ; i++)
	{
	  memcpy(xxx, w->content, sizeof(xxx));

	}
      w = w->next;
    }
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
  //sem_t first_done;
  //sem_t second_done;

  //sem_init(&first_done, 0, 0);
  //sem_init(&second_done, 0, 0);

  std::atomic<uint32_t> first_done(0);
  std::atomic<uint32_t> second_done(0);

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
	      for(int l=0;l<load;l++)
		{
		time -= now_usec();
		
		//func(chain_begin(data, i));
		func(chain_begin(data, (i&~1) + (((i+l)&1)) ));
		time += now_usec();
		
		first_done++;
		//sem_post(&first_done);
		
		while(second_done.load()==0);
		second_done--;
		//sem_post(&first_done);
		//sem_wait(&second_done);
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
	      for(int l=0;l<load;l++)
	      {
		time -= now_usec();
		func(chain_begin(data, (i&~1) + (1 - ((i+l)&1)) ));
		time += now_usec();


		second_done++;
		//sem_post(&second_done);
		while(first_done.load()==0);
		first_done--;

		//		sem_post(&second_done);
		//sem_wait(&first_done);
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





std::pair<uint64_t, uint64_t> multi_thread_solowork(char* data, size_t no_chains, int cpu, int load, func_t func)
{
  //sem_t first_done;
  //sem_t second_done;

  //sem_init(&first_done, 0, 0);
  //sem_init(&second_done, 0, 0);

  alignas(64) std::atomic<uint32_t> first_done(0);
  alignas(64) std::atomic<uint32_t> second_done(0);

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
	      for(int l=0;l<load;l++)
		{
		time -= now_usec();
		 {
		  func(chain_begin(data, i));
		  func(chain_begin(data, i));
		}
		time += now_usec();
		first_done++;
		//sem_post(&first_done);
		
		while(second_done.load()==0);
		second_done--;
		//sem_wait(&second_done);
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
	      for(int l=0;l<load;l++)
	      {
		time -= now_usec();
		time += now_usec();
		second_done++;
		//sem_post(&second_done);
		while(first_done.load()==0);
		first_done--;
		//sem_wait(&first_done);
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
set_affinity(3);
  for (size_t active_size=cache_size; active_size<=cache_size*3; active_size+=cache_size)

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

    std::pair<char*,size_t> p;
    p = generate(active_size, chain_len);

    for (int mode=0; mode<=1; mode++)
      for (int f=0; f<5; f++)
	{
	  for(int cpu=1; cpu<=2; cpu++)
	    {
	      for(int load=1; load<=4; load++)
		{
		  func_t func = mode==0?write_functions[f]:read_functions[f];

		  std::pair<uint64_t, uint64_t> solowork_time;
		  std::pair<uint64_t, uint64_t> cowork_time;

		  solowork_time = multi_thread_solowork(p.first, p.second, cpu, load, func);
		  cowork_time = multi_thread_cowork(p.first, p.second, cpu, load, func);
		  printf("active_size=%10lu ch_len=%4d no_chains=%5ld %s(%2d) cpu.aff=%2d load=%dx "
			 "solo=%8ld cowork_a=%8ld cowork_b=%8ld\n",
			 active_size,
			 chain_len, p.second, 
			 mode==0?"write":"read ", 
			 proc_sizes[f],
			 cpu, load, 
			 solowork_time.first, cowork_time.first, cowork_time.second 
			 );
			     
		}
	    }
	}
    free(p.first);
  }

      
  

}
