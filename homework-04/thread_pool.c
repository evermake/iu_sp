#include "thread_pool.h"

#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

struct thread_task {
  thread_task_f function;
  void *arg;
  struct thread_pool *pool;

  bool is_detached;
  bool is_running;
  bool is_finished;
  void *result;

  pthread_cond_t cv;
  pthread_mutex_t mutex;

  struct thread_task *next;
};

struct thread_pool {
  pthread_t *threads;
  int max_thread_count;
  int thread_count;
  int idle_thread_count;

  int thread_idx_that_should_exit;

  pthread_cond_t cv;
  pthread_cond_t spawn_cv;
  pthread_mutex_t mutex;

  int pending_task_count;
  int running_task_count;
  struct thread_task *pending_task_list;
};

struct _thread_worker_arg {
  struct thread_pool *pool;
  int idx;
};

/**
 * Worker-function that executes tasks from a pool.
 *
 * It takes tasks from a pool and executes them until the pool will
 * ask it to exit.
 */
static void *_thread_worker(void *arg) {
  struct _thread_worker_arg *args = arg;
  struct thread_pool *pool = args->pool;
  int idx = args->idx;

  free(args);

  pthread_mutex_lock(&pool->mutex);
  ++pool->thread_count;
  pthread_cond_signal(&pool->spawn_cv);
  while (true) {
    ++pool->idle_thread_count;
    while (pool->pending_task_count < 1 &&
           pool->thread_idx_that_should_exit != idx) {
      pthread_cond_wait(&pool->cv, &pool->mutex);
    }

    if (pool->thread_idx_that_should_exit == idx) {
      pthread_mutex_unlock(&pool->mutex);
      break;
    }

    struct thread_task *task = pool->pending_task_list;  // must not be NULL
    pool->pending_task_list = task->next;
    task->next = NULL;
    --pool->idle_thread_count;
    --pool->pending_task_count;
    ++pool->running_task_count;

    pthread_mutex_unlock(&pool->mutex);

    // It is safe to access task->is_running without mutex
    // because we know that no one else can change it.
    task->is_running = true;

    // Execute the task
    void *result = task->function(task->arg);

    pthread_mutex_lock(&pool->mutex);
    pthread_mutex_lock(&task->mutex);
    --pool->running_task_count;
    if (task->is_detached) {
      thread_task_delete(task);
    } else {
      task->is_running = false;
      task->is_finished = true;
      task->result = result;
      pthread_cond_signal(&task->cv);
    }
    pthread_mutex_unlock(&task->mutex);
  }
  return NULL;
}

int thread_pool_new(int max_thread_count, struct thread_pool **pool) {
  if (max_thread_count < 1 || max_thread_count > TPOOL_MAX_THREADS) {
    return TPOOL_ERR_INVALID_ARGUMENT;
  }

  struct thread_pool *new_pool = malloc(sizeof(struct thread_pool));
  new_pool->threads = NULL;
  new_pool->max_thread_count = max_thread_count;
  new_pool->thread_count = 0;
  new_pool->idle_thread_count = 0;
  new_pool->thread_idx_that_should_exit = -1;
  new_pool->pending_task_count = 0;
  new_pool->running_task_count = 0;
  new_pool->pending_task_list = NULL;
  pthread_mutex_init(&new_pool->mutex, NULL);
  pthread_cond_init(&new_pool->cv, NULL);
  pthread_cond_init(&new_pool->spawn_cv, NULL);

  *pool = new_pool;

  return 0;
}

int thread_pool_thread_count(const struct thread_pool *pool) {
  return pool->thread_count;
}

int thread_pool_delete(struct thread_pool *pool) {
  pthread_mutex_lock(&pool->mutex);
  if (pool->running_task_count > 0 || pool->pending_task_count > 0 || pool->idle_thread_count != pool->thread_count) {
    pthread_mutex_unlock(&pool->mutex);
    return TPOOL_ERR_HAS_TASKS;
  }

  if (pool->threads != NULL) {
    // Tell workers to exit
    for (int i = 0; i < pool->thread_count; ++i) {
      pool->thread_idx_that_should_exit = i;
      pthread_cond_broadcast(&pool->cv);
      pthread_mutex_unlock(&pool->mutex);
      pthread_join(pool->threads[i], NULL);
      pthread_mutex_lock(&pool->mutex);
    }
    free(pool->threads);
  }
  pthread_mutex_unlock(&pool->mutex);

  pthread_mutex_destroy(&pool->mutex);
  pthread_cond_destroy(&pool->cv);
  pthread_cond_destroy(&pool->spawn_cv);

  free(pool);

  return 0;
}

int thread_pool_push_task(struct thread_pool *pool, struct thread_task *task) {
  pthread_mutex_lock(&pool->mutex);
  if (pool->pending_task_count >= TPOOL_MAX_TASKS) {
    pthread_mutex_unlock(&pool->mutex);
    return TPOOL_ERR_TOO_MANY_TASKS;
  }

  int required_thread_count = pool->thread_count;

  if (pool->threads == NULL) {
    // Create first worker
    pool->threads = malloc(sizeof(pthread_t));
    struct _thread_worker_arg *arg = malloc(sizeof(struct _thread_worker_arg));
    arg->pool = pool;
    arg->idx = 0;
    required_thread_count = 1;
    pthread_create(&pool->threads[0], NULL, _thread_worker, arg);
  } else if (pool->idle_thread_count == 0 &&
             pool->thread_count < pool->max_thread_count) {
    // Create additional worker
    pool->threads =
      realloc(pool->threads, sizeof(pthread_t) * (pool->thread_count + 1));
    struct _thread_worker_arg *arg = malloc(sizeof(struct _thread_worker_arg));
    arg->pool = pool;
    arg->idx = pool->thread_count;
    pthread_create(&pool->threads[pool->thread_count], NULL, _thread_worker,
                   arg);
    ++required_thread_count;
  }

  while (pool->thread_count != required_thread_count) {
    // Wait until spawned threads will be ready
    pthread_cond_wait(&pool->spawn_cv, &pool->mutex);
  }

  task->pool = pool;
  task->next = pool->pending_task_list;
  pool->pending_task_list = task;
  task->is_finished = false;
  task->is_running = false;
  ++pool->pending_task_count;
  pthread_cond_signal(&pool->cv);
  pthread_mutex_unlock(&pool->mutex);

  return 0;
}

int thread_task_new(struct thread_task **task, thread_task_f function,
                    void *arg) {
  struct thread_task *new_task = malloc(sizeof(struct thread_task));
  new_task->function = function;
  new_task->arg = arg;
  new_task->pool = NULL;
  new_task->is_detached = false;
  new_task->is_running = false;
  new_task->is_finished = false;
  new_task->result = NULL;
  new_task->next = NULL;
  pthread_mutex_init(&new_task->mutex, NULL);
  pthread_cond_init(&new_task->cv, NULL);

  *task = new_task;

  return 0;
}

bool thread_task_is_finished(const struct thread_task *task) {
  return task->is_finished;
}

bool thread_task_is_running(const struct thread_task *task) {
  return task->is_running;
}

int thread_task_join(struct thread_task *task, void **result) {
  if (task->pool == NULL) {
    return TPOOL_ERR_TASK_NOT_PUSHED;
  }
  pthread_mutex_lock(&task->mutex);
  while (!task->is_finished) {
    pthread_cond_wait(&task->cv, &task->mutex);
  }
  *result = task->result;
  task->pool = NULL;
  pthread_mutex_unlock(&task->mutex);

  return 0;
}

#ifdef NEED_TIMED_JOIN

int thread_task_timed_join(struct thread_task *task, double timeout, void **result) {
  if (task->pool == NULL) {
    return TPOOL_ERR_TASK_NOT_PUSHED;
  }

  pthread_mutex_lock(&task->mutex);

  if (timeout < 0.000000001) {
    if (!task->is_finished) {
      pthread_mutex_unlock(&task->mutex);
      return TPOOL_ERR_TIMEOUT;
    }
  } else {
    long timeout_sec = (long)timeout;
    long timeout_nsec = (long)((timeout - (double)timeout_sec) * 1000000000);
    struct timeval tv;
    struct timespec ts;
    gettimeofday(&tv, NULL);
    long total_nsec = tv.tv_usec * 1000 + timeout_nsec;
    long total_sec = tv.tv_sec + timeout_sec + total_nsec / 1000000000;
    total_nsec %= 1000000000;

    ts.tv_sec = total_sec;
    ts.tv_nsec = total_nsec;

    int cond_res = 0;
    while (!task->is_finished && cond_res != ETIMEDOUT) {
      cond_res = pthread_cond_timedwait(&task->cv, &task->mutex, &ts);
    }

    if (cond_res == ETIMEDOUT) {
      pthread_mutex_unlock(&task->mutex);
      return TPOOL_ERR_TIMEOUT;
    }
  }

  *result = task->result;
  task->pool = NULL;
  pthread_mutex_unlock(&task->mutex);

  return 0;
}

#endif

int thread_task_delete(struct thread_task *task) {
  if (task->pool != NULL) {
    return TPOOL_ERR_TASK_IN_POOL;
  }
  pthread_mutex_destroy(&task->mutex);
  pthread_cond_destroy(&task->cv);
  free(task);
  return 0;
}

#ifdef NEED_DETACH

int thread_task_detach(struct thread_task *task) {
  if (task->pool == NULL) {
    return TPOOL_ERR_TASK_NOT_PUSHED;
  }
  pthread_mutex_lock(&task->mutex);
  task->pool = NULL;
  if (task->is_finished) {
    pthread_mutex_unlock(&task->mutex);
    thread_task_delete(task);
    return 0;
  }
  task->is_detached = true;
  pthread_mutex_unlock(&task->mutex);
  return 0;
}

#endif
