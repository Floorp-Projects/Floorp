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

/*
 *  npassoc.c $Revision: 1.2 $
 *  some xp mime-type file extension associations
 */

#include "npassoc.h"
#include "xp_mem.h"
#include "xp_mcom.h"
#include "net.h"

static NPFileTypeAssoc *np_fassoc_list=0; /* should use xp hash */

extern void NET_cdataAdd(NET_cdataStruct *cd);

#define NP_MAX_EXTS 64
static char *np_array[NP_MAX_EXTS];

/* allocate and fill a null-terminated array of extensions from
   a comma and space delimited list */
static char **
np_parse_exts(const char *exts)
{
	char *p, *e, **res;
	int n=0;

	if(!exts)
		return 0;

	/* p is the current position, e is the start of the nth extension */
	for(e=p=(char *)exts; *p; p++)
	{
		if( (*p==' ') || (*p==',') )
		{
			if(*e == '.') e++;
			if(p>e) 
			{				
				if(!(np_array[n] = (char *)XP_ALLOC(p-e+1)))
					return 0;
				XP_MEMCPY(np_array[n], e, (p-e));
				*(np_array[n]+(p-e)) = 0;
				n++;
				e = p+1;
			}
		}
	}
	if(*e == '.') e++;
	if(p>e) 
	{				
		if(!(np_array[n] = (char *)XP_ALLOC(p-e+1)))
			return 0;
		XP_MEMCPY(np_array[n], e, (p-e));
		*(np_array[n]+(p-e)) = 0;
		n++;
	}

	if(!(res = (char **)XP_ALLOC((n+1)*sizeof(char *))))
		return 0;
	XP_MEMCPY(res, np_array, n*sizeof(char *));
	res[n] = 0;

	return res;
}


/* construct a file association from a mime type.  
 *   - extensions is a list of comma/space separated file extensions
 *     with or without leading .s
 *   - filetype is platform specific data for this list of extensions,
 *     currently creator on mac and open file dialog descriptions on win.
 *     filetype is callee owned data and must remain valid
 */
NPFileTypeAssoc *
NPL_NewFileAssociation(const char *MIMEType, const char *extensions, const char *description, void *fileType)
{
	NPFileTypeAssoc *fassoc = 0;
	
	/* make a file association struct */
    if(!(fassoc=XP_NEW_ZAP(NPFileTypeAssoc)))
        return 0;

	StrAllocCopy((fassoc->type), MIMEType ? MIMEType : "");
	StrAllocCopy((fassoc->description), description ? description : "");
	StrAllocCopy((fassoc->extentstring), extensions ? extensions : "");

	fassoc->fileType = fileType; /* caller owns this data */
	fassoc->extentlist = np_parse_exts(extensions);
	return fassoc;
}


/* deletes a file association.  Returns the platform specific fileType
   data that we dont know how to dispose of.
*/
void *
NPL_DeleteFileAssociation(NPFileTypeAssoc *fassoc)
{
	void* fileType;
	
	if (!fassoc)
		return NULL;

	fileType = fassoc->fileType;
	
	NPL_RemoveFileAssociation(fassoc);

	if (fassoc->type) 
	{ 
		XP_FREE(fassoc->type);
		fassoc->type = NULL;
	}

	if (fassoc->description) 
	{ 
		XP_FREE(fassoc->description);
		fassoc->description = NULL;
	}

	if (fassoc->extentstring) 
	{ 
		XP_FREE(fassoc->extentstring);
		fassoc->extentstring = NULL;
	}

	{
		char** charPtrPtr;
		for (charPtrPtr = &fassoc->extentlist[0]; *charPtrPtr; *charPtrPtr=0, charPtrPtr++)
			XP_FREE(*charPtrPtr);

		fassoc->extentlist = NULL;
	}

	XP_FREE(fassoc);
	
	return fileType;
}


/* Register a file association with us and netlib.
 */
void
NPL_RegisterFileAssociation(NPFileTypeAssoc *fassoc)
{
	if (fassoc)
	{
		fassoc->pNext = np_fassoc_list;
		np_fassoc_list = fassoc;

		NET_cdataCommit(fassoc->type, fassoc->extentstring);
	
		/*
		 * We need to add the description, too, which unfortunately requires
		 * looking the cinfo up AGAIN and setting the desc field...
		 */
		if (fassoc->description)
		{
			NET_cdataStruct temp;
			NET_cdataStruct* cdata;
			
			XP_BZERO(&temp, sizeof(temp));
			temp.ci.type = fassoc->type;
			cdata = NET_cdataExist(&temp);
			XP_ASSERT(cdata);
			if (cdata)
				StrAllocCopy(cdata->ci.desc, fassoc->description);
		}
	}
}


/* Unregister a file association.
 */
NPFileTypeAssoc *
NPL_RemoveFileAssociation(NPFileTypeAssoc *fassoc)
{
	NPFileTypeAssoc *f = np_fassoc_list;

	if(!fassoc)
		return 0;

	/* unregister with netlib */
	if(fassoc == np_fassoc_list)
		np_fassoc_list = np_fassoc_list->pNext;
	else
	{
		for(; f; f=f->pNext)
			if(f->pNext == fassoc)
			{
				NPFileTypeAssoc *ft;
				ft = f->pNext;
				f->pNext = f->pNext->pNext;
				f = ft;
				break;
			}
	}
	return f;
}


/* returns a linked list of registered associations.  
 * if type is NULL you get the entire list else the association matching
 * that MIME type
*/
NPFileTypeAssoc *
NPL_GetFileAssociation(const char *type)
{
	NPFileTypeAssoc *f=NULL;

	if(!np_fassoc_list)
		return NULL;

	if(type==NULL)
		return np_fassoc_list;

	for(f=np_fassoc_list; f; f=f->pNext)
		if(!(XP_STRCMP(type, f->type)))
			return f;

	return NULL;
}


