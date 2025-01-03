// Test program to allocate new memory
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <fstream>
#include <cassert>
#include <map>

#include <thread>
#include <atomic>
#include <iostream>
#include <cstring>
#include <mutex>

#include "../include/lockwrapper.h"
#include "../include/settings.h"
#include "zmalloc.h"
#include "util.h"
#include "gallocator.h"

#define TEST_ALLOC_FLAG MAP_PRIVATE|MAP_ANONYMOUS    // default: 0xef
#define TEST_INIT_ALLOC_SIZE (unsigned long)9 * 1024 * 1024 * 1024 // default: 16 GB
#define TEST_METADATA_SIZE 16

#define LOG_NUM_ONCE (unsigned long)1000
#define LOG_NUM_TOTAL (unsigned long)500000//00
#define MMAP_ADDR_MASK 0xffffffffffff
#define MAX_NUM_THREAD 16
#define SLEEP_THRES_NANOS 10
#define TIMEWINDOW_US 100000//00
#define DEBUG_LEVEL LOG_WARNING
//#define SYNC_KEY 204800
#define PASS_KEY 40960000
#define SYNC_KEY (unsigned long)10 * 1024 * 1024 * 1024 // default: 10 GB
//#define num_comp_nodes 4
//#define NUM_MEM_NODES 2
#define CDF_BUCKET_NUM	512

int addr_size = sizeof(GAddr);

// Test configuration
//#define single_thread_test
//#define meta_data_test

using namespace std;

struct log_header_5B {
  char op;
  unsigned int usec;
}__attribute__((__packed__));

struct RWlog {
  char op;
  union {
    struct {
      char pad[6];
      unsigned long usec;
    }__attribute__((__packed__));
    unsigned long addr;
  }__attribute__((__packed__));
}__attribute__((__packed__));

struct Mlog {
  struct log_header_5B hdr;
  union {
    unsigned long start;
    struct {
      char pad[6];
      unsigned len;
    }__attribute__((__packed__));
  }__attribute__((__packed__));
}__attribute__((__packed__));

struct Blog {
  char op;
  union {
    struct {
      char pad[6];
      unsigned long usec;
    }__attribute__((__packed__));
    unsigned long addr;
  }__attribute__((__packed__));
}__attribute__((__packed__));

struct Ulog {
  struct log_header_5B hdr;
  union {
    unsigned long start;
    struct {
      char pad[6];
      unsigned len;
    }__attribute__((__packed__));
  }__attribute__((__packed__));
}__attribute__((__packed__));

// Defined in server.h
//static int latency_to_bkt(unsigned long lat_in_us)
//{
//  if (lat_in_us < 100)
//    return (int)lat_in_us;
//  else if (lat_in_us < 1000)
//    return 100 + ((lat_in_us - 100) / 10);
//  else if (lat_in_us < 10000)
//    return 190 + ((lat_in_us - 1000) / 100);
//  else if (lat_in_us < 100000)
//    return 280 + ((lat_in_us - 10000) / 1000);
//  else if (lat_in_us < 1000000)
//    return 370 + ((lat_in_us - 100000) / 10000);
//  return CDF_BUCKET_NUM - 1;	// over 1 sec
//}

struct trace_t {
  char *logs;
  unsigned long len;
  int node_idx;
  int num_nodes;
  int num_comp_nodes;
  int master_thread;
  int tid;
  long time;
  long read_time;
  long long read_ops;
  long write_time;
  long long write_ops;
  long total_interval;
  long total_fence;
  unsigned long benchmark_size;
  bool is_master;
  bool is_compute;
  int num_threads;
  int pass;
  long long control_ops;
  GAlloc *alloc;
  unsigned long cdf_cnt_r[CDF_BUCKET_NUM] = {0};
  unsigned long cdf_cnt_w[CDF_BUCKET_NUM] = {0};
};

struct memory_config_t {
  int num_comp_nodes;
};

struct trace_t args[MAX_NUM_THREAD];
struct memory_config_t memory_args;

struct metadata_t {
  unsigned int node_mask;
  unsigned int fini_node_pass[8];
};

// int first;
int num_nodes;
int num_comp_nodes;
int node_id = -1;
int num_threads;
GAddr *remote;
int resize_ratio = 1;
int max_pass;


static int calc_mask_sum(unsigned int mask) {
  int sum = 0;
  while (mask > 0) {
    if (mask & 0x1)
      sum++;
    mask >>= 1;
  }
  return sum;
}

int pin_to_core(int core_id) {
  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (core_id < 0 || core_id >= num_cores)
    return -1;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

inline void interval_between_access(long delta_time_usec) {
  if (delta_time_usec <= 0)
    return;
  else {
    struct timespec ts;
    ts.tv_nsec = (delta_time_usec << 1) / 3;
    if (ts.tv_nsec > SLEEP_THRES_NANOS) {
      ts.tv_sec = 0;
      nanosleep(&ts, NULL);
    }
  }
}

void do_log(void *arg) {
  struct trace_t *trace = (struct trace_t *) arg;
  GAlloc *alloc = trace->alloc;

  int ret;


  //pin to core first
  pin_to_core(trace->tid);

  unsigned len = trace->len;

  multimap<unsigned int, GAddr> len2addr;
  unsigned long old_ts = 0;
  unsigned long i = 0;

  long pass_start = get_time();
  char *cur;

  if (trace->is_compute) {
    for (i = 0; i < trace->len; ++i) {
      volatile char op = trace->logs[i * sizeof(RWlog)];
      cur = &(trace->logs[i * sizeof(RWlog)]);
      if (op == 'R') {
        struct RWlog *log = (struct RWlog *) cur;
        if(log->usec - old_ts <= 999999999) {
          trace->total_interval += log->usec - old_ts;
        }
        interval_between_access(log->usec - old_ts);
        char buf;
        unsigned long addr = log->addr & MMAP_ADDR_MASK;
        size_t cache_line_block = (addr) / (BLOCK_SIZE * resize_ratio);
        size_t cache_line_offset = (addr) % (BLOCK_SIZE * resize_ratio);
#ifdef COLLECT_CDF
        long read_start = get_time();
#endif
        ret = alloc->Read(remote[cache_line_block] + cache_line_offset, &buf, 1);
#ifdef COLLECT_CDF
        long read_end = get_time();
        trace->cdf_cnt_r[latency_to_bkt((read_end - read_start) / 1000)]++;
#endif
        //trace->read_time += read_end - read_start;
        trace->read_ops += 1;
        assert(ret == 1);
        old_ts = log->usec;

      } else if (op == 'W') {
        struct RWlog *log = (struct RWlog *) cur;
        if(log->usec - old_ts <= 999999999) {
          trace->total_interval += log->usec - old_ts;
        }
        interval_between_access(log->usec - old_ts);
        char buf = '0';
        unsigned long addr = log->addr & MMAP_ADDR_MASK;
        size_t cache_line_block = (addr) / (BLOCK_SIZE * resize_ratio);
        size_t cache_line_offset = (addr) % (BLOCK_SIZE * resize_ratio);
#ifdef COLLECT_CDF
        long write_start = get_time();
#endif

        ret = alloc->Write(remote[cache_line_block] + cache_line_offset, &buf, 1);

#ifdef GAM_SC
        //alloc->MFence();
#endif

#ifdef COLLECT_CDF
        long write_end = get_time();
        trace->cdf_cnt_w[latency_to_bkt((write_end - write_start) / 1000)]++;
#endif
        //trace->write_time += write_end - write_start;
        trace->write_ops += 1;
        assert(ret == 1);
        old_ts = log->usec;

      } else if (op == 'M') {
        struct Mlog *log = (struct Mlog *) cur;
        if(log->hdr.usec <= 999999999) {
          trace->total_interval += log->hdr.usec;
        }
        interval_between_access(log->hdr.usec);
        unsigned int len = log->len;
        trace->control_ops += 1;
        //GAddr ret_addr = alloc->Malloc(len, REMOTE);
        //len2addr.insert(pair<unsigned int, GAddr>(len, ret_addr));
        old_ts += log->hdr.usec;
      } else if (op == 'B') {
        struct Blog *log = (struct Blog *) cur;
        interval_between_access(log->usec - old_ts);
        if(log->usec - old_ts <= 999999999) {
          trace->total_interval += log->usec - old_ts;
        }
        trace->control_ops += 1;
        old_ts = log->usec;
      } else if (op == 'U') {
        struct Ulog *log = (struct Ulog *) cur;
        interval_between_access(log->hdr.usec);
        if(log->hdr.usec <= 999999999) {
          trace->total_interval += log->hdr.usec;
        }
        trace->control_ops += 1;
        //auto itr = len2addr.find(log->len);
        //if (itr == len2addr.end()) {
          //printf("no memory to free\n");
        //} else {
          //alloc->Free(itr->second);
          //len2addr.erase(itr);
        //}
        old_ts += log->hdr.usec;
      } else {
        printf("unexpected log: %c at line: %lu\n", op, i);
      }
    }
    //long fence_start = get_time();
    alloc->MFence();
    alloc->WLock(remote[0], BLOCK_SIZE * resize_ratio);
    alloc->UnLock(remote[0], BLOCK_SIZE * resize_ratio);
    long pass_end = get_time();
    //printf("done in %ld ns, thread: %d, pass: %d\n", pass_end - pass_start, trace->tid, trace->pass);
    trace->time += pass_end - pass_start;
    //trace->total_fence += pass_end - fence_start;
    //printf("total run time is %ld ns, fence_time is %ld, sleep time is %ld, thread: %d, pass: %d\n", trace->time, trace->total_fence, trace->total_interval, trace->tid, trace->pass);
    //printf("total run time is %ld ns, thread: %d, pass: %d\n", trace->time, trace->tid, trace->pass);
    if(trace->pass % 1000 == 0) {
#ifdef PROFILE_LATENCY
      if(trace->tid == 0) {
        alloc->CollectCacheStatistics();
      }
#endif
      //printf("thread: %d Number of read: %lld write: %lld control: %lld\n", trace->tid, trace->read_ops, trace->write_ops, trace->control_ops);

#ifdef COLLECT_CDF
      for (i = 0; i < CDF_BUCKET_NUM; i++)
        printf("CDF WRITE: thread: %d pass: %d count: %lu\n", trace->tid, trace->pass, trace->cdf_cnt_w[i]);
      for (i = 0; i < CDF_BUCKET_NUM; i++)
        printf("CDF READ: thread: %d pass: %d count: %lu\n", trace->tid, trace->pass, trace->cdf_cnt_r[i]);
#endif

#ifdef COLLECT_NETWORK_LATENCY
      if(trace->tid == 0) {
        alloc->CollectNetworkCdf(trace->tid, trace->pass);
      }
#endif
      if(trace->tid == 0) {
        alloc->CollectEvictCdf(trace->tid, trace->pass);
        alloc->CollectEvictStatistics(trace->tid, trace->pass);
        alloc->CollectInvalidStatistics(trace->tid, trace->pass);
        alloc->CollectRemoteStatistics(trace->tid, trace->pass);
        alloc->CollectLocalRemoteCdf(trace->tid, trace->pass);
      }
    }
    //if(trace->read_ops)
    //  printf("total read time is %ld ns, thread: %d, pass: %d\n", trace->read_time, trace->tid, trace->pass);
    //if(trace->write_ops)
    //  printf("total write time is %ld ns, thread: %d, pass: %d\n", trace->write_time, trace->tid, trace->pass);
    //fflush(stdout);
    //trace->write_time = 0;
    //trace->read_time = 0;
    //trace->read_ops = 0;
    //trace->write_ops = 0;
  }

  //FIXME warm up here?
  //make sure all the requests are complete
  long sync_start = get_time();
  uint64_t SYNC_RUN_BASE = SYNC_KEY + trace->num_nodes * 2;
  uint64_t sync_id = SYNC_RUN_BASE + trace->num_nodes * node_id + trace->tid + PASS_KEY * trace->pass;
  //printf("Putting node_id: %d, thread id: %d, pass: %d, key: %lld, value: %lld\n", node_id, trace->tid, trace->pass, sync_id, sync_id);
  alloc->Put(sync_id, &sync_id, sizeof(uint64_t));
  for (int i = 1; i <= trace->num_comp_nodes; i++) {
    for (int j = 0; j < trace->num_threads; j++) {
      //epicLog(LOG_WARNING, "waiting for node %d, thread %d", i, j);
      alloc->Get(SYNC_RUN_BASE + trace->num_nodes * i + j + PASS_KEY * trace->pass, &sync_id);
      //epicLog(LOG_WARNING, "get sync_id %lld from node %d, thread %d, should be: %lld", sync_id, i,
              //j, SYNC_RUN_BASE + trace->num_nodes * i + j + PASS_KEY * trace->pass);
      epicAssert(sync_id == SYNC_RUN_BASE + trace->num_nodes * i + j + PASS_KEY * trace->pass);
    }
  }
  long sync_end = get_time();
  //printf("All nodes synced in %ld ns\n", sync_end - sync_start);

}

void standalone(void *arg) {
  //printf("Show the start of standalone\n");
  struct memory_config_t *trace = (struct memory_config_t *) arg;
  //GAlloc *alloc = GAllocFactory::CreateAllocator();
  //for (int i = 1; i <= trace->num_comp_nodes; i++) {
    //printf("Getting %lld\n", SYNC_KEY + i + 10);
    //int id;
    //alloc->Get(SYNC_KEY + i + 10, &id);
    //printf("Get done \n");
    //epicAssert(id == i);
  //}
  while(1){}
}

int load_trace(int fd, struct trace_t *arg, unsigned long ts_limit) {
  //printf("ts_limit: %lu\n", ts_limit);
  assert(sizeof(RWlog) == sizeof(Mlog));
  assert(sizeof(RWlog) == sizeof(Blog));
  assert(sizeof(RWlog) == sizeof(Ulog));
/*
	char *chunk = (char *)malloc(LOG_NUM_TOTAL * sizeof(RWlog));
	char *buf;
	if (!chunk) {
		printf("fail to alloc buf to hold logs\n");
		return -1;
	} else {
		arg->logs = chunk;
	}
	int fd = open(trace_name, O_RDONLY);
	if (fd < 0) {
		printf("fail to open log file\n");
		return fd;
	}
*/
  char *chunk = arg->logs;
  memset(chunk, 0, LOG_NUM_TOTAL * sizeof(RWlog));
  size_t size = 0;
  for (char *buf = chunk; true; buf += LOG_NUM_ONCE * sizeof(RWlog)) {
    size_t dsize = read(fd, buf, LOG_NUM_ONCE * sizeof(RWlog));
    if (dsize == 0)
      break;
    if (dsize % sizeof(RWlog) != 0)
      printf("dsize is :%lu\n", dsize);
    size += dsize;

    char *tail = buf + dsize - sizeof(RWlog);
    unsigned long last_ts = 0;
    while (tail - buf >= 0) {
      if (*tail == 'R' || *tail == 'W' || *tail == 'B')
        last_ts = ((struct RWlog *) tail)->usec;
      else if (*tail == 'M' || *tail == 'U') {
        tail -= sizeof(RWlog);
        continue;
      } else
        printf("unexpected op %c\n", *tail);
      break;
    }
    if (last_ts >= ts_limit)
      break;
  }
  assert(size <= LOG_NUM_TOTAL * sizeof(RWlog));
  //assert(size % sizeof(RWlog) == 0);
  arg->len = size / (sizeof(RWlog));
  //printf("finish loading %lu logs\n", arg->len);

  return 0;
}

enum {
  arg_node_cnt = 1,
  arg_num_threads = 2,
  arg_ip_master = 3,
  arg_ip_worker = 4,
  arg_port_master = 5,
  arg_port_worker = 6,
  arg_is_master = 7,
  arg_is_compute = 8,
  arg_max_pass = 9,
  arg_log1 = 10,
};

int main(int argc, char **argv) {
  int ret;
  char *buf_test = NULL;
  if (argc < arg_log1) {
    fprintf(stderr, "Incomplete args\n");
    return 1;
  }

  num_nodes = atoi(argv[arg_node_cnt]);
  num_comp_nodes = num_nodes - 1;
  num_threads = atoi(argv[arg_num_threads]);
  max_pass = atoi(argv[arg_max_pass]);
#ifdef single_thread_test
    num_threads = 1;
    num_comp_nodes = 1;
#endif
  string ip_master = string(argv[arg_ip_master]);
  string ip_worker = string(argv[arg_ip_worker]);
  int port_master = atoi(argv[arg_port_master]);
  int port_worker = atoi(argv[arg_port_worker]);
  //FIXME check this is failed
  bool is_master = atoi(argv[arg_is_master]);
  bool is_compute = atoi(argv[arg_is_compute]);
  long long benchmark_size = 6442450944;
  printf("%ld %d %d %d\n", benchmark_size, num_comp_nodes, is_master, is_compute);
  printf("Num Nodes: %d, Num Threads: %d\n", num_nodes, num_threads);
#ifndef single_thread_test
  //if (argc != arg_log1 + num_threads) {
  //  fprintf(stderr, "thread number and log files provided not match\n");
  //  return 1;
  //}
#endif
  /**
   *	struct Conf {
   * 	bool is_master = true;  //mark whether current process is the master (obtained from conf and the current ip)
   * 	int master_port = 12345;
   * 	std::string master_ip = "localhost";
   * 	std::string master_bindaddr;
   * 	int worker_port = 12346;
   * 	std::string worker_bindaddr;
   * 	std::string worker_ip = "localhost";
   * 	Size size = 1024 * 1024L * 512;  //per-server size of memory pre-allocated
   * 	Size ghost_th = 1024 * 1024;
   * 	double cache_th = 0.15;  //if free mem is below this threshold, we start to allocate memory from remote nodes
   * 	int unsynced_th = 1;
   * 	double factor = 1.25;
   * 	int maxclients = 1024;
   * 	int maxthreads = 10;
   * 	int backlog = TCP_BACKLOG;
   * 	int loglevel = LOG_WARNING;
   * 	std::string* logfile = nullptr;
   * 	int timeout = 10;  //ms
   * 	int eviction_period = 100;  //ms
   *	};
  **/

  // Global configuration here

  printf("Currently configuration is: ");
  printf(
      "master: %s:%d, worker: %s:%d, is_master: %s, size to allocate: %ld\n",
      ip_master.c_str(), port_master, ip_worker.c_str(), port_worker,
      is_master == 1 ? "true" : "false", benchmark_size / num_nodes);

  Conf conf;
  conf.loglevel = DEBUG_LEVEL;
  conf.is_master = is_master;
  conf.master_ip = ip_master;
  conf.master_port = port_master;
  conf.worker_ip = ip_worker;
  conf.worker_port = port_worker;

  if(is_compute) {
    conf.cache_th = 1.0;
    //long long size = (long long)((double)benchmark_size / (double)num_comp_nodes * (double)remote_ratio);
    //long long size = (long long)((double)benchmark_size  * (double)remote_ratio);
    long long size = (long long) 512 * 1024 * 1024;
    //conf.size = size < conf.size ? conf.size : size;
    conf.size = size;
    //conf.size = 1024 * 1024 * 1024 * 6L;
  } else {
    conf.cache_th = 0.0;
    //this is for voltDB 10GB
    conf.size = 1024 * 1024 * 1024 * 10L;
    //this is for tensorflow 4GB
    //conf.size = 1024 * 1024 * 1024 * 4L;
  }
  printf("Size to allocate: %ld\n", conf.size);

  // Global memory allocator
  printf("Start the allocator here !!!!!!!!!\n");
  GAlloc *alloc = GAllocFactory::CreateAllocator(&conf);
  printf("End the allocator here !!!!!!!!!\n");
  sleep(1);
  fflush(stdout);

  //sync with all the other workers
  //check all the workers are started
  int id;
  node_id = alloc->GetID();
  printf("Waiting for all the nodes !!!!!!!!!\n");
  printf("Putting %lld, %lld\n", SYNC_KEY + node_id, node_id);
  alloc->Put(SYNC_KEY + node_id, &node_id, sizeof(int));
  printf("Put done\n");
  for (int i = 1; i <= num_nodes; i++) {
    printf("Gettting %lld\n", SYNC_KEY + i);
    alloc->Get(SYNC_KEY + i, &id);
    printf("Get done \n");
    epicAssert(id == i);
  }
  fflush(stdout);
  //open files
  // FILE *progress = fopen("/tmp_test/progress.txt", "w");
  FILE *progress = fopen("/home/llh/mind_ae_gam/log/progress.txt", "w");
  if(is_compute) {
    int *fd = new int[num_threads];
    for (int i = 0; i < num_threads; ++i) {
      fd[i] = open(argv[arg_log1 + i], O_RDONLY);
      if (fd[i] < 0) {
        printf("fail to open log file\n");
        return 1;
      }
    }

    //get start ts
    struct RWlog first_log;
    unsigned long start_ts = -1;
    for (int i = 0; i < num_threads; ++i) {
      int size = read(fd[i], &first_log, sizeof(RWlog));
      start_ts = min(start_ts, first_log.usec);
    }
    printf("start ts is: %lu\n", start_ts);

    int remote_step = benchmark_size / BLOCK_SIZE / resize_ratio;
    remote = (GAddr *) malloc(sizeof(GAddr) * remote_step);
    //GAddr *remote;
    long alloc_start = get_time();

    if(is_compute) {
      if (is_master) {
        //printf("Master malloc the remote memory in slices: %d, node: %d, in thread: %d\n",
        //remote_step,
        //trace->node_idx,
        //trace->tid);
        for (int i = 0; i < remote_step; i++) {
          remote[i] = alloc->Malloc(BLOCK_SIZE * resize_ratio, REMOTE);
          alloc->Put(i, &remote[i], addr_size);
        }
        //printf("Finish malloc the remote memory in slices node: %d, in thread: %d\n",
        //trace->node_idx,
        //trace->tid);
      } else {
        //printf("Worker malloc the remote memory in slices node: %d, in thread: %d\n",
        //trace->node_idx,
        //trace->tid);
        for (int i = 0; i < remote_step; i++) {
          GAddr addr;
          int ret = alloc->Get(i, &addr);
          epicAssert(ret == addr_size);
          remote[i] = addr;
        }
        //printf("Finish worker malloc the remote memory in slices node: %d, in thread: %d\n",
        //trace->node_idx,
        //trace->tid);
      }
      //for (int i = 0; i < remote_step; i++) {
      //  printf("Addr is: %lu\n", remote[i]);
      //}

    }
    long alloc_end = get_time();
    printf("allocate is %ld ns\n", alloc_end - alloc_start);
    fflush(stdout);


    for (int i = 0; i < num_threads; ++i) {
      args[i].num_threads = num_threads;
      args[i].node_idx = node_id;
      args[i].num_nodes = num_nodes;
      args[i].num_comp_nodes = num_comp_nodes;
      args[i].master_thread = (i == 0);
      args[i].is_master = is_master;
      args[i].is_compute = is_compute;
      args[i].tid = i;
      args[i].logs = (char *) malloc(LOG_NUM_TOTAL * sizeof(RWlog)); // This should be allocated locally
      args[i].benchmark_size = benchmark_size;
      args[i].alloc = GAllocFactory::CreateAllocator();
      if (!args[i].logs)
        printf("fail to alloc buf to hold logs\n");
    }

    //start load and run logs in time window
    unsigned long pass = 0;
    unsigned long ts_limit = start_ts;
    //for (int i = 0; i < num_threads; ++i) {
    //  printf("Thread[%d]: loading log...\n", i);
    //  ret = load_trace(fd[i], &args[i], ts_limit);
    //  if (ret) {
    //    printf("fail to load trace\n");
    //  }
    //}
    unsigned long tot_run_time = 0;
    while (1) {
      ts_limit += TIMEWINDOW_US;

      //printf("Pass[%lu] Node[%d]: loading log...\n", pass, node_id);
      for (int i = 0; i < num_threads; ++i) {
        //printf("Thread[%d]: loading log...\n", i);
        ret = load_trace(fd[i], &args[i], ts_limit);
        if (ret) {
          printf("fail to load trace\n");
        }
      }

      pthread_t thread[MAX_NUM_THREAD];
      //printf("running trace...\n");

      for (int i = 0; i < num_threads; ++i) {
        args[i].pass = pass;
        //printf("args has len: %d\n", args[i].len);
        if (args[i].len) {
          if (pthread_create(&thread[i], NULL, (void *(*)(void *)) do_log, &args[i])) {
            printf("Error creating thread %d\n", i);
            return 1;
          }
        }
      }

      for (int i = 0; i < num_threads; ++i) {
        if (args[i].len) {
          if (pthread_join(thread[i], NULL)) {
            printf("Error joining thread %d\n", i);
            return 2;
          }
        }
      }

      //sync on the end of the time window
      ++pass;
      unsigned long longest_time = 0;
      for (int i = 0; i < num_threads; ++i) {
        if(args[i].time > longest_time) {
            longest_time = args[i].time;
        }
       }
      tot_run_time += longest_time;
      if (pass % 1000 == 0) {
      	char progress_text[256] = "";
      	memset(progress_text, 0, 256);
        sprintf(progress_text, "Pass[%lu] Node[%d] || Time [%lu] ||  ", pass, args[0].node_idx - 1, longest_time / 1000);
        for (int i = 0; i < num_threads; ++i) {
      	    sprintf(progress_text, "%s %lu, ", progress_text, args[i].time / 1000);
      	}
        fprintf(progress, "%s in us\n", progress_text);
        fflush(progress);
        printf("%s in us\n", progress_text);
        fflush(stdout);
      }


      bool all_done = true;
      for (int i = 0; i < num_threads; ++i) {
        if (args[i].len != 0) {
          all_done = false;
        }
      }
      if (all_done || pass >= max_pass) {
        printf("All done here\n");
        break;
      }
    }
    //printf("Putting %lld, %lld\n", SYNC_KEY + node_id + 10, node_id);
    //alloc->Put(SYNC_KEY + node_id + 10, &node_id, sizeof(int));
    sleep(20);
    //printf("Put done\n");

    for (int i = 0; i < num_threads; ++i) {
      close(fd[i]);
    }
    delete[] fd;
    free(remote);
  } else {
    pthread_t memory_thread;
    memory_args.num_comp_nodes = num_comp_nodes;
    if (pthread_create(&memory_thread, NULL, (void *(*)(void *)) standalone, &memory_args)) {
      printf("Error creating memory thread \n");
      return 1;
    }
    if (pthread_join(memory_thread, NULL)) {
      printf("Error joining memory thread\n");
      return 2;
    }

  }
  fclose(progress);

  return 0;
}

