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

