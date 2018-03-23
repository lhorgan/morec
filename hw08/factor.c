
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <pthread.h>

#include "queue.h"
#include "factor.h"

// FIXME: Shared mutable data
static queue* iqueue;
static queue* oqueue;
int terminate = 0;

pthread_mutex_t mutey2 = PTHREAD_MUTEX_INITIALIZER;

void
factor_init()
{
    if (iqueue == 0) iqueue = make_queue();
    if (oqueue == 0) oqueue = make_queue();
}

void
factor_cleanup()
{
    free_queue(iqueue);
    iqueue = 0;
    free_queue(oqueue);
    oqueue = 0;
}

factor_job*
make_job(int128_t nn)
{
    factor_job* job = malloc(sizeof(factor_job));
    job->number  = nn;
    job->factors = 0;
    return job;
}

void
free_job(factor_job* job)
{
    if (job->factors) {
        free_ivec(job->factors);
    }
    free(job);
}

void
submit_job(factor_job* job)
{
    queue_put(iqueue, job);
}

factor_job*
get_result()
{
    return queue_get(oqueue);
}

static
int128_t
isqrt(int128_t xx)
{
    double yy = ceil(sqrt((double)xx));
    return (int128_t) yy;
}

ivec*
factor(int128_t xx)
{
    ivec* ys = make_ivec();

    while (xx % 2 == 0) {
        ivec_push(ys, 2);
        xx /= 2;
    }

    for (int128_t ii = 3; ii <= isqrt(xx); ii += 2) {
        int128_t x1 = xx / ii;
        if (x1 * ii == xx) {
            ivec_push(ys, ii);
            xx = x1;
            ii = 1;
        }
    }

    ivec_push(ys, xx);

    return ys;
}

void*
run_jobs()
{
    factor_job* job;

    while ((job = queue_get(iqueue)) /*|| terminate*/) {
        unsigned long start_time = (unsigned long)time(NULL);
      	job->factors = factor(job->number);
      	unsigned long end_time = (unsigned long)time(NULL);
      	pthread_t thread_id = pthread_self();
        queue_put(oqueue, job);
    }
    submit_job(0);
    return 0;
}

void
clean_queue()
{
    queue_get(iqueue);
}
