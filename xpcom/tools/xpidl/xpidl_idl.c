/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Pass 1 generates #includes for headers.
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
  fprintf(stderr, "%s:%d: %s\n", file, line, message);
  return 1;
}

#define INPUT_BUF_CHUNK		8192

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
	if (data->input && data->buf) {
      data->len = sprintf(data->buf, "# 1 \"%s\"\n",
                          cb_data->init.filename);
      return 0;
	}
	return -1;
	
   case IDL_INPUT_REASON_FILL:
	avail = data->buf + data->len - data->point;
	assert(avail >= 0);

	if (!avail) {
      char *comment_start = NULL, *ptr;
      data->point = data->buf;

      /* fill the buffer */
	fill_buffer:
      data->len = fread(data->buf, 1, data->max, data->input);
      if (!data->len) {
		if (ferror(data->input))
          return -1;
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
  IDL_ns_free(state.ns);
  IDL_tree_free(top);
    
  return 1;
}
