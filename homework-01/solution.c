#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include "libcoro.h"

#define DEFAULT_TARGET_LATENCY 1000
#define DEFAULT_COROUTINES 3
#define OUTPUT_FILE "output.txt"

/**
 * Context of a coroutine.
 */
struct coro_context {
  /**
   * Name of the coroutine.
   */
  char *name;

  /**
   * Total time spent by the coroutine on work.
   */
  long long total_work_time;

  /**
   * Total number of switches to other coroutines.
   */
  long long total_switch_count;
};

int global_files_count;
char **global_filenames_to_sort;
int global_file_to_sort_idx;
int **global_sorted_arrays;
size_t *global_sorted_arrays_sizes;
long long global_coroutine_quantum;

/**
 * Returns the current monotonic time in microseconds.
 */
static long long get_now() {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return now.tv_sec * 1000000 + now.tv_nsec / 1000;
}

static void yield_if_necessary_record_work_time(
  long long *total_work_time,
  long long *last_work_timer_start
) {
  long long work_time = get_now() - *last_work_timer_start;
  if (work_time > global_coroutine_quantum) {
    *total_work_time += work_time;
    coro_yield();
    *last_work_timer_start = get_now();
  }
}

static void heap_sort(
  int *array,
  int size,
  long long *total_work_time,
  long long *last_work_timer_start
) {
  for (int i = size / 2 - 1; i >= 0; --i) {
    int j = i;
    while (true) {
      int left = 2 * j + 1;
      int right = 2 * j + 2;
      int largest = j;
      if (left < size && array[left] > array[largest]) {
        largest = left;
      }
      if (right < size && array[right] > array[largest]) {
        largest = right;
      }
      if (largest == j) {
        break;
      }
      int tmp = array[j];
      array[j] = array[largest];
      array[largest] = tmp;
      j = largest;

      yield_if_necessary_record_work_time(
        total_work_time,
        last_work_timer_start
      );
    }
  }
  for (int i = size - 1; i > 0; --i) {
    int tmp1 = array[0];
    array[0] = array[i];
    array[i] = tmp1;
    int j = 0;
    while (true) {
      int left = 2 * j + 1;
      int right = 2 * j + 2;
      int largest = j;
      if (left < i && array[left] > array[largest]) {
        largest = left;
      }
      if (right < i && array[right] > array[largest]) {
        largest = right;
      }
      if (largest == j) {
        break;
      }
      int tmp2 = array[j];
      array[j] = array[largest];
      array[largest] = tmp2;
      j = largest;

      yield_if_necessary_record_work_time(
        total_work_time,
        last_work_timer_start
      );
    }
  }
}

/**
 * Coroutine body, which sorts the files.
 * This code is executed by all the coroutines.
 */
static int coroutine_func_f(void *context) {
  long long work_timer_last_start = get_now();
  struct coro_context *ctx = context;
  struct coro *this = coro_this();

  while (global_file_to_sort_idx < global_files_count) {
    // "Pick" the file to sort.
    int taken_file_idx = global_file_to_sort_idx;
    ++global_file_to_sort_idx;
    char *filename = global_filenames_to_sort[taken_file_idx];

    // Open the file.
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
      printf("Error opening file %s\n", filename);
      exit(-1);
    }

    // Count the number of numbers in the file.
    int numbers_count = 0;
    while (!feof(file)) {
      int tmp;
      fscanf(file, "%d", &tmp);
      ++numbers_count;
    }

    yield_if_necessary_record_work_time(
      &ctx->total_work_time,
      &work_timer_last_start
    );

    // Allocate memory for the numbers.
    int *numbers = malloc(sizeof(int) * numbers_count);

    global_sorted_arrays[taken_file_idx] = numbers;
    global_sorted_arrays_sizes[taken_file_idx] = numbers_count;

    // Read the numbers from the file.
    rewind(file);
    for (int i = 0; i < numbers_count; ++i) {
      fscanf(file, "%d", &numbers[i]);
    }

    yield_if_necessary_record_work_time(
      &ctx->total_work_time,
      &work_timer_last_start
    );

    // Sort the numbers with yielding.
    heap_sort(
      numbers,
      numbers_count,
      &ctx->total_work_time,
      &work_timer_last_start
    );
  }

  ctx->total_work_time += get_now() - work_timer_last_start;
  ctx->total_switch_count = coro_switch_count(this);

  return 0;
}

void print_usage(char *program_name) {
  printf(
    "Usage: %s [-n <number of coroutines>] [-t <target latency>] file1 ...",
    program_name
  );
}

int main(int argc, char **argv) {
  long long start_time = get_now();

  long target_latency = DEFAULT_TARGET_LATENCY;
  long coroutines_count = DEFAULT_COROUTINES;

  /* Parse CLI arguments. */
  bool args_parsed = true;
  int files_count = argc - 1;
  for (int i = 1; i < argc; ++i) {
    bool is_last_arg = i + 1 == argc;
    if (strcmp(argv[i], "-n") == 0) {
      if (is_last_arg) {
        args_parsed = false;
        break;
      }
      coroutines_count = strtol(argv[i + 1], NULL, 10);
      files_count -= 2;
    } else if (strcmp(argv[i], "-t") == 0) {
      if (is_last_arg) {
        args_parsed = false;
        break;
      }
      target_latency = strtol(argv[i + 1], NULL, 10);
      files_count -= 2;
    }
  }
  if (coroutines_count < 1 || target_latency < coroutines_count || files_count < 1) {
    args_parsed = false;
  }
  if (!args_parsed) {
    print_usage(argv[0]);
    return 1;
  }

  global_coroutine_quantum = target_latency / coroutines_count;
  printf(
    "Sorting %d files, with %ld coroutines, each with %lldμs quantum...\n\n",
    files_count,
    coroutines_count,
    global_coroutine_quantum
  );

  global_files_count = files_count;
  global_filenames_to_sort = argv + (argc - files_count);
  global_file_to_sort_idx = 0;
  global_sorted_arrays = malloc(sizeof(int *) * files_count);
  global_sorted_arrays_sizes = malloc(sizeof(size_t) * files_count);

  /* Initialize our coroutine global cooperative scheduler. */
  coro_sched_init();

  struct coro_context *contexts = malloc(sizeof(struct coro_context) * coroutines_count);

  /* Start several coroutines. */
  for (int i = 0; i < coroutines_count; ++i) {
    // Create context for the coroutine.
    struct coro_context *ctx = &contexts[i];
    char name[16];
    sprintf(name, "coro#%d", i + 1);
    ctx->name = strdup(name);
    ctx->total_work_time = 0;
    ctx->total_switch_count = 0;

    printf("Starting coroutine %s...\n", ctx->name);

    // Start the coroutine.
    coro_new(coroutine_func_f, ctx);
  }

  /* Wait for all the coroutines to end. */
  struct coro *c;
  while ((c = coro_sched_wait()) != NULL) {
    coro_delete(c);
  }
  printf("All coroutines finished\n\n");

  /* All coroutines have finished. */

  long long coroutines_total_work_time = 0;
  for (int i = 0; i < coroutines_count; ++i) {
    struct coro_context *ctx = &contexts[i];
    printf(
      "Coroutine %s\n"
      "  total work time %lldμs\n"
      "  total switch count %lld\n",
      ctx->name,
      ctx->total_work_time,
      ctx->total_switch_count
    );
    coroutines_total_work_time += ctx->total_work_time;
    free(ctx->name);
  }
  free(contexts);

  // Open the output file.
  FILE *output_file = fopen(OUTPUT_FILE, "w");
  if (output_file == NULL) {
    fprintf(stderr, "Failed to open the output file %s\n", OUTPUT_FILE);
    return 1;
  }

  // Merge sorted arrays into the output file.
  size_t *current_indices = malloc(sizeof(size_t) * files_count);
  for (int i = 0; i < files_count; ++i) {
    current_indices[i] = 0;
  }

  bool first = true;
  while (true) {
    if (!first) {
      fprintf(output_file, " ");
    }
    int min_value = INT_MAX;
    int min_value_idx = -1;
    for (int i = 0; i < files_count; ++i) {
      if (current_indices[i] < global_sorted_arrays_sizes[i]) {
        int value = global_sorted_arrays[i][current_indices[i]];
        if (value < min_value) {
          min_value = value;
          min_value_idx = i;
        }
      }
    }
    if (min_value_idx == -1) {
      break;
    }
    fprintf(output_file, "%d", min_value);
    ++current_indices[min_value_idx];
    first = false;
  }

  // Close the output file.
  fclose(output_file);

  // Free memory.
  free(current_indices);
  for (int i = 0; i < files_count; ++i) {
    free(global_sorted_arrays[i]);
  }
  free(global_sorted_arrays);
  free(global_sorted_arrays_sizes);

  long long total_work_time = get_now() - start_time;

  printf("\n");
  printf("Total program execution time = %lldμs\n", total_work_time);
  printf("Coroutines total execution time = %lldμs\n", coroutines_total_work_time);

  return 0;
}
