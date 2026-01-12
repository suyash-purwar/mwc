#ifndef MWC
#define MWC

typedef struct {
    int fd;
    off_t offset;
    ssize_t size;
} FileChunkData;

long mwc(const char *);
#endif
