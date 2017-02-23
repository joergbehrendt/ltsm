#ifndef WORKERPOOL_H
#define WORKERPOOL_H
#include <pthread.h>


struct worker {
	void *worker_data; //tsm sessio etc
	struct worker_pool *pool; //tsm sessio etc
	unsigned int id;
	int running;
};

struct work {
	int (*function_work)(const struct worker*,const void*);
	void* work_data;
};

struct worker_pool {
	pthread_mutex_t mutex;
	pthread_mutex_t mutex_jobqueue;
	unsigned int running_id;
	unsigned int current_threads;
	unsigned int wanted_threads;
	int (*function_setup)(struct worker* w); //create worker_data
	int (*function_cleanup)(struct worker* w); //cleanup worker_data
};

//initialize a pool
struct worker_pool* worker_pool_init(unsigned int count, int(*worker_setup_function)(struct worker *w), int (*worker_cleanup_function)(struct worker *w));


void worker_pool_set_wanted_thread_count(struct worker_pool* pool, unsigned int count);

//add a function with an argument which should be called by a worker thread
// thread calls function work (job_data, worker_data)

void worker_pool_add_work(struct worker_pool *pool, struct work *job);

#endif /* WORKERPOOL_H */
