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
/* 
 * wfMime.cpp (wfMimeList.cpp)
 *
 * Implements a list of mime types
 *
 * dp Suresh <dp@netscape.com>
 */


#include "wfMime.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "ctype.h"

static void free_mime_store(wfList *object, void *item);

wfMimeList::wfMimeList(const char *str) : wfList(free_mime_store)
{
	// Parse the mimeString and create mime_store(s)
	// The format of the mimeString is
	//		mime/type:suffix[,suffix]...:Description
	//		[;mime/type:suffix[,suffix]...:Description]...
	if (str && *str)
	{
		reconstruct(str);
	}
}


char *
wfMimeList::describe()
{
	char buf[1024];	// XXX this should be dynamic
	char *s = buf;

	struct wfListElement *tmp = head;

	buf[0] = '\0';
    for (; tmp; tmp = tmp->next)
	{
		struct mime_store *ele = (struct mime_store *) tmp->item;
		// XXX Need to check for memory overflow
		// NO I18N REQUIRED FOR THESE STRINGS
		sprintf(s, "%s:%s:%s:%s;", ele->mimetype,
			(ele->extensions?ele->extensions:" "),
			(ele->description?ele->description:" "),
			(ele->isEnabled?"enable":"disable"));
		s += strlen(s);
	}
	return (*buf ? strdup(buf) : 0);
}

int
wfMimeList::reconstruct(const char *str)
{
    char buf[512];
	char *mimetype, *extensions, *description;
	int enabled;
	int err = 0;
	struct mime_store *ele;

	if (!str || !*str)
	{
		// Error. No string to reconstruct from.
		err--;
	}

	while (err == 0 && *str )
	{
		mimetype = extensions = description = NULL;
		enabled = 1;
		str = wf_scanToken(str, buf, sizeof(buf), ":", 0);
		if (buf[0] == '\0' || *str != ':')
		{
			break;
		}
		str++;
		mimetype = CopyString(buf);
	
		str = wf_scanToken(str, buf, sizeof(buf), ";:", 1);
		extensions = CopyString(buf);
		
		if (*str == ':')
		{
			str++;
			str = wf_scanToken(str, buf, sizeof(buf), ";:", 0);
			description = CopyString(buf);
			if (*str == ':')
			{
				str++;
				str = wf_scanToken(str, buf, sizeof(buf), ";", 0);
				// NO I18N REQUIRED FOR THESE STRINGS
				if (buf[0] != '\0' && !strncmp(buf, "disable", 7))
				{
					enabled = 0;
				}
			}
		}
		ele = new mime_store;
		if (!ele)
		{
			// No memory
			err--;
			continue;
		}
		ele->mimetype = mimetype;
		ele->description = description;
		ele->extensions = extensions;
		ele->isEnabled = enabled;
		add(ele);
		if (*str != ';')
		{
			break;
		}
		str++;

	}
	return err;
}


wfMimeList::~wfMimeList()
{
	finalize();
}

int wfMimeList::finalize(void)
{
	wfList::removeAll();
	return (0);
}

void
/*ARGSUSED*/
free_mime_store(wfList *object, void *item)
{
	struct mime_store *ele = (struct mime_store *) item;
	if (ele->mimetype)
	{
		delete ele->mimetype;
		ele->mimetype = NULL;
	}
	if (ele->extensions)
	{
		delete ele->extensions;
		ele->extensions = NULL;
	}
	if (ele->description)
	{
		delete ele->description;
		ele->description = NULL;
	}
	delete ele;
}


//
// Implementation of public methods
//

int wfMimeList::setEnabledStatus(const char *mimetype, int enabledStatus)
{
	struct mime_store *ele = find(mimetype);
	if (!ele)
	{
		// mimetype not found
		return (-1);
	}

	ele->isEnabled = (enabledStatus?1:0);
	return (0);
}


int wfMimeList::isEnabled(const char *mimetype)
{
	struct mime_store *ele = find(mimetype);
	if (!ele)
	{
		// mimetype not found
		return (-1);
	}

	return (ele->isEnabled);
}


const char * wfMimeList::getMimetypeFromExtension(const char *ext)
{
	const char *mimetype = NULL;
	struct wfListElement *tmp = head;
	struct mime_store *ele = NULL;

	if (!ext || !*ext)
	{
		return (NULL);
	}

	for (; tmp; tmp = tmp->next)
	{
		ele = (struct mime_store *) tmp->item;

		if (!ele->extensions || !*(ele->extensions))
		{
			continue;
		}

		//
		// Check if ele->extensions contains ext
		// Extension is a comma separated list. We have normalized it
		// before storing it into ele so that it contains no leading '.'
		// or spaces.
		//
		const char *p = ele->extensions;
		const char *q = ext;
		int found = 0;
		while (*p)
		{
			if (*p == ',')
			{
				// Move to the next extension
				p++;
				q = ext;
			}
			else if (!*q)
			{
				// maybe Success if *p is either , or NULL
				if ((*p == '\0') || (*p == ','))
				{
					found = 1;
					break;
				}
				else
				{
					q = ext;
					// Nope. Skip to next extension.
					while (*p && *p != ',') p++;
				}
			}
			else if ((*p == *q) || (tolower(*p) == tolower(*q)))
			{
				// keep going.
				p++; q++;
			}
			else
			{
				// *p != *q
				q = ext;
				// Nope. Skip to next extension.
				while (*p && *p != ',') p++;
			}
		}
		if (!*q)
		{
			// We came out becasue p is null. If q is also null, then
			// we did find a match.
			found = 1;
		}
		if (found)
		{
			mimetype = ele->mimetype;
			break;
		}
	}

	return (mimetype);
}

//
// Implementation of private methods
//

struct mime_store *
wfMimeList::find(const char *mimetype)
{
	struct mime_store *ret = NULL;
	struct wfListElement *tmp = head;

    for (; tmp; tmp = tmp->next)
	{
		struct mime_store *ele = (struct mime_store *) tmp->item;
		if (!wf_strcasecmp(ele->mimetype, mimetype))
		{
			ret = ele;
		}
	}

	return (ret);
}
