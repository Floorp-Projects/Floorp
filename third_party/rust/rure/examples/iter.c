/*
 * This example code shows how to iterate over all regex matches in a file,
 * emit the match location and print the contents of a capturing group.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "rure.h"

int main() {
    /* Open a file and mmap it. */
    int fd = open("sherlock.txt", O_RDONLY);
    if (fd == -1) {
        perror("failed to open sherlock.txt");
        exit(1);
    }
    struct stat status;
    if (fstat(fd, &status) == -1) {
        perror("failed to stat sherlock.txt");
        exit(1);
    }
    if ((uintmax_t)status.st_size > SIZE_MAX) {
        perror("file too big");
        exit(1);
    }
    if (status.st_size == 0) {
        perror("file empty");
        exit(1);
    }
    size_t sherlock_len = (size_t)status.st_size;
    const uint8_t *sherlock = (const uint8_t *)mmap(
        NULL, status.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (sherlock == MAP_FAILED) {
        perror("could not mmap file");
        exit(1);
    }

    /*
     * Compile the regular expression. A more convenient routine,
     * rure_compile_must, is also available, which will abort the process if
     * and print an error message to stderr if the regex compilation fails.
     * We show the full gory details here as an example.
     */
    const char *pattern = "(\\w+)\\s+Holmes";
    size_t pattern_len = strlen(pattern);
    rure_error *err = rure_error_new();
    rure *re = rure_compile((const uint8_t *)pattern, pattern_len,
                            RURE_FLAG_UNICODE | RURE_FLAG_CASEI, NULL, err);
    if (NULL == re) {
        /* A null regex means compilation failed and an error exists. */
        printf("compilation of %s failed: %s\n",
               pattern, rure_error_message(err));
        rure_error_free(err);
        munmap((char*)sherlock, sherlock_len);
        exit(1);
    }
    rure_error_free(err);

    /*
     * Create an iterator to find all successive non-overlapping matches.
     * For each match, we extract the location of the capturing group.
     */
    rure_match group0 = {0};
    rure_match group1 = {0};
    rure_captures *caps = rure_captures_new(re);
    rure_iter *it = rure_iter_new(re);

    while (rure_iter_next_captures(it, sherlock, sherlock_len, caps)) {
        /*
         * Get the location of the full match and the capturing group.
         * We know that both accesses are successful since the body of the
         * loop only executes if there is a match and both capture groups
         * must match in order for the entire regex to match.
         *
         * N.B. The zeroth group corresponds to the full match of the regex.
         */
        rure_captures_at(caps, 0, &group0);
        rure_captures_at(caps, 1, &group1);
        printf("%.*s (match at: %zu, %zu)\n",
               (int)(group1.end - group1.start),
               sherlock + group1.start,
               group0.start, group0.end);
    }

    /* Free all our resources. */
    munmap((char*)sherlock, sherlock_len);
    rure_captures_free(caps);
    rure_iter_free(it);
    rure_free(re);
    return 0;
}
