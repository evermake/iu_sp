---
language: C
deadline: July 20, 2023
---

# Homework 04 — Thread pool

> Original materials have been copied from
> [here](https://github.com/Gerold103/sysprog/blob/31f432a724dcbf9c4f68273197635505ff2479a6/4/task_eng.txt).

You need to implement a thread pool. In various programs executing many
independent and easily parallelized tasks it is often quite handy to distribute
them across multiple threads. But creating a thread for each necessity to
execute something in parallel is very expensive in time and resources. If a task
is not too long, doesn't read disk, doesn't touch network, then
creation/deletion of a thread might take more time than the task itself.

Then tasks are either not paralleled at all, or when there are many of them,
people make thread pools. It is usually a task queue and a few so called "worker
threads" which take tasks from the queue. Thus there is always an already
created thread which can quickly pick up a task. And instead of exiting the
thread simply picks up a next task.

In big general purpose libraries often there is an out of the box solution: in
Qt it is `QThreadPool` class, in .NET it is `ThreadPool` class, in boost it is
`thread_pool` class. In the task you have to implement an own similar pool.

In the files `thread_pool.h` and `thread_pool.c` you can find templates of
functions and structures which need to be implemented.

The thread pool is described by a struct `thread_pool` implemented in
`thread_pool.c`. A user can only have a pointer at it. Each task/job is
described with struct `thread_task`, which a user can create and put into the
pool's queue.

User can check task's status (waits for getting a worker; is already being
executed), can wait for its end and get its result with `thread_task_join`,
similar to how `pthread_join` works.

Since the task is to implement a library, there is no `main` function and no
input from anywhere. You can write tests in C in a separate file with `main` and
which will `include` your solution. For example, make a file `main.c`, add
`include "thread_pool.h"`, and in the function `main` you do tests. It can all
be built like this:

```shell
gcc thread_pool.c main.c
```

## Rules

- `thread_pool` at creation via `thread_pool_new()` shouldn't start all max
  number of threads at once. The threads have to be started gradually when
  needed, until reach the limit specified in `thread_pool_new()`.

- Joined but not yet deleted tasks can be re-pushed back into the pool.

- The other rules can be read in the documentation in `thread_pool.h` and
  deducted from the tests.

## Restrictions

- Global variables are not allowed (except for the already existing ones).

- Memory leaks are not allowed. To check them you can use `utils/heap_help`
  tool. Showing zero leak reports from Valgrind or ASAN is not enough - they
  often miss the leaks.

- The code should have zero busy loops or sleep loops. These are only allowed in
  tests. I.e. in `thread_task_join()` you can't just do
  `while (!task->is_finished) {usleep(1)};`. For waiting on anything you need to
  use `pthread_cond_t`.

## Relaxations

- There is no limit on the number of mutexes and condvars. You can even store
  them in each task.

## Points

- **15 points**: implement all functions from `thread_pool.h`, which are not
  hidden inside macros.

- **+5 points**: implement `thread_task_detach()`. The documentation is in
  `thread_pool.h`. You need to define the macro `NEED_DETACH` then.

- **+5 points**: implement `thread_task_timed_join()`. The documentation is in
  `thread_pool.h`. You need to define the macro `NEED_TIMED_JOIN` then.

- **-5 points**: you can use C++, STL containers, `std::thread`, `std::mutex`,
  etc.

The additional options for +5 points do not include each other. That is, you can
do none, or do only one, or do only another, or both for +10. Or use C++ and get
-5 to your sum.
