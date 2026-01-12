#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include "cores.h"
#include "mwc.h"

#define READ_BUFFER_SIZE 4096

long file_size(const char *filepath) {
    struct stat file_stat;

    if (stat(filepath, &file_stat)) {
        perror("Failed to read file size.");
        exit(-1);
    }

    return file_stat.st_size;
}

long count_words(const char *buffer, const size_t read_size, const char prev_character, const off_t offset) {
    char inside_a_word = 0;
    long count = 0;

    if (offset != 0 && !isspace(prev_character))
        inside_a_word = 1;

    for (int i = 0; i < read_size; i++) {
        const char character = buffer[i];

        if (isspace(character)) {
            inside_a_word = 0;
        } else {
            if (inside_a_word == 0) {
                count++;
                inside_a_word = 1;
            }
        }
    }

    return count;
}

/**
 * chunk_start = 300 kb;
 * buffer_size = 64 kb;
 * offset = 0;
 *
 * bytes_read = 30 kb;
 * bytes_to_read = (chunk_size - bytes_read) > buffer_size ? buffer_size : (chunk_size - bytes_read)
 * offset += bytes_read
 * bytes_read += bytes_to_read
 */

void* worker(void *worker_arg) {
    const FileChunkData *args = (FileChunkData *) worker_arg;

    char buffer[READ_BUFFER_SIZE];
    size_t bytes_read = 0;
    long* word_count = calloc(1, sizeof(long));

    if (word_count == NULL) {
        perror("Failed to allocate memory");
        exit(-1);
    }

    const int fd = args -> fd;
    off_t offset = args -> offset;
    const ssize_t chunk_size = args -> size;

    char prev_buffer_character;

    if (offset != 0) {
        if (pread(fd, &prev_buffer_character, 1, offset - 1) == -1) {
            perror("Failed to read file");
            exit(-1);
        }
    }

    while (bytes_read < chunk_size) {
        size_t remaining_bytes_to_be_ready = chunk_size - bytes_read;
        size_t bytes_to_read = remaining_bytes_to_be_ready > READ_BUFFER_SIZE
                                   ? READ_BUFFER_SIZE
                                   : remaining_bytes_to_be_ready;

        if (pread(fd, buffer, bytes_to_read, offset) == -1) {
            perror("Failed to read file");
            exit(-1);
        }

        *word_count += count_words(buffer, bytes_to_read, prev_buffer_character, offset);

        offset += bytes_to_read;
        bytes_read += bytes_to_read;
        prev_buffer_character = buffer[bytes_to_read - 1];
    }

    free(worker_arg);

    return word_count;
}

pthread_t start_worker_thread(const int fd, const off_t offset, const ssize_t size) {
    FileChunkData *worker_data = malloc(sizeof(FileChunkData));

    if (worker_data == NULL) {
        perror("Failed to allocate memory");
        exit(-1);
    }

    worker_data->fd = fd;
    worker_data->offset = offset;
    worker_data->size = size;

    pthread_t thread;
    pthread_attr_t thread_attr;

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);

    if (pthread_create(&thread, &thread_attr, worker, worker_data)) {
        perror("Thread creation failed.");
        exit(-1);
    }

    return thread;
}

long mwc(const char *file_path) {
    const int fd = open(file_path, O_RDONLY);

    if (fd == -1) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    const int cores = cpu_cores_count();
    const long fsize = file_size(file_path);
    long word_count = 0;

    pthread_t *threads_array = malloc(sizeof(pthread_t) * cores);

    if (threads_array == NULL) {
        perror("Failed to allocate memory");
        exit(-1);
    }

    const int file_chunk_size = floor(fsize / cores);
    const int last_chunk_size = file_chunk_size + fsize % cores;

    off_t offset = 0;

    for (int i = 0; i < cores; i++) {
        const int byte_read_size = i == cores - 1 ? last_chunk_size : file_chunk_size;

        threads_array[i] = start_worker_thread(fd, offset, byte_read_size);
        offset += byte_read_size;
    }

    for (int j = 0; j < cores; j++) {
        void *thread_local_word_count;

        if (pthread_join(threads_array[j], &thread_local_word_count)) {
            perror("Failed to join the thread.");
            exit(-1);
        }
        word_count += *(long*)thread_local_word_count;
        free(thread_local_word_count);
    }

    close(fd);
    free(threads_array);

    return word_count;
}
