___
language: C
deadline: June 20, 2023
___

# Homework 01 — Merge sort in coroutines

> Original materials have been copied from [here](https://github.com/Gerold103/sysprog/tree/d72b72880d6ee5295cea224cc734dac7c14d4b54/1).

Files are stored on the disk. They store integers in arbitrary
order separated by whitespaces. Each file should be sorted, and
then results should be merged into a new file. That is, merge sort
should be implemented.

### Rules:

- Each file should be sorted in its own coroutine. This rule
  targets your understanding of what cooperative multitasking is.

- Files have ASCII encoding. I.e. they are plain text. Not unicode
  or anything.

- For time measurements you should use
  `clock_gettime(CLOCK_MONOTONIC)`. Non-monotonic clocks (such as
  `time()`, `gettimeofday()`, `CLOCK_REALTIME`, etc.) can go backwards
  sometimes, screwing the time measurements. `clock()` is monotonic,
  but doesn't account time spent in blocking syscalls.


### Restrictions:

- Global variables are not allowed (except for the already
  existing ones).

- Sorting should be implemented by you without built-in functions
  like `qsort()`, `system("sort ...")`, etc.

- Sorting complexity of the individual files should be < O(N^2)
  (for example, you can't use bubble sort). Given that
  restriction, you can choose any sorting algorithm such as
  "quick sort".

- Total execution time is limited. But since the students have
  different hardware, the limit is for my own PC. With 2.5GHz
  CPU and an SSD sorting of 6 files each having 40k numbers
  shouldn't take longer than a second. Normally it should even
  take 100-200 ms, but 1 second will do as well.

- Work with files should be done

  - either via a numeric file descriptor using `open()` / `read()` /
    `write()` / `close()` functions,

  - or via `FILE*` and functions `fopen()` / `fscanf()` / `fprintf()` /
    `fclose()`. It is not allowed to use `std::iostream`,
    `std::ostream`, `std::istream` and other STL helpers.

### Relaxations:

- Numbers fit into `int` type.

- You can assume, that all the files fit into the main memory,
  even together.

- The final step - merging of the sorted files - can be done right
  in `main()` without any coroutines.

### Advices:

- You can find more info about various unknown functions using
  `man` command line utility. For example, `man read` (or
  `man 2 read`) prints manual for `read()`` function. `man strdup`
  can tell more about `strdup()` function. Similar for other
  built-in functions.

- Steps which you can follow if don't know where to start:

  - Implement normal sorting of one file. No coroutines or
    multiple files. Just read and sort a single file. Test this
    code.

  - Extend your code to sort multiple files and do the merge
    sort, also without coroutines. Check it on the real tests.
    This will allow to concentrate on adding coroutines, and not
    to waste time on debugging both coroutines and sorting at the
    same time.

  - Start using coroutines.


### Possible solutions:

The coroutines should switch between each other. Do the so called
"yield"s. These are `coro_yield()` in the `solution.c` file. There are
several options how you use them:

- **15 points**: yield after each iteration in your individual files
  sorting loops.

- **+5 points**: each of N coroutines is given T / N microseconds,
  where T - target latency given as a command line parameter.
  After each sorting loop iteration you yield only if the current
  coroutine's time quantum is over.

- **+5 points**: allow to specify the number of coroutines. Each
  coroutine should work as follows: if there are unsorted files,
  then pick one and sort it, then repeat. If there are no unsorted
  files, then the coroutine exits. For example, assume there are
  10 files given and 3 coroutines. They pick 1 file each, 7 files
  are waiting. One coroutine finishes the sorting, picks up a next
  file (6 are waiting). Another coroutine finishes some other
  and picks up the next one (5 files remaining). And so on until
  all files are sorted. Then you do the normal merge sort. This
  bonus task basically offers you do implement a coroutine pool.

- **-5 points**: you can use C++ and STL containers. Including
  `std::iostream`/`ostream`/etc. But you still can't use
  `std::sort()`.

The additional options for +5 points do not include each other.
That is, you can do none, or do only one, or do only another, or
both for +10. Or use C++ and get -5 to your sum.

Input: names of files to sort via the command line arguments. If
you do the bonus tasks, then you also get the target latency in
microseconds (if you do that bonus) and the coroutine count (if
you do that bonus) before the file names.

Output: total work time, work time and number of context switches
for each individual coroutine. Keep in mind that the coroutine
work time doesn't include its wait time, i.e. while it was
sleeping during the `coro_yield()`. So you should stop coroutine
timer before each `coro_yield()` and start it again right after.

For testing you can create your files or generate them using
`generator.py` script. An example, which should work for 15 points
solution:

```
python3 generator.py -f test1.txt -c 10000 -m 10000
python3 generator.py -f test2.txt -c 10000 -m 10000
python3 generator.py -f test3.txt -c 10000 -m 10000
python3 generator.py -f test4.txt -c 10000 -m 10000
python3 generator.py -f test5.txt -c 10000 -m 10000
python3 generator.py -f test6.txt -c 100000 -m 10000

./main test1.txt test2.txt test3.txt test4.txt test5.txt test6.txt
```

For checking the result you can use the script `checker.py`. All
scripts assume working in python 3.
