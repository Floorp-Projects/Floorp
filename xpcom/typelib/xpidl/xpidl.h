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

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h> /* After glib.h to avoid warnings about shadowing 'index'. */

#ifndef XP_MAC
#include <libIDL/IDL.h>
#else
#include <IDL.h>
#endif

/*
 * Internal operation flags.
 */
extern gboolean enable_debug;
extern gboolean enable_warnings;
extern gboolean verbose_mode;

typedef struct TreeState TreeState;

/*
 * A function to handle an IDL_tree type.
 */
typedef gboolean (*nodeHandler)(TreeState *);

/* Function that produces a table of nodeHandlers for a given mode */
typedef nodeHandler *(*nodeHandlerFactory)();

extern nodeHandler *xpidl_header_dispatch(void);
extern nodeHandler *xpidl_typelib_dispatch(void);
extern nodeHandler *xpidl_doc_dispatch(void);
extern nodeHandler *xpidl_java_dispatch(void);

/*
 * nodeHandler that reports an error.
 */
gboolean node_is_error(TreeState *state);

typedef struct ModeData {
    char               *mode;
    char               *modeInfo;
    char               *suffix;
    nodeHandlerFactory factory;
} ModeData;

typedef struct IncludePathEntry {
    char                    *directory;
    struct IncludePathEntry *next;
} IncludePathEntry;

struct TreeState {
    FILE             *file;
    /* Maybe supplied by -o. Not related to (g_)basename from string.h or glib */
    char             *basename;
    IDL_ns           ns;
    IDL_tree         tree;
    GHashTable       *includes;
    IncludePathEntry *include_path;
    nodeHandler      *dispatch;
    void             *priv;     /* mode-private data */
};

/*
 * Process an IDL file, generating InterfaceInfo, documentation and headers as
 * appropriate.  Use file_basename instead of basename to avoid conflict
 * warnings with basename from some versions of string.h.
 */
int
xpidl_process_idl(char *filename, IncludePathEntry *include_path,
                  char *file_basename, ModeData *mode);

/*
 * Iterate over an IDLN_LIST -- why is this not part of libIDL?
 */
void
xpidl_list_foreach(IDL_tree p, IDL_tree_func foreach, gpointer user_data);

/*
 * Wrapper whines to stderr then exits after null return from malloc or strdup.
 */
void *
xpidl_malloc(size_t nbytes);

char *
xpidl_strdup(const char *s);

/*
 * Process an XPIDL node and its kids, if any.
 */
gboolean
xpidl_process_node(TreeState *state);

/*
 * Write a newline folllowed by an indented, one-line comment containing IDL
 * source decompiled from state->tree.
 */
void
xpidl_write_comment(TreeState *state, int indent);



/*
 * Functions for parsing and printing UUIDs.
 */
#include "xpt_struct.h"

/*
 * How large should the buffer supplied to xpidl_sprint_IID be?
 */
#define UUID_LENGTH 37

/*
 * Print an iid to into a supplied buffer; the buffer should be at least
 * UUID_LENGTH bytes.
 */
gboolean
xpidl_sprint_iid(struct nsID *iid, char iidbuf[]);

/*
 * Parse a uuid string into an nsID struct.  We cannot link against libxpcom,
 * so we re-implement nsID::Parse here.
 */
gboolean
xpidl_parse_iid(nsID *id, const char *str);


/* Try to common a little node-handling stuff. */

/* is this node from an aggregate type (interface)? */
#define UP_IS_AGGREGATE(node)                                                 \
    (IDL_NODE_UP(node) &&                                                     \
     (IDL_NODE_TYPE(IDL_NODE_UP(node)) == IDLN_INTERFACE ||                   \
      IDL_NODE_TYPE(IDL_NODE_UP(node)) == IDLN_FORWARD_DCL))

#define UP_IS_NATIVE(node)                                                    \
    (IDL_NODE_UP(node) &&                                                     \
     IDL_NODE_TYPE(IDL_NODE_UP(node)) == IDLN_NATIVE)

/* is this type output in the form "<foo> *"? */
#define STARRED_TYPE(node) (IDL_NODE_TYPE(node) == IDLN_TYPE_STRING ||        \
                            IDL_NODE_TYPE(node) == IDLN_TYPE_WIDE_STRING ||   \
                            (IDL_NODE_TYPE(node) == IDLN_IDENT &&             \
                             UP_IS_AGGREGATE(node)))

/*
 * Perform various validation checks on methods.
 */
gboolean
verify_method_declaration(TreeState *state);

#endif /* __xpidl_h */
