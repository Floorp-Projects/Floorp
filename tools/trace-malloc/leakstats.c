/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int  getopt(int argc, char *const *argv, const char *shortopts);
extern char *optarg;
extern int  optind;
#ifdef XP_WIN32
int optind=1;
#endif
#endif
#include <time.h>
#include "nsTraceMalloc.h"
#include "tmreader.h"
#include "prlog.h"

static char *program;

typedef struct handler_data {
    uint32 current_heapsize;
    uint32 max_heapsize;
    uint32 bytes_allocated;
    uint32 current_allocations;
    uint32 total_allocations;
    uint32 unmatched_frees;
    int finished;
} handler_data;

static void handler_data_init(handler_data *data)
{
    data->current_heapsize = 0;
    data->max_heapsize = 0;
    data->bytes_allocated = 0;
    data->current_allocations = 0;
    data->total_allocations = 0;
    data->unmatched_frees = 0;
    data->finished = 0;
}

static void handler_data_finish(handler_data *data)
{
}

static void my_tmevent_handler(tmreader *tmr, tmevent *event)
{
    handler_data *data = (handler_data*) tmr->data;

    switch (event->type) {
      case TM_EVENT_REALLOC:
        /* On Windows, original allocation could be before we overrode malloc */
        if (event->u.alloc.oldserial != 0) {
          data->current_heapsize -= event->u.alloc.oldsize;
          --data->current_allocations;
        } else {
          ++data->unmatched_frees;
          PR_ASSERT(event->u.alloc.oldsize == 0);
        }
        /* fall-through intentional */
      case TM_EVENT_MALLOC:
      case TM_EVENT_CALLOC:
        ++data->current_allocations;
        ++data->total_allocations;
        data->bytes_allocated += event->u.alloc.size;
        data->current_heapsize += event->u.alloc.size;
        if (data->current_heapsize > data->max_heapsize)
            data->max_heapsize = data->current_heapsize;
        break;
      case TM_EVENT_FREE:
        /* On Windows, original allocation could be before we overrode malloc */
        if (event->serial != 0) {
          --data->current_allocations;
          data->current_heapsize -= event->u.alloc.size;
        } else {
          ++data->unmatched_frees;
          PR_ASSERT(event->u.alloc.size == 0);
        }
        break;
      case TM_EVENT_STATS:
        data->finished = 1;
        break;
    }
}


int main(int argc, char **argv)
{
    int i, j, rv;
    tmreader *tmr;
    FILE *fp;
    time_t start;
    handler_data data;

    program = *argv;

    handler_data_init(&data);
    tmr = tmreader_new(program, &data);
    if (!tmr) {
        perror(program);
        exit(1);
    }

    start = time(NULL);
    fprintf(stdout, "%s starting at %s", program, ctime(&start));
    fflush(stdout);

    argc -= optind;
    argv += optind;
    if (argc == 0) {
        if (tmreader_eventloop(tmr, "-", my_tmevent_handler) <= 0)
            exit(1);
    } else {
        for (i = j = 0; i < argc; i++) {
            fp = fopen(argv[i], "r");
            if (!fp) {
                fprintf(stderr, "%s: can't open %s: %s\n",
                        program, argv[i], strerror(errno));
                exit(1);
            }
            rv = tmreader_eventloop(tmr, argv[i], my_tmevent_handler);
            if (rv < 0)
                exit(1);
            if (rv > 0)
                j++;
            fclose(fp);
        }
        if (j == 0)
            exit(1);
    }
    
    if (!data.finished) {
        fprintf(stderr, "%s: log file incomplete\n", program);
        exit(1);
    }

    fprintf(stdout,
            "Leaks: %u bytes, %u allocations\n"
            "Maximum Heap Size: %u bytes\n"
            "%u bytes were allocated in %u allocations.\n",
            data.current_heapsize, data.current_allocations,
            data.max_heapsize,
            data.bytes_allocated, data.total_allocations);
    if (data.unmatched_frees != 0)
        fprintf(stdout,
                "Logged %u free (or realloc) calls for which we missed the "
                "original malloc.\n",
                data.unmatched_frees);

    handler_data_finish(&data);
    tmreader_destroy(tmr);

    exit(0);
}
