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
    {"header",  "Generate C++ header",         "h",    xpidl_header_dispatch},
    {"typelib", "Generate XPConnect typelib",  "xpt",  xpidl_typelib_dispatch},
    {"doc",     "Generate HTML documentation", "html", xpidl_doc_dispatch},
    {"java",    "Generate Java interface",     "java", xpidl_java_dispatch},
    {0}
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

static char xpidl_usage_str[] =
"Usage: %s [-m mode] [-w] [-v] [-I path] [-o basename] filename.idl\n"
"       -a emit annotaions to typelib\n"
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

/* XXXbe static */ char OOM[] = "ERROR: out of memory\n";

void *
xpidl_malloc(size_t nbytes)
{
    void *p = malloc(nbytes);
    if (!p) {
        fputs(OOM, stderr);
        exit(1);
    }
    return p;
}

#ifdef XP_MAC
static char *strdup(const char *c)
{
	char	*newStr = malloc(strlen(c) + 1);
	if (newStr)
	{
		strcpy(newStr, c);
	}
	return newStr;
}
#endif

char *
xpidl_strdup(const char *s)
{
    char *ns = strdup(s);
    if (!ns) {
        fputs(OOM, stderr);
        exit(1);
    }
    return ns;
}

#if defined(XP_MAC) && defined(XPIDL_PLUGIN)
#define main xpidl_main
int xpidl_main(int argc, char *argv[]);
#endif

int main(int argc, char *argv[])
{
    int i, idlfiles;
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
            file_basename = argv[i + 1];
            i++;
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

    for (idlfiles = 0; i < argc; i++)
        idlfiles += xpidl_process_idl(argv[i], inc_head, file_basename, mode);

    if (!idlfiles)
        return 1;

    return 0;
}
