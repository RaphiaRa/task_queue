#ifndef TQ_TQ_H
#define TQ_TQ_H

typedef enum tq_err
{
    TQ_ERR_OK = 0,
    TQ_ERR_OOM = -1,
    TQ_ERR_OS = -2,
} tq_err;

typedef struct tq_task
{
    void (*fn)(void *self);
    void (*destroy)(void *self);
    struct tq_task *next;
} tq_task;

void tq_task_init(tq_task *task, void (*fn)(void *self), void (*destroy)(void *self));

typedef struct tq_runner tq_runner;

typedef struct tq_strand tq_strand;

tq_err tq_runner_create(tq_runner **runner);
tq_err tq_runner_push(tq_runner *runner, tq_task *task);
tq_err tq_runner_run(tq_runner *runner);
tq_err tq_runner_run_one(tq_runner *runner);
void tq_runner_destroy(tq_runner *runner);

tq_err tq_strand_create(tq_strand **strand, tq_runner *runner);
tq_err tq_strand_push(tq_strand *strand, tq_task *task);
void tq_strand_destroy(tq_strand *strand);

#endif // TQ_TQ_H
