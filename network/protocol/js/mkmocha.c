/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

/* Please leave at the top for windows precompiled headers */
#include "xp.h"
#include "plstr.h"
#include "prmem.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"

#include <stddef.h>
#include <memory.h>
#include <time.h>
#include "net.h"
#include "libmocha.h"
#include "jsurl.h"
#include "libevent.h"
#include "fe_proto.h"
#include "pa_tags.h"	/* included via -I../libparse */
#include "libi18n.h"    /* unicode */
#include "layout.h"	/* for lo_ContextToCell only */

extern char lm_unknown_origin_str[];

extern int MK_OUT_OF_MEMORY;
extern int MK_MALFORMED_URL_ERROR;

typedef struct {
    char *	  buffer;
    size_t	  offset;
    size_t  	  length;
    MWContext	* context;
    char	* content_type;
    int16	  char_set;
} MochaStream;

typedef struct {
    int32             ref_count;
    ActiveEntry     * active_entry;
    PRPackedBool      is_valid;
    PRPackedBool      eval_what;
    PRPackedBool      single_shot;
    PRPackedBool      polling;
    char            * str;
    size_t            len;
    MWContext       * context;
    int               status;
    char            * wysiwyg_url;
    char	    * base_href;
    NET_StreamClass * stream;
} MochaConData;

#define HOLD_CON_DATA(con_data) ((con_data)->ref_count++)
#define DROP_CON_DATA(con_data) {                                             \
    if (--(con_data)->ref_count == 0)                                         \
	free_con_data(con_data);                                              \
}

static void
free_con_data(MochaConData * con_data)
{
    PR_FREEIF(con_data->str);
    PR_FREEIF(con_data->wysiwyg_url);
    PR_FREEIF(con_data->base_href);
    PR_Free(con_data);
}

#define START_POLLING(ae, con_data) {                                         \
    (con_data)->polling = TRUE;                                               \
    NET_SetCallNetlibAllTheTime((ae)->window_id, "mkmocha");                              \
}

#define STOP_POLLING(ae, con_data) {                                          \
	NET_ClearCallNetlibAllTheTime((ae)->window_id, "mkmocha");                        \
    (con_data)->polling = FALSE;                                              \
}

/* forward decl */
PRIVATE int32 net_ProcessMocha(ActiveEntry * ae);

/*
 * Add the new bits to our buffer
 */
PRIVATE int
mocha_process(NET_StreamClass *stream, const char *str, int32 len)
{
    MochaStream * mocha_stream = (MochaStream *) stream->data_object;

    mocha_stream->length += len;
    if (!mocha_stream->buffer) {
        mocha_stream->buffer = (char *)PR_Malloc(mocha_stream->length);
    } 
    else {
        mocha_stream->buffer = (char *)PR_Realloc(mocha_stream->buffer, 
						  mocha_stream->length);
    }
    if (!mocha_stream->buffer) {
        return MK_OUT_OF_MEMORY;
    }
    memcpy(mocha_stream->buffer + mocha_stream->offset, str, len);
    mocha_stream->offset += len;
    return len;
}

PRIVATE unsigned int
mocha_ready(NET_StreamClass *stream)
{
    return MAX_WRITE_READY;  /* always ready for writing */
}


/*
 * All of the processing for this needs to be done in the mocha thread
 */
PRIVATE void 
mocha_complete(NET_StreamClass *stream)
{
    void * data;
    JSBool isUnicode = JS_FALSE;

    MochaStream * mocha_stream = (MochaStream *) stream->data_object;

    if (mocha_stream->char_set != CS_DEFAULT) {
	uint32 len;
	INTL_Unicode * unicode;

	/* find out how many unicode characters we'll end up with */
	len = INTL_TextToUnicodeLen(mocha_stream->char_set, 
				    (unsigned char *) mocha_stream->buffer,
				    mocha_stream->length);
	unicode = PR_Malloc(sizeof(INTL_Unicode) * len);
	if (!unicode)
	    return;

	/* do the conversion */
	mocha_stream->length = INTL_TextToUnicode(mocha_stream->char_set,
						  (unsigned char *) mocha_stream->buffer,
						  mocha_stream->length,
						  unicode,
						  len);

	data = unicode;
	isUnicode = JS_TRUE;

	PR_Free(mocha_stream->buffer);
	mocha_stream->buffer = NULL;
    }
    else {
	data = mocha_stream->buffer;
    }

    /* this will grab ownership of data */
    ET_MochaStreamComplete(mocha_stream->context, data, 
			   mocha_stream->length, 
			   mocha_stream->content_type,
			   isUnicode);

    PR_FREEIF(mocha_stream->content_type);
    PR_Free(mocha_stream);

}

PRIVATE void 
mocha_abort(NET_StreamClass *stream, int status)
{
    MochaStream * mocha_stream = (MochaStream *) stream->data_object;

    ET_MochaStreamAbort(mocha_stream->context, status);
    PR_Free(mocha_stream->buffer);
	PR_FREEIF(mocha_stream->content_type);
    PR_Free(mocha_stream);
}

int16
net_check_for_charset(URL_Struct *url_struct)
{
    int i, max;
    char *key, *value;
    static char charset[] = "charset=";
    int len = PL_strlen(charset);
    
    max = url_struct->all_headers.empty_index;

    for (i = 0; i < max; i++) {
	key = url_struct->all_headers.key[i];

	/* keep looking until we find the content type one */
	if (PL_strcasecmp(key, "Content-type"))
	    continue;

	value = url_struct->all_headers.value[i];

	value = strtok(value, ";");
	while (value) {
	    value = XP_StripLine(value);

	    if (!PL_strncasecmp(value, charset, len)) {
		value += len;
		value = XP_StripLine(value);
		return (INTL_CharSetNameToID(value));
	    }

	    /* move to next arg */
	    value = strtok(NULL, ";");

	}

	/* found content-type but no charset */
	return CS_DEFAULT;

    }

    /* didn't find content-type */
    return CS_DEFAULT;
}

static char *
getOriginFromURLStruct(MWContext *context, URL_Struct *url_struct) 
{
    char *referer;
    /*
     * Look in url_struct->origin_url for this javascript: URL's
     * original referrer.
     *
     * In the basis case, a javascript: or *.js URL targets a context
     * from a (the same, or not) context loaded with a non-javascript URL.
     * The referring document's URL is in url_struct->referer and we
     * duplicate it as origin_url.  If we find a non-null origin_url
     * later, we know by induction where it came from.
     */
    referer = url_struct->origin_url;
    if (referer == NULL) {
        /*
         * url_struct->referer (sic) tells the URL of the page in
         * which a javascript: or *.js link or form was clicked or submitted.
         * If it's non-null, but the context is a frame cell that's
         * being (re-)created for this load, don't use referer as the
         * decoder's source URL for the evaluation, because the FE has
         * arbitrarily set it to the top frameset's URL.  Instead, use
         * the immediate parent frameset context's wysiwyg URL to get
         * the origin of the generator (or the parent's address URL if
         * not wysiwyg).
         *
         * If referer is null, the user typed this javascript: URL or
         * its frameset's URL directly into the Location toolbar.
         */
        referer = url_struct->referer;
        if (referer) {
	    lo_GridRec *grid = NULL;

	    if (context->is_grid_cell &&
	        !lo_ContextToCell(context, FALSE, &grid)) {
	        /*
	         * Context is a frame being (re)created: veto referer.
	         * The javascript: or *.js URL can't be a LAYER SRC= URL in a
	         * frame because the frame's cell must point to its
	         * context by the time the first tag (even LAYER) is
	         * processed by layout.
	         */
	        referer = NULL;
	    }
        }
        if (!referer) {
	    History_entry *he;

	    if (context->grid_parent) {
	        /*
	         * If grid parent, use its history entry to get the
	         * wysiwyg or real URL, which tells the subject origin
	         * of (any code in it that may have generated) this
	         * javascript: or *.js URL open.  If no grid parent, this must
	         * be a javascript: or *.js URL that was typed directly into
	         * Location.
	         */
	        context = context->grid_parent;
	    }
	    he = SHIST_GetCurrent(&context->hist);
	    if (!he) {
	        referer = lm_unknown_origin_str;
	    } else {
	        referer = he->wysiwyg_url;
	        if (!referer)
		    referer = he->address;
	    }
        }
    }

    PR_ASSERT(referer);
    referer = PL_strdup(referer);
    if (!referer) {
	return NULL;
    }
    url_struct->origin_url = referer;
    return referer;
}

NET_StreamClass *
NET_CreateMochaConverter(FO_Present_Types format_out,
                         void             *data_object,
                         URL_Struct       *url_struct,
                         MWContext        *context)
{
    MochaStream * mocha_stream;
    NET_StreamClass *stream;
    char *origin;

    mocha_stream = (MochaStream *) PR_NEWZAP(MochaStream);
    if (!mocha_stream)
	return NULL;

    mocha_stream->context = context;
    mocha_stream->content_type = PL_strdup(url_struct->content_type);
    mocha_stream->char_set = net_check_for_charset(url_struct);

    /* Get the origin from the URL struct. We don't have to free origin
     * here because url_struct->origin_url owns it.
     */
    origin = getOriginFromURLStruct(context, url_struct);
    if (origin == NULL)
        return NULL;

    if (NET_URL_Type(url_struct->address) == FILE_TYPE_URL &&
        NET_URL_Type(origin) != FILE_TYPE_URL)
    {
        /*
         * Don't allow loading a file: URL from a non-file URL to
         * prevent privacy attacks against the local machine from
         * web pages.
         */
        return NULL;
    }

    stream = NET_NewStream("mocha", mocha_process, mocha_complete, 
			   mocha_abort, mocha_ready, mocha_stream, 
			   context); 
    return stream;
}

PRIVATE void
mk_mocha_eval_exit_fn(void * data, char * str, size_t len, char * wysiwyg_url, 
		      char * base_href, Bool valid)
{
    MochaConData * con_data = (MochaConData *) data;

    if (!valid) {
        con_data->status = MK_MALFORMED_URL_ERROR;
        con_data->is_valid = TRUE;
        return;
    }
    if (str == NULL) {
        con_data->status = MK_DATA_LOADED;
        con_data->is_valid = TRUE;
        return;
    }

    con_data->base_href = base_href;
    con_data->wysiwyg_url = wysiwyg_url;
    con_data->str = str;
    con_data->len = len;
    con_data->is_valid = TRUE;

}

/*
 * Handle both 'mocha:<stuff>' urls and the mocha type-in widget
 */
MODULE_PRIVATE int32
net_MochaLoad(ActiveEntry *ae)
{
    MWContext *context;
    URL_Struct *url_struct;
    char *what;
    Bool eval_what, single_shot;
    MochaConData * con_data;

    context = ae->window_id;
    url_struct = ae->URL_s;
    what = PL_strchr(url_struct->address, ':');
    PR_ASSERT(what);
    what++;
    eval_what = FALSE;
    single_shot = (*what != '?');

    if (single_shot) {
        /* Don't use an existing Mocha output window's stream. */
        if (*what == '\0') {
            /* Generate two grid cells, one for output and one for input. */
            what = PR_smprintf("<frameset rows=\"75%%,25%%\">\n"
                               "<frame name=MochaOutput src=about:blank>\n"
                               "<frame name=MochaInput src=%.*s#input>\n"
                               "</frameset>",
                               what - url_struct->address,
                               url_struct->address);
        } else if (!PL_strcmp(what, "#input")) {
            /* The input cell contains a form with one magic isindex field. */
            what = PR_smprintf("<b>%.*s typein</b>\n"
                               "<form action=%.*s target=MochaOutput"
                               " onsubmit='this.isindex.focus()'>\n"
                               "<input name=isindex size=60>\n"
                               "</form>",
                               what - url_struct->address - 1,
                               url_struct->address,
                               what - url_struct->address,
                               url_struct->address);
            url_struct->internal_url = TRUE;
        } else {
            eval_what = TRUE;
        }
    } else {
        /* Use the Mocha output window if it exists. */
        url_struct->auto_scroll = 1000;

        /* Skip the leading question-mark and clean up the string. */
        what++;
        NET_PlusToSpace(what);
        NET_UnEscape(what);
        eval_what = TRUE;
    }

    /* something got hosed.  bail */
    if (!what) {
        ae->status = MK_OUT_OF_MEMORY;
	return -1;
    }

    /* make space for the connection data */
    con_data = PR_NEWZAP(MochaConData);
    if (!con_data) {
        ae->status = MK_OUT_OF_MEMORY;
        return -1;
    }

    /* remember for next time */
    con_data->ref_count = 1;
    con_data->active_entry = ae;
    ae->con_data = con_data;

    /* set up some state so we remember where we are */
    con_data->eval_what = eval_what;
    con_data->single_shot = single_shot;
    con_data->context = context;

    /* fake out netlib so we don't select on the socket id */
    ae->socket = NULL;
    ae->local_file = TRUE;

    if (eval_what) {
	char *referer;
        JSPrincipals *principals;
	ETEvalStuff *stuff;

        referer = getOriginFromURLStruct(context, url_struct);
	if (!referer) {
	    ae->status = MK_OUT_OF_MEMORY;
	    return -1;
	}

	principals = LM_NewJSPrincipals(NULL, NULL, referer);
	if (!principals) {
	    ae->status = MK_OUT_OF_MEMORY;
	    return -1;
	}

        /* 
         * send the buffer off to be evaluated 
         */
	stuff = (ETEvalStuff *) PR_NEWZAP(ETEvalStuff);
	if (!stuff) {
	    ae->status = MK_OUT_OF_MEMORY;
	    return -1;
	}

	stuff->len = PL_strlen(what);
	stuff->line_no = 0;
	stuff->scope_to = NULL;
	stuff->want_result = JS_TRUE;
	stuff->data = con_data;
	stuff->version = JSVERSION_UNKNOWN;
	stuff->principals = principals;

        ET_EvaluateScript(context, what, stuff, mk_mocha_eval_exit_fn);
    
    }
    else {

        /* allocated above, don't need to free */
        con_data->str = what;
	con_data->len = PL_strlen(what);
        con_data->is_valid = TRUE;

    }

    /* make sure netlib gets called so we know when our data gets back */
    START_POLLING(ae, con_data);

    /* ya'll come back now, ya'hear? */
    return net_ProcessMocha(ae);

}

PRIVATE int
net_process_mocha(MochaConData * con_data)
{
    int32 ref_count;

    ref_count = con_data->ref_count;
    DROP_CON_DATA(con_data);
    if (ref_count == 1 || !con_data->active_entry)
	return -1;
    return net_ProcessMocha(con_data->active_entry);
}

static char nscp_open_tag[] = "<" PT_NSCP_OPEN ">";

/*
 * If the mocha has finished evaluation shove the result
 *   down the stream and continue else just wait
 */
PRIVATE int32
net_ProcessMocha(ActiveEntry * ae)
{
    FO_Present_Types output_format;
    Bool to_layout;
    Bool first_time;
    NET_StreamClass *stream = NULL;
    MochaConData * con_data = (MochaConData *) ae->con_data;
    MWContext * context;
    int status;

    /* if we haven't gotten our data yet just return and wait */
    if (!con_data || !con_data->is_valid)
        return 0;

    context = ae->window_id;

    /*
     * Race with the mocha thread until we can grab the JSLock
     */
    if (!con_data->single_shot) {
	MochaDecoder * decoder;

	HOLD_CON_DATA(con_data);
	if (!LM_AttemptLockJS(context,
			      (JSLockReleaseFunc)net_process_mocha, con_data))
	    return 0;
	DROP_CON_DATA(con_data);
	decoder = LM_GetMochaDecoder(context);
	if (!decoder) {
	    LM_UnlockJS(context);
	    ae->status = MK_OUT_OF_MEMORY;
	    goto done;
	}
        stream = decoder->stream;
        LM_PutMochaDecoder(decoder);
	LM_UnlockJS(context);
    }
    else {
	stream = con_data->stream;
    }

    /* we've gotten valid data, clear the callme all the time flag */
    STOP_POLLING(ae, con_data);

    /* see if there were any problems */
    if (con_data->status < 0 || con_data->str == NULL) {
	ET_SendLoadEvent(context, EVENT_LOAD, NULL, NULL, 
                         LO_DOCUMENT_LAYER_ID, FALSE);
        ae->status = con_data->status;
	goto done;
    }

    /*
     * If we don't already have a stream to take our output create one now
     */
    first_time = !stream;
    if (first_time) {
        StrAllocCopy(ae->URL_s->content_type, TEXT_HTML);
        ae->format_out = CLEAR_CACHE_BIT(ae->format_out);

        stream = NET_StreamBuilder(ae->format_out, ae->URL_s, ae->window_id);
        if (!stream) {
            ae->status = MK_UNABLE_TO_CONVERT;
            goto done;
        }
    }

    /*
     * If the stream we just created isn't ready for writing just
     *   hold onto the stream and try again later
     */
    if (!stream->is_write_ready(stream)) {
	con_data->stream = stream;
	START_POLLING(ae, con_data);
	return 0;
    }

    /* XXX this condition is fairly bogus */
    output_format = CLEAR_CACHE_BIT(ae->format_out);
    to_layout = (output_format != FO_INTERNAL_IMAGE &&
		 output_format != FO_MULTIPART_IMAGE &&
		 output_format != FO_EMBED &&
		 output_format != FO_PLUGIN);

    if (to_layout) {
	/* The string must end in a newline so the parser will flush it.  */
	if (con_data->len != 0 && con_data->str[con_data->len-1] != '\n') {
	    size_t new_len = con_data->len + 1;
	    char * new_str = PR_Malloc((new_len + 1) * sizeof(char));

	    if (!new_str) {
		ae->status = MK_OUT_OF_MEMORY;
		goto done;
	    }
	    memcpy(new_str, con_data->str, con_data->len);
	    new_str[new_len-1] = '\n';
	    new_str[new_len] = '\0';
	    con_data->str = new_str;
	    con_data->len = new_len;
	}
    }


    /*
     * If this is the first time do some initial setup.  We'll set
     *   the window title and maybe shove out a <BASE> tag
     */
    status = 0;
    if (to_layout && first_time && con_data->eval_what) {

	/* Feed layout an internal tag so it will create a new top state. */
	stream->put_block(stream, nscp_open_tag,
			  sizeof nscp_open_tag - 1);

        (void) LM_WysiwygCacheConverter(context, ae->URL_s, 
					con_data->wysiwyg_url,
					con_data->base_href);

        if (*con_data->str != '<') {
            char * prefix = NULL;
            StrAllocCopy(prefix, "<TITLE>");
            StrAllocCat(prefix, ae->URL_s->address);
            StrAllocCat(prefix, "</TITLE><PLAINTEXT>");
            if (!prefix) {
		ae->status = MK_OUT_OF_MEMORY;
		goto done;
	    }
            status = (*stream->put_block)(stream, prefix,
                                          PL_strlen(prefix));
            PR_Free(prefix);
        }
	else {
	    if (con_data->base_href) {
		status = (*stream->put_block)(stream, 
		    			      con_data->base_href,
					      PL_strlen(con_data->base_href));
	    }
	}
    }
	 
    if (status >= 0) {
        status = (*stream->put_block)(stream, con_data->str,
                                      (int32)con_data->len);
    }

    if (con_data->single_shot && status >= 0)
        (*stream->complete)(stream);

    if (!con_data->single_shot && status >= 0) {
        if (first_time) {
	    ET_SetDecoderStream(context, stream, ae->URL_s, JS_TRUE);
	    goto done;
        }
    } else {
        if (status < 0)
            (*stream->abort)(stream, status);
        if (first_time)
            PR_Free(stream);
    }

    ae->status = MK_DATA_LOADED;

done:
    ae->con_data = NULL;
    con_data->active_entry = NULL;
    DROP_CON_DATA(con_data);
    return -1;

}

PRIVATE int32
net_InterruptMocha(ActiveEntry *ae)
{
    MochaConData * con_data = (MochaConData *) ae->con_data;

    if (!con_data)
	return MK_INTERRUPTED;

    /* do we need to decrement the callme all the time flag? */
    if (con_data->polling) {
	STOP_POLLING(ae, con_data);
    }

    /* ae is about to go away, break its connection with con_data */
    ae->con_data = NULL;
    con_data->active_entry = NULL;

    /* ae is about to go away, better get it off the JS lock waiters list */
    if (LM_ClearAttemptLockJS(con_data->context,
			      (JSLockReleaseFunc)net_process_mocha, con_data))
	DROP_CON_DATA(con_data);

    return ae->status = MK_INTERRUPTED;
}

PRIVATE void
net_CleanupMocha(void)
{
    /* nothing so far needs freeing */
    return;
}

MODULE_PRIVATE void
NET_InitMochaProtocol(void)
{
        static NET_ProtoImpl mocha_proto_impl;

        mocha_proto_impl.init = net_MochaLoad;
        mocha_proto_impl.process = net_ProcessMocha;
        mocha_proto_impl.interrupt = net_InterruptMocha;
        mocha_proto_impl.resume = NULL;
        mocha_proto_impl.cleanup = net_CleanupMocha;

        NET_RegisterProtocolImplementation(&mocha_proto_impl, MOCHA_TYPE_URL);
}

