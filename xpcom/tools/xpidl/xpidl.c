/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * Main xpidl program entry point.
 */

#include "xpidl.h"

static ModeData modes[] = {
    {"header",  "Generate C++ header",         "h",    NULL },
    {"typelib", "Generate XPConnect typelib",  "xpt",  NULL },
    {"doc",     "Generate HTML documentation", "html", NULL },
    {0}
};

static ModeData *
FindMode(char *mode)
{
    int i;
    for (i = 0; modes[i].mode && strcmp(modes[i].mode, mode); i++)
        ;
    if (modes[i].mode)
        return &modes[i];
    return NULL;
}    

gboolean enable_debug      = FALSE;
gboolean enable_warnings   = FALSE;
gboolean verbose_mode      = FALSE;

static char xpidl_usage_str[] = 
"Usage: %s [-m mode] [-w] [-v] [-I path] [-n] [-o basename] filename.idl\n"
"       -w turn on warnings (recommended)\n"
"       -v verbose mode (NYI)\n"
"       -I add entry to start of include path for ``#include \"nsIThing.idl\"''\n"
"       -o use basename (e.g. ``/tmp/nsIThing'') for output\n"
"       -m specify output mode:\n";

static void 
xpidl_usage(int argc, char *argv[])
{
    int i;
    fprintf(stderr, xpidl_usage_str, argv[0]);
    for (i = 0; modes[i].mode; i++) {
        fprintf(stderr, "          %-12s  %-30s (.%s)\n", modes[i].mode,
                modes[i].modeInfo, modes[i].suffix);
    }
}

int
main(int argc, char *argv[])
{
    int i, idlfiles;
    IncludePathEntry *inc, *inc_head = NULL;
    char *basename = NULL;
    ModeData *mode = NULL;

    inc_head = malloc(sizeof *inc);
    if (!inc_head)
        return 1;
    inc_head->directory = ".";
    inc_head->next = NULL;

    /* initialize mode factories */
    modes[0].factory = headerDispatch;
    modes[1].factory = typelibDispatch;
    modes[2].factory = docDispatch;
    
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-')
            break;
        switch (argv[i][1]) {
          case '-':
            argc++;
            /* fall through */
          case 0:               /* - is a legal input filename (stdin)  */
            goto done_options;
          case 'w':
            enable_warnings = TRUE;
            break;
          case 'v':
            verbose_mode = TRUE;
            break;
          case 'I':
            if (i == argc) {
                fputs("ERROR: missing path after -I\n", stderr);
                xpidl_usage(argc, argv);
                return 1;
            }
            inc = malloc(sizeof *inc);
            if (!inc)
                return 1;
            inc->directory = argv[i + 1];
#ifdef DEBUG_shaver_includes
            fprintf(stderr, "adding %s to include path\n", inc->directory);
#endif
            inc->next = inc_head;
            inc_head = inc;
            i++;
            break;
          case 'o':
            if (i == argc) {
                fprintf(stderr, "ERROR: missing basename after -o\n");
                xpidl_usage(argc, argv);
                return 1;
            }
            basename = argv[i + 1];
            i++;
            break;
          case 'h':             /* legacy stuff, already! */
            mode = FindMode("header");
            if (!mode) {
                xpidl_usage(argc, argv);
                return 1;
            }
            break;
          case 'm':
            if (i == argc) {
                fprintf(stderr, "ERROR: missing modename after -m\n");
                xpidl_usage(argc, argv);
                return 1;
            }
            if (mode) {
                fprintf(stderr,
                        "ERROR: can only specify one mode "
                        "(first \"%s\", now \"%s\")\n", mode->mode,
                        argv[i + 1]);
                xpidl_usage(argc, argv);
                return 1;
            }
            mode = FindMode(argv[++i]);
            if (!mode) {
                fprintf(stderr, "ERROR: unknown mode \"%s\"\n", argv[i]);
                xpidl_usage(argc, argv);
                return 1;
            }
            break;

          default:
            fprintf(stderr, "unknown option %s\n", argv[i]);
            xpidl_usage(argc, argv);
            return 1;
        }
    }
 done_options:
    if (!mode) {
        fprintf(stderr, "ERROR: must specify output mode\n");
        xpidl_usage(argc, argv);
        return 1;
    }

    for (idlfiles = 0; i < argc; i++) {
        if (argv[i][0])
            idlfiles += xpidl_process_idl(argv[i], inc_head, basename, mode);
    }
    
    if (!idlfiles)
        return 1;

    return 0;
}
