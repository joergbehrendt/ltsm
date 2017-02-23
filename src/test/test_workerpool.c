/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Copyright (c) 2016, 2017 Thomas Stibor <t.stibor@gsi.de>
 */

#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include "tsmapi.h"
#include "qarray.h"
#include "log.h"
#include "CuTest.h"

#include "workerpool.h"

/******************* JOB FUNCTIONS ***************/
int worker_setup_function(struct worker *w){
//      printf("  [%2i] Initialize Worker Thread - e.g. start tsm session\n", w->id);
        int *worker_data = malloc(sizeof(int));
        *worker_data = 100 + w->id;
        w->worker_data = worker_data;
        return 0;
}


int worker_cleanup_function(struct worker *w){
//      printf("  [%2i] Cleanup Worker Thread- e.g. close tsm session\n", w->id);
        return 0;
}

int function_work(const struct worker *w,const void* job_data){
        printf("  Processing job -- \n");
        printf("  Thread ID  : %i\n", w->id);
        printf("  Thread Data: %i\n", *(int*) w->worker_data);
        printf("  Job Data   : %i\n", *(int*) job_data);
        printf("  Processing job end -- \n");
        return 0;
}
/******************* JOB FUNCTIONS END ***********/



void test_workerpool(CuTest *tc)
{

	struct worker_pool *pool = worker_pool_init(3,worker_setup_function,worker_cleanup_function);
        CuAssertTrue(tc, pool != NULL);
	sleep(2);
        CuAssertTrue(tc, pool->current_threads == 3);

	//create a job
  	struct work * job = malloc(sizeof(struct work));
        job->function_work = function_work;
        int i = 42;
        job->work_data = &i;

	//set pool threads to 30
        worker_pool_set_wanted_thread_count(pool,30);
	sleep(2);
	CuAssertTrue(tc, pool->current_threads == 30);

        worker_pool_set_wanted_thread_count(pool,3);
	sleep(2);
	CuAssertTrue(tc, pool->current_threads == 3);

        worker_pool_set_wanted_thread_count(pool,13);
	sleep(2);
	CuAssertTrue(tc, pool->current_threads == 13);

        worker_pool_set_wanted_thread_count(pool,20);
	sleep(2);
	CuAssertTrue(tc, pool->current_threads == 20);

        for(int x = 0; x < 10; x++){
		sleep(1);
                worker_pool_add_work(pool, job);
        }

	sleep(3);
	printf("Done Workerpool tests\n");
}

CuSuite* workerpool_get_suite()
{
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_workerpool);

    return suite;
}
