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
#include "xp_core.h"
#include "xp_mcom.h"
#include "jsapi.h"
#include "prefapi.h"
#include "fe_proto.h"
#include "hk_funcs.h"

#include "hk_private.h"


/**************************************************
 * This is a special function just to read in the
 * users special hook.js file and interpret it
 * into the special javascript hook filter object.
 **************************************************/


void
HK_JSErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);


void
HK_ReadHookFile(char *filename)
{
	JSErrorReporter err_reporter;
	JSContext *j_context;
	JSObject *hook_obj;
	JSBool ret;
	jsval result;
	XP_File file_ptr;
	XP_StatStruct stat_info;
	int32 file_len;
	int32 read_len;
	char *read_buf;

	/*
	 * If we can't get a valid javascript context, just return.
	 */
	if (!PREF_GetConfigContext(&j_context))
	{
		return;
	}
	if (j_context == NULL)
	{
		return;
	}

	/*
	 * If we don't have a javascript hook object, try to create one,
	 * if we fail, then return.
	 */
	hook_obj = hk_GetHookObject();
	if (hook_obj == NULL)
	{
		if (HK_Init() == 0)
		{
			return;
		}
		else
		{
			hook_obj = hk_GetHookObject();
		}

		if (hook_obj == NULL)
		{
			return;
		}
	}

	/*
	 * If the hook.js file doesn't exist or has
	 * zero size, return.
	 */
	if (XP_Stat(filename, &stat_info, xpJSHTMLFilters) < 0)
	{
		return;
	}
	file_len = stat_info.st_size;
	if (file_len <= 1)
	{
		return;
	}

	/*
	 * If the hook.js cannot be opened for reading, return.
	 */
	file_ptr = XP_FileOpen(filename, xpJSHTMLFilters, XP_FILE_READ);
	if (file_ptr == NULL)
	{
		return;
	}

	/*
	 * If a buffer to read hook.js cannot be allocated, return.
	 */
	read_buf = XP_ALLOC(file_len * sizeof(char));
	if (read_buf == NULL)
	{
		return;
	}

	read_len = XP_FileRead(read_buf, file_len, file_ptr);

	(void)XP_FileClose(file_ptr);

	/*
	 * Evaluate the file in the javascript hook object.
	 */
	err_reporter = JS_SetErrorReporter(j_context, HK_JSErrorReporter);
	ret = JS_EvaluateScript(j_context, hook_obj, read_buf, read_len,
			filename, 0, &result);
	JS_SetErrorReporter(j_context, err_reporter);

	XP_FREE(read_buf);
}


/* copied from libpref */
/* Platform specific alert messages */
static void
hk_Alert(char* msg)
{
#if defined(XP_MAC) || defined(XP_UNIX)
#if defined(XP_UNIX)
	if (getenv("NO_PREF_SPAM") == NULL)
#endif
	FE_Alert(NULL, msg);
#endif
#if defined (XP_WIN)
	MessageBox (NULL, msg, "Netscape -- JS Preference Warning", MB_OK);
#endif
}


/*
 * Mostly copied from elsewhere, reports errors in the
 * hook.js file as it is read.
 */
void
HK_JSErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	char *last;
	int i, j, k, n;
	const char *s, *t;

	last = PR_sprintf_append(0, "An error occurred reading the HTML hook file.");
	last = PR_sprintf_append(last, "\n\n");
	if (!report)
	{
		last = PR_sprintf_append(last, "%s\n", message);
	}
	else
	{
		if (report->filename)
		{
			last = PR_sprintf_append(last, "%s, ",
				report->filename);
		}
		if (report->lineno)
		{
			last = PR_sprintf_append(last, "line %u: ",
				report->lineno);
		}
		last = PR_sprintf_append(last, "%s. ", message);
		if (report->linebuf)
		{
			for (s = t = report->linebuf; *s != '\0'; s = t)
			{
				for (; t != report->tokenptr && *t != '<' && *t != '\0'; t++)
					;
				last = PR_sprintf_append(last, "%.*s", t - s, s);
				if (*t == '\0')
				{
					break;
				}
				last = PR_sprintf_append(last, (*t == '<') ? "" : "%c", *t);
				t++;
			}
		}
	}

	if (last)
	{
		hk_Alert(last);
		XP_FREE(last);
	}
}

