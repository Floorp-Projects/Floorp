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
#include "ntypes.h"
#include "structs.h"
#include "pa_tags.h"
#include "proto.h"

#ifdef XP_MAC
int32 hk_NumKnownTags(void);
char *hk_TagIndexToFunctionString(int32 indx);
char *hk_TagFunctionString(const char *func_name, void *extra);
int32 hk_TagStringToIndex(char *tag_str);
#endif

/*
 * No need for everyone to include pa_tags.h
 */
int32
hk_NumKnownTags(void)
{
	return(P_MAX);
}


char *
hk_TagIndexToFunctionString(int32 tag_indx)
{
	char *total_name;

	total_name = NULL;
	if (tag_indx == P_TEXT)
	{
		total_name = XP_STRDUP("TEXT_hook");
	}
	else if (tag_indx == P_UNKNOWN)
	{
		total_name = NULL;
	}
	else if (tag_indx >= P_MAX)
	{
		total_name = NULL;
	}
	else
	{
		const char *tag_name;

		tag_name = PA_TagString(tag_indx);
		if (tag_name == NULL)
		{
			total_name = NULL;
		}
		else
		{
			int32 len;

			len = XP_STRLEN(tag_name) + XP_STRLEN("_hook") + 1;
			total_name = XP_ALLOC(len);
			if (total_name != NULL)
			{
				XP_STRCPY(total_name, tag_name);
				XP_STRCAT(total_name, "_hook");
			}
		}
	}
	return(total_name);
}


/*
 * Unlike hk_TagIndexToFunctionString (above) this function is passed
 * a PA_Tag inside extra, so it can even make the dynamic hook
 * name for UNKNOWN HTML tags.
 */
char *
hk_TagFunctionString(const char *func_name, void *extra)
{
	PA_Tag *tag;
	char *tag_text;
	char *total_name;

	tag_text = NULL;
	tag = (PA_Tag *)extra;

	if ((tag == NULL)||(func_name == NULL))
	{
		return NULL;
	}

	if (tag->type == P_TEXT)
	{
		tag_text = XP_STRDUP("TEXT");
	}
	else if (tag->type == P_UNKNOWN)
	{
		char *tptr;
		int32 cnt;

		tptr = (char *)tag->data;
		cnt = 0;
		while ((*tptr != '>')&&(!XP_IS_SPACE(*tptr))&&
			(cnt < tag->data_len))
		{
			tptr++;
			cnt++;
		}
		if ((cnt > 0)&&(cnt < tag->data_len))
		{
			char tchar;

			tchar = *tptr;
			*tptr = '\0';
			tag_text = XP_STRDUP((char *)tag->data);
			*tptr = tchar;
		}
		else
		{
			tag_text = XP_STRDUP("UNKNOWN");
		}
	}
	else
	{
		const char *tag_name;

		tag_name = PA_TagString((int32)tag->type);
		if (tag_name == NULL)
		{
			tag_text = XP_STRDUP("UNKNOWN");
		}
		else
		{
			tag_text = XP_STRDUP(tag_name);
		}
	}

	if (tag_text != NULL)
	{
		int32 total_len;

		total_len = XP_STRLEN(func_name) + XP_STRLEN(tag_text) + 3;
		total_name = XP_ALLOC(total_len);
		if (total_name == NULL)
		{
			XP_FREE(tag_text);
			return NULL;
		}
		XP_STRCPY(total_name, tag_text);
		XP_STRCAT(total_name, func_name);

		return total_name;
	}
	else
	{
		return NULL;
	}
}


int32
hk_TagStringToIndex(char *tag_str)
{
	return(PA_TagIndex(tag_str));
}

