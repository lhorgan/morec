#include <stdlib.h>
#include <assert.h>

#include "queue.h"
#include <stdio.h>
#include <pthread.h>

pthread_mutex_t mutey = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condy = PTHREAD_COND_INITIALIZER;

queue*
make_queue()
{
    queue* qq = malloc(sizeof(queue));
    qq->head = 0;
    qq->tail = 0;
    return qq;
}

void
free_queue(queue* qq)
{
    assert(qq->head == 0 && qq->tail == 0);
    free(qq);
}

void
queue_put(queue* qq, void* msg)
{
    qnode* node = malloc(sizeof(qnode));
    node->data = msg;
    node->prev = 0;
    node->next = 0;

    pthread_mutex_lock(&mutey); // lock while we modify qq
    node->next = qq->head;
    qq->head = node;

    if (node->next) {
        node->next->prev = node;
    }
    else {
        qq->tail = node;
        //printf("qq id %i\n", qq);
        pthread_cond_broadcast(&condy);
    }
    pthread_mutex_unlock(&mutey); // release the lock
}

void*
queue_get(queue* qq)
{
    pthread_mutex_lock(&mutey); // lock while we modify qq

    while (!qq->tail) {
        //printf("waiting....\n");
        pthread_cond_wait(&condy, &mutey);
        //printf("wait is over\n");
    }
    pthread_cond_broadcast(&condy);

    qnode* node = qq->tail;

    if (node->prev) {
        qq->tail = node->prev;
        node->prev->next = 0;
    }
    else {
        qq->head = 0;
        qq->tail = 0;
    }

    void* msg = node->data;
    free(node);
    pthread_mutex_unlock(&mutey); // release the lock
    return msg;
}
