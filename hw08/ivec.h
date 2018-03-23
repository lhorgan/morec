#ifndef IVEC_H
#define IVEC_H

#include <stdint.h>

typedef struct ivec {
    int64_t* data;
    int64_t len;
    int64_t cap;
} ivec;

ivec* make_ivec();
void free_ivec(ivec* vv);
void ivec_push(ivec* vv, int64_t xx);
void print_ivec(ivec* vv);

#endif
