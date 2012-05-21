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

static char *program;

static void my_tmevent_handler(tmreader *tmr, tmevent *event)
{
    tmcallsite *callsite;

    switch (event->type) {
      case TM_EVENT_REALLOC:
      case TM_EVENT_MALLOC:
      case TM_EVENT_CALLOC:
        for (callsite = tmreader_callsite(tmr, event->serial);
             callsite != &tmr->calltree_root; callsite = callsite->parent) {
            fprintf(stdout, "%s +%08X (%s:%d)\n",
                    (const char*)callsite->method->graphnode.entry.value,
                    callsite->offset,
                    callsite->method->sourcefile,
                    callsite->method->linenumber);
        }
        fprintf(stdout, "\n");
    }
}

int main(int argc, char **argv)
{
    int i, j, rv;
    tmreader *tmr;
    FILE *fp;
    time_t start;

    program = *argv;

    tmr = tmreader_new(program, NULL);
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
    
    tmreader_destroy(tmr);

    exit(0);
}
