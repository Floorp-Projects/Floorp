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
 * Intramodule declarations.
 */

#ifndef __xpidl_h
#define __xpidl_h

#include <glib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libIDL/IDL.h>
#include <assert.h>
#include <stdlib.h>

/*
 * Internal operation flags.
 */
extern gboolean enable_debug;
extern gboolean enable_warnings;
extern gboolean verbose_mode;
extern gboolean generate_docs;
extern gboolean generate_typelib;
extern gboolean generate_headers;
extern gboolean generate_nothing;

typedef struct IncludePathEntry {
    char *directory;
    struct IncludePathEntry *next;
} IncludePathEntry;

typedef struct {
    FILE *file;
    char *basename;
    IDL_ns ns;
    IDL_tree tree;
    int mode;
    GHashTable *includes;
    IncludePathEntry *include_path;
} TreeState;

#define TREESTATE_HEADER	0
#define TREESTATE_TYPELIB	1
#define TREESTATE_DOC		2
#define TREESTATE_NUM		3
 
/*
 * A function to handle an IDL_tree type.
 */
typedef gboolean (*nodeHandler)(TreeState *);

/*
 * An array of vectors of nodeHandlers, for handling each kind of node.
 */
extern nodeHandler *nodeDispatch[TREESTATE_NUM];

extern nodeHandler *headerDispatch();
extern nodeHandler *typelibDispatch();
extern nodeHandler *docDispatch();

/*
 * nodeHandler that reports an error.
 */
gboolean node_is_error(TreeState *state);

/*
 * Process an IDL file, generating InterfaceInfo, documentation and headers as
 * appropriate.
 */
int
xpidl_process_idl(char *filename, IncludePathEntry *include_path,
                  char *basename);

/*
 * Iterate over an IDLN_LIST -- why is this not part of libIDL?
 */
void
xpidl_list_foreach(IDL_tree p, IDL_tree_func foreach, gpointer user_data);

/*
 * Add an output file to an internal list.  Used to clean up temporary files
 * in case of fatal error.
 */

void XPIDL_add_output_file(char *fn);
void XPIDL_cleanup_on_error();

gboolean process_node(TreeState *state);


#endif /* __xpidl_h */
