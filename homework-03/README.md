---
language: C
deadline: July 10, 2023
---

# Homework 03 — Filesystem

> Original materials have been copied from
> [here](https://github.com/Gerold103/sysprog/blob/31f432a724dcbf9c4f68273197635505ff2479a6/3/task_eng.txt).

You need to implement own file system in memory. Don't be afraid, it is not too
complex. For start you are given a template of the FS interface with some
pre-implemented structures, in files `userfs.h` and `userfs.c`.

The file system is called UserFS, and it is very primitive. It has no folders -
all files are in "root". Files can be created, deleted, opened, closed. Each
file's structure is similar to the file system FAT: it is a block list. In
`userfs.c` you can look up structures which describe a block and a file storing
a list of blocks.

Nothing is stored on disk - all is in the main memory, on the heap. Files can be
read/written-to by their descriptors. API strongly resembles the one from
`libc`. You can read it fully in `userfs.h`.

The task is to implement this API in the way how it is explained in the comments
in `userfs.h`.

## Rules

- Need to strictly follow the behaviour of each function explained in `userfs.h`
  comments.

## Restrictions

- `userfs.h` can not be changed except for enabling the bonus tasks, see them
  below.

- The tests can not be changed.

- Global variables are not allowed (except for the already existing ones).

- Memory leaks are not allowed. To check them you can use `utils/heap_help`
  tool. Showing zero leak reports from Valgrind or ASAN is not enough - they
  often miss the leaks.

## Relaxations

- Can assume that all the data fits into the main memory and memory allocation
  functions (like `malloc()`) never fail.

## Points

- **15 points**: implement all functions from `userfs.h`.

- **+5 points**: implement file opening modes: for reading, writing,
  reading-and-writing. See `NEED_OPEN_FLAGS` in `userfs.h` and in the tests. A
  file descriptor opened for reading can not be used for writing. And
  vice-versa. By default the file has to be opened in read-write mode.

- **+5 points**: implement file resize. See `NEED_RESIZE` in `userfs.h`.

- **-5 points**: you can use C++ and STL containers.

The additional options for +5 points do not include each other. That is, you can
do none, or do only one, or do only another, or both for +10. Or use C++ and get
-5 to your sum.

## Advice

Your main tasks are:

1. Implement file growth when new data is being written;
2. Implement a file descriptor.

Couple of examples:

There is code:

```c
int fd = ufs_open("any_file_name", UFS_CREATE);
```

After this line inside `userfs.c` is created a struct file with a name
"any_file_name", if it doesn't exist yet. Then is created a file descriptor
`struct filedesc`.

```c
const char *data = "bla bla bla";
ufs_write(fd, data, strlen(data));
```

The file is empty, it has no blocks, so you have to allocate the needed number
of struct blocks. In this case it is just 1. The data is copied into there. The
file looks like this now:

```
file:
+---------------------+
| bla bla bla|        | -> NULL.
+---------------------+
             ^
          filedesc - descriptor points here. For example,
                     in the descriptor you can store a
                     block number and offset in it.
```

Then I keep writing but more data this time:

```c
char buf[1024];
memset(buf, 0, sizeof(buf));
ufs_write(fd, buf, sizeof(buf));
```

This is how it looks now:

```
        file:
        +---------------------+    +---------------------+
        | bla bla bla 0 0 0 0 | -> | 0 0 0 0 0 0 0 0 0 0 | ->
        +---------------------+    +---------------------+

        +---------------------+
     -> | 0 0 0 0 0 0|        | -> NULL.
        +---------------------+
                     ^
                  filedesc
```

The first block was filled to the end, and 2 new blocks were created. They were
just appended to the end of the list.

Same with the reading - the descriptor reads sequentially, jumping to the next
block when the previous one is fully read.
