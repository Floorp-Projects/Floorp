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
#include "xp_core.h"
#include "xp_mcom.h"
#include "hk_types.h"
#include "jsapi.h"
#include "prefapi.h"
#include "ntypes.h"
#include "structs.h"
#include "pa_tags.h"

#include "hk_private.h"

#ifdef XP_MAC
#include "hk_funcs.h"
#endif


static hk_FunctionRec **FunctionList = NULL;
static int32 FunctionCount = 0;

static char *hk_FunctionStrings[HK_MAX] = {
	NULL,
	"location_hook",
	"_hook",
	"document_start_hook"};

static JSClass autoconf_class = {
	"HookConfig", 0,
	hk_HookObjectAddProperty, hk_HookObjectDeleteProperty,
	JS_PropertyStub, hk_HookObjectSetProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};


/*******************************************
 * This function really bugs me!  It needs to
 * be super fast since it will be called for every
 * HTML tag ever processed.  This means it should be
 * self-contained, and not call off to other functions.
 * Making it do that means it can't call off into
 * hk_tag.c to get inside the PA_Tag structure in extra,
 * and thus makes me include ntypes.h, structs.h, and pa_tags.h.
 ********************************************/
/*
 * A FAST function existence lookup, using an array set up in
 * the initialization process.
 */
intn
HK_IsHook(int32 hook_id, void *extra)
{
	int32 func_id;
	hk_FunctionRec *rec_ptr;

	/*
	 * The no hook case never exists.
	 */
	if (hook_id == HK_NONE)
	{
		return 0;
	}
	/*
	 * Tag hooks map to the tag types, or alternatly
	 * unknown tags which are handled elsewhere.
	 */
	else if (hook_id == HK_TAG)
	{
		PA_Tag *tag;

		tag = (PA_Tag *)extra;
		if (tag->type != P_UNKNOWN)
		{
			func_id = HK_MAX - 1 + tag->type;
		}
		/*
		 * P_UNKNOWN tags must be looked up the slow
		 * dynamic way.
		 */
		else
		{
			return(hk_IsUnknownTagHook(extra));
		}
	}
	else
	{
		func_id = hook_id - 1;
	}

	/*
	 * If we are outside the known funtion array, fail.
	 */
	if ((func_id < 0)||(func_id >= FunctionCount))
	{
		return 0;
	}

	/*
	 * If no function is registered, fail.
	 */
	rec_ptr = FunctionList[func_id];
	if (rec_ptr == NULL)
	{
		return 0;
	}

	if (rec_ptr->func_exists == FALSE)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}


/*
 * Set the existence state of a particular index in the array.
 */
static void
hk_set_function_existence(int32 indx, XP_Bool exists)
{
	if ((indx < 0)||(indx >= FunctionCount))
	{
		return;
	}

	if (FunctionList[indx] != NULL)
	{
		FunctionList[indx]->func_exists = exists;
	}
}


void
hk_SetFunctionExistence(char *func_name, XP_Bool exists)
{
	int32 i, len;
	char *tptr;
	char *up_str;
	char *ptr1, *ptr2;
	int32 indx;

	/*
	 * A NULL function one too short for the hook suffix
	 * cannot be a hook function.
	 */
	if (func_name == NULL)
	{
		return;
	}
	len = XP_STRLEN(func_name);
	if (len < XP_STRLEN("_hook"))
	{
		return;
	}

	/*
	 * If the function has no hook suffix we just don't care.
	 */
	tptr = func_name + len - XP_STRLEN("_hook");
	if (XP_STRCMP(tptr, "_hook") != 0)
	{
		return;
	}


	/*
	 * Make an all upper-case copy of the prefix.
	 */
	*tptr = '\0';
	up_str = XP_ALLOC(XP_STRLEN(func_name) + 1);
	/*
	 * If allocation fails, restore original and return.
	 */
	if (up_str == NULL)
	{
		*tptr = '_';
		return;
	}
	ptr1 = func_name;
	ptr2 = up_str;
	while (*ptr1 != '\0')
	{
		*ptr2 = (char)(XP_TO_UPPER(*ptr1));
		ptr1++;
		ptr2++;
	}
	*ptr2 = '\0';
	*tptr = '_';

	/*
	 * Check if the prefix is a known TAG name.
	 */
	indx = hk_TagStringToIndex(up_str);
	XP_FREE(up_str);
	if (indx >= 0)
	{
		indx = HK_MAX - 1 + indx;
		hk_set_function_existence(indx, exists);
		return;
	}

	/*
	 * Special check for the TEXT_hook which is a tag hook but
	 * won't be caught by the hk_TagStringToIndex test.
	 */
	if (XP_STRCMP(func_name, "TEXT_hook") == 0)
	{
		indx = HK_MAX - 1 + 0;
		hk_set_function_existence(indx, exists);
		return;
	}

	/*
	 * Finally, check the array of known tag hooks.
	 */
	for (i=1; i<HK_MAX; i++)
	{
		if ((hk_FunctionStrings[i] != NULL)&&
			(XP_STRCMP(func_name, hk_FunctionStrings[i]) == 0))
		{
			indx = i - 1;
			hk_set_function_existence(indx, exists);
			return;
		}
	}
}


/*
 * Free all members of the function list array up to
 * the passed index.
 */
static void
hk_free_function_list(int32 indx)
{
	int32 i;

	for (i=0; i<indx; i++)
	{
		if (FunctionList[i] != NULL)
		{
			if (FunctionList[i]->func_name != NULL)
			{
				XP_FREE(FunctionList[i]->func_name);
				FunctionList[i]->func_name = NULL;
			}
			XP_FREE(FunctionList[i]);
			FunctionList[i] = NULL;
		}
	}
	XP_FREE(FunctionList);
	FunctionList = NULL;
	FunctionCount = 0;
}


/*
 * Initialize the function list array to contain all the known tags
 * plus all the known special hook functions.
 */
static intn
hk_initialize_function_list(void)
{
	int32 i, indx;
	int32 tag_cnt;

	/*
	 * Subtract one from HK_MAX because it includes the unknown
	 * at 0.  For the tag count, unknown is -1, so we can just
	 * use it as is.
	 */
	tag_cnt = hk_NumKnownTags();
	FunctionCount = HK_MAX - 1 + tag_cnt;
	FunctionList = (hk_FunctionRec **)XP_ALLOC(FunctionCount *
						sizeof(hk_FunctionRec *));
	if (FunctionList == NULL)
	{
		return 0;
	}

	indx = 0;
	/*
	 * First allocate all the known special hooks.
	 */
	for (i=1; i < HK_MAX; i++)
	{
		hk_FunctionRec *rec_ptr;

		rec_ptr = XP_NEW(hk_FunctionRec);
		if (rec_ptr == NULL)
		{
			hk_free_function_list(indx);
			return 0;
		}
		rec_ptr->func_exists = FALSE;
		rec_ptr->func_name = hk_FunctionStrings[i];
		FunctionList[indx] = rec_ptr;
		indx++;
	}

	/*
	 * Now do all known tags.
	 */
	for (i=0; i < tag_cnt; i++)
	{
		hk_FunctionRec *rec_ptr;

		rec_ptr = XP_NEW(hk_FunctionRec);
		if (rec_ptr == NULL)
		{
			hk_free_function_list(indx);
			return 0;
		}
		rec_ptr->func_exists = FALSE;
		rec_ptr->func_name = hk_TagIndexToFunctionString(i);
		FunctionList[indx] = rec_ptr;
		indx++;
	}

	return 1;
}


/*
 * Initialize all the libhook stuff.  SHould only be called once.
 * Usually just before reading hook.js.
 */
intn
HK_Init(void)
{
	intn ret;
	JSContext *j_context;
	JSObject *j_object;
	JSObject *hook_obj;

	/*
	 * If a hook object does not already exist, create
	 * one.  If you cannot create one, then return failure.
	 */
	hook_obj = hk_GetHookObject();
	if (hook_obj == NULL)
	{
		if ((!PREF_GetConfigContext(&j_context))||
			(!PREF_GetGlobalConfigObject(&j_object)))
		{
			return 0;
		}

                JS_BeginRequest(j_context);
		hook_obj = JS_DefineObject(j_context, j_object,
			"HookConfig",
			&autoconf_class,
			NULL,
			JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT);
                JS_EndRequest(j_context);
	}
	if (hook_obj == NULL)
	{
		return 0;
	}

	hk_SetHookObject(hook_obj);

	ret = hk_initialize_function_list();

	return ret;
}


/*
 * Get the hook know for the passed hook id.
 */
const char *
HK_GetFunctionName(int32 hook_id, void *extra)
{
	if ((hook_id < 0)||(hook_id >= HK_MAX))
	{
		return NULL;
	}

	/*
	 * Special name creation for tag hooks.
	 */
	if (hook_id == HK_TAG)
	{
		const char *ret_str;

		ret_str = (const char *)hk_TagFunctionString(
				hk_FunctionStrings[hook_id], extra);
		return ret_str;
	}
	else
	{
		return hk_FunctionStrings[hook_id];
	}
}

