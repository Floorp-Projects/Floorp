/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

typedef struct hk_FunctionRecStruct {
	XP_Bool func_exists;
	char *func_name;
} hk_FunctionRec;


extern int32 hk_NumKnownTags(void);
extern int32 hk_TagStringToIndex(char *tag_str);
extern char *hk_TagIndexToFunctionString(int32 indx);
extern char *hk_TagFunctionString(const char *func_name, void *extra);
extern JSObject *hk_GetHookObject(void);
extern void hk_SetHookObject(JSObject *hook_obj);
extern intn hk_IsUnknownTagHook(void *extra);
extern void hk_SetFunctionExistence(char *func_name, XP_Bool exists);

extern JSBool hk_HookObjectAddProperty(JSContext *cx, JSObject *obj,
					jsval id, jsval *vp);
extern JSBool hk_HookObjectDeleteProperty(JSContext *cx, JSObject *obj,
					jsval id, jsval *vp);
extern JSBool hk_HookObjectSetProperty(JSContext *cx, JSObject *obj,
					jsval id, jsval *vp);

