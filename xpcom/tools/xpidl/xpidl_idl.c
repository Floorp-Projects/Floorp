#include "xpidl.h"

FILE *typelib_file = NULL;
FILE *invoke_file = NULL;
FILE *header_file = NULL;

nodeHandler *nodeDispatch[TREESTATE_NUM] = { NULL };

/*
 * Pass 1 generates #includes for headers.
 */
static gboolean
process_tree_pass1(TreeState *state)
{
    return TRUE;
}

gboolean
node_is_error(TreeState *state)
{
    fprintf(stderr, "ERROR: Unexpected node type %d\n",
	    IDL_NODE_TYPE(state->tree));
    return FALSE;
}

/*
 * The bulk of the generation happens here.
 */
gboolean
process_node(TreeState *state)
{
    char *name = NULL;
    nodeHandler *handlerp = nodeDispatch[state->mode], handler;
    assert(state->tree);

    if (handlerp && (handler = handlerp[IDL_NODE_TYPE(state->tree)]))
	return handler(state);
    return TRUE;
}

static gboolean
process_tree(TreeState *state)
{
    IDL_tree top = state->tree;
    if (!process_tree_pass1(state))
	return FALSE;
    state->tree = top;		/* pass1 might mutate state */
    return process_node(state);
}

int
xpidl_process_idl(char *filename) {
    char *basename, *tmp;
    IDL_tree top;
    TreeState state;
    int rv;

    rv = IDL_parse_filename(filename, NULL, NULL, &top,
			    &state.ns, IDLF_XPIDL,
			    enable_warnings ? IDL_WARNING1 : 0);
    if (rv != IDL_SUCCESS) {
	if (rv == -1) {
	    g_warning("Parse of %s failed: %s", filename, g_strerror(errno));
	} else {
	    g_warning("Parse of %s failed", filename);
	}
	return 0;
    }

    basename = g_strdup(filename);
    tmp = strrchr(basename, '.');
    if (tmp)
	*tmp = '\0';

    state.file = stdout;	/* XXX */
    nodeDispatch[TREESTATE_HEADER] = headerDispatch;
    nodeDispatch[TREESTATE_INVOKE] = invokeDispatch;
    nodeDispatch[TREESTATE_DOC] = docDispatch;
    if (generate_headers) {
	state.mode = TREESTATE_HEADER;
	state.tree = top;
	if (!process_tree(&state))
	    return 0;
    }
    if (generate_invoke) {
	state.mode = TREESTATE_INVOKE;
	state.tree = top;
	if (!process_tree(&state))
	    return 0;
    }
    if (generate_docs) {
	state.mode = TREESTATE_DOC;
	state.tree = top;
	if (!process_tree(&state))
	    return 0;
    }
    IDL_ns_free(state.ns);
    IDL_tree_free(top);
    
    return 1;
}
