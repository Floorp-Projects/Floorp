/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Main xpidl program entry point.
 */

#include "xpidl.h"

static ModeData modes[] = {
    {"header",  "Generate C++ header",         "h",    xpidl_header_dispatch},
    {"typelib", "Generate XPConnect typelib",  "xpt",  xpidl_typelib_dispatch},
    {"doc",     "Generate HTML documentation", "html", xpidl_doc_dispatch},
    {"java",    "Generate Java interface",     "java", xpidl_java_dispatch},
    {0,         0,                             0,      0}
};

static ModeData *
FindMode(char *mode)
{
    int i;
    for (i = 0; modes[i].mode; i++) {
        if (!strcmp(modes[i].mode, mode))
            return &modes[i];
    }
    return NULL;
}

gboolean enable_debug             = FALSE;
gboolean enable_warnings          = FALSE;
gboolean verbose_mode             = FALSE;
gboolean emit_typelib_annotations = FALSE;
gboolean explicit_output_filename = FALSE;

static char xpidl_usage_str[] =
"Usage: %s [-m mode] [-w] [-v]\n"
"          [-I path] [-o basename | -e filename.ext] filename.idl\n"
"       -a emit annotations to typelib\n"
"       -w turn on warnings (recommended)\n"
"       -v verbose mode (NYI)\n"
"       -I add entry to start of include path for ``#include \"nsIThing.idl\"''\n"
"       -o use basename (e.g. ``/tmp/nsIThing'') for output\n"
"       -e use explicit output filename\n"
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

#if defined(XP_MAC) && defined(XPIDL_PLUGIN)
#define main xpidl_main
int xpidl_main(int argc, char *argv[]);
#endif

int main(int argc, char *argv[])
{
    int i;
    IncludePathEntry *inc, *inc_head, **inc_tail;
    char *file_basename = NULL;
    ModeData *mode = NULL;

    /* turn this on for extra checking of our code */
/*    IDL_check_cast_enable(TRUE); */

    inc_head = xpidl_malloc(sizeof *inc);
#ifndef XP_MAC
    inc_head->directory = ".";
#else
    inc_head->directory = "";
#endif
    inc_head->next = NULL;
    inc_tail = &inc_head->next;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-')
            break;
        switch (argv[i][1]) {
          case '-':
            argc++;             /* pretend we didn't see this */
            /* fall through */
          case 0:               /* - is a legal input filename (stdin)  */
            goto done_options;
          case 'a':
            emit_typelib_annotations = TRUE;
            break;
          case 'w':
            enable_warnings = TRUE;
            break;
          case 'v':
            verbose_mode = TRUE;
            break;
          case 'I':
            if (argv[i][2] == '\0' && i == argc) {
                fputs("ERROR: missing path after -I\n", stderr);
                xpidl_usage(argc, argv);
                return 1;
            }
            inc = xpidl_malloc(sizeof *inc);
            if (argv[i][2] == '\0') {
                /* is it the -I foo form? */
                inc->directory = argv[++i];
            } else {
                /* must be the -Ifoo form.  Don't preincrement i. */
                inc->directory = argv[i] + 2;
            }
#ifdef DEBUG_shaver_includes
            fprintf(stderr, "adding %s to include path\n", inc->directory);
#endif
            inc->next = NULL;
            *inc_tail = inc;
            inc_tail = &inc->next;
            break;
          case 'o':
            if (i == argc) {
                fprintf(stderr, "ERROR: missing basename after -o\n");
                xpidl_usage(argc, argv);
                return 1;
            }
            file_basename = argv[++i];
            explicit_output_filename = FALSE;
            break;
          case 'e':
            if (i == argc) {
                fprintf(stderr, "ERROR: missing basename after -e\n");
                xpidl_usage(argc, argv);
                return 1;
            }
            file_basename = argv[++i];
            explicit_output_filename = TRUE;
            break;
          case 'm':
            if (i == argc) {
                fprintf(stderr, "ERROR: missing modename after -m\n");
                xpidl_usage(argc, argv);
                return 1;
            }
            if (mode) {
                fprintf(stderr,
                        "ERROR: must specify exactly one mode "
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
    if (argc != i + 1) {
        fprintf(stderr, "ERROR: extra arguments after input file\n");
    }

    /*
     * Don't try to process multiple files, given that we don't handle -o
     * multiply.
     */
    if (xpidl_process_idl(argv[i], inc_head, file_basename, mode))
        return 0;

    return 1;
}
