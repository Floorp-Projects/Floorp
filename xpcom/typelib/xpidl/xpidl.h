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
extern gboolean generate_invoke;
extern gboolean generate_headers;
extern gboolean generate_nothing;

typedef struct {
    FILE *file;
    IDL_ns ns;
    IDL_tree tree;
    int mode;
} TreeState;

#define TREESTATE_HEADER	0
#define TREESTATE_INVOKE	1
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

extern nodeHandler headerDispatch[];
extern nodeHandler invokeDispatch[];
extern nodeHandler docDispatch[];

/*
 * nodeHandler that reports an error.
 */
gboolean node_is_error(TreeState *state);

/*
 * Process an IDL file, generating typelib, invoke glue and headers as
 * appropriate.
 */
int
xpidl_process_idl(char *filename);

/*
 * Add an output file to an internal list.  Used to clean up temporary files
 * in case of fatal error.
 */

void XPIDL_add_output_file(char *fn);
void XPIDL_cleanup_on_error();

#endif /* __xpidl_h */
