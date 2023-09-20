#include "tq.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST(x)                                                        \
    if ((x) == 0) {                                                    \
        printf("Test failed: %s, at %s:%d\n", #x, __FILE__, __LINE__); \
        return -1;                                                     \
    }

static void* run_rountine(void* arg)
{
    tq_runner* runner = arg;
    tq_runner_run(runner);
    return NULL;
}

static void run_in_threads(tq_runner* runner, int num_threads)
{
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; ++i)
        pthread_create(&threads[i], NULL, run_rountine, runner);
    for (int i = 0; i < num_threads; ++i)
        pthread_join(threads[i], NULL);
}

typedef struct count_task {
    tq_task base;
    int* result;
} count_task;

static void count_task_fn(void* self);
static void count_task_destroy(void* self);

static count_task* count_task_create(int* result)
{
    count_task* task;
    if ((task = calloc(1, sizeof(count_task))) == NULL)
        return NULL;
    task->base.fn      = count_task_fn;
    task->base.destroy = count_task_destroy;
    task->result       = result;
    return task;
}

static void count_task_fn(void* self)
{
    count_task* task = self;
    *task->result    = *task->result + 1;
}

static void count_task_destroy(void* self)
{
    count_task* task = self;
    free(task);
}

static int test_tq_runner_create()
{
    tq_runner* runner;
    TEST(tq_runner_create(&runner) == TQ_ERR_OK);
    TEST(runner != NULL);
    tq_runner_destroy(runner);
    return 0;
}

static int test_tq_runner_push_count_task()
{
    { // Push zero tasks
        tq_runner* runner;
        TEST(tq_runner_create(&runner) == TQ_ERR_OK);
        TEST(tq_runner_run(runner) == TQ_ERR_OK);
        tq_runner_destroy(runner);
    }
    { // Push one task
        tq_runner* runner;
        TEST(tq_runner_create(&runner) == TQ_ERR_OK);

        int result    = 0;
        count_task* t = count_task_create(&result);
        tq_runner_push(runner, &t->base);
        TEST(tq_runner_run(runner) == TQ_ERR_OK);
        TEST(result == 1);
        tq_runner_destroy(runner);
    }
    { // Push five tasks
        tq_runner* runner;
        TEST(tq_runner_create(&runner) == TQ_ERR_OK);

        int result = 0;
        for (int i = 0; i < 5; ++i) {
            count_task* t = count_task_create(&result);
            tq_runner_push(runner, &t->base);
        }
        TEST(tq_runner_run(runner) == TQ_ERR_OK);
        TEST(result == 5);
        tq_runner_destroy(runner);
    }
    return 0;
}

static int test_tq_runner_push_count_task_parallel()
{
    { // Run zero tasks
        tq_runner* runner;
        TEST(tq_runner_create(&runner) == TQ_ERR_OK);
        run_in_threads(runner, 4);
        tq_runner_destroy(runner);
    }
    { // Run one task
        tq_runner* runner;
        TEST(tq_runner_create(&runner) == TQ_ERR_OK);

        int result    = 0;
        count_task* t = count_task_create(&result);
        tq_runner_push(runner, &t->base);
        run_in_threads(runner, 4);
        TEST(result == 1);
        tq_runner_destroy(runner);
    }
    { // Run 500 tasks
        tq_runner* runner;
        TEST(tq_runner_create(&runner) == TQ_ERR_OK);

        int result[500] = { 0 };
        for (int i = 0; i < 500; ++i) {
            count_task* t = count_task_create(&result[i]);
            tq_runner_push(runner, &t->base);
        }
        run_in_threads(runner, 4);
        for (int i = 0; i < 500; ++i)
            TEST(result[i] == 1);
        tq_runner_destroy(runner);
    }
    return 0;
}

static int test_tq_strand_create()
{
    tq_strand* strand;
    tq_runner* runner;
    TEST(tq_runner_create(&runner) == TQ_ERR_OK);
    TEST(tq_strand_create(&strand, runner) == TQ_ERR_OK);
    TEST(strand != NULL);
    tq_strand_destroy(strand);
    tq_runner_destroy(runner);
    return 0;
}

int test_tq_strand_push_count_task()
{
    { // Push zero tasks
        tq_strand* strand;
        tq_runner* runner;
        TEST(tq_runner_create(&runner) == TQ_ERR_OK);
        TEST(tq_strand_create(&strand, runner) == TQ_ERR_OK);
        TEST(tq_runner_run(runner) == TQ_ERR_OK);
        tq_strand_destroy(strand);
        tq_runner_destroy(runner);
    }
    { // Push one task
        tq_strand* strand;
        tq_runner* runner;
        TEST(tq_runner_create(&runner) == TQ_ERR_OK);
        TEST(tq_strand_create(&strand, runner) == TQ_ERR_OK);

        int result    = 0;
        count_task* t = count_task_create(&result);
        tq_strand_push(strand, &t->base);
        TEST(tq_runner_run(runner) == TQ_ERR_OK);
        TEST(result == 1);
        tq_strand_destroy(strand);
        tq_runner_destroy(runner);
    }
    { // Push 500 tasks
        tq_strand* strand;
        tq_runner* runner;
        TEST(tq_runner_create(&runner) == TQ_ERR_OK);
        TEST(tq_strand_create(&strand, runner) == TQ_ERR_OK);

        int result = 0;
        for (int i = 0; i < 500; ++i) {
            count_task* t = count_task_create(&result);
            tq_strand_push(strand, &t->base);
        }
        TEST(tq_runner_run(runner) == TQ_ERR_OK);
        TEST(result == 500);
        tq_strand_destroy(strand);
        tq_runner_destroy(runner);
    }
    return 0;
}

static int test_tq_strand_push_count_task_parallel()
{
    { // Run zero tasks
        tq_strand* strand;
        tq_runner* runner;
        TEST(tq_runner_create(&runner) == TQ_ERR_OK);
        TEST(tq_strand_create(&strand, runner) == TQ_ERR_OK);
        run_in_threads(runner, 4);
        tq_strand_destroy(strand);
        tq_runner_destroy(runner);
    }
    { // Run one task
        tq_strand* strand;
        tq_runner* runner;
        TEST(tq_runner_create(&runner) == TQ_ERR_OK);
        TEST(tq_strand_create(&strand, runner) == TQ_ERR_OK);

        int result    = 0;
        count_task* t = count_task_create(&result);
        tq_strand_push(strand, &t->base);
        run_in_threads(runner, 4);
        TEST(result == 1);
        tq_strand_destroy(strand);
        tq_runner_destroy(runner);
    }
    { // Run 500 tasks
        tq_strand* strand;
        tq_runner* runner;
        TEST(tq_runner_create(&runner) == TQ_ERR_OK);
        TEST(tq_strand_create(&strand, runner) == TQ_ERR_OK);

        int result = 0;
        for (int i = 0; i < 500; ++i) {
            count_task* t = count_task_create(&result);
            tq_strand_push(strand, &t->base);
        }
        run_in_threads(runner, 4);
        TEST(result == 500);
        tq_strand_destroy(strand);
        tq_runner_destroy(runner);
    }
    return 0;
}

int main()
{
    TEST(test_tq_runner_create() == 0);
    TEST(test_tq_runner_push_count_task() == 0);
    TEST(test_tq_runner_push_count_task_parallel() == 0);
    TEST(test_tq_strand_create() == 0);
    TEST(test_tq_strand_push_count_task() == 0);
    TEST(test_tq_strand_push_count_task_parallel() == 0);
    printf("All tests passed\n");
    return 0;
}
