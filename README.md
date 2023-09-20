## C Task Queue

#### Description

Simple task queue implementation in C that allows to add tasks to a queue and execute them from multiple threads.

Currently, only pthreads are supported.

#### Example

```c
#include "tq.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct count_task {
    tq_task base;
    tq_runner* runner;
    int count;
} count_task;

static void count_task_fn(void* self)
{
    count_task* task = self;
    printf("Task count: %d\n", ++task->count);
    if (task->count < 100)
        tq_runner_push(task->runner, &task->base);
}

int main()
{
    tq_runner* runner;
    tq_runner_create(&runner);

    count_task task = { 0 };
    task.base.fn    = count_task_fn;
    task.runner     = runner;

    tq_runner_push(runner, &task.base);
    tq_runner_run(runner);
    tq_runner_destroy(runner);
    return 0;
}

```

#### Parallel execution

To execute tasks in parallel, simply call `tq_runner_run` in multiple threads.

```c
static void* run_rountine(void* arg)
{
    tq_runner* runner = arg;
    tq_runner_run(runner);
    return NULL;
}

.
.
.

    pthread_t threads[100];
    for (int i = 0; i < 100; ++i)
        pthread_create(&threads[i], NULL, run_rountine, runner);
    for (int i = 0; i < 100; ++i)
        pthread_join(threads[i], NULL);
```
