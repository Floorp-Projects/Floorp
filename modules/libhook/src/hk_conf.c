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

#include "hk_private.h"

/**********************************************
 * These are to configuration functions registered
 * with the new javascript HookConfig object.
 * They are designed to maintain an array of the
 * special hooks with simple booleans to tell
 * if they already exist in the object or not.
 **********************************************/


/*
 * Called when a new object is added.
 * If the object is a function then we
 * add it to the existence array.
 * NOTE: hk_SetFunctionExistence only keeps track of those functions
 *       that are known special javascript hooks.
 */
JSBool
hk_HookObjectAddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	JSString *j_str;
	char *str;

	j_str = JS_ValueToString(cx, id);
	str = JS_GetStringBytes(j_str);
	if (JS_TypeOfValue(cx, *vp) == JSTYPE_FUNCTION)
	{
		/*
		 * hk_SetFunctionExistence is not allowed to change
		 * the contents of str or the wrong function
		 * will be added.
		 */
		hk_SetFunctionExistence(str, TRUE);
	}

	return JS_TRUE;
}


/*
 * Called when an object is deleted.
 * If the object is a function then we
 * delete it from the existence array.
 * NOTE: hk_SetFunctionExistence only keeps track of those functions
 *       that are known special javascript hooks.
 */
JSBool
hk_HookObjectDeleteProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	JSString *j_str;
	char *str;

	j_str = JS_ValueToString(cx, id);
	str = JS_GetStringBytes(j_str);
	if (JS_TypeOfValue(cx, *vp) == JSTYPE_FUNCTION)
	{
		/*
		 * hk_SetFunctionExistence is not allowed to change
		 * the contents of str or the wrong function
		 * will be deleted.
		 */
		hk_SetFunctionExistence(str, FALSE);
	}

	return JS_TRUE;
}


/*
 * Called when an object has a new value set to it.
 * The object could have been a value (like NULL) but
 * now be set to a function.  The object could also have once been
 * set to a function and now be set to a non-function type.
 * NOTE: hk_SetFunctionExistence only keeps track of those functions
 *       that are known special javascript hooks.
 */
JSBool
hk_HookObjectSetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	JSString *j_str;
	char *str;

	j_str = JS_ValueToString(cx, id);
	str = JS_GetStringBytes(j_str);
	/*
	 * hk_SetFunctionExistence is not allowed to change
	 * the contents of str or the wrong object
	 * will be set.
	 */
	if (JS_TypeOfValue(cx, *vp) == JSTYPE_FUNCTION)
	{
		hk_SetFunctionExistence(str, TRUE);
	}
	else
	{
		hk_SetFunctionExistence(str, FALSE);
	}

	return JS_TRUE;
}

