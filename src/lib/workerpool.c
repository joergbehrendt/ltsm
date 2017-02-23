#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "workerpool.h"

//fetch next job from queue and remove it from list - returns NULL if no job in queue
struct work* worker_pool_get_next_job(struct worker_pool *pool){
	pthread_mutex_lock(&pool->mutex_jobqueue);
	struct work* pointer_to_work = NULL;
	list_rem_next(&pool->jobqueue, NULL, (void **) &pointer_to_work); //fetch from head
	pthread_mutex_unlock(&pool->mutex_jobqueue);
	return pointer_to_work;
}

//data is pointer to worker_pool parent
void *worker_thread(void *data){
	struct worker *self = (struct worker *) data;
	printf("[%2i] SETUP\n",self->id);
	self->pool->function_setup(self);

	while(self->running){
		printf("[%2i] WORK\n",self->id);
		struct work* job = worker_pool_get_next_job(self->pool);
		printf("[%2i] FETCHED\n",self->id);
		if(job != NULL){
			printf("[%2i] WORKING ON JOB\n", self->id);
			int rc = job->function_work(self, job->work_data);
			printf("[%2i] JOB RETURNED %i\n", self->id, rc);
		}else{
			printf("[%2i] EMPTY JOB - sleeping 1 sec\n", self->id);
			sleep(1);
		}
		pthread_mutex_lock(&self->pool->mutex);

		if(self->pool->current_threads > self->pool->wanted_threads){
			printf("[%2i] CLOSE - too many threads",self->id);
			goto break_loop;
		}

		pthread_mutex_unlock(&self->pool->mutex);
	}

	pthread_mutex_lock(&self->pool->mutex);
	printf("[%2i] CLOSE - manual abort",self->id);
break_loop:
	self->pool->current_threads -= 1;
        printf("> current running/wanted threads = %i/%i \n", self->pool->current_threads, self->pool->wanted_threads);
	pthread_mutex_unlock(&self->pool->mutex);

//	printf("[%2i] CLEANUP\n", self->id);
	self->pool->function_cleanup(self);

//	printf("[%2i] FREE and EXIT\n", self->id);
	free(self);
	pthread_exit(NULL);
}


int worker_pool_create_worker(struct worker_pool *pool){

	struct worker* w = (struct worker*) malloc(sizeof(struct worker));
	w->pool = pool;

	pthread_attr_t attr;
	pthread_t thread;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_mutex_lock(&pool->mutex);
	w->id = pool->running_id;
	w->running = 1;
	pthread_create(&thread, &attr, worker_thread, w);
	pool->current_threads += 1;
	pool->running_id += 1;
	pthread_attr_destroy(&attr);
	printf("CREATED THREAD - current running/wanted threads = %i/%i \n", pool->current_threads, pool->wanted_threads);
	pthread_mutex_unlock(&pool->mutex);
	return 0;
}


//master thread which creates new threads if needed
// parameter is pool to manage
void *management_thread(void *data){
	struct worker_pool *pool = (struct worker_pool*) data;
	int last_current_threads = 0;
	int last_wanted_threads = 0;
	while(1){
		if( last_current_threads != pool->current_threads || last_wanted_threads != pool->wanted_threads){
			printf("[MANAGEMENT] Something changed\n");
			pthread_mutex_lock(&pool->mutex);
			int diff = pool->wanted_threads - pool->current_threads;
			last_current_threads = pool->current_threads;
			last_wanted_threads = pool->wanted_threads;
			pthread_mutex_unlock(&pool->mutex);

			if(diff > 0){
				printf("[MANAGEMENT] Need to create %i threads\n", diff);
				for(int x = 0; x < diff; x++){
					worker_pool_create_worker(pool);
				}
			}else if(diff < 0){
				printf("[MANAGEMENT] Need to close %i more threads\n", -diff);
				//threads close themselfs after work finished. dont do anything
			}else{
				printf("[MANAGEMENT] Reached wanted thread count %i \n", last_wanted_threads);
			}
		}
	}
}


//initialize a pool
struct worker_pool* worker_pool_init(unsigned int count, int (*worker_setup_function)(struct worker *w), int (*worker_cleanup_function)(struct worker *w)){
	struct worker_pool* wp = (struct worker_pool*) malloc(sizeof(struct worker_pool));
	printf("Initialize Worker Pool with max count %i \n", count);
	pthread_mutex_init (&wp->mutex, NULL);
	pthread_mutex_init (&wp->mutex_jobqueue, NULL);
	wp->function_setup = worker_setup_function;
	wp->function_cleanup = worker_cleanup_function;

	pthread_mutex_lock(&wp->mutex);
	wp->running_id = 1;
	wp->current_threads = 0;
	wp->wanted_threads = count;
	list_init(&wp->jobqueue, free);
	pthread_mutex_unlock(&wp->mutex);


	printf("Creating management thread\n");
        pthread_attr_t attr;
        pthread_t thread;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&thread, &attr, management_thread, wp);
        pthread_attr_destroy(&attr);

	return wp;
}


void worker_pool_set_wanted_thread_count(struct worker_pool* pool, unsigned int count){
	printf("Set wanted thread count to %i\n",count);
	pthread_mutex_lock(&pool->mutex);
	pool->wanted_threads = count;
	pthread_mutex_unlock(&pool->mutex);

}


//add a function with an argument which should be called by a worker thread
// thread calls function_work (job_data, worker_data)
void worker_pool_add_work(struct worker_pool *pool, struct work *job){
	pthread_mutex_lock(&pool->mutex_jobqueue);
	printf("Adding work to pool\n");
	int rc = list_ins_next(&pool->jobqueue, list_tail(&pool->jobqueue), job); //ADDED TO TAIL
	printf("Added work to pool RC:%i   q size:%i \n",rc, (int)list_size(&pool->jobqueue));
	pthread_mutex_unlock(&pool->mutex_jobqueue);

}
