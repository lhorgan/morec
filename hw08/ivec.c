#include <stdio.h>
#include <stdlib.h>

#include "ivec.h"

ivec*
make_ivec()
{
    ivec* vec = malloc(sizeof(ivec));
    vec->len  = 0;
    vec->cap  = 4;
    vec->data = malloc(vec->cap * sizeof(int64_t));
    return vec;
}

void
free_ivec(ivec* vv)
{
    free(vv->data);
    free(vv);
}


void
ivec_push(ivec* vv, int64_t xx)
{
    vv->len += 1;

    if (vv->cap < vv->len) {
        vv->cap *= 2;
        vv->data = realloc(vv->data, vv->cap * sizeof(int64_t));
    }

    vv->data[vv->len - 1] = xx;
}

void
print_ivec(ivec* vv)
{
    for (int64_t ii = 0; ii < vv->len; ++ii) {
        printf("%ld ", vv->data[ii]);
    }
    printf("\n");
}
