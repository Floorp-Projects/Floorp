/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "xp_core.h"
#include "xp_mcom.h"
#include "jsapi.h"
#include "prefapi.h"
#include "hk_funcs.h"

#include "hk_private.h"


static JSObject *HookObject = NULL;


JSObject *
hk_GetHookObject(void)
{
	return HookObject;
}


void
hk_SetHookObject(JSObject *hook_obj)
{
	HookObject = hook_obj;
}


/*
 * Some hooks have dynamic names and can't be in the
 * fast hook array lookup, this covers looking up those
 * hooks.  For example, if I invented a <BLURT> tag,
 * this would look for a BLURT_hook function.
 */
intn
hk_IsUnknownTagHook(void *extra)
{
	const char *hook_func;
	jsval tmp_val;
	JSContext *j_context;

	hook_func = HK_GetFunctionName(HK_TAG, extra);
	if (hook_func == NULL)
	{
		return 0;
	}

	if (!PREF_GetConfigContext(&j_context))
	{
		return 0;
	}

	if ((j_context == NULL)||(HookObject == NULL))
	{
		return 0;
	}

        JS_BeginRequest(j_context);
	if ((JS_GetProperty(j_context, HookObject, hook_func, &tmp_val))&&
		(JS_TypeOfValue(j_context, tmp_val) == JSTYPE_FUNCTION))
	{
        	JS_EndRequest(j_context);
		return 1;
	}
        JS_EndRequest(j_context);

	return 0;
}


/*
 * There is no guarantee that someone has checked the existence of the
 * hook before calling it, so we check again here.  We don't use the
 * fast array lookup since it can't catch all hooks anyway, and we
 * may well have to fall back to the slow method.
 */
intn
HK_CallHook(int32 hook_id, void *extra, int32 window_id,
		char *hook_str, char **hook_ret)
{
	const char *hook_func;
	intn ok;
	jsval tmp_val;
	jsval argv[2];
	JSContext *j_context;
	JSString *str;
	const char *result_str;
	char *return_str;

	*hook_ret = NULL;

	if (hook_str == NULL)
	{
		return 0;
	}

	/*
	 * Make a function name form the hook_is and possibly the PA_Tag
	 * pointed to by extra.
	 */
	hook_func = HK_GetFunctionName(hook_id, extra);
	if (hook_func == NULL)
	{
		return 0;
	}

	/*
	 * Make sure you have a javascript context and object.
	 */
	if (!PREF_GetConfigContext(&j_context))
	{
		return 0;
	}
	if ((j_context == NULL)||(HookObject == NULL))
	{
		return 0;
	}

        JS_BeginRequest(j_context);
	/*
	 * Check that there is a function to call.
	 */
	if ((!JS_GetProperty(j_context, HookObject, hook_func, &tmp_val))||
		(JS_TypeOfValue(j_context, tmp_val) != JSTYPE_FUNCTION))
	{
                JS_EndRequest(j_context);
		return 0;
	}

	/*
	 * Create the argument/parameter list, and call the function.
	 */
	str = JS_NewStringCopyZ(j_context, hook_str);
	if (str == NULL)
	{
                JS_EndRequest(j_context);
		return 0;
	}
	argv[0] = STRING_TO_JSVAL(str);
	argv[1] = INT_TO_JSVAL(window_id);
	if (!JS_CallFunctionName(j_context, HookObject,	hook_func, 2, argv,
							 &tmp_val))
	{
                JS_EndRequest(j_context);
		return 0;
	}

	/*
	 * A false return from the function means leave the
	 * passed string unchanged.
	 */
	if (tmp_val != JSVAL_FALSE)
	{
		str = JS_ValueToString(j_context, tmp_val);
		if (str == NULL)
		{
                	JS_EndRequest(j_context);
			return 0;
		}
		result_str = JS_GetStringBytes(str);
		return_str = XP_STRDUP(result_str);
		if (return_str == NULL)
		{
                	JS_EndRequest(j_context);
			return 0;
		}
		*hook_ret = return_str;
	}
      	JS_EndRequest(j_context);

	return 1;
}
