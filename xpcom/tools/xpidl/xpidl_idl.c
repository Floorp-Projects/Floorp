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
 * Common IDL-processing code.
 */

#include "xpidl.h"

FILE *typelib_file = NULL;
FILE *invoke_file = NULL;
FILE *header_file = NULL;

nodeHandler *nodeDispatch[TREESTATE_NUM] = { NULL };

/*
 * Pass 1 generates #includes for headers and the like.
 */
static gboolean
process_tree_pass1(TreeState *state)
{
    nodeHandler handler;

    if ((handler = nodeDispatch[state->mode][0]))
        return handler(state);
    return TRUE;
}

gboolean
node_is_error(TreeState *state)
{
    fprintf(stderr, "ERROR: Unexpected node type %d\n",
            IDL_NODE_TYPE(state->tree));
    return FALSE;
}

void
xpidl_list_foreach(IDL_tree p, IDL_tree_func foreach, gpointer user_data)
{
    IDL_tree iter = p;
    for (; iter; iter = IDL_LIST(iter).next)
        if (!foreach(IDL_LIST(iter).data, user_data))
            return;
}

/*
 * The bulk of the generation happens here.
 */
gboolean
process_node(TreeState *state)
{
    char *name = NULL;
    nodeHandler *handlerp = nodeDispatch[state->mode], handler;
    gint type;
    assert(state->tree);

    type = IDL_NODE_TYPE(state->tree);

    /*
     * type == 0 shouldn't ever happen for real, so we use that slot for
     * pass-1 processing
     */
    if (type && handlerp && (handler = handlerp[type]))
        return handler(state);
    return TRUE;
}

/*
 * Call the IDLN_NONE handler for pre-generation, then process the tree,
 * then call the IDLN_NONE handler again with state->tree == NULL for
 * post-generation.
 */
static gboolean
process_tree(TreeState *state)
{
    IDL_tree top = state->tree;
    if (!process_tree_pass1(state))
        return FALSE;
    state->tree = top;		/* pass1 might mutate state */
    if (!process_node(state))
        return FALSE;
    state->tree = NULL;
    if (!process_tree_pass1(state))
        return FALSE;
    return TRUE;
}

static int
msg_callback(int level, int num, int line, const char *file,
             const char *message)
{
    /* XXX Mac */
    fprintf(stderr, "%s:%d: %s\n", file, line, message);
    return 1;
}

#define INPUT_BUF_CHUNK		8192

struct input_callback_data {
    FILE *input;
    char *filename;
    int lineno;
    char *buf;
    char *point;
    int len;
    int max;
    struct input_callback_data *next;
};

struct input_callback_stack {
    struct input_callback_data *top;
    GHashTable *includes;
    IncludePathEntry *include_path;
};

static FILE *
fopen_from_includes(const char *filename, const char *mode,
                    IncludePathEntry *include_path)
{
    char *filebuf = NULL;
    FILE *file = NULL;
    for (; include_path && !file; include_path = include_path->next) {
        filebuf = g_strdup_printf("%s/%s", include_path->directory, filename);
        if (!filebuf)
            return NULL;
#ifdef DEBUG_shaver
        fprintf(stderr, "looking for %s as %s\n", filename, filebuf);
#endif
        file = fopen(filebuf, mode);
        free(filebuf);
    }
    return file;
}

static struct input_callback_data *
new_input_callback_data(const char *filename, IncludePathEntry *include_path)
{
    struct input_callback_data *new_data = malloc(sizeof *new_data);
    if (!new_data)
        return NULL;
    new_data->input = fopen_from_includes(filename, "r", include_path);
    if (!new_data->input)
        return NULL;
    new_data->buf = malloc(INPUT_BUF_CHUNK);
    if (!new_data->buf) {
        fclose(new_data->input);
        return NULL;
    }
    new_data->len = 0;
    new_data->point = new_data->buf;
    new_data->max = INPUT_BUF_CHUNK;
    new_data->filename = strdup(filename);
    if (!new_data->filename) {
        free(new_data->buf);
        fclose(new_data->input);
        return NULL;
    }
    new_data->lineno = 1;
    new_data->next = NULL;
    return new_data;
}

static int
input_callback(IDL_input_reason reason, union IDL_input_data *cb_data,
               gpointer user_data)
{
    struct input_callback_stack *stack = user_data;
    struct input_callback_data *data = stack->top, *new_data = NULL;
    int rv, avail, copy;
    char *include_start, *ptr;

    switch(reason) {
      case IDL_INPUT_REASON_INIT:
        new_data = new_input_callback_data(cb_data->init.filename,
                                           stack->include_path);
        if (!new_data)
            return -1;

        /* XXX replace with IDL_set_file_position */
        new_data->len = sprintf(new_data->buf, "# 1 \"%s\"\n",
                                cb_data->init.filename);
        stack->top = new_data;
        return 0;
	
      case IDL_INPUT_REASON_FILL:
    fill_start:
        avail = data->buf + data->len - data->point;
        assert(avail >= 0);

        if (!avail) {
            char *comment_start = NULL, *include_start = NULL, *ptr;
            data->point = data->buf;

            /* fill the buffer */
        fill_buffer:
            data->len = fread(data->buf, 1, data->max, data->input);
            if (!data->len) {
                if (ferror(data->input))
                    return -1;
                /* pop include */
                if (data->next) {
#ifdef DEBUG_shaver_includes
                    fprintf(stderr, "leaving %s, returning to %s\n",
                            data->filename, data->next->filename);
#endif
                    data = data->next;
                    stack->top = data;
                    IDL_file_set(data->filename, ++data->lineno);
                    IDL_inhibit_pop();
                    goto fill_start;
                }
                return 0;
            }

            /*
             * strip comments
             */

            /*
             * XXX
             * What if the last char in this block is '/' and the first in the
             * next block is '*'?  I'm not sure it matters, because I don't
             * think there are any legal IDL syntaxes with '/' in them.
             *
             * XXX what about "/* " appearing in the IDL?
             */
            if (!comment_start)
                comment_start = strstr(data->buf, "/*");
            while (comment_start) {
                char *end = strstr(comment_start, "*/");
                int comment_length;
                int bytes_after_comment;
		    
                if (!end)
                    goto fill_buffer;

                end += 2; /* star-slash */
                comment_length = end - comment_start;
                bytes_after_comment = data->buf + data->len - end;

                /* found the end, move data around */
#ifdef DEBUG_shaver_bufmgmt
                fprintf(stderr,
                        "FOUND COMMENT: (%d) %.*s, moving %d back\n",
                        comment_length, comment_length, 
                        comment_start, bytes_after_comment);
#endif			

                memmove(comment_start, end, bytes_after_comment);
                comment_start[bytes_after_comment] = '\0';
                data->len -= comment_length;

#ifdef DEBUG_shaver_bufmgmt
                fprintf(stderr, "new buffer:\n---\n%.*s\n---\n",
                        data->len, data->buf);
#endif

                /* look for the next comment */
                comment_start = strstr(data->buf, "/*");

            } /* while(comment_start) */

            /* we set avail here, because data->len is changed above */
                
            avail = data->buf + data->len - data->point;
        }
    
        /*
         * process includes
         */
        
        /*
         * we only do #include magic at the beginning of the buffer.
         * otherwise, we just set avail to cap the amount of data sent
         * on this pass.
         */
        include_start = strstr(data->point, "#include \"");
        if (include_start == data->point) {
            /* time to process the #include */
            const char *scratch;
            char *filename = include_start + 10;
            ptr = strchr(filename, '\"');
            if (!ptr) {
                /* XXX report error */
                return -1;
            }
            data->point = ptr+1;
            
            *ptr = 0;
            ptr = strrchr(filename, '.');
            /* XXX is this a safe optimization? */
            if (!g_hash_table_lookup(stack->includes, filename)) {
                char *basename = filename;
#ifdef DEBUG_shaver_includes
                fprintf(stderr, "processing #include %s\n", filename);
#endif
                filename = strdup(filename);
                ptr = strrchr(basename, '.');
                if (ptr)
                    *ptr = 0;
                basename = strdup(basename);
                g_hash_table_insert(stack->includes, filename, basename);
                new_data = new_input_callback_data(filename,
                                                   stack->include_path);
                if (!new_data) {
                    free(filename);
                    return -1;
                }
                new_data->next = stack->top;
                IDL_inhibit_push();
                IDL_file_get(&scratch, &data->lineno);
                data = stack->top = new_data;
                IDL_file_set(data->filename, data->lineno);
                /* now continue getting data from new file */
                goto fill_start;
            } else {
#ifdef DEBUG_shaver_includes
                fprintf(stderr, "not processing #include %s again\n",
                        filename);
#endif
            }
        } else if (include_start) {
#ifdef DEBUG_shaver_includes
            fprintf(stderr, "not processing #include yet\n");
#endif
            avail = include_start - data->point;
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
xpidl_process_idl(char *filename, IncludePathEntry *include_path)
{
    char *basename, *tmp;
    IDL_tree top;
    TreeState state;
    int rv;
    struct input_callback_stack stack;

    stack.includes = g_hash_table_new(g_str_hash, g_str_equal);
    stack.include_path = include_path;
    if (!stack.includes)
        return 0;

    rv = IDL_parse_filename_with_input(filename, input_callback, &stack,
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
    state.basename = basename;
    state.includes = stack.includes;
    state.include_path = include_path;
    nodeDispatch[TREESTATE_HEADER] = headerDispatch();
    nodeDispatch[TREESTATE_INVOKE] = invokeDispatch();
    nodeDispatch[TREESTATE_DOC] = docDispatch();
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
    free(state.basename);
    /* g_hash_table_foreach(state.includes, free_name, NULL);
       g_hash_table_destroy(state.includes);
    */
    IDL_ns_free(state.ns);
    IDL_tree_free(top);
    
    return 1;
}
