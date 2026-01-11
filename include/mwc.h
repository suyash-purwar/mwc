#ifndef MWC
#define MWC

typedef struct {
    FILE *file;
    off_t offset;
    ssize_t size;
} FileChunkData;

long mwc(const char *);
#endif
