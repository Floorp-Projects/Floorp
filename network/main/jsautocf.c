/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "mkutils.h"	/* LF */

#include <time.h>

#include "jsautocf.h"
#include "xp_mem.h"	/* PR_NEWZAP() */
#ifndef XP_MAC
#include <sys/types.h>
#endif
#include "libi18n.h"

#include "mkgeturl.h"
#include "mkstream.h"
#include "mkgeturl.h"
#include "mkformat.h"
/* acharya: This file is obsolete and not used in this file.
#include "il.h" 
*/
#include "mime.h"
#include "cvactive.h"
#include "gui.h"
#include "msgcom.h"

#if defined(XP_UNIX) || defined(XP_WIN32)
#include "prnetdb.h"
#else
#define PRHostEnt struct hostent
#endif

/*
 * Private object for the proxy config loader stream.
 *
 *
 */
typedef struct _JSACF_Object {
   MWContext *	context;
   int		flag;
} JSACF_Object;


/*
 * A struct for holding queued state.
 *
 * The state is saved when NET_GetURL() is called for the first time,
 * and the Javascript autoconfig retrieve has to be started (and finished)
 * before the first actual document can be loaded.
 *
 * A pre-exit function for the Javascript autoconfig URL restarts the
 * original URL retrieve by calling NET_GetURL() again with the
 * same parameters.
 *
 */
typedef struct _JSACF_QueuedState {
    URL_Struct *	URL_s;
    FO_Present_Types	output_format;
    MWContext *		window_id;
    Net_GetUrlExitFunc*	exit_routine;
} JSACF_QueuedState;

PRIVATE Bool	jsacf_loading      = FALSE;
PRIVATE Bool	jsacf_ok           = FALSE;
PRIVATE char *	jsacf_url          = NULL;
PRIVATE char *	jsacf_src_buf      = NULL;
PRIVATE int	jsacf_src_len      = 0;


/*
 * Saves out the proxy autoconfig file to disk, in case the server
 * is down the next time.
 *
 * Returns 0 on success, -1 on failure.
 *
 */
PRIVATE int jsacf_save_config(void)
{
    XP_File fp;
	int32 len = 0;

	// TODO: jonm
    if(!(fp = XP_FileOpen("", xpProxyConfig, XP_FILE_WRITE)))
	return -1;

    len = XP_FileWrite(jsacf_src_buf, jsacf_src_len, fp);
    XP_FileClose(fp);
	if (len != jsacf_src_len)
		return -1;

    return 0;
}


/*
 * Reads the proxy autoconfig file from disk.
 * This is called if the config server is not responding.
 *
 * returns 0 on success -1 on failure.
 *
 */
PRIVATE int jsacf_read_config(void)
{
    XP_StatStruct st;
    XP_File fp;

    if (XP_Stat("", &st, xpProxyConfig) == -1)
	return -1;

    if (!(fp = XP_FileOpen("", xpProxyConfig, XP_FILE_READ)))
	return -1;

    jsacf_src_len = st.st_size;
    jsacf_src_buf = (char *)PR_Malloc(jsacf_src_len + 1);
    if (!jsacf_src_buf) {
	XP_FileClose(fp);
	jsacf_src_len = 0;
	return -1;
    }

    if ((jsacf_src_len = XP_FileRead(jsacf_src_buf, jsacf_src_len, fp)) > 0)
      {
	  jsacf_src_buf[jsacf_src_len] = '\0';
      }
    else
      {
	  PR_Free(jsacf_src_buf);
	  jsacf_src_buf = NULL;
	  jsacf_src_len = 0;
      }

    XP_FileClose(fp);

    return 0;
}



/*
 * Private stream object methods for receiving the proxy autoconfig
 * file.
 *
 *
 */
PRIVATE int jsacf_write(NET_StreamClass *stream, CONST char *buf, int32 len)
{
	JSACF_Object *obj=stream->data_object;	
    if (len > 0) {
	if (!jsacf_src_buf)
	    jsacf_src_buf = (char*)PR_Malloc(len + 1);
	else
	    jsacf_src_buf = (char*)PR_Realloc(jsacf_src_buf,
					     jsacf_src_len + len + 1);

	if (!jsacf_src_buf) {	/* Out of memory */
	    jsacf_src_len = 0;
	    return MK_DATA_LOADED;
	}

	memcpy(jsacf_src_buf + jsacf_src_len, buf, len);
	jsacf_src_len += len;
	jsacf_src_buf[jsacf_src_len] = '\0';
    }
    return MK_DATA_LOADED;
}


PRIVATE unsigned int jsacf_write_ready(NET_StreamClass *stream)
{	
    return MAX_WRITE_READY;
}

extern int PREF_EvaluateJSBuffer(char * js_buffer, size_t length);
 
PRIVATE void jsacf_complete(NET_StreamClass *stream)
{
	JSACF_Object *obj=stream->data_object;
	int err = PREF_EvaluateJSBuffer(jsacf_src_buf,jsacf_src_len);	
	if (jsacf_src_buf) PR_Free(jsacf_src_buf);
}


PRIVATE void jsacf_abort(NET_StreamClass *stream, int status)
{
	JSACF_Object *obj=stream->data_object;	
    jsacf_loading = FALSE;
//    FE_Alert(obj->context, CONFIG_LOAD_ABORTED);
    PR_Free(obj);
}

/*
 * A stream constructor function for application/x-ns-proxy-autoconfig.
 *
 *
 *
 */
MODULE_PRIVATE NET_StreamClass *
NET_JavaScriptAutoConfig(int fmt, void *data_obj, URL_Struct *URL_s, MWContext *w)
{
    JSACF_Object		*obj;
    NET_StreamClass	*stream;

#ifdef LATER
    if (!jsacf_loading) {
	/*
	 * The Navigator didn't start this config retrieve
	 * intentionally.  Discarding the config.
	 */
	alert2(w, CONFIG_BLAST_WARNING, URL_s->address);
	return NULL;
    }
    else {
	NET_Progress(w, XP_GetString( XP_RECEIVING_PROXY_AUTOCFG ) ); 
    }
#endif

    if (jsacf_src_buf) {
	PR_Free(jsacf_src_buf);
	jsacf_src_buf = NULL;
	jsacf_src_len = 0;
    }

    if (!(stream = PR_NEWZAP(NET_StreamClass)))
	return NULL;

    if (!(obj = PR_NEWZAP(JSACF_Object))) {
	PR_Free(stream);
	return NULL;
    }

    obj->context = w;

    stream->data_object		= obj;
    stream->name		= "JavaScriptAutoConfigLoader";
    stream->complete		= (MKStreamCompleteFunc)  jsacf_complete;
    stream->abort		= (MKStreamAbortFunc)     jsacf_abort;
    stream->put_block		= (MKStreamWriteFunc)     jsacf_write;
    stream->is_write_ready	= (MKStreamWriteReadyFunc)jsacf_write_ready;
    stream->window_id		= w;

    return stream;
}


void jsacf_exit_routine(URL_Struct *URL_s, int status, MWContext *window_id)
{}

PRIVATE void
simple_exit(URL_Struct *URL_s, int status, MWContext *window_id)
{
	if(status != MK_CHANGING_CONTEXT)
		NET_FreeURLStruct(URL_s);
}

/*
 * Called by mkgeturl.c to originally retrieve, and re-retrieve
 * the javascript autoconfig file.
 *
 * autoconfig_url is the URL pointing to the autoconfig.
 *
 */
MODULE_PRIVATE int NET_LoadJavaScriptConfig(char *autoconf_url,MWContext *window_id)
{
    URL_Struct *my_url_s = NULL;

    if (!autoconf_url)
	return -1;

    StrAllocCopy(jsacf_url, autoconf_url);
    my_url_s = NET_CreateURLStruct(autoconf_url, NET_SUPER_RELOAD);

    /* Alert the proxy autoconfig module that config is coming */
    jsacf_loading = TRUE;

	return NET_GetURL(my_url_s, FO_CACHE_AND_JAVASCRIPT_CONFIG, window_id, simple_exit);
    //return FE_GetURL(window_id, my_url_s);
}

/*
 * Returns a pointer to a NULL-terminted buffer which contains
 * the text of the proxy autoconfig file.
 *
 *
 */
PUBLIC char * NET_GetJavaScriptConfigSource(void)
{
    return jsacf_src_buf;
}
