---
language: C
deadline: July 3, 2023
---

# Homework 02 — Shell

> Original materials have been copied from [here](https://github.com/Gerold103/sysprog/blob/6dc1dd9695f122756f7dbde967cd5bb0253dd361/2/task_eng.txt).


You need to implement a simplified version of a command line
console. It should take lines like this:

```
$ command_name param1 param2 ...
```

and execute them by calling `command_name` with the given
parameters. So be a terminal, basically.


### Rules

- The program should correctly handle quoted strings even if there
  are whitespaces inside it.

- You need to handle comments (by trimming them).

- The console should support pipes expressed as symbol '|' and
  output redirection using '>' and '>>'.

- The program should print all the same what would be printed by a
  real terminal like `/bin/bash`.

- You need to use functions `pipe()`, `dup/dup2()`, `fork()`, `wait`,
  `open`, `close`, at least one of `execl`/`execle`/`execlp`/`execv`/`execvp`/
  `execvP`.

- The `cd` command you need to implement yourself, not via `exec`
  functions. Because it changes the current working directory of
  the terminal itself. Luckily, it is easy to do with the C
  function `chdir()`.

- The `exit` command is also special, like `cd`. Because it
  concerns the terminal itself. It needs to be implemented
  manually. But keep in mind that it should exit from the terminal
  itself only if it is alone in the line. `exit` - terminate the
  shell. `exit | echo 100` - do not terminate, execute it via
  `exec` like any other command.

- Your console should support tokens written without spaces when
  `/bin/bash` is able to parse them too. Like this:
  `echo "4">file` (works same as `echo "4" > file`), or
  `echo 100|grep 100` (works same as `echo 100 | grep 100`).

- When something is unclear how it should work, you should test it
  in the real console like `/bin/bash` and repeat the behaviour.

- The code should be built successfully with these compiler flags:
  `-Wextra -Werror -Wall -Wno-gnu-folding-constant`.


### Restrictions

- Global variables are not allowed (except for the already
  existing ones).

- Memory leaks are not allowed. To check them you can use
  `utils/heap_help` tool. Showing zero leak reports from Valgrind or
  ASAN is not enough - they often miss the leaks.

- Length of command line input is not limited, so you can't read
  it into a buffer of a fixed size. But each line obviously fits
  into the main memory.

- It is forbidden to use functions like `system()`, `popen()` or use
  any other way to access existing terminal-like ready-to-use
  functions.


### Relaxations

- You don't need to support redirect of specific output streams,
  like stderr. These commands: `1>`, `2>`, `$>`, `1>>`, and alike are not
  needed. (Usual `>` and `>>` still are needed.)

- No need to support multiple redirects like
  `cmd > file1 > file2` or `cmd > file1 file2`.

- You don't need to support `~` path component in `cd` command.


### Input examples

- Print process list and find 'init' string in them:
  ```
  $ ps aux | grep init
  ```

- Execute code in python and search a string in its output:
  ```
  $ echo "print('result is ', 123 + 456)" | python -i | grep result
  ```

- Print escaped quote into a file and print it:
  ```
  $ echo "some text\" with quote" > test.txt
  > cat test.txt
  ```
    

- Append to a file:
  ```
  $ echo "first data" > test.txt
  $ echo "second" >> test.txt
  $ cat test.txt
  ```

- Start an interactive python console and do something in it:
  ```
  $ python
  >>> print(1 + 2)
  >>> 3
  ```


### Points

  - **15 points**: all described above,
  - **20 points**: support operators `&&` and `||`,
  - **25 points**: 20 + support `&`.
  - **-5 points**: you can use C++ and STL containers.

**Input**: commands and their arguments, input/output redirection operators.

**Output**: the same what a real terminal prints.


### Where to begin?

The recommended implementation plan is below:

- Implement command parser. This is an independent part of a solution, and at
  the same time the simplest in terms of understanding what to do. It is worth
  doing it in a separate source file, and test independently. Your parser
  should turn a command like "command arg1 arg2 ... argN" into separate
  "command" and an array of its arguments. For example, in a form of const char
  \*\*args, int arg_count. The parser should specially handle |, considering
  it a command separator.

  For example, let the parser to parse a command into:

  ```c
  struct cmd {
    const char *name;
    const char **argv;
    int argc;
  }
  ```

  The the parser should return an array of struct cmd. If there are no |, it
  will contain 1 element;

- Implement command execution without `|`, `>`, `>>`. Just one command;

- Add support for `|`, `>`, `>>`.

Architecture of the solution may be the following: there is a process-terminal,
which reads user commands. On each command it does `fork()`. The new child
executes the command using `exec\*()` functions. The parent process waits for
the child termination. For `|` the terminal opens a pipe, which is used to link
input of one child and output of another. For `>` and `>>` the terminal opens a
file, and using `dup/dup2()` changes standard output for a child.
