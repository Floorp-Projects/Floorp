/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is nsTraceMalloc.c/bloatblame.c code, released
 * April 19, 2000.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brendan Eich, 14-April-2000
 *   L. David Baron, 2001-06-07, created leakstats.c based on bloatblame.c
 *   L. David Baron, 2004-02-19, created allocation-stats.c based
 *   on leakstats.c
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
