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

/*
 * Pass 1 generates #includes for headers and the like.
 */
static gboolean
process_tree_pass1(TreeState *state)
{
    nodeHandler handler;

    if ((handler = state->dispatch[0]))
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
    IDL_tree_func_data tfd;

    while (p) {
        struct _IDL_LIST *list = &IDL_LIST(p);
        tfd.tree = list->data;
        if (!foreach(&tfd, user_data))
            return;
        p = list->next;
    }
}

/*
 * The bulk of the generation happens here.
 */
gboolean
xpidl_process_node(TreeState *state)
{
    gint type;
    nodeHandler *dispatch, handler;

    assert(state->tree);
    type = IDL_NODE_TYPE(state->tree);

    /*
     * type == 0 shouldn't ever happen for real, so we use that slot for
     * pass-1 processing
     */
    if (type && (dispatch = state->dispatch) && (handler = dispatch[type]))
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
    state->tree = top;          /* pass1 might mutate state */
    if (!xpidl_process_node(state))
        return FALSE;
    state->tree = NULL;
    return process_tree_pass1(state);
}

static int
msg_callback(int level, int num, int line, const char *file,
             const char *message)
{
    /* XXX Mac */
    fprintf(stderr, "%s:%d: %s\n", file, line, message);
    return 1;
}

#define INPUT_BUF_CHUNK         8192

struct input_callback_data {
    FILE *input;                /* stream for getting data */
    char *filename;             /* where did I come from? */
    int lineno;                 /* last lineno processed */
    char *buf;                  /* buffer for data */
    char *point;                /* next char to feed to libIDL */
    int len;                    /* amount of data read into the buffer */
    int max;                    /* size of the buffer */
    struct input_callback_data *next; /* file from which we were included */
    int  f_raw : 2,             /* in a raw block when starting next block */
         f_comment : 2,         /* in a comment when starting next block */
         f_include : 2;         /* in an #include when starting next block */
    char last_read[2];          /* last 1/2 chars read, for spanning blocks */
};

/* values for f_{raw,comment,include} */
#define INPUT_IN_NONE   0x0
#define INPUT_IN_FULL   0x1     /* we've already started one */
#define INPUT_IN_START  0x2     /* we're about to start one */
#define INPUT_IN_MAYBE  0x3     /* we might be about to start one (check
                                   last_read to be sure) */

struct input_callback_stack {
    struct input_callback_data *top;
    GHashTable *includes;
    IncludePathEntry *include_path;
};

static FILE *
fopen_from_includes(const char *filename, const char *mode,
                    IncludePathEntry *include_path)
{
    char *pathname;
    FILE *file;
    if (!strcmp(filename, "-"))
        return stdin;

    if (filename[0] != '/') {
        while (include_path) {
#ifdef XP_MAC
				if (!*include_path->directory)
		        pathname = g_strdup_printf("%s", filename);
				else
		        pathname = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s",
	                             include_path->directory, filename);
#else
        pathname = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s",
                                   include_path->directory, filename);
#endif
            if (!pathname)
                return NULL;
#ifdef DEBUG_shaver_bufmgmt
            fprintf(stderr, "looking for %s as %s\n", filename, pathname);
#endif
            file = fopen(pathname, mode);
            free(pathname);
            if (file)
                return file;
            include_path = include_path->next;
        }
    } else {
        file = fopen(filename, mode);
        if (file)
            return file;
    }
    fprintf(stderr, "can't open %s for reading\n", filename);
    return NULL;
}

#ifdef XP_MAC
extern FILE* mac_fopen(const char* filename, const char *mode);
#endif

static struct input_callback_data *
new_input_callback_data(const char *filename, IncludePathEntry *include_path)
{
    struct input_callback_data *new_data = xpidl_malloc(sizeof *new_data);
    memset(new_data, 0, sizeof *new_data);
#ifdef XP_MAC
    // if file is a full path name, just use fopen, otherwise search access paths.
    if (strchr(filename, ':') == NULL)
        new_data->input = mac_fopen(filename, "r");
    else
        new_data->input = fopen(filename, "r");
#else
    new_data->input = fopen_from_includes(filename, "r", include_path);
#endif
    if (!new_data->input)
        return NULL;
    new_data->buf = xpidl_malloc(INPUT_BUF_CHUNK + 1); /* trailing NUL */
    new_data->point = new_data->buf;
    new_data->max = INPUT_BUF_CHUNK;
    new_data->filename = xpidl_strdup(filename);
    new_data->lineno = 1;
    return new_data;
}

static int
input_callback(IDL_input_reason reason, union IDL_input_data *cb_data,
               gpointer user_data)
{
    struct input_callback_stack *stack = user_data;
    struct input_callback_data *data = stack->top, *new_data = NULL;
    int avail, copy;
    char *check_point, *ptr, *end_copy, *raw_start, *comment_start,
         *include_start;

    switch(reason) {
      case IDL_INPUT_REASON_INIT:
        new_data = new_input_callback_data(cb_data->init.filename,
                                           stack->include_path);
        if (!new_data)
            return -1;

        IDL_file_set(new_data->filename, new_data->lineno);
        stack->top = new_data;
        return 0;

      case IDL_INPUT_REASON_FILL:
    fill_start:
        avail = data->buf + data->len - data->point;
        assert(avail >= 0);

        if (!avail) {
            data->point = data->buf;

            /* fill the buffer */
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
                    data->f_include = INPUT_IN_NONE;
                    goto fill_start;
                }
                return 0;
            }
            data->buf[data->len] = 0;
        }

    scan_for_special:
        check_point = data->point;
        end_copy = data->buf + data->len;
        /*
         * When we're stripping comments and processing #includes,
         * we need to be sure that we don't process anything inside
         * \n%{ and \n%}.  In order to simplify things, we process only
         * comment, include or raw-block stuff when they're at the
         * beginning of the block we're about to send (data->point).
         * This makes the processing much simpler, since we can skip
         * data->point ahead for comments and #include, and skip
         * check_point ahead for raw blocks.
         */

        if (!(data->f_raw || data->f_comment || data->f_include)) {
            /* look for first raw/comment/include */
#ifdef DEBUG_shaver_bufmgmt
            fprintf(stderr, "looking for specials\n");
#endif
            /* raw block */
            if ((raw_start = strstr(check_point, "\n%{"))) {
#ifdef DEBUG_shaver_bufmgmt
                fprintf(stderr, "found raw at %x\n", raw_start);
#endif
                end_copy = raw_start;
            }

            /* comment */
            if ((comment_start = strstr(check_point, "/*")) &&
                (!raw_start || comment_start < raw_start)) {
#ifdef DEBUG_shaver_bufmgmt
                fprintf(stderr, "comment starts with %.7s\n", comment_start);
#endif
                end_copy = comment_start;
            }

            /* include */
            if ((include_start = strstr(check_point, "#include")) &&
                (!raw_start || include_start < raw_start) &&
                (!comment_start || include_start < comment_start)) {
                end_copy = include_start;
            }

            if (end_copy == raw_start)
                data->f_raw = INPUT_IN_START;
            else if (end_copy == comment_start)
                data->f_comment = INPUT_IN_START;
            else if (end_copy == include_start)
                data->f_include = INPUT_IN_START;
#ifdef DEBUG_shaver_bufmgmt
            fprintf(stderr,
                    "specials: %d/%d/%d, end = %x, buf = %x, b+l = %x\n",
                    data->f_raw, data->f_comment, data->f_include,
                    end_copy, data->buf, data->buf + data->len);
#endif
        } else {
#ifdef DEBUG_shaver_bufmgmt
            fprintf(stderr, "already have special %d/%d/%d\n",
                    data->f_raw, data->f_comment, data->f_include);
#endif
        }

        if ((end_copy == data->point || /* just found one at the start */
             end_copy == data->buf + data->len /* left over */) &&
            (data->f_raw || data->f_comment || data->f_include)) {

            if (data->f_raw) {
                ptr = strstr(check_point, "\n%}\n");
                if (ptr) {
                    data->f_raw = INPUT_IN_NONE;
                    end_copy = ptr + 4;
#ifdef DEBUG_shaver_bufmgmt
                    fprintf(stderr, "RAW->%.*s<-RAW\n", end_copy - data->point,
                            data->point);
#endif
                }
                assert(!data->f_comment && !data->f_include);
            } else if (data->f_comment) {
                /* XXX process doc comment */
                ptr = strstr(check_point, "*/");
                if (ptr) {
                    data->f_comment = INPUT_IN_NONE;
                    data->point = ptr + 2; /* star-slash */
#ifdef DEBUG_shaver_bufmgmt
                    fprintf(stderr, "COMMENT->%.*s<-COMMENT\n",
                            data->point - check_point, check_point);
#endif
                }
                else {
                    data->point = data->buf + data->len;
                    goto fill_start;
                }

                assert(!data->f_raw && !data->f_include);
                /*
                 * Now that we've advanced data->point to skip the comment,
                 * we want to check for specials that were ``shadowed'' by
                 * the comment.
                 */
                goto scan_for_special;
            } else if (data->f_include) {
                /* process include */
                const char *scratch;
                char *filename;
                include_start = data->point;

                assert(!strncmp(include_start, "#include \"", 10));
                filename = include_start + 10; /* skip #include " */

                assert(filename < data->buf + data->len);
                ptr = strchr(filename, '\"');
                if (!ptr) {
                    /* XXX report error */
                    return -1;
                }
                data->point = ptr+1;

                *ptr = 0;
                ptr = strrchr(filename, '.');

#ifdef DEBUG_shaver_bufmgmt
                fprintf(stderr, "found #include %s\n", filename);
#endif
                assert(stack->includes);
                if (!g_hash_table_lookup(stack->includes, filename)) {
                    char *basename = filename;
                    filename = xpidl_strdup(filename);
                    ptr = strrchr(basename, '.');
                    if (ptr)
                        *ptr = 0;
                    basename = xpidl_strdup(basename);
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
#ifdef DEBUG_shaver_bufmgmt
                    fprintf(stderr, "processing #include %s\n", filename);
#endif
                    /* now continue getting data from new file */
                    goto fill_start;
                }
                /*
                 * if we started with a #include, but we've already
                 * processed that file, we need to continue scanning
                 * for special sequences.
                 */
                data->f_include = INPUT_IN_NONE;
                goto scan_for_special;
            }
        } else {
#ifdef DEBUG_shaver_bufmgmt
            fprintf(stderr, "no specials\n");
#endif
        }

        avail = MIN(data->buf + data->len, end_copy) - data->point;
#ifdef DEBUG_shaver_bufmgmt
        fprintf(stderr,
                "avail[%d] = MIN((data->buf[%x] + data->len[%d])[%x], "
                "end_copy[%x])[%x] - data->point[%x]\n",
                avail, data->buf, data->len, data->buf + data->len,
                end_copy, MIN(data->buf + data->len, end_copy),
                data->point);
#endif
        copy = MIN(avail, (int) cb_data->fill.max_size);
#ifdef DEBUG_shaver_bufmgmt
        fprintf(stderr, "COPYING->%.*s<-COPYING\n", copy, data->point);
#endif
        memcpy(cb_data->fill.buffer, data->point, copy);
        data->point += copy;
        return copy;

      case IDL_INPUT_REASON_ABORT:
      case IDL_INPUT_REASON_FINISH:
        if (data->input != stdin)
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
xpidl_process_idl(char *filename, IncludePathEntry *include_path,
                  char *basename, ModeData *mode)
{
    char *tmp, *outname, *mode_outname;
    IDL_tree top;
    TreeState state;
    int rv;
    struct input_callback_stack stack;
    gboolean ok;
    char *fopen_mode;

    stack.includes = g_hash_table_new(g_str_hash, g_str_equal);
    if (!stack.includes) {
        fprintf(stderr, "failed to create hashtable (EOM?)\n");
        return 0;
    }

    state.basename = xpidl_strdup(filename);
    tmp = strrchr(state.basename, '.');
    if (tmp)
        *tmp = '\0';

    if (!basename)
        outname = xpidl_strdup(state.basename);
    else
        outname = xpidl_strdup(basename);

    /* so we don't include it again! */
    g_hash_table_insert(stack.includes, filename, state.basename);

    stack.include_path = include_path;

    rv = IDL_parse_filename_with_input(filename, input_callback, &stack,
                                       msg_callback, &top,
                                       &state.ns,
#if LIBIDL_VERSION_CODE >= LIBIDL_VERSION (0,6,2)
                                       IDLF_IGNORE_FORWARDS |
#endif
                                       IDLF_XPIDL,
                                       enable_warnings ? IDL_WARNING1 :
                                       IDL_ERROR);
    if (rv != IDL_SUCCESS) {
        if (rv == -1) {
            g_warning("Parse of %s failed: %s", filename, g_strerror(errno));
        } else {
            g_warning("Parse of %s failed", filename);
        }
        return 0;
    }

    state.basename = xpidl_strdup(filename);
    tmp = strrchr(state.basename, '.');
    if (tmp)
        *tmp = '\0';

    /* so we don't make a #include for it  */
    g_hash_table_remove(stack.includes, filename);

    state.includes = stack.includes;
    state.include_path = include_path;
    state.dispatch = mode->factory();
    if (!state.dispatch) {
        /* XXX error */
        return 0;
    }
    if (strcmp(outname, "-")) {
        mode_outname = g_strdup_printf("%s.%s", outname, mode->suffix);
        fopen_mode = mode->factory == xpidl_typelib_dispatch ? "wb" : "w";
        state.file = fopen(mode_outname, fopen_mode);
        free(mode_outname);
        if (!state.file) {
            perror("error opening output file");
            return 0;
        }
    } else {
        state.file = stdout;
    }
    state.tree = top;
    ok = process_tree(&state);
    if (state.file != stdout)
        fclose(state.file);
    if (!ok)
        return 0;
    free(state.basename);
    free(outname);
    /* g_hash_table_foreach(state.includes, free_name, NULL);
       g_hash_table_destroy(state.includes);
    */
    IDL_ns_free(state.ns);
    IDL_tree_free(top);

    return 1;
}

void
xpidl_write_comment(TreeState *state, int indent)
{
    fprintf(state->file, "\n%*s/* ", indent, "");
    IDL_tree_to_IDL(state->tree, state->ns, state->file,
                    IDLF_OUTPUT_NO_NEWLINES |
                    IDLF_OUTPUT_NO_QUALIFY_IDENTS |
                    IDLF_OUTPUT_PROPERTIES);
    fputs(" */\n", state->file);
}
