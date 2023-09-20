#include "tq.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>

typedef struct sum_task
{
    tq_task base;
    size_t start;
    size_t end;
    size_t *result;
} sum_task;

static void sum_task_fn(void *self);
static void sum_task_destroy(sum_task *task);

static sum_task *sum_task_create(size_t start, size_t end, size_t *result)
{
    sum_task *task;
    if ((task = calloc(1, sizeof(sum_task))) == NULL)
        return NULL;
    tq_task_init(&task->base, sum_task_fn, (void (*)(void *))sum_task_destroy);
    task->start = start;
    task->end = end;
    task->result = result;
    return task;
}

static void sum_task_fn(void *self)
{
    sum_task *task = self;
    *task->result = 0;
    for (size_t v = task->start; v < task->end; ++v)
        *task->result += v;
}

static void sum_task_destroy(sum_task *task)
{
    free(task);
}

void *run(void *arg)
{
    tq_runner *runner = arg;
    tq_runner_run(runner);
    return NULL;
}

int64_t timespecDiff(struct timespec *timeA_p, struct timespec *timeB_p)
{
    return ((timeA_p->tv_sec * 1000000000) + timeA_p->tv_nsec) -
           ((timeB_p->tv_sec * 1000000000) + timeB_p->tv_nsec);
}

int main(int argc, char **argv)
{
    struct timespec tstart, tend;
    tq_runner *runner;
    if (tq_runner_create(&runner) != TQ_ERR_OK)
        return -1;

    size_t start = 0;
    size_t end = 100000000000;
    const int num_tasks = 8;

    size_t results[num_tasks] = {0};
    for (int i = 0; i < num_tasks; i++)
    {
        size_t range = (end - start) / num_tasks;
        size_t rstart = start + i * range;
        size_t rend = (i != num_tasks - 1 ? start + (i + 1) * range : end);
        sum_task *task = sum_task_create(rstart, rend, &results[i]);
        tq_runner_push(runner, &task->base);
    }

    clock_gettime(CLOCK_MONOTONIC, &tstart);
    pthread_t thread[num_tasks];
    for (int i = 0; i < num_tasks; i++)
        pthread_create(&thread[i], NULL, run, runner);
    for (int i = 0; i < num_tasks; i++)
        pthread_join(thread[i], NULL);

    size_t sum = 0;
    for (size_t i = 0; i < num_tasks; ++i)
        sum += results[i];

    clock_gettime(CLOCK_MONOTONIC, &tend);
    uint64_t timeElapsed = timespecDiff(&tend, &tstart);

    printf("Sum: %lu\n", sum);
    printf("Time Elapsed: %llu\n", timeElapsed);
    tq_runner_destroy(runner);
}
