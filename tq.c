#include "tq.h"
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>

//
#define FREE(ptr)   \
    {               \
        free(ptr);  \
        ptr = NULL; \
    }

// move 'src' to 'dest' and set 'src' to NULL
#define MOVE_PTR(dest, src) \
    dest = src;             \
    src = NULL

// call 'fn' on 'value' if 'value' != 0
#define CALL_ON_IF(value, fn) \
    if (value)                \
    fn(value)

// goto 'label' if 'value' != 0
#define GOTO_IF(value, label) \
    if (value)                \
    goto label

/* tq_task init function */

void tq_task_init(tq_task *task, void (*fn)(void *self), void (*destroy)(void *self))
{
    task->fn = fn;
    task->destroy = destroy;
    task->next = NULL;
}

/* tq_mutex */

typedef struct tq_mutex
{
    pthread_mutex_t mtx;
} tq_mutex;

/* tq_mutex functions */

static tq_err tq_mutex_create(tq_mutex **mutex)
{
    if ((*mutex = calloc(1, sizeof(tq_mutex))) == NULL)
        return TQ_ERR_OOM;
    if (pthread_mutex_init(&(*mutex)->mtx, NULL))
    {
        FREE(*mutex);
        return TQ_ERR_OS;
    }
    return TQ_ERR_OK;
}

static void tq_mutex_destroy(tq_mutex *mutex)
{
    pthread_mutex_destroy(&mutex->mtx);
    free(mutex);
}

static void tq_mutex_lock(tq_mutex *mutex)
{
    int ret = pthread_mutex_lock(&mutex->mtx);
    assert(ret == 0 && "pthread_mutex_lock failed");
}

static void tq_mutex_unlock(tq_mutex *mutex)
{
    int ret = pthread_mutex_unlock(&mutex->mtx);
    assert(ret == 0 && "pthread_mutex_unlock failed");
}

/* tq_cond */

typedef struct tq_cond
{
    pthread_cond_t cond;
} tq_cond;

/* tq_cond functions */

static tq_err tq_cond_create(tq_cond **cond)
{
    if ((*cond = calloc(1, sizeof(tq_cond))) == NULL)
        return TQ_ERR_OOM;
    if (pthread_cond_init(&(*cond)->cond, NULL))
    {
        FREE(*cond);
        return TQ_ERR_OS;
    }
    return TQ_ERR_OK;
}

static void tq_cond_destroy(tq_cond *cond)
{
    pthread_cond_destroy(&cond->cond);
    FREE(cond);
}

static void tq_cond_wait(tq_cond *cond, tq_mutex *mtx)
{
    int ret = pthread_cond_wait(&cond->cond, &mtx->mtx);
    assert(ret == 0 && "pthread_cond_wait failed");
}

static void tq_cond_wake_one(tq_cond *cond)
{
    int ret = pthread_cond_signal(&cond->cond);
    assert(ret == 0 && "pthread_cond_signal failed");
}

/* tq_queue */
typedef struct tq_queue
{
    tq_task *head;
    tq_task *tail;
} tq_queue;

/* tq_queue functions */

static void tq_queue_init(tq_queue *queue)
{
    queue->head = NULL;
    queue->tail = NULL;
}

static void tq_queue_push(tq_queue *queue, tq_task *task)
{
    if (!queue->head)
    {
        queue->head = task;
        queue->tail = task;
    }
    else
    {
        queue->tail->next = task;
        queue->tail = task;
    }
}

static tq_task *tq_queue_pop(tq_queue *queue)
{
    tq_task *task = queue->head;
    if (task)
    {
        queue->head = task->next;
        if (!queue->head)
            queue->tail = NULL;
    }
    return task;
}

static int tq_queue_is_empty(tq_queue *queue)
{
    return queue->head == NULL;
}

/* tq_thread_data */
typedef struct tq_thread_data
{
    tq_queue queue;
} tq_thread_data;

/* tq_thread_data functions */

static tq_err tq_thread_data_create(tq_thread_data **data)
{
    *data = calloc(1, sizeof(tq_thread_data));
    if (!*data)
        return TQ_ERR_OOM;
    tq_queue_init(&(*data)->queue);
    return TQ_ERR_OK;
}

static void tq_thread_data_destroy(tq_thread_data *data)
{
    free(data);
}

/* tq_thread_data_storage */

typedef struct tq_thread_data_storage
{
    pthread_key_t key;
} tq_thread_data_storage;

/* tq_thread_data_storage functions */

static void tq_thread_data_storage_on_key_destroy(void *data)
{
    if (data)
        tq_thread_data_destroy(data);
}

static tq_err tq_thread_data_storage_create(tq_thread_data_storage **storage)
{
    pthread_key_t key;
    if (pthread_key_create(&key, tq_thread_data_storage_on_key_destroy))
        return TQ_ERR_OS;

    *storage = calloc(1, sizeof(tq_thread_data_storage));
    if (!*storage)
        return TQ_ERR_OOM;
    (*storage)->key = key;
    return TQ_ERR_OK;
}

static tq_thread_data *tq_thread_data_storage_get(tq_thread_data_storage *storage)
{
    return pthread_getspecific(storage->key);
}

static tq_err tq_thread_data_storage_ensure_and_get(tq_thread_data_storage *storage, tq_thread_data **data)
{
    tq_err err = TQ_ERR_OK;
    *data = tq_thread_data_storage_get(storage);
    if (!*data)
    {
        if ((err = tq_thread_data_create(data)) != TQ_ERR_OK)
            return err;
        if (pthread_setspecific(storage->key, *data))
        {
            tq_thread_data_destroy(*data);
            return TQ_ERR_OS;
        }
    }
    return TQ_ERR_OK;
}

static void tq_thread_data_storage_destroy(tq_thread_data_storage *storage)
{
    pthread_key_delete(storage->key);
    free(storage);
}

/* tq_runer */

struct tq_runner
{
    tq_thread_data_storage *storage;
    tq_mutex *mtx;
    tq_cond *cond;
    tq_queue queue;
    size_t tasks;
};

tq_err tq_runner_create(tq_runner **runner)
{
    tq_err err = TQ_ERR_OK;

    tq_thread_data_storage *storage = NULL;
    GOTO_IF((err = tq_thread_data_storage_create(&storage)) != TQ_ERR_OK, cleanup);

    tq_mutex *mtx = NULL;
    GOTO_IF((err = tq_mutex_create(&mtx)) != TQ_ERR_OK, cleanup);

    tq_cond *cond = NULL;
    GOTO_IF((err = tq_cond_create(&cond)) != TQ_ERR_OK, cleanup);

    *runner = calloc(1, sizeof(tq_runner));
    GOTO_IF(!*runner && (err = TQ_ERR_OOM), cleanup);
    tq_queue_init(&(*runner)->queue);
    (*runner)->tasks = 0;

    MOVE_PTR((*runner)->storage, storage);
    MOVE_PTR((*runner)->mtx, mtx);
    MOVE_PTR((*runner)->cond, cond);

cleanup:
    CALL_ON_IF(storage, tq_thread_data_storage_destroy);
    CALL_ON_IF(mtx, tq_mutex_destroy);
    CALL_ON_IF(cond, tq_cond_destroy);
    return err;
}

static void tq_runner_push_to_thread_queue(tq_runner *runner, tq_task *task, tq_thread_data *thread_data)
{
    tq_queue_push(&thread_data->queue, task);
}

void tq_runner_push(tq_runner *runner, tq_task *task)
{
    tq_thread_data *thread_data = tq_thread_data_storage_get(runner->storage);
    if (thread_data)
    {
        tq_runner_push_to_thread_queue(runner, task, thread_data);
    }
    else
    {
        tq_mutex_lock(runner->mtx);
        tq_queue_push(&runner->queue, task);
        ++runner->tasks;
        tq_cond_wake_one(runner->cond);
        tq_mutex_unlock(runner->mtx);
    }
}

static void tq_runner_cleanup_thread_queue(tq_runner *runer, tq_thread_data *thread_data)
{
    if (tq_queue_is_empty(&thread_data->queue))
        return;

    tq_mutex_lock(runer->mtx);
    tq_task *task;
    while ((task = tq_queue_pop(&thread_data->queue)))
    {
        tq_queue_push(&runer->queue, task);
        ++runer->tasks;
    }
    tq_mutex_unlock(runer->mtx);
}

tq_err tq_runner_run_one_impl(tq_runner *runner, tq_thread_data *thread_data)
{
    while (1)
    {
        if (!tq_queue_is_empty(&runner->queue))
        {
            tq_task *task = tq_queue_pop(&runner->queue);
            --runner->tasks;
            tq_mutex_unlock(runner->mtx);
            task->fn(task);
            if (task->destroy)
                task->destroy(task);
            tq_runner_cleanup_thread_queue(runner, thread_data);
            return TQ_ERR_OK;
        }
        else
        {
            tq_cond_wait(runner->cond, runner->mtx);
        }
    }
}

tq_err tq_runner_run(tq_runner *runner)
{
    tq_thread_data *thread_data = NULL;
    tq_err err = tq_thread_data_storage_ensure_and_get(runner->storage, &thread_data);
    if (err != TQ_ERR_OK)
        return err;

    tq_mutex_lock(runner->mtx);
    for (; runner->tasks; tq_mutex_lock(runner->mtx))
    {
        if (tq_runner_run_one_impl(runner, thread_data) != TQ_ERR_OK)
            return TQ_ERR_OS;
    }
    tq_mutex_unlock(runner->mtx);
    return TQ_ERR_OK;
}

tq_err tq_runner_run_one(tq_runner *runner)
{
    tq_thread_data *thread_data = NULL;
    tq_err err = tq_thread_data_storage_ensure_and_get(runner->storage, &thread_data);
    if (err != TQ_ERR_OK)
        return err;

    tq_mutex_lock(runner->mtx);
    if (runner->tasks == 0)
        return TQ_ERR_OK;
    return tq_runner_run_one_impl(runner, thread_data);
}

void tq_runner_destroy(tq_runner *runner)
{
    CALL_ON_IF(runner->storage, tq_thread_data_storage_destroy);
    CALL_ON_IF(runner->mtx, tq_mutex_destroy);
    CALL_ON_IF(runner->cond, tq_cond_destroy);
    free(runner);
}

/* tq_strand_task */

typedef struct tq_strand_task
{
    tq_task base;
    tq_strand *strand;
} tq_strand_task;

static void tq_strand_task_fn(void *self);

static void tq_strand_task_init(tq_strand_task *task, tq_strand *strand);

/* tq_strand */

struct tq_strand
{
    tq_runner *runner;
    tq_mutex *mtx;
    tq_queue queue;
    tq_strand_task task;
};

/* tq_strand functions */

tq_err tq_strand_create(tq_strand **strand, tq_runner *runner)
{
    tq_err err = TQ_ERR_OK;

    tq_mutex *mtx = NULL;
    GOTO_IF((err = tq_mutex_create(&mtx)) != TQ_ERR_OK, cleanup);

    *strand = calloc(1, sizeof(tq_strand));
    GOTO_IF(!*strand && (err = TQ_ERR_OOM), cleanup);
    (*strand)->runner = runner;
    tq_queue_init(&(*strand)->queue);
    tq_strand_task_init(&(*strand)->task, *strand);

    MOVE_PTR((*strand)->mtx, mtx);
cleanup:
    CALL_ON_IF(mtx, tq_mutex_destroy);
    return err;
}

void tq_strand_push(tq_strand *strand, tq_task *task)
{
    tq_mutex_lock(strand->mtx);
    tq_queue_push(&strand->queue, task);
    int is_empty = tq_queue_is_empty(&strand->queue);
    tq_mutex_unlock(strand->mtx);

    if (is_empty)
        tq_runner_push(strand->runner, &strand->task.base);
}

void tq_strand_destroy(tq_strand *strand)
{
    CALL_ON_IF(strand->mtx, tq_mutex_destroy);
    free(strand);
}

/* tq_strand_task function impl */

static void tq_strand_task_init(tq_strand_task *task, tq_strand *strand)
{
    tq_task_init(&task->base, tq_strand_task_fn, NULL);
    task->strand = strand;
}

static void tq_strand_task_fn(void *self)
{
    tq_strand_task *stt = self;
    tq_mutex_lock(stt->strand->mtx);
    tq_task *task = tq_queue_pop(&stt->strand->queue);
    int is_empty = tq_queue_is_empty(&stt->strand->queue);
    tq_mutex_unlock(stt->strand->mtx);

    task->fn(task);
    if (task->destroy)
        task->destroy(task);

    if (!is_empty)
        tq_runner_push(stt->strand->runner, &stt->base);
}
