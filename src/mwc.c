#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include "cores.h"
#include "mwc.h"

#define READ_BUFFER_SIZE 64000

long file_size(char *filepath)
{
    struct stat file_stat;
    stat(filepath, &file_stat);

    return file_stat.st_size;
}

int validate_input(int argc, char *argv[])
{
    if (argc >= 2 && strlen(argv[1]) > 0)
    {
        printf("Input file path in not present.\n");
        return 0;
    }

    return -1;
}

/**
 * Buffer 0: |suyash says hell|
 * Buffer 1: |o! krish says fu|
 * Buffer 2: |ck you to him!! |
 * Buffer 3: |Suyash is mad!!!|
 * Buffer 4: | What to now?   |
 */
long count_words(char *buffer, char prev_character, off_t offset)
{
    char inside_a_word = 0;
    long count = 0;

    for (int i = 0; i < READ_BUFFER_SIZE; i++)
    {
        char character = buffer[i];

        if (character == '\0')
            break;

        if (isspace(character))
        {
            inside_a_word = 0;
        }
        else
        {
            if (inside_a_word == 0)
            {
                count++;
                inside_a_word = 1;
            }
        }
    }

    if (offset != 0 && isspace(prev_character))
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

void *worker(void *worker_arg)
{
    FileChunkData *args = (FileChunkData *)worker_arg;

    char buffer[READ_BUFFER_SIZE];
    char prev_buffer_character = ' ';
    size_t bytes_read = 0;
    long word_count = 0;

    FILE *file = args->file;
    off_t offset = args->offset;
    ssize_t chunk_size = args->size;

    fseek(args->file, offset, SEEK_SET);

    while (bytes_read < chunk_size)
    {
        size_t remaining_bytes_to_be_ready = chunk_size - bytes_read;
        size_t bytes_to_read = remaining_bytes_to_be_ready > READ_BUFFER_SIZE ? READ_BUFFER_SIZE : remaining_bytes_to_be_ready;

        int fd = fileno(file);
        printf("File Descriptor: %d\n", fd);
        pread(fd, buffer, bytes_to_read, offset);

        word_count += count_words(buffer, prev_buffer_character, offset);

        offset += bytes_to_read;
        bytes_read += bytes_to_read;
        prev_buffer_character = buffer[bytes_to_read - 1];
    }

    free(worker_arg);

    return (void *)word_count;
}

pthread_t start_worker_thread(FILE *file, off_t offset, ssize_t size)
{
    FileChunkData *worker_data = (FileChunkData *)malloc(sizeof(FileChunkData));

    worker_data->file = file;
    worker_data->offset = offset;
    worker_data->size = size;

    pthread_t thread;
    pthread_attr_t thread_attr;
    void *thread_result;

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);

    if (pthread_create(&thread, &thread_attr, worker, (void *)worker_data))
    {
        perror("Thread creation failed.");
        exit(-1);
    }

    return thread;
}

int mwc(char *file_path)
{
    FILE *file = fopen(file_path, O_RDONLY);
    int cores = cpu_cores_count();
    long fsize = file_size(file_path);
    long word_count = 0;

    pthread_t *threads_array = (pthread_t *)malloc(sizeof(pthread_t) * cores);

    int file_chunk_size = floor(fsize / cores);
    int last_chunk_size = file_chunk_size + (fsize % cores);

    off_t offset = 0;

    for (int i = 0; i < cores; i++)
    {
        int byte_read_size = (i == cores - 1) ? last_chunk_size : last_chunk_size;

        threads_array[i] = start_worker_thread(file, offset, byte_read_size);
    }

    for (int i = 0; i < cores; i++)
    {
        void *thread_local_word_count;

        if (pthread_join(threads_array[i], &thread_local_word_count))
        {
            perror("Failed to join the thread.");
            exit(-1);
        }
        word_count += *(long *)thread_local_word_count;
    }

    free(threads_array);

    return word_count;
}

int main(int argc, char *argv[])
{
    if (!validate_input(argc, argv))
        return -1;

    return 0;
}
