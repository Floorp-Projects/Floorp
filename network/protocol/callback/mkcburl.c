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


#include "xp.h"
#include "mkgeturl.h"
#include "netcburl.h"
#include "secrng.h"				/* For RNG_GenerateGlobalRandomBytes */

typedef struct _NET_CallbackURLData {
	int id;
	char* match;
	NET_CallbackURLFunc func;
	void* closure;
	struct _NET_CallbackURLData* next;
} NET_CallbackURLData;

static NET_CallbackURLData* First = NULL;
static int Counter = 1;


char*
NET_CallbackURLCreate(NET_CallbackURLFunc func, void* closure) {
	unsigned char rand_buf[13];
	char* result = NULL;
	NET_CallbackURLData* tmp;
	NET_CallbackURLFree(func, closure);
	tmp = XP_NEW(NET_CallbackURLData);
	if (!tmp) return NULL;
	tmp->id = Counter++;
	RNG_GenerateGlobalRandomBytes((void *) rand_buf, 12);
	tmp->match =
		PR_smprintf("%02X%02X%02X%02X"
					"%02X%02X%02X%02X"
					"%02X%02X%02X%02X",
					rand_buf[0], rand_buf[1], rand_buf[2], rand_buf[3],
					rand_buf[4], rand_buf[5], rand_buf[6], rand_buf[7],
					rand_buf[8], rand_buf[9], rand_buf[10], rand_buf[11]);
	if (tmp->match) {
		result = PR_smprintf("internal-callback-handler:%d/%s",
							 tmp->id, tmp->match);
	}
	if (result == NULL) {
		if (tmp->match) XP_FREE(tmp->match);
		XP_FREE(tmp);
		return NULL;
	}
	tmp->next = First;
	First = tmp;
	tmp->func = func;
	tmp->closure = closure;
	return result;
}


int
NET_CallbackURLFree(NET_CallbackURLFunc func, void* closure) {
	NET_CallbackURLData** tmp;
	NET_CallbackURLData* t;
	for (tmp = &First ; *tmp ; tmp = &((*tmp)->next)) {
		t = *tmp;
		if (t->func == func && t->closure == closure) {
			*tmp = t->next;
			XP_FREE(t->match);
			XP_FREE(t);
			return 0;
		}
	}
	return -1;
}
	


int NET_LoadCallbackURL (ActiveEntry* ce)
{
	char* url = ce->URL_s->address;
	char* path;
	char* match;
	int id;
	NET_CallbackURLData* tmp;

	path = NET_ParseURL(url, GET_PATH_PART);
	if (path == NULL) return -1;
	id = XP_ATOI(path);
	match = XP_STRCHR(path, '/');
	if (match) match++;
	else match = "";

	for (tmp = First ; tmp ; tmp = tmp->next) {
		if (id == tmp->id && XP_STRCMP(match, tmp->match) == 0) {
			(*tmp->func)(tmp->closure, url);
			break;
		}
	}
	XP_FREE(path);
	return -1;
}


int NET_ProcessCallbackURL(ActiveEntry* ce)
{
	return -1;			/* Should never get here */
}


int NET_InterruptCallbackURL(ActiveEntry* ce)
{
	return -1;			/* Should never get here */
}

