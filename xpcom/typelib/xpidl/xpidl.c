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

gboolean enable_debug      = FALSE;
gboolean enable_warnings   = FALSE;
gboolean verbose_mode      = FALSE;
gboolean generate_docs     = FALSE;
gboolean generate_typelib  = FALSE;
gboolean generate_headers  = FALSE;
gboolean generate_nothing  = FALSE;

static char xpidl_usage_str[] = 
"Usage: %s [-t] [-d] [-h] [-w] [-v] [-I path] [-n] [-o basename] filename.idl\n"
"       -t generate typelib data       (filename.xpt) (NYI)\n"
"       -d generate HTML documentation (filename.html) (NYI)\n"
"       -h generate C++ headers	       (filename.h)\n"
"       -w turn on warnings (recommended)\n"
"       -v verbose mode (NYI)\n"
"       -I add entry to start of include path for ``#include \"nsIThing.idl\"''\n"
"       -n do not generate output files, just test IDL (NYI)\n"
"       -o use basename (e.g. ``/tmp/nsIThing'') for output\n";

static void 
xpidl_usage(int argc, char *argv[])
{
    /* XXX Mac! */
    fprintf(stderr, xpidl_usage_str, argv[0]);
}

int
main(int argc, char *argv[])
{
    int i, idlfiles;
    IncludePathEntry *inc, *inc_head = NULL;
    char *basename = NULL;

    inc_head = malloc(sizeof *inc);
    if (!inc_head)
        return 1;
    inc_head->directory = ".";
    inc_head->next = NULL;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-')
            break;
        switch (argv[i][1]) {
          case 'd':
            generate_docs = TRUE;
            break;
          case 't':
            generate_typelib = TRUE;
            break;
          case 'h':
            generate_headers = TRUE;
            break;
          case 'w':
            enable_warnings = TRUE;
            break;
          case 'v':
            verbose_mode = TRUE;
            break;
          case 'n':
            generate_nothing = TRUE;
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
                fprintf(stderr, "ERROR: missing basename after -o\n", stderr);
                xpidl_usage(argc, argv);
                return 1;
            }
            basename = argv[i + 1];
            i++;
          default:
            xpidl_usage(argc, argv);
            return 1;
        }
    }
    
    if (!(generate_docs || generate_typelib || generate_headers)) {
        fprintf(stderr, "ERROR: must specify one of -t, -d, -h\n");
        xpidl_usage(argc, argv);
        return 1;
    }

    for (idlfiles = 0; i < argc; i++) {
        if (argv[i][0] && argv[i][0] != '-')
            idlfiles += xpidl_process_idl(argv[i], inc_head, basename);
    }
    
    if (!idlfiles)
        return 1;

    return 0;
}
