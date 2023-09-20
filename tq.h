#ifndef TQ_TQ_H
#define TQ_TQ_H

typedef enum tq_err
{
    TQ_ERR_OK = 0,

    /** TQ_ERR_OOM
     * @brief Out of memory
     */
    TQ_ERR_OOM = -1,

    /** TQ_ERR_OS
     * @brief An error occured in the underlying OS API
     * @note errno will be set if this error is returned.
     */
    TQ_ERR_OS = -2,
} tq_err;

/** tq_task
 * @brief A task is an object that can be executed by a runner.
 * The fn and destroy functions must be implemented and set by the user.
 *
 * tq_task is designed to be used as a base class for other tasks.
 * Thus, the first member of the user defined task should be a tq_task.
 * For example:
 * typedef struct my_task
 * {
 *      tq_task base;
 *      int my_data;
 * };
 *
 * The user then must implement the fn and destroy functions
 * and set them when creating the task:
 */
typedef struct tq_task
{
    /** fn
     * @brief The function to execute.
     */
    void (*fn)(void *self);

    /** destroy
     * @brief The destructor for the task.
     * Can be NULL if the task does not need to be destroyed.
     */
    void (*destroy)(void *self);

    /** This is used internally by the runner. */
    struct tq_task *next;
} tq_task;

/** tq_runner
 * @brief A runner manages a queue of tasks and executes them.
 */
typedef struct tq_runner tq_runner;

/** tq_runner_create
 * @brief Creates a runner.
 * @param runner (out) Output pointer for the runner.
 * @return TQ_ERR_OK on success, otherwise an error code.
 */
tq_err tq_runner_create(tq_runner **runner);

/** tq_runner_push
 * @brief Pushes a task to the runner.
 * @param runner (in) The runner to push the task to.
 * @param task (in) The task to push. Ownership of the task is transferred
 */
void tq_runner_push(tq_runner *runner, tq_task *task);

/** tq_runner_run
 * @brief Executes the tasks from the task queue. Can be called from
 * multiple threads simulationously to achieve parallelism.
 * This function blocks until there are no more tasks to execute.
 * @param runner (in) The runner to run.
 * @return TQ_ERR_OK on success, otherwise an error code.
 */
tq_err tq_runner_run(tq_runner *runner);

/** tq_runner_destroy
 * @brief Destroys the runner.
 * @param runner (in) The runner to destroy.
 */
void tq_runner_destroy(tq_runner *runner);

/** tq_strand
 * @brief A strand object is used to dispatch tasks to a runner with
 * the guarantee that the tasks sequentially.
 */
typedef struct tq_strand tq_strand;

/** tq_strand_create
 * @brief Creates a strand for the given runner.
 * @param strand (out) Output pointer for the strand.
 * @param runner (in) The runner to dispatch tasks to.
 * @return TQ_ERR_OK on success, otherwise an error code.
 */
tq_err tq_strand_create(tq_strand **strand, tq_runner *runner);

/** tq_strand_push
 * @brief Pushes a task to the strand. Every task pushed to the strand
 * is guaranteed to be executed sequentially.
 * @param strand (in) The strand to push the task to.
 * @param task (in) The task to push. Ownership of the task is transferred
 */
void tq_strand_push(tq_strand *strand, tq_task *task);

/** tq_strand_destroy
 * @brief Destroys the strand.
 * @param strand (in) The strand to destroy.
 */
void tq_strand_destroy(tq_strand *strand);

#endif // TQ_TQ_H
