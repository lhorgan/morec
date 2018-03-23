
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#include "int128.h"
#include "factor.h"
#include "ivec.h"
#include <pthread.h>
#include <unistd.h>
#include "queue.h"

int
main(int argc, char* argv[])
{
    if (argc != 4) {
        printf("Usage:\n");
        printf("  ./main threads start count\n");
        return 1;
    }

    int threads = atoi(argv[1]);
    pthread_t thread_ids[threads];

    int128_t start = atoh(argv[2]);
    int64_t  count = atol(argv[3]);

    factor_init();
    for(int ii = 0; ii < threads; ii++)
    {
        //printf("starting threads\n");
	    pthread_create(&thread_ids[ii], NULL, run_jobs, NULL);
    }

    for (int64_t ii = 0; ii < count; ++ii) {
        factor_job* job = make_job(start + ii);
        submit_job(job);
    }
    submit_job(0);

    unsigned long start_time = (unsigned long)time(NULL);

    for(int ii = 0; ii < threads; ii++)
    {
        pthread_join(thread_ids[ii], NULL);
    }

    unsigned long end_time = (unsigned long)time(NULL);

    int64_t oks = 0;
    clean_queue();

    for (int64_t ii = 0; ii < count; ++ii) {
        factor_job* job = get_result();

        print_int128(job->number);
        printf(": ");
        print_ivec(job->factors);

        ivec* ys = job->factors;

        int128_t prod = 1;
        for (int ii = 0; ii < ys->len; ++ii) {
            prod *= ys->data[ii];
        }

        if (prod == job->number) {
            ++oks;
        }
        else {
            fprintf(stderr, "Warning: bad factorization");
        }

        free_job(job);
    }

    printf("Factored %ld / %ld numbers.\n", oks, count);

    factor_cleanup();

    return 0;
}
