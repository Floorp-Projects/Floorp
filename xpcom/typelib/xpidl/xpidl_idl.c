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

#ifdef XP_MAC
extern void mac_warning(const char* warning_message);
#endif

static int
msg_callback(int level, int num, int line, const char *file,
             const char *message)
{
#ifdef XP_MAC
    static char warning_message[1024];
    sprintf(warning_message, "%s:%d: %s\n", file, line, message);
    mac_warning(warning_message);
#else
    /* XXX Mac */
    fprintf(stderr, "%s:%d: %s\n", file, line, message);
#endif
    return 1;
}

#define INPUT_BUF_CHUNK         8192

struct input_callback_data {
    FILE *input;                /* stream for getting data */
    char *filename;             /* where did I come from? */
    unsigned int lineno;       /* last lineno processed */
    char *buf;                  /* buffer for data */
    char *point;                /* next char to feed to libIDL */
    int start;                  /* are we at the start of the file? */
    unsigned int len;           /* amount of data read into the buffer */
    unsigned int max;           /* size of the buffer */
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
#if 0
    if (strchr(filename, ':') == NULL)
        new_data->input = mac_fopen(filename, "r");
    else
        new_data->input = fopen(filename, "r");
#else
    // on Mac, fopen knows how to find files.
    new_data->input = fopen(filename, "r");
#endif
#else
    new_data->input = fopen_from_includes(filename, "r", include_path);
#endif
    if (!new_data->input)
        return NULL;
    new_data->buf = xpidl_malloc(INPUT_BUF_CHUNK + 1); /* trailing NUL */
    new_data->point = new_data->buf;
    new_data->max = INPUT_BUF_CHUNK;
    new_data->filename = xpidl_strdup(filename);

    /* libIDL expects the line number to be that of the *next* line */
    new_data->lineno = 2;
    return new_data;
}

static int
NextIsRaw(struct input_callback_data *data, char **startp, int *lenp)
{
    char *end, *start, *data_end = data->buf + data->len;

#ifdef DEBUG_shaver_input
    fputs("[R]", stderr);
#endif

    /* process pending raw section, or rest of current one */
    if (!(data->f_raw || 
          (data->point[0] == '%' && data->point[1] == '{')))
        return 0;
        
    start = *startp = data->point;
    
    while (start < data_end && (end = strstr(start, "%}"))) {
        if (end[-1] == '\r' ||
            end[-1] == '\n')
            break;
        start = end + 1;
    }
    
    if (end) {
        *lenp = end - data->point + 2;
        data->f_raw = 0;
    } else {
        *lenp = data->len;
        data->f_raw = 1;
    }
    return 1;
    
}

static int
NextIsComment(struct input_callback_data *data, char **startp, int *lenp)
{
    char *end;
    /* process pending comment, or rest of current one */
    
    /*
     * XXX someday, my prince will come.  Also, we'll upgrade to a
     * libIDL that handles comments for us.
     */

#ifdef DEBUG_shaver_input
    fputs("[C]", stderr);
#endif

    /*
     * I tried to do the right thing by handing doc comments to libIDL, but
     * it barfed.  Go figger.
     */
    if (!(data->f_comment ||
          (data->point[0] == '/' && data->point[1] == '*')))
        return 0;

    end = strstr(data->point, "*/");
    *lenp = 0;
    if (end) {
	int skippedLines;

	/* get current lineno */
	IDL_file_get(NULL,(int *)&data->lineno);

	*startp = data->point;

	/* get line count */
	for(skippedLines = 0;;)
	    {
		*startp = strstr(*startp,"\n");
		if (!*startp || *startp >= end)
		    break;
		
		*startp += 1;
		skippedLines++;
	    }
	data->lineno += skippedLines;
	IDL_file_set(data->filename, (int)data->lineno);

        *startp = end + 2;
        data->f_comment = 0;
    } else {
        *startp = data->buf + data->len;
        data->f_comment = 1;
    }
    data->point = *startp;
    return 1;
}

static int
NextIsInclude(struct input_callback_stack *stack, char **startp, int *lenp)
{
    struct input_callback_data *data = stack->top, *new_data;
    char *filename, *start, *end;
    const char *scratch;

#ifdef DEBUG_shaver_input
    fputs("[I", stderr);
#endif
    /* process the #include that we're in now */
    if (strncmp(data->point, "#include \"", 10)) {
#ifdef DEBUG_shaver_input
        fputs("0]", stderr);
#endif
        return 0;
    }
    
    start = filename = data->point + 10; /* skip #include " */
    assert(start < data->buf + data->len);
    end = strchr(filename, '\"');
    if (!end) {
        /* filename probably stops at next whitespace */
        char *data_end = data->buf + data->len;
        start = end = filename - 1;
        
        while (end < data_end) {
            if (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t') {
                *end = '\0';
                break;
            }
            end++;
        }
        
        /* make sure we have accurate line info */
        IDL_file_get(&scratch, (int *)&data->lineno);
        fprintf(stderr,
                "%s:%d: didn't find end of quoted include name %*s\n",
                scratch, data->lineno, end - start, start);
        return -1;
    }

    if (*end == '\r' || *end == '\n')
	IDL_file_set(NULL,(int)(++data->lineno));

    *end = '\0';
    *startp = end + 1;
    end = strrchr(filename, '.');

#ifdef DEBUG_shaver_inc
    fprintf(stderr, "found #include %s\n", filename);
#endif

    /* store offset for when we pop, or if we skip this one */
    data->point = *startp;

    assert(stack->includes);
    if (!g_hash_table_lookup(stack->includes, filename)) {
        char *file_basename = filename;
        filename = xpidl_strdup(filename);
        if (end)
            *end = '\0';
        file_basename = xpidl_strdup(file_basename);
        g_hash_table_insert(stack->includes, filename, file_basename);
        new_data = new_input_callback_data(filename, stack->include_path);
        if (!new_data) {
#ifdef XP_MAC
	    static char warning_message[1024];
	    IDL_file_get(&scratch, (int *)&data->lineno);
	    sprintf(warning_message, "%s:%d: can't open included file %s for reading\n", scratch,
		    data->lineno, filename);
	    mac_warning(warning_message);
#else
	    IDL_file_get(&scratch, (int *)&data->lineno);
	    fprintf(stderr, "%s:%d: can't open included file %s for reading\n", scratch,
		    data->lineno, filename);
#endif
            return -1;
        }

        new_data->next = data;
        IDL_inhibit_push();
        IDL_file_get(&scratch, (int *)&data->lineno);
        data = stack->top = new_data;
        IDL_file_set(data->filename, (int)data->lineno);
    }
    *lenp = 0;               /* this is magic, see the comment below */
#ifdef DEBUG_shaver_input
    fputs("1]", stderr);
#endif
    return 1;
}    

static void
FindSpecial(struct input_callback_data *data, char **startp, int *lenp) {
    char *point = data->point, *end = data->buf + data->len;

    /* magic sequences are:
     * "%{"               raw block
     * "/\*"               comment
     * "#include \""      include
     * The first and last want a newline [\r\n] before, or the start of the
     * file.
     */

#define LINE_START(data, point) (data->start ||                              \
                                 (point > data->point &&                     \
                                  (point[-1] == '\r' || point[-1] == '\n')))
                                                 
    while (point < end) {
        /* XXX unroll? */
        if (point[0] == '%' && LINE_START(data, point) && point[1] == '{')
            break;
        if (point[0] == '/' && point[1] == '*') /* && point[2] != '*') */
            break;
        if (!strncmp(point, "#include \"", 10) && LINE_START(data, point))
            break;
        point++;
    }

#undef LINE_START

    *startp = data->point;
    *lenp = point - data->point;
#ifdef DEBUG_shaver_input
    fprintf(stderr, "*startp = offset %d, *lenp = %d\n", *startp - data->buf,
            *lenp);
#endif
}

/* set this with a debugger to see exactly what libIDL sees */
static FILE *tracefile;

static int
input_callback(IDL_input_reason reason, union IDL_input_data *cb_data,
               gpointer user_data)
{
 struct input_callback_stack *stack = user_data;
    struct input_callback_data *data = stack->top, *new_data = NULL;
    unsigned int len, copy, avail, rv;
    char *start;

    switch(reason) {
      case IDL_INPUT_REASON_INIT:
        if (data == NULL || data->next == NULL) {
            /*
             * This is the first file being processed.  As it's the target
             * file, we only look for it in the first entry in the include
             * path, which we assume to be the current directory.
             */
            IncludePathEntry first_entry;

            first_entry.directory = stack->include_path->directory;
            first_entry.next = NULL;

            new_data = new_input_callback_data(cb_data->init.filename,
                                               &first_entry);
        } else {
            new_data = new_input_callback_data(cb_data->init.filename,
                                               stack->include_path);
        }

        if (!new_data)
            return -1;

        IDL_file_set(new_data->filename, (int)new_data->lineno);
        stack->top = new_data;
        return 0;

      case IDL_INPUT_REASON_FILL:
        /* get data into the buffer */
        data->start = 0;
        start = NULL;
        len = 0;

        while (!(avail = data->buf + data->len - data->point)) {
            data->point = data->buf;

            /* fill the buffer */
            data->len = fread(data->buf, 1, data->max, data->input);
            /* if we've got data in the buffer, no need for the tricks below */
            if (data->len) {
                data->start = 1;
                break;
            }
            if (ferror(data->input))
                return -1;

            /* pop include */
            /* beard: what about closing the file? */
            fclose(data->input);
            data->input = NULL; /* prevent double close */
            if (!data->next)
                return 0;
            data->input = NULL;
#ifdef DEBUG_shaver_includes
            fprintf(stderr, "leaving %s, returning to %s\n",
                    data->filename, data->next->filename);
#endif
            stack->top = data->next;
            /* shaver: what about freeing the input structure? */
            free(data);
            data = stack->top;

            IDL_file_set(data->filename, (int)data->lineno);
            IDL_inhibit_pop();
            data->f_include = INPUT_IN_NONE;
        }

        /* force NUL-termination on the buffer */
        data->buf[data->len] = 0;
        
        /*
         * Now we scan for sequences which require special attention:
         *   \n#include                   begins an include statement
         *   \n%{                         begins a raw-source block
         *   /\*                           begins a comment
         * 
         * We used to be fancier here, so make sure that we sent the most
         * data possible at any given time.  To that end, we skipped over
         * \n%{ raw \n%} blocks and then _continued_ the search for special
         * sequences like \n#include or /\* comments .
         * 
         * It was really ugly, though -- liberal use of goto!  lots of implicit
         * state!  what fun! -- so now we just do this:
         *
         * if (special at start) {
         *     process that special;
         *     send next chunk of data to libIDL;
         * } else {
         *     scan for next special;
         *     send data up to that special to libIDL;
         * }
         *
         * Easy as pie.  Except that libIDL takes a return of zero to mean
         * ``all done'', so whenever we would want to do that, we send a single
         * space in the buffer.  Nobody'll notice, promise.  (For the curious,
         * we would want to send zero bytes but be called again when we start
         * include processing or when a comment/raw follows another
         * immediately, etc.)
         *
         * XXX we don't handle the case where the \n#include or /\* or \n%{
         * sequence straddles a block boundary.  We should really fix that.
         *
         * XXX we don't handle doc-comments correctly (correct would be sending
         * them to through to libIDL where they will get tokenized and made
         * available to backends that want to generate docs).  When we next
         * upgrade libIDL versions, it'll handle comments for us, which will
         * help a lot.
         *
         * XXX const string foo = "/\*" will just screw us horribly.
         */

        /*
         * Order is important, so that you can have /\* comments and
         * #includes within raw sections, and so that you can comment out
         * #includes.
         */

        /* XXX should check for errors here, too */
        rv = NextIsRaw(data, &start, (int *)&len);
        if (rv == -1) return -1;
        if (!rv) {
            rv = NextIsComment(data, &start, (int *)&len);
            if (rv == -1) return -1;
            if (!rv) {
                /* includes might need to push a new file */
                rv = NextIsInclude(stack, &start, (int *)&len);
                if (rv == -1) return -1;
                if (!rv)
                    /* FindSpecial can't fail? */
                    FindSpecial(data, &start, (int *)&len);
            }
        }

        assert(start);
        copy = MIN(len, (int) cb_data->fill.max_size);

        if (copy) {
            memcpy(cb_data->fill.buffer, start, copy);
            data->point = start + copy;
        } else {
            cb_data->fill.buffer[0] = ' ';
            copy = 1;
        }
#ifdef DEBUG_shaver_input
        fprintf(stderr, "COPYING %d->%.*s<-COPYING\n", copy, (int)copy,
                cb_data->fill.buffer);
#endif
        if (tracefile)
            fwrite(cb_data->fill.buffer, copy, 1, tracefile);

        return copy;

      case IDL_INPUT_REASON_ABORT:
      case IDL_INPUT_REASON_FINISH:
        if (data->input && data->input != stdin)
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
                  char *file_basename, ModeData *mode)
{
    char *tmp, *outname, *mode_outname;
    IDL_tree top;
    TreeState state;
    int rv;
    struct input_callback_stack stack;
    gboolean ok;
    char *fopen_mode;

    /* Initialize so that stack->top, etc. doesn't come up as garbage. */
    memset(&stack, 0, sizeof(struct input_callback_stack));

    stack.includes = g_hash_table_new(g_str_hash, g_str_equal);
    if (!stack.includes) {
        fprintf(stderr, "failed to create hashtable (EOM?)\n");
        return 0;
    }

    state.basename = xpidl_strdup(filename);
    tmp = strrchr(state.basename, '.');
    if (tmp)
        *tmp = '\0';

#ifdef XP_MAC
	  // on the Mac, we let CodeWarrior tell us where to put the output file.
    if (!file_basename) {
        outname = xpidl_strdup(filename);
        tmp = strrchr(outname, '.');
        if (tmp != NULL) *tmp = '\0';
    } else {
        outname = xpidl_strdup(file_basename);
    }
#else
    if (!file_basename)
        outname = xpidl_strdup(state.basename);
    else
        outname = xpidl_strdup(file_basename);
#endif

    /* so we don't include it again! */
    g_hash_table_insert(stack.includes, filename, state.basename);

    stack.include_path = include_path;

    rv = IDL_parse_filename_with_input(filename, input_callback, &stack,
                                       msg_callback, &top,
                                       &state.ns,
                                       IDLF_IGNORE_FORWARDS |
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

/*
 * Print an iid to into a supplied buffer; the buffer should be at least
 * UUID_LENGTH bytes.
 */
gboolean
xpidl_sprint_iid(struct nsID *id, char iidbuf[])
{
    int printed;

    printed = sprintf(iidbuf,
                       "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                       (PRUint32) id->m0, (PRUint32) id->m1,(PRUint32) id->m2,
                       (PRUint32) id->m3[0], (PRUint32) id->m3[1],
                       (PRUint32) id->m3[2], (PRUint32) id->m3[3],
                       (PRUint32) id->m3[4], (PRUint32) id->m3[5],
                       (PRUint32) id->m3[6], (PRUint32) id->m3[7]);
    return (printed == 36);
}

/* We only parse the {}-less format.  (xpidl_header never has, so we're safe.) */
static const char nsIDFmt2[] =
  "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x";

/*
 * Parse a uuid string into an nsID struct.  We cannot link against libxpcom,
 * so we re-implement nsID::Parse here.
 */
gboolean
xpidl_parse_iid(struct nsID *id, const char *str)
{
    PRInt32 count = 0;
    PRInt32 n1, n2, n3[8];
    PRInt32 n0, i;

    assert(str != NULL);
    
    if (strlen(str) != 36) {
        return FALSE;
    }
     
#ifdef DEBUG_shaver_iid
    fprintf(stderr, "parsing iid   %s\n", str);
#endif

    count = sscanf(str, nsIDFmt2,
                   &n0, &n1, &n2,
                   &n3[0],&n3[1],&n3[2],&n3[3],
                   &n3[4],&n3[5],&n3[6],&n3[7]);

    id->m0 = (PRInt32) n0;
    id->m1 = (PRInt16) n1;
    id->m2 = (PRInt16) n2;
    for (i = 0; i < 8; i++) {
      id->m3[i] = (PRInt8) n3[i];
    }

#ifdef DEBUG_shaver_iid
    if (count == 11) {
        fprintf(stderr, "IID parsed to ");
        print_IID(id, stderr);
        fputs("\n", stderr);
    }
#endif
    return (gboolean)(count == 11);
}
