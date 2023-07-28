#include "userfs.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum {
  BLOCK_SIZE = 512,
  MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/**
 * Global error code.
 * Set from any function on any error.
 */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
  /**
   * Pointer to the allocated memory block.
   */
  char *memory;

  /**
   * How many bytes in the block are occupied.
   */
  int occupied;

  /**
   * Next block in the file. NULL if it is the last block.
   */
  struct block *next;

  /**
   * Previous block in the file. NULL if it is the first block.
   */
  struct block *prev;

  /* PUT HERE OTHER MEMBERS */
};

struct file {
  /**
   * Pointer to the first block in the file.
   */
  struct block *first_block;

  /**
   * Pointer to the last block in the file.
   */
  struct block *last_block;

  /**
   * Number of file descriptors that are using the file.
   */
  int refs;

  /**
   * Size of the file in bytes.
   */
  size_t size;

  /**
   * File name.
   */
  char *name;

  /**
   * Pointer to the next file. NULL if it is the last file in the list.
   */
  struct file *next;

  /**
   * Pointer to the previous file. NULL if it is the first file in the list.
   */
  struct file *prev;

  /* PUT HERE OTHER MEMBERS */
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
  /**
   * Pointer to the file of the descriptor.
   */
  struct file *file;

  /**
   * Code bitwise combination of flags, with which the file was open.
   */
  int open_flags;

  /**
   * Current offset in the file (0 means first byte).
   */
  size_t offset;

  /* PUT HERE OTHER MEMBERS */
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
// static int file_descriptor_count = 0; RENAMED TO
static int file_descriptor_used = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code ufs_errno() { return ufs_error_code; }

void _ufs_free_file(struct file *file) {
  if (file->name != NULL) {
    free(file->name);
  }

  for (struct block *block = file->first_block; block != NULL;) {
    struct block *next = block->next;
    free(block->memory);
    free(block);
    block = next;
  }

  free(file);
}

int ufs_open(const char *filename, int flags) {
  if (!(flags & UFS_READ_ONLY) && !(flags & UFS_WRITE_ONLY)) {
    flags |= UFS_READ_WRITE;
  }

  struct file *file = NULL;

  for (file = file_list; file != NULL; file = file->next) {
    if (strcmp(file->name, filename) == 0) {
      // Found the file
      break;
    }
  }

  if (file == NULL) {
    if (!(flags & UFS_CREATE)) {
      // File not found and UFS_CREATE flag is not set
      ufs_error_code = UFS_ERR_NO_FILE;
      return -1;
    }

    // Create a new file
    file = malloc(sizeof(struct file));
    file->name = malloc(strlen(filename) + 1);
    strcpy(file->name, filename);
    file->refs = 0;
    file->size = 0;
    file->first_block = NULL;
    file->last_block = NULL;
    if (file_list == NULL) {
      // This is the first file in the list
      file->next = NULL;
      file->prev = NULL;
      file_list = file;
    } else {
      // This is not the first file in the list, prepend it to the beginning
      file->next = file_list;
      file->prev = NULL;
      file_list->prev = file;
      file_list = file;
    }
  }

  if (file_descriptor_capacity <= 0) {
    // There is no file descriptors yet, initialize them
    file_descriptors = malloc(sizeof(struct filedesc *) * 10);

    // Set all file descriptors to NULL
    for (int i = 0; i < 10; i++) {
      file_descriptors[i] = NULL;
    }

    file_descriptor_capacity = 10;
    file_descriptor_used = 0;
  }

  int idx = -1;

  if (file_descriptor_used < file_descriptor_capacity) {
    // Find the available one
    for (int i = 0; i < file_descriptor_capacity; i++) {
      if (file_descriptors[i] == NULL) {
        // Found an available file descriptor
        idx = i;
        break;
      }
    }

    if (idx < 0) {
      // Error: no available file descriptor, but it MUST be
      ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
      return -1;
    }
  } else {
    // No more available file descriptors, need to expand the array
    file_descriptors =
        realloc(file_descriptors,
                sizeof(struct filedesc *) * (file_descriptor_capacity * 2));
    file_descriptor_capacity *= 2;
    idx = file_descriptor_used;
  }

  file_descriptors[idx] = malloc(sizeof(struct filedesc));
  file_descriptor_used++;
  file_descriptors[idx]->open_flags = flags;
  file_descriptors[idx]->offset = 0;
  file_descriptors[idx]->file = file;
  file->refs++;

  return idx + 1;  // Return the file descriptor number (i.e. idx + 1)
}

ssize_t ufs_write(int fd, const char *buf, size_t size) {
  int desc_idx = fd - 1;
  if (desc_idx < 0 || desc_idx >= file_descriptor_capacity) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  struct filedesc *desc = file_descriptors[desc_idx];
  if (desc == NULL) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  if (!(desc->open_flags & UFS_WRITE_ONLY) &&
      !(desc->open_flags & UFS_READ_WRITE)) {
    // File is opened for reading
    ufs_error_code = UFS_ERR_NO_PERMISSION;
    return -1;
  }

  if (size == 0) {
    // Nothing to write
    return 0;
  }

  if (desc->file->size + size > MAX_FILE_SIZE) {
    // Max file size exceeded
    ufs_error_code = UFS_ERR_NO_MEM;
    return -1;
  }

  struct block *block = desc->file->first_block;
  for (size_t i = 0; i < desc->offset / BLOCK_SIZE; i++) {
    block = block->next;
  }

  size_t bytes_written = 0;
  while (bytes_written < size) {
    size_t write_from = desc->offset % BLOCK_SIZE;

    if (block == NULL || block->occupied == BLOCK_SIZE) {
      // Create a new block
      block = malloc(sizeof(struct block));
      block->memory = malloc(BLOCK_SIZE);
      block->next = NULL;
      block->occupied = 0;
      block->prev = desc->file->last_block;
      if (desc->file->last_block == NULL) {
        // This is the first block
        desc->file->first_block = block;
        desc->file->last_block = block;
      } else {
        // Append the block to the end of the file
        desc->file->last_block->next = block;
        desc->file->last_block = block;
      }
    }

    size_t bytes_to_write = size - bytes_written;
    if (bytes_to_write > BLOCK_SIZE - write_from) {
      bytes_to_write = BLOCK_SIZE - write_from;
    }

    memcpy(block->memory + write_from, buf + bytes_written,
           bytes_to_write);
    if ((int)(write_from + bytes_to_write) > block->occupied) {
      // Update the occupied size
      block->occupied = (int)(write_from + bytes_to_write);
    }
    bytes_written += bytes_to_write;
    desc->offset += bytes_to_write;
  }

  if (desc->offset > desc->file->size) {
    // Update the file size
    desc->file->size = desc->offset;
  }

  return (ssize_t)bytes_written;
}

ssize_t ufs_read(int fd, char *buf, size_t size) {
  int desc_idx = fd - 1;
  if (desc_idx < 0 || desc_idx >= file_descriptor_capacity) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  struct filedesc *desc = file_descriptors[desc_idx];
  if (desc == NULL) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  if (!(desc->open_flags & UFS_READ_ONLY) &&
      !(desc->open_flags & UFS_READ_WRITE)) {
    // File is opened for writing
    ufs_error_code = UFS_ERR_NO_PERMISSION;
    return -1;
  }

  struct block *block = desc->file->first_block;
  for (size_t i = 0; i < desc->offset / BLOCK_SIZE; i++) {
    if (block == NULL) {
      // We've reached the end of the file
      return 0;
    }
    block = block->next;
  }

  if (desc->offset >= desc->file->size) {
    // We've reached the end of the file
    return 0;
  }

  ssize_t read_count = 0;
  while (block != NULL && read_count < (ssize_t)size) {
    int block_offset = (int)(desc->offset) % BLOCK_SIZE;

    if (block_offset - block->occupied >= 0) {
      // We've reached the end of the file
      return read_count;
    }

    size_t bytes_to_copy = block->occupied - block_offset;
    if (bytes_to_copy > size - read_count) {
      bytes_to_copy = size - read_count;
    }

    memcpy(buf + read_count, block->memory + block_offset, bytes_to_copy);
    read_count += (ssize_t)(bytes_to_copy);
    desc->offset += bytes_to_copy;
    block = block->next;
  }

  return read_count;
}

int ufs_close(int fd) {
  int desc_idx = fd - 1;
  if (desc_idx < 0 || desc_idx >= file_descriptor_capacity) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  struct filedesc *desc = file_descriptors[desc_idx];
  if (desc == NULL) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  desc->file->refs--;
  if (desc->file->refs <= 0 && file_list != desc->file &&
      desc->file->prev == NULL && desc->file->next == NULL) {
    // No more file descriptors and no any references to the file, delete it
    _ufs_free_file(desc->file);
  }

  free(desc);
  file_descriptors[desc_idx] = NULL;
  file_descriptor_used--;

  return 0;
}

int ufs_delete(const char *filename) {
  struct file *file = NULL;
  for (file = file_list; file != NULL; file = file->next) {
    if (strcmp(file->name, filename) == 0) {
      // Found the file
      break;
    }
  }

  if (file == NULL) {
    // File not found
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  // Remove file from list
  if (file->prev != NULL) {
    file->prev->next = file->next;
  }
  if (file->next != NULL) {
    file->next->prev = file->prev;
  }
  if (file_list == file) {
    file_list = file->next;
  }

  if (file->refs <= 0) {
    // No file descriptor is opened
    _ufs_free_file(file);
  }

  return 0;
}

void ufs_destroy(void) {
  for (int i = 0; i < file_descriptor_capacity; i++) {
    if (file_descriptors[i] != NULL) {
      free(file_descriptors[i]);
    }
  }
  file_descriptors = NULL;
  file_descriptor_capacity = 0;
  file_descriptor_used = 0;

  for (struct file *file = file_list; file != NULL;) {
    struct file *next = file->next;
    _ufs_free_file(file);
    file = next;
  }
  file_list = NULL;
}
