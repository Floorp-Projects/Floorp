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
 *       Michael Ang <mang@subcarrier.org>
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
 * Common IDL-processing code.
 */

#include "xpidl.h"

static gboolean parsed_empty_file;

/*
 * The bulk of the generation happens here.
 */
gboolean
xpidl_process_node(TreeState *state)
{
    gint type;
    nodeHandler *dispatch, handler;

    XPT_ASSERT(state->tree);
    type = IDL_NODE_TYPE(state->tree);

    if ((dispatch = state->dispatch) && (handler = dispatch[type]))
        return handler(state);
    return TRUE;
}

#if defined(XP_MAC) && defined(XPIDL_PLUGIN)
extern void mac_warning(const char* warning_message);
#endif

static int
msg_callback(int level, int num, int line, const char *file,
             const char *message)
{
    char *warning_message;

    /*
     * Egregious hack to permit empty files.
     * XXX libIDL needs an API to detect this case robustly.
     */
    if (0 == strcmp(message, "File empty after optimization")) {
        parsed_empty_file = TRUE;
        return 1;
    }

    if (!file)
        file = "<unknown file>";
    warning_message = g_strdup_printf("%s:%d: %s\n", file, line, message);

#if defined(XP_MAC) && defined(XPIDL_PLUGIN)
    mac_warning(warning_message);
#else
    fputs(warning_message, stderr);
#endif

    g_free(warning_message);
    return 1;
}

/*
 * To keep track of the state associated with a given input file.  The 'next'
 * field lets us maintain a stack of input files.
 */
typedef struct input_data {
    char *filename;             /* where did I come from? */
    unsigned int lineno;        /* last lineno processed */
    char *buf;                  /* contents of file */
    char *point;                /* next char to feed to libIDL */
    char *max;                  /* 1 past last char in buf */
    struct input_data *next;    /* file from which we were included */
} input_data;

/*
 * Passed to us by libIDL.  Holds global information and the current stack of
 * include files.
 */
typedef struct input_callback_state {
    struct input_data *input_stack; /* linked list of input_data */   
    GHashTable *already_included;   /* to prevent redundant includes */
    IncludePathEntry *include_path; /* search path for included files */
    GSList *base_includes;          /* to accumulate #includes from *first* file;
                                     * for passing thru TreeState to
                                     * xpidl_header backend. */
} input_callback_state;

static FILE *
fopen_from_includes(const char *filename, const char *mode,
                    IncludePathEntry *include_path)
{
    IncludePathEntry *current_path = include_path;
    char *pathname;
    FILE *inputfile;
    if (!strcmp(filename, "-"))
        return stdin;

    if (filename[0] != '/') {
        while (current_path) {
            pathname = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s",
                                       current_path->directory, filename);
            if (!pathname)
                return NULL;
            inputfile = fopen(pathname, mode);
            g_free(pathname);
            if (inputfile)
                return inputfile;
            current_path = current_path->next;
        }
    } else {
        inputfile = fopen(filename, mode);
        if (inputfile)
            return inputfile;
    }
    return NULL;
}

#if defined(XP_MAC) && defined(XPIDL_PLUGIN)
extern FILE* mac_fopen(const char* filename, const char *mode);
#endif

static input_data *
new_input_data(const char *filename, IncludePathEntry *include_path)
{
    input_data *new_data;
    FILE *inputfile;
    char *buffer = NULL;
    size_t offset = 0;
    size_t buffer_size;
#ifdef XP_MAC
    size_t i;
#endif

#if defined(XP_MAC) && defined(XPIDL_PLUGIN)
    /* on Mac, fopen knows how to find files. */
    inputfile = fopen(filename, "r");
#elif defined(XP_OS2)
    // if filename is fully qualified (starts with driver letter), then
    // just call fopen();  else, go with fopen_from_includes()
    if( filename[1] == ':' )
      inputfile = fopen(filename, "r");
    else
      inputfile = fopen_from_includes(filename, "r", include_path);
#else
    inputfile = fopen_from_includes(filename, "r", include_path);
#endif

    if (!inputfile)
        return NULL;

    /*
     * Rather than try to keep track of many different varieties of state
     * around the boundaries of a circular buffer, we just read in the entire
     * file.
     *
     * We iteratively grow the buffer here; an alternative would be to use
     * stat to find the exact buffer size we need, as xpt_dump does.
     */
    for (buffer_size = 8191; ; buffer_size *= 2) {
        size_t just_read;
        buffer = realloc(buffer, buffer_size + 1); /* +1 for trailing nul */
        just_read = fread(buffer + offset, 1, buffer_size - offset, inputfile);
        if (ferror(inputfile))
            return NULL;

        if (just_read < buffer_size - offset || just_read == 0) {
            /* Done reading. */
            offset += just_read;
            break;
        }
        offset += just_read;
    }
    fclose(inputfile);

#ifdef XP_MAC
    /*
     * libIDL doesn't speak '\r' properly - always make sure lines end with
     * '\n'.
     */
    for (i = 0; i < offset; i++) {
        if (buffer[i] == '\r')
            buffer[i] = '\n';
    }
#endif

    new_data = xpidl_malloc(sizeof (struct input_data));
    new_data->point = new_data->buf = buffer;
    new_data->max = buffer + offset;
    *new_data->max = '\0';
    new_data->filename = xpidl_strdup(filename);
    /* libIDL expects the line number to be that of the *next* line */
    new_data->lineno = 2;
    new_data->next = NULL;

    return new_data;
}

/* process pending raw section */
static int
NextIsRaw(input_data *data, char **startp, int *lenp)
{
    char *end, *start;

    /*
     * XXXmccabe still needed: an in_raw flag to handle the case where we're in
     * a raw block, but haven't managed to copy it all to xpidl.  This will
     * happen when we have a raw block larger than
     * IDL_input_data->fill.max_size (currently 8192.)
     */
    if (!(data->point[0] == '%' && data->point[1] == '{'))
        return 0;
        
    start = *startp = data->point;
    
    end = NULL;
    while (start < data->max && (end = strstr(start, "%}"))) {
        if (end[-1] == '\r' ||
            end[-1] == '\n')
            break;
        start = end + 1;
    }

    if (end && start < data->max) {
        *lenp = end - data->point + 2;
        return 1;
    } else {
        const char *filename;
        int lineno;

        IDL_file_get(&filename, &lineno);
        msg_callback(IDL_ERROR, 0, lineno, filename,
                     "unterminated %{ block");
        return -1;
    }
}

/* process pending comment */
static int
NextIsComment(input_data *data, char **startp, int *lenp)
{
    char *end;

    if (!(data->point[0] == '/' && data->point[1] == '*'))
        return 0;

    end = strstr(data->point, "*/");
    *lenp = 0;
    if (end) {
        int skippedLines = 0;
        char *tempPoint;
        
        /* get current lineno */
        IDL_file_get(NULL,(int *)&data->lineno);

        /* get line count */
        for (tempPoint = data->point; tempPoint < end; tempPoint++) {
            if (*tempPoint == '\n')
                skippedLines++;
        }

        data->lineno += skippedLines;
        IDL_file_set(data->filename, (int)data->lineno);
        
        *startp = end + 2;

        /* If it's a ** comment, tell libIDL about it. */
        if (data->point[2] == '*') {
            /* hack termination.  +2 to get past '*' '/' */
            char t = *(end + 2);
            *(end + 2) = '\0';
            IDL_queue_new_ident_comment(data->point);
            *(end + 2) = t;
        }

        data->point = *startp; /* XXXmccabe move this out of function? */
        return 1;
    } else {
        const char *filename;
        int lineno;

        IDL_file_get(&filename, &lineno);
        msg_callback(IDL_ERROR, 0, lineno, filename,
                     "unterminated comment");
        return -1;
    }
}

static int
NextIsInclude(input_callback_state *callback_state, char **startp,
              int *lenp)
{
    input_data *data = callback_state->input_stack;
    input_data *new_data;
    char *filename, *end;
    const char *scratch;

    /* process the #include that we're in now */
    if (strncmp(data->point, "#include \"", 10)) {
        return 0;
    }
    
    filename = data->point + 10; /* skip #include " */
    XPT_ASSERT(filename < data->max);
    end = filename;
    while (end < data->max) {
        if (*end == '\"' || *end == '\n' || *end == '\r')
            break;
        end++;
    }

    if (*end != '\"') {
        /*
         * Didn't find end of include file.  Scan 'til next whitespace to find
         * some reasonable approximation of the filename, and use it to report
         * an error.
         */

        end = filename;
        while (end < data->max) {
            if (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')
                break;
            end++;
        }
        *end = '\0';
        
        /* make sure we have accurate line info */
        IDL_file_get(&scratch, (int *)&data->lineno);
        fprintf(stderr,
                "%s:%d: didn't find end of quoted include name \"%s\n",
                scratch, data->lineno, filename);
        return -1;
    }

    *end = '\0';
    *startp = end + 1;

    if (data->next == NULL) {
        /*
         * If we're in the initial file, add this filename to the list
         * of filenames to be turned into #include "filename.h"
         * directives in xpidl_header.c.  We do it here rather than in the
         * block below so it still gets added to the list even if it's
         * already been recursively included from some other file.
         */
        char *filename_cp = xpidl_strdup(filename);
        
        /* note that g_slist_append accepts and likes null as list-start. */
        callback_state->base_includes =
            g_slist_append(callback_state->base_includes, filename_cp);
    }

    /* store offset for when we pop, or if we skip this one */
    data->point = *startp;

    if (!g_hash_table_lookup(callback_state->already_included, filename)) {
        filename = xpidl_strdup(filename);
        g_hash_table_insert(callback_state->already_included,
                            filename, (void *)TRUE);
        new_data = new_input_data(filename, callback_state->include_path);
        if (!new_data) {
            char *error_message;
            IDL_file_get(&scratch, (int *)&data->lineno);
            error_message =
                g_strdup_printf("can't open included file %s for reading\n",
                                filename);
            msg_callback(IDL_ERROR, 0,
                         data->lineno, scratch, error_message);
            g_free(error_message);
            return -1;
        }

        new_data->next = data;
        /* tell libIDL to exclude this IDL from the toplevel tree */
        IDL_inhibit_push();
        IDL_file_get(&scratch, (int *)&data->lineno);
        callback_state->input_stack = new_data;
        IDL_file_set(new_data->filename, (int)new_data->lineno);
    }

    *lenp = 0;               /* this is magic, see the comment below */
    return 1;
}    

static void
FindSpecial(input_data *data, char **startp, int *lenp)
{
    char *point = data->point;

    /* magic sequences are:
     * "%{"               raw block
     * "/\*"              comment
     * "#include \""      include
     * The first and last want a newline [\r\n] before, or the start of the
     * file.
     */

#define LINE_START(data, point) (point == data->buf ||                       \
                                 (point > data->point &&                     \
                                  (point[-1] == '\r' || point[-1] == '\n')))
                                                 
    while (point < data->max) {
        if (point[0] == '/' && point[1] == '*')
            break;
        if (LINE_START(data, point)) {
            if (point[0] == '%' && point[1] == '{')
                break;
            if (point[0] == '#' && !strncmp(point + 1, "include \"", 9))
                break;
        }
        point++;
    }

#undef LINE_START

    *startp = data->point;
    *lenp = point - data->point;
}

/* set this with a debugger to see exactly what libIDL sees */
static FILE *tracefile;

static int
input_callback(IDL_input_reason reason, union IDL_input_data *cb_data,
               gpointer user_data)
{
    input_callback_state *callback_state = user_data;
    input_data *data = callback_state->input_stack;
    input_data *new_data = NULL;
    unsigned int len, copy;
    int rv;
    char *start;

    switch(reason) {
      case IDL_INPUT_REASON_INIT:
        if (data == NULL || data->next == NULL) {
            /*
             * This is the first file being processed.  As it's the target
             * file, we only look for it in the first entry in the include
             * path, which we assume to be the current directory.
             */

            /* XXXmccabe proper assumption?  Do we handle files in other
               directories? */

            IncludePathEntry first_entry;

            first_entry.directory = callback_state->include_path->directory;
            first_entry.next = NULL;

            new_data = new_input_data(cb_data->init.filename,
                                               &first_entry);
        } else {
            new_data = new_input_data(cb_data->init.filename,
                                               callback_state->include_path);
        }

        if (!new_data)
            return -1;

        IDL_file_set(new_data->filename, (int)new_data->lineno);
        callback_state->input_stack = new_data;
        return 0;

      case IDL_INPUT_REASON_FILL:
        start = NULL;
        len = 0;

        while (data->point >= data->max) {
            if (!data->next)
                return 0;

            /* Current file is done; revert to including file */
            callback_state->input_stack = data->next;
            free(data->filename);
            free(data->buf);
            free(data);
            data = callback_state->input_stack;

            IDL_file_set(data->filename, (int)data->lineno);
            IDL_inhibit_pop();
        }
        
        /*
         * Now we scan for sequences which require special attention:
         *   \n#include                   begins an include statement
         *   \n%{                         begins a raw-source block
         *   /\*                          begins a comment
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
         *     process that special -
         *         - raw: send it to libIDL, and don't look inside for specials
         *         - comments: adjust point and start over
         *         - includes: push new input_data struct for included file, and
         *           start over
         * } else {
         *     scan for next special
         *     send data up to that special to libIDL
         * }
         *
         * If len is set to zero, it is a sentinel value indicating we a comment
         * or include was found, and parsing should start over.
         *
         * XXX const string foo = "/\*" will just screw us horribly.
         * Hm but.  We could treat strings as we treat raw blocks, eh?
         */

        /*
         * Order is important, so that you can have /\* comments and
         * #includes within raw sections, and so that you can comment out
         * #includes.
         */
        rv = NextIsRaw(data, &start, (int *)&len);
        if (rv == -1) return -1;
        if (!rv) {
            /*
             * When NextIsComment succeeds, it returns a 0 len (requesting a
             * restart) and adjusts data->point to pick up after the comment.
             */
            rv = NextIsComment(data, &start, (int *)&len);
            if (rv == -1) return -1;
            if (!rv) {
                /*
                 * NextIsInclude might push a new input_data struct; if so, it
                 * will return a 0 len, letting the callback pick up the new
                 * file the next time around.
                 */
                rv = NextIsInclude(callback_state, &start, (int *)&len);
                if (rv == -1) return -1;
                if (!rv)
                    FindSpecial(data, &start, (int *)&len);
            }
        }

        if (len == 0) {
            /*
             * len == 0 is a sentinel value that means we found a comment or
             * include.  If we found a comment, point has been adjusted to
             * point past the comment.  If we found an include, a new input_data
             * has been pushed.  In both cases, calling the input_callback again
             * will pick up the new state.
             */
            return input_callback(reason, cb_data, user_data);
        }

        copy = MIN(len, (unsigned int) cb_data->fill.max_size);
        memcpy(cb_data->fill.buffer, start, copy);
        data->point = start + copy;

        if (tracefile)
            fwrite(cb_data->fill.buffer, copy, 1, tracefile);

        return copy;

      case IDL_INPUT_REASON_ABORT:
      case IDL_INPUT_REASON_FINISH:
        while (data != NULL) {
            input_data *next;

            next = data->next;
            free(data->filename);
            free(data->buf);
            free(data);
            data = next;
        }
        return 0;

      default:
        g_error("unknown input reason %d!", reason);
        return -1;
    }
}

static void
free_ghash_key(gpointer key, gpointer value, gpointer user_data)
{
    /* We're only storing TRUE in the value... */
    free(key);
}

static void
free_gslist_data(gpointer data, gpointer user_data)
{
    free(data);
}

/* Pick up unlink. */
#ifdef XP_UNIX
#include <unistd.h>
#elif XP_WIN
/* We get it from stdio.h. */
#endif

int
xpidl_process_idl(char *filename, IncludePathEntry *include_path,
                  char *file_basename, ModeData *mode)
{
    char *tmp, *outname, *real_outname = NULL;
    IDL_tree top;
    TreeState state;
    int rv;
    input_callback_state callback_state;
    gboolean ok = TRUE;
    backend *emitter;

    callback_state.input_stack = NULL;
    callback_state.base_includes = NULL;
    callback_state.include_path = include_path;
    callback_state.already_included = g_hash_table_new(g_str_hash, g_str_equal);

    if (!callback_state.already_included) {
        fprintf(stderr, "failed to create hashtable.  out of memory?\n");
        return 0;
    }

    state.basename = xpidl_strdup(filename);

    /* if basename has an .extension, truncate it. */
    tmp = strrchr(state.basename, '.');
    if (tmp)
        *tmp = '\0';

    if (!file_basename)
        outname = xpidl_strdup(state.basename);
    else
        outname = xpidl_strdup(file_basename);

    /* so we don't include it again! */
    g_hash_table_insert(callback_state.already_included,
                        xpidl_strdup(filename), (void *)TRUE);

    parsed_empty_file = FALSE;

    rv = IDL_parse_filename_with_input(filename, input_callback, &callback_state,
                                       msg_callback, &top,
                                       &state.ns,
                                       IDLF_IGNORE_FORWARDS |
                                       IDLF_XPIDL,
                                       enable_warnings ? IDL_WARNING1 :
                                       IDL_ERROR);
    if (parsed_empty_file) {
        /*
         * If we've detected (via hack in msg_callback) that libIDL returned
         * failure because it found a file with no IDL, set the parse tree to
         * null and proceed.  Allowing this is useful to permit .idl files that
         * collect #includes.
         */
        top = NULL;
        state.ns = NULL;
    } else if (rv != IDL_SUCCESS) {
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

    /* so xpidl_header.c can use it to generate a list of #include directives */
    state.base_includes = callback_state.base_includes;

    emitter = mode->factory();
    state.dispatch = emitter->dispatch_table;

    if (strcmp(outname, "-")) {
        const char *fopen_mode;
        const char *out_basename;
#if defined(XP_UNIX)
        const char os_separator = '/';
#elif defined(XP_WIN)
        const char os_separator = '\\';
#endif

        /* explicit_output_filename can't be true without a filename */
        if (explicit_output_filename) {
            real_outname = g_strdup(outname);
        } else {

#if defined(XP_UNIX) || defined(XP_WIN)
            tmp = strrchr(outname, os_separator);
            if (!file_basename && tmp) {
                out_basename = tmp + 1;
            } else {
                out_basename = outname;
            }
#else
            out_basename = outname;
#endif
            real_outname = g_strdup_printf("%s.%s", out_basename, mode->suffix);
        }

        /* Use binary write for typelib mode */
        fopen_mode = (strcmp(mode->mode, "typelib")) ? "w" : "wb";
        state.file = fopen(real_outname, fopen_mode);
        if (!state.file) {
            perror("error opening output file");
            return 0;
        }
    } else {
        state.file = stdout;
    }
    state.tree = top;

    if (emitter->emit_prolog)
        emitter->emit_prolog(&state);
    if (state.tree) /* Only if we have a tree to process. */
        ok = xpidl_process_node(&state);
    if (emitter->emit_epilog)
        emitter->emit_epilog(&state);

    if (state.file != stdout)
        fclose(state.file);
    free(state.basename);
    free(outname);
    g_hash_table_foreach(callback_state.already_included, free_ghash_key, NULL);
    g_hash_table_destroy(callback_state.already_included);
    g_slist_foreach(callback_state.base_includes, free_gslist_data, NULL);

    if (state.ns)
        IDL_ns_free(state.ns);
    if (top)
        IDL_tree_free(top);

    if (real_outname != NULL) {
        /*
         * Delete partial output file on failure.  (Mac does this in the plugin
         * driver code, if the compiler returns failure.)
         */
#if defined(XP_UNIX) || defined(XP_WIN)
        if (!ok)
            unlink(real_outname);
#endif
        g_free(real_outname);
    }

    return ok;
}

/*
 * Our own version of IDL_tree_warning, which we use when IDL_tree_warning
 * would crash on us.
 */
void
xpidl_tree_warning(IDL_tree p, int level, const char *fmt, ...)
{
    va_list ap;
    char *msg, *file;
    int lineno;

    /* XXX need to check against __IDL_max_msg_level, no accessor */
    va_start(ap, fmt);
    msg = g_strdup_vprintf(fmt, ap);

    if (p) {
        file = p->_file;
        lineno = p->_line;
    } else {
        file = NULL;
        lineno = 0;
    }

    /* call our message callback, like IDL_tree_warning would */
    msg_callback(level, 0, lineno, file, msg);
    g_free(msg);
    va_end(ap);
}
