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
    stat(filepath, &file_stat);

    return file_stat.st_size;
}

long count_words(const char *buffer, size_t read_size, const char prev_character, const off_t offset) {
    char inside_a_word = 0;
    long count = 0;

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

    if (buffer[0] != ' ' && offset != 0 && !isspace(prev_character))
        count--;

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

    FILE *file = args->file;
    off_t offset = args->offset;
    const ssize_t chunk_size = args->size;
    const int fd = fileno(file);

    char prev_buffer_character;

    if (offset != 0) {
        fseek(args->file, offset - 1, SEEK_SET);
        pread(fd, &prev_buffer_character, 1, offset - 1);
    }

    fseek(args->file, offset, SEEK_SET);

    while (bytes_read < chunk_size) {
        size_t remaining_bytes_to_be_ready = chunk_size - bytes_read;
        size_t bytes_to_read = remaining_bytes_to_be_ready > READ_BUFFER_SIZE
                                   ? READ_BUFFER_SIZE
                                   : remaining_bytes_to_be_ready;

        pread(fd, buffer, bytes_to_read, offset);

        *word_count += count_words(buffer, bytes_to_read, prev_buffer_character, offset);

        offset += bytes_to_read;
        bytes_read += bytes_to_read;
        prev_buffer_character = buffer[bytes_to_read - 1];
    }

    free(worker_arg);

    return word_count;
}

pthread_t start_worker_thread(FILE *file, off_t offset, ssize_t size) {
    FileChunkData *worker_data = malloc(sizeof(FileChunkData));

    worker_data->file = file;
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
    FILE *file = fopen(file_path, "r");

    if (file == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    const int cores = cpu_cores_count();
    const long fsize = file_size(file_path);
    long word_count = 0;

    pthread_t *threads_array = malloc(sizeof(pthread_t) * cores);

    const int file_chunk_size = floor(fsize / cores);
    const int last_chunk_size = file_chunk_size + fsize % cores;

    off_t offset = 0;

    for (int i = 0; i < cores; i++) {
        const int byte_read_size = i == cores - 1 ? last_chunk_size : file_chunk_size;

        threads_array[i] = start_worker_thread(file, offset, byte_read_size);
        offset += byte_read_size;
    }

    for (int j = 0; j < cores; j++) {
        void *thread_local_word_count;

        if (pthread_join(threads_array[j], &thread_local_word_count)) {
            perror("Failed to join the thread.");
            exit(-1);
        }
        word_count += *(long*)thread_local_word_count;
    }

    free(threads_array);

    return word_count;
}
