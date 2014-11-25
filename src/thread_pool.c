#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include<semaphore.h>

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
  sem_t sem_lock;
  sem_t sem_hasjob;
  pthread_mutex_t lock;
  pthread_cond_t flag;
  pthread_t *threads;
  pool_task_t *queue;
  int job_count;
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
  thread_pool->job_count = 0;

  // Initialize the variable
  sem_init(&thread_pool->sem_lock, 0, 1);
  sem_init(&thread_pool->sem_hasjob, 0, 0);
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
  sem_wait(&pool->sem_lock);

  task = &pool->queue[pool->last];
  pool->last = pool->last + 1;
  pool->last = ((pool->last == pool->size) ? 0 : pool->last);
  task->function = function;
  task->argument = argument;
  
  pool->job_count++;

  //notify the wating thread
  //pthread_cond_signal(&pool->flag);

  //Unlock 
  sem_post(&pool->sem_lock);
  sem_post(&pool->sem_hasjob);
        
  return 0;
}

/*
 * Work loop for threads. Should be passed into the pthread_create() method.
 */
static void *thread_do_work(void *pool)
{ 
  pool_t *thread_pool = pool;
  pool_task_t *task;

  while(1) {
    //wait the signal clear
    while(thread_pool->job_count == 0)
      sem_wait(&thread_pool->sem_hasjob); 

    //get the lock
    sem_wait(&thread_pool->sem_lock);

    thread_pool->job_count--;
    //if the server is not available
    if (thread_pool->closed == 1) {
      break;
    }
    task = &thread_pool->queue[thread_pool->first];
    thread_pool->first += 1;
    thread_pool->first = ((thread_pool->first == thread_pool->size) ? 0 : thread_pool->first);

    //unlock
    sem_post(&thread_pool->sem_lock);

    //execute the connection handler
    (* task->function)(&task->argument);
  }

  pthread_exit(NULL);
  return NULL;
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
    sem_post(&pool->sem_lock);
    sem_post(&pool->sem_hasjob);
    //pthread_cond_signal(&pool->flag);
  }
  for (i = 0; i < pool->count; i++) {
    pthread_join(pool->threads[i], NULL);
  }
  free(pool->queue); 
  free(pool->threads); 
  free(pool);

  return 0;
}

