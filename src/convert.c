#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "uproc.h"

#if HAVE_MMAP
#define FORMATS "PBM"
#else
#define FORMATS "PB"
#endif

void
print_time(FILE *stream, unsigned seconds)
{
    fprintf(stream, "elapsed time: ");
    if (seconds >= 60) {
        fprintf(stream, "%um", seconds / 60);
        seconds %= 60;
    }
    fprintf(stream, "%us\n", seconds);
}

enum args
{
    IN_FMT = 1,
    IN_PATH,
    OUT_FMT,
    OUT_PATH,
    ARGC
};

int
main(int argc, char **argv)
{
    int res = UPROC_FAILURE;
    struct uproc_ecurve ecurve;
    time_t start, end;
    char in_fmt, out_fmt;
    enum uproc_io_type iotype = UPROC_IO_STDIO;

    if (argc != ARGC) {
        fprintf(stderr, "%d %d\n", argc, ARGC);
        fprintf(stderr,
                "usage: %s in_format in_filename out_format out_filename\n"
                "Available formats:\n"
                "   P[Z]   plain text\n"
                "   B[Z]   binary\n"
#if HAVE_MMAP
                "   M   mmap()-able\n"
#endif
                ,
                argv[0]);
        return EXIT_FAILURE;
    }
    in_fmt = argv[IN_FMT][0];
    out_fmt = argv[OUT_FMT][0];
    if (argv[OUT_FMT][1] == 'Z') {
        iotype = UPROC_IO_GZIP;
    }

    if (!strchr(FORMATS, in_fmt)) {
        fprintf(stderr, "invalid input format: %c\n", *argv[IN_FMT]);
        return EXIT_FAILURE;
    }
    if (!strchr(FORMATS, out_fmt)) {
        fprintf(stderr, "invalid output format: %c\n", *argv[OUT_FMT]);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "loading %s...\n", argv[IN_PATH]);
    start = time(NULL);
    switch (in_fmt) {
        case 'P':
            res = uproc_storage_load(&ecurve, UPROC_STORAGE_PLAIN, UPROC_IO_GZIP,
                                    argv[IN_PATH]);
            break;
        case 'B':
            res = uproc_storage_load(&ecurve, UPROC_STORAGE_BINARY, UPROC_IO_GZIP,
                                    argv[IN_PATH]);
            break;
        case 'M':
            res = uproc_mmap_map(&ecurve, argv[IN_PATH]);
            break;
    }
    end = time(NULL);
    if (res != UPROC_SUCCESS) {
        fprintf(stderr, "cannot load ecurve!\n");
        perror("");
        return EXIT_FAILURE;
    }
    print_time(stderr, end - start);

    fprintf(stderr, "storing to %s...\n", argv[OUT_PATH]);
    start = time(NULL);
    switch (out_fmt) {
        case 'P':
            res = uproc_storage_store(&ecurve, UPROC_STORAGE_PLAIN, iotype,
                                     argv[OUT_PATH]);
            break;
        case 'B':
            res = uproc_storage_store(&ecurve, UPROC_STORAGE_BINARY, iotype,
                                     argv[OUT_PATH]);
            break;
        case 'M':
            res = uproc_mmap_store(&ecurve, argv[OUT_PATH]);
            break;
    }
    end = time(NULL);
    if (res != UPROC_SUCCESS) {
        fprintf(stderr, "error storing ecurve\n");
        perror("");
        return EXIT_FAILURE;
    }
    fprintf(stderr, "elapsed time: ");
    print_time(stderr, end - start);

    uproc_ecurve_destroy(&ecurve);
    return EXIT_SUCCESS;
}
