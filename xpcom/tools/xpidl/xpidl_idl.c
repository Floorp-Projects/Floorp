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

static int
msg_callback(int level, int num, int line, const char *file,
	     const char *message)
{
    /* XXX Mac */
    fprintf(stderr, "%s:%d: %d/%d/%s\n", file, line, level, num, message);
    return 1;
}

#define INPUT_BUF_CHUNK		4096

struct input_callback_data {
    FILE *input;
    char *buf;
    char *point;
    int len;
    int max;
};

static int
input_callback(IDL_input_reason reason, union IDL_input_data *cb_data,
	       gpointer user_data)
{
    struct input_callback_data *data = user_data;
    int rv, avail, copy;

    switch(reason) {
      case IDL_INPUT_REASON_INIT:
	data->input = fopen(cb_data->init.filename, "r");
	data->buf = malloc(INPUT_BUF_CHUNK);
	data->len = 0;
	data->point = data->buf;
	data->max = INPUT_BUF_CHUNK;
	return data->input && data->buf ? 0 : -1;
	
      case IDL_INPUT_REASON_FILL:
	avail = data->buf + data->len - data->point;

	assert(avail >= 0);

	if (!avail) {
	    data->point = data->buf;
	    avail = data->len = fread(data->buf, 1, data->max, data->input);
	    if (!avail && ferror(data->input))
		return -1;
	    /*
	     * XXX need to:
	     * - strip comments
	     * - parse doc comments
	     * - convert
	     *   #include <interface nsIBar>
	     * to
	     *   __declspec(inhibit) interface nsIBar { };
	     */
	}

	copy = MIN(avail, cb_data->fill.max_size);
	memcpy(cb_data->fill.buffer, data->point, copy);
	data->point += copy;
	return copy;
	
      case IDL_INPUT_REASON_ABORT:
      case IDL_INPUT_REASON_FINISH:
	fclose(data->input);
	data->input = NULL;
	free(data->buf);
	data->buf = data->point = NULL;
	data->len = data->max = 0;
	return 0;

      default:
	g_error("unknown input reason %d!", reason);
	return -1;
    }
}

int
xpidl_process_idl(char *filename) {
    char *basename, *tmp;
    IDL_tree top;
    TreeState state;
    int rv;
    struct input_callback_data data;

    rv = IDL_parse_filename_with_input(filename, input_callback, &data,
				       msg_callback, &top,
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
