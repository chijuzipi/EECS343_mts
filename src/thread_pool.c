#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "thread_pool.h"

/**
 *  @struct threadpool_task
 *  @brief the work struct
 *
 *  Feel free to make any modifications you want to the function prototypes and structs
 *
 *  @var function Pointer to the function that will perform the task.
 *  @var argument Argument to be passed to the function.
 */

#define MAX_THREADS 20
#define STANDBY_SIZE 8

typedef struct {
  void (*function)(void *);
  void *argument;
} pool_task_t;

struct pool_t {
  pthread_mutex_t lock;
  pthread_cond_t flag;
  pthread_t *threads;
  pool_task_t *queue;
  int size;
  int closed;
  int first;
  int last;
  int count;
};

static void *thread_do_work(void *pool);


/*
 * Create a threadpool, initialize variables, etc
 *
 */
pool_t *pool_create(int queue_size, int num_threads)
{
  int i;
  pool_t *thread_pool;
  if (num_threads == 0) {
    return NULL;
  }

  //allocate memory for the threads 
  thread_pool = (pool_t *)malloc(sizeof(pool_t));
  thread_pool->threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
  thread_pool->count = num_threads;
  thread_pool->size = queue_size;

  // Create pool threads 
  for (i = 0; i < num_threads; i++) {
    pthread_create(&thread_pool->threads[i], NULL,
                   thread_do_work, (void *)thread_pool);
  }

  // Initialize task queue
  thread_pool->queue = (pool_task_t *)malloc(queue_size * sizeof(pool_task_t)); 
  thread_pool->closed = 0;
  thread_pool->first = 0;
  thread_pool->last = 0;

  // Initialize the variable
  pthread_mutex_init(&thread_pool->lock, NULL);
  pthread_cond_init(&thread_pool->flag, NULL);

  return thread_pool;
}


/*
 * Add a task to the threadpool
 *
 */
int pool_add_task(pool_t *pool, void (*function)(void *), void *argument)
{
  pool_task_t *task;

  //get the lock
  pthread_mutex_lock(&pool->lock);

  task = &pool->queue[pool->last];
  pool->last = pool->last + 1;
  pool->last = ((pool->last == pool->size) ? 0 : pool->last);
  task->function = function;
  task->argument = argument;

  //notify the wating thread
  pthread_cond_signal(&pool->flag);

  //Unlock 
  pthread_mutex_unlock(&pool->lock);
        
  return 0;
}

/*
 * Destroy the threadpool, free all memory, destroy treads, etc
 *
 */
int pool_destroy(pool_t *pool)
{
  int i;
  if (pool == NULL)
    return -1;
  pool->closed = 1;

  // Wake the other threads 
  for (i = 0; i < pool->count; i++) {
    pthread_cond_signal(&pool->flag);
  }
  for (i = 0; i < pool->count; i++) {
    pthread_join(pool->threads[i], NULL);
  }
  free(pool->queue); 
  free(pool->threads); 
  free(pool);

  return 0;
}

/*
 * Work loop for threads. Should be passed into the pthread_create() method.
 *
 */
static void *thread_do_work(void *pool)
{ 
  pool_t *thread_pool = pool;
  pool_task_t *task;

  while(1) {
    //get the lock
    pthread_mutex_lock(&thread_pool->lock);

    //wait the signal clear
    pthread_cond_wait(&thread_pool->flag, &thread_pool->lock); 

    //if the server is not available
    if (thread_pool->closed == 1) {
      break;
    }
    task = &thread_pool->queue[thread_pool->first];
    thread_pool->first += 1;
    thread_pool->first = ((thread_pool->first == thread_pool->size) ? 0 : thread_pool->first);

    //unlock
    pthread_mutex_unlock(&thread_pool->lock);

    //execute the connection handler
    (* task->function)(&task->argument);
  }

  pthread_exit(NULL);
  return NULL;
}
