/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "xpidl.h"

gboolean enable_debug      = FALSE;
gboolean enable_warnings   = FALSE;
gboolean verbose_mode      = FALSE;
gboolean generate_docs     = FALSE;
gboolean generate_invoke   = FALSE;
gboolean generate_headers  = FALSE;
gboolean generate_nothing  = FALSE;

static char xpidl_usage_str[] = 
"Usage: %s -IDHdwvn filename.idl\n"
"       -I generate Invoke glue       (filename_invoke.c)\n"
"       -D generate HTML documenation (filename.html)\n"
"       -H generate C++ headers	      (filename.h)\n"
"       -d turn on xpidl debugging\n"
"       -w turn on warnings (recommended)\n"
"       -v verbose mode\n"
"       -n do not generate output files, just test IDL\n";

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

    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    switch (argv[i][1]) {
	      case 'D':
		generate_docs = TRUE;
		break;
	      case 'I':
		generate_invoke = TRUE;
		break;
	      case 'H':
		generate_headers = TRUE;
		break;
	      case 'd':
		enable_debug = TRUE;
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
	      default:
		xpidl_usage(argc, argv);
		return 1;
	    }
	}
    }
    
    for (i = 1, idlfiles = 0; i < argc; i++) {
	if (argv[i][0] && argv[i][0] != '-')
	    idlfiles += xpidl_process_idl(argv[i]);
    }
    
    if (!idlfiles) {
	xpidl_usage(argc, argv);
	return 1;
    }

    return 0;
}
