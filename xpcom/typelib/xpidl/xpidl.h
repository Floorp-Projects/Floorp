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
 * Intramodule declarations.
 */

#ifndef __xpidl_h
#define __xpidl_h

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
 * IDL_tree_warning bombs on libIDL version 6.5, and I don't want to not write
 * warnings... so I define a versioned one here.  Thanks to Mike Shaver for the
 * ## idiom, that allows us to pass through varargs calls.
 */
#if !(LIBIDL_MAJOR_VERSION == 0 && LIBIDL_MINOR_VERSION == 6 && \
      LIBIDL_MICRO_VERSION == 5) && !defined(DEBUG_shaver)
/*
 * This turns a varargs call to XPIDL_WARNING directly into a varargs
 * call to IDL_tree_warning or xpidl_tree_warning as appropriate.  The
 * only tricky bit is that you must call XPIDL_WARNING with extra
 * parens, e.g. XPIDL_WARNING((foo, bar, "sil"))
 *
 * Probably best removed when we leave 6.5.  */
#define XPIDL_WARNING(x) IDL_tree_warning##x
#else
extern void xpidl_tree_warning(IDL_tree p, int level, const char *fmt, ...);
#define XPIDL_WARNING(x) xpidl_tree_warning##x
#endif

/*
 * Internal operation flags.
 */
extern gboolean enable_debug;
extern gboolean enable_warnings;
extern gboolean verbose_mode;
extern gboolean emit_typelib_annotations;
extern gboolean explicit_output_filename;

typedef struct TreeState TreeState;

/*
 * A function to handle an IDL_tree type.
 */
typedef gboolean (*nodeHandler)(TreeState *);

/*
 * Struct containing functions to define the behavior of a given output mode.
 */
typedef struct backend {
    nodeHandler *dispatch_table; /* nodeHandlers table, indexed by node type. */
    nodeHandler emit_prolog;     /* called at beginning of output generation. */
    nodeHandler emit_epilog;     /* called at end. */
} backend;

/* Function that produces a struct of output-generation functions */
typedef backend *(*backendFactory)();
 
extern backend *xpidl_header_dispatch(void);
extern backend *xpidl_typelib_dispatch(void);
extern backend *xpidl_doc_dispatch(void);
extern backend *xpidl_java_dispatch(void);

typedef struct ModeData {
    char               *mode;
    char               *modeInfo;
    char               *suffix;
    backendFactory     factory;
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
    GSList           *base_includes;
    nodeHandler      *dispatch;
    void             *priv;     /* mode-private data */
};

/*
 * Process an IDL file, generating InterfaceInfo, documentation and headers as
 * appropriate.
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
#include <xpt_struct.h>

/*
 * How large should the buffer supplied to xpidl_sprint_IID be?
 */
#define UUID_LENGTH 37

/*
 * Print an iid to into a supplied buffer; the buffer should be at least
 * UUID_LENGTH bytes.
 */
gboolean
xpidl_sprint_iid(nsID *iid, char iidbuf[]);

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

#define DIPPER_TYPE(node)                                                     \
    (NULL != IDL_tree_property_get(node, "domstring"))

/*
 * Find the underlying type of an identifier typedef.  Returns NULL
 * (and doesn't complain) on failure.
 */
IDL_tree /* IDL_TYPE_DCL */
find_underlying_type(IDL_tree typedef_ident);

/*
 * Check that const declarations match their stated sign and are of the
 * appropriate types.
 */
gboolean
verify_const_declaration(IDL_tree const_tree);

/*
 * Check that scriptable attributes in scriptable interfaces actually are.
 */
gboolean
verify_attribute_declaration(IDL_tree method_tree);

/*
 * Perform various validation checks on methods.
 */
gboolean
verify_method_declaration(IDL_tree method_tree);

/*
 * Verify that a native declaration has an associated C++ expression, i.e. that
 * it's of the form native <idl-name>(<c++-name>)
 */
gboolean
check_native(TreeState *state);

void
printlist(FILE *outfile, GSList *slist);

#endif /* __xpidl_h */
