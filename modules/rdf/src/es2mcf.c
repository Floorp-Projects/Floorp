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

/* 
   This file implements FTP support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "es2mcf.h"
#include "glue.h"
#include "ht.h"


	/* externs */
extern	RDF	gNCDB;



RDFT
MakeESFTPStore (char* url)
{
  RDFT ntr = (RDFT)getMem(sizeof(struct RDF_TranslatorStruct));
  ntr->assert = ESAssert;
  ntr->unassert = ESUnassert;
  ntr->getSlotValue = ESGetSlotValue;
  ntr->getSlotValues = ESGetSlotValues;
  ntr->hasAssertion = ESHasAssertion;
  ntr->nextValue = ESNextValue;
  ntr->disposeCursor = ESDisposeCursor;
  ntr->url = copyString(url);
  return ntr;
}



PRBool
ESFTPRT (RDF_Resource u)
{
  return ((resourceType(u) == ES_RT) ||
	  (resourceType(u) == FTP_RT));
}



PRBool
ESAssert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
		   RDF_ValueType type, PRBool tv)
{
  if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE)  &&
      (ESFTPRT((RDF_Resource)v)) &&
      (tv) && (containerp((RDF_Resource)v))) {
    ESAddChild((RDF_Resource)v, u);
  } else {
    return 0;
  }
}



PRBool
ESUnassert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
		   RDF_ValueType type)
{
  if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE)  &&
      (ESFTPRT((RDF_Resource)v)) &&
      (containerp((RDF_Resource)v))) {
    ESRemoveChild((RDF_Resource)v, u);
  } else {
    return 0;
  }
}



PRBool
ESDBAdd (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
		RDF_ValueType type)
{
  Assertion nextAs, prevAs, newAs; 
  if ((s == gCoreVocab->RDF_instanceOf) && (v == gWebData->RDF_Container)) {
    setContainerp(u, true);
    return 1;
  }
 	
  nextAs = prevAs = u->rarg1;
  while (nextAs != null) {
    if (asEqual(nextAs, u, s, v, type)) return 1;
    prevAs = nextAs;
    nextAs = nextAs->next;
  }
  newAs = makeNewAssertion(u, s, v, type, 1);
  if (prevAs == null) {
    u->rarg1 = newAs;
  } else {
    prevAs->next = newAs;
  }
  if (type == RDF_RESOURCE_TYPE) {
    nextAs = prevAs = ((RDF_Resource)v)->rarg2;
    while (nextAs != null) {
      prevAs = nextAs;
      nextAs = nextAs->invNext;
    }
    if (prevAs == null) {
      ((RDF_Resource)v)->rarg2 = newAs;
    } else {
      prevAs->invNext = newAs;
    }
  }
  sendNotifications2(rdf, RDF_ASSERT_NOTIFY, u, s, v, type, 1);
  return true;
}



PRBool
ESDBRemove (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type)
{
  Assertion nextAs, prevAs,  ans;
  PRBool found = false;
  nextAs = prevAs = u->rarg1;
  while (nextAs != null) {
    if (asEqual(nextAs, u, s, v, type)) {
      if (prevAs == null) {
	u->rarg1 = nextAs->next;
      } else {
	prevAs->next = nextAs->next;
      }
      found = true;
      ans = nextAs;
      break;
    }
    prevAs = nextAs;
    nextAs = nextAs->next; 
  }
  if (found == false) return false;
  if (type == RDF_RESOURCE_TYPE) {
    nextAs = prevAs = ((RDF_Resource)v)->rarg2;
    while (nextAs != null) {
      if (nextAs == ans) {
	if (prevAs == nextAs) {
	  ((RDF_Resource)v)->rarg2 =  nextAs->invNext;
	} else {
	  prevAs->invNext = nextAs->invNext;
	}
      }
      prevAs = nextAs;
      nextAs = nextAs->invNext;
    }
  }
  sendNotifications2(rdf, RDF_DELETE_NOTIFY, u, s, v, type, 1);
  return true;
}



PRBool
ESHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
		       RDF_ValueType type, PRBool tv)
{
	Assertion	nextAs;

	if (!ESFTPRT(u)) return 0;

	nextAs = u->rarg1;
	while (nextAs != NULL)
	{
		if (asEqual(nextAs, u, s, v, type) && (nextAs->tv == tv))
		{
			return(true);
		}
		nextAs = nextAs->next;
	}
	possiblyAccessES(rdf, u, s, false);
	return false;
}



void *
ESGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,
		PRBool inversep,  PRBool tv)
{
	if (!ESFTPRT(u)) return NULL;

	if ((s == gCoreVocab->RDF_name) && (type == RDF_STRING_TYPE) && (tv))
	{
		char *pathname, *name = NULL;
		int16 n,len;

		if (pathname = copyString(resourceID(u)))
		{
			len = strlen(pathname);
			if (pathname[len-1] == '/')  pathname[--len] = '\0';
			n = revCharSearch('/', pathname);
			name = unescapeURL(&pathname[n+1]);
			freeMem(pathname);
		}
		return(name);
	}
	else
	if (u->rarg1 == NULL) possiblyAccessES(rdf, u, s, inversep);
	return null;
}



RDF_Cursor
ESGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s,
		RDF_ValueType type,  PRBool inversep, PRBool tv)
{
  Assertion as;
  if (!ESFTPRT(u)) return 0;
  as  = (inversep ? u->rarg2 : u->rarg1);
  if (as == null) {
    possiblyAccessES(rdf, u, s, inversep);
    return null;
  }
  return NULL;
}



void *
ESNextValue (RDFT mcf, RDF_Cursor c)
{
  return null;
}



RDF_Error
ESDisposeCursor (RDFT mcf, RDF_Cursor c)
{
  freeMem(c);
  return noRDFErr;
}



/** To be written **/



void
es_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx)
{
	RDF_Resource		parent = NULL, child = NULL, r;
	_esFEData		*feData;
	char			*newURL, *p;

	feData = (_esFEData *)urls->fe_data;
	if ((status >= 0) && (feData != NULL))
	{
		parent = RDF_GetResource(gNCDB, feData->parent, false);
		child = RDF_GetResource(gNCDB, feData->child, false);
		if ((parent != NULL) && (child != NULL))
		{
			switch(feData->method)
			{
				case	URL_POST_METHOD:
				if (((p = strrchr(resourceID(child), '/')) != NULL) && (*++p != '\0'))
				{
					if ((newURL = append2Strings(resourceID(parent), p)) != NULL)
					{
						if ((r = RDF_GetResource(gNCDB, newURL, 1)) != NULL)
						{
							setContainerp(r, containerp(child));
							setResourceType(r, resourceType(child));
						
							remoteStoreAdd(gRemoteStore, r,
								gCoreVocab->RDF_parent, parent,
								RDF_RESOURCE_TYPE, 1);
						}
						freeMem(newURL);
					}
				}
				break;

				case	URL_DELETE_METHOD:
				remoteStoreRemove(gRemoteStore, child,
					gCoreVocab->RDF_parent, parent,
					RDF_RESOURCE_TYPE);
				break;
			}
		}
	}
	else if (status < 0)
	{
		if ((cx != NULL) && (urls != NULL) && (urls->error_msg != NULL))
		{
			FE_Alert(cx, urls->error_msg);
		}
	}
	if (feData != NULL)
	{
		esFreeFEData(feData);
	}
        NET_FreeURLStruct (urls);
}



char *
nativeFilename(char *filename)
{
	char		*newName = NULL, *temp;
	int		x = 0;

	if (filename == NULL)	return(NULL);
	if ((newName = unescapeURL(filename)) != NULL)
	{
		if ((temp = convertFileURLToNSPRCopaceticPath(newName)) != NULL)
		{
			temp = copyString(temp);
		}
		freeMem(newName);
		newName = temp;
#ifdef	XP_WIN
		if (newName != NULL)
		{
			while (newName[x] != '\0')
			{
				if (newName[x] == '/')
				{
					newName[x] = '\\';
				}
				++x;
			}
		}
#endif
	}
	return(newName);
}



_esFEData *
esMakeFEData(RDF_Resource parent, RDF_Resource child, int method)
{
	_esFEData		*feData;
	
	if ((feData = (_esFEData *)XP_ALLOC(3*sizeof(char *))) != NULL)
	{
		feData->parent = copyString(resourceID(parent));
		feData->child = copyString(resourceID(child));
		feData->method = method;
	}
	return(feData);
}



void
esFreeFEData(_esFEData *feData)
{
	if (feData != NULL)
	{
		if (feData->parent)	freeMem(feData->parent);
		if (feData->child)	freeMem(feData->child);
		freeMem(feData);
	}
}




  /** go tell the directory that child got added to parent **/
void
ESAddChild (RDF_Resource parent, RDF_Resource child)
{
	URL_Struct		*urls;
	void			*feData, **files_to_post = NULL;

	if ((urls = NET_CreateURLStruct(resourceID(parent), NET_SUPER_RELOAD)) != NULL)
	{
		feData = (void *)esMakeFEData(parent, child, URL_POST_METHOD);
		if ((files_to_post = (char **)XP_ALLOC(2*sizeof(char *))) != NULL)
		{
			files_to_post[0] = nativeFilename(resourceID(child));
			files_to_post[1] = NULL;
		}
		if ((feData != NULL) && (files_to_post != NULL))
		{
			urls->files_to_post = (void *)files_to_post;
			urls->post_to = NULL;
			urls->method = URL_POST_METHOD;
			urls->fe_data = (void *)feData;
			NET_GetURL(urls, FO_PRESENT,
				(MWContext *)gRDFMWContext(),
				es_GetUrlExitFunc);
		}
		else
		{
			if (feData != NULL)
			{
				esFreeFEData(feData);
			}
			if (files_to_post != NULL)
			{
				if (files_to_post[0] != NULL)	freeMem(files_to_post[0]);
				XP_FREE(files_to_post);
			}
			NET_FreeURLStruct(urls);
		}
	}
}



  /** remove the child from the directory **/
void
ESRemoveChild (RDF_Resource parent, RDF_Resource child)
{
	URL_Struct		*urls;
	void			*feData;

	if ((urls = NET_CreateURLStruct(resourceID(child), NET_SUPER_RELOAD)) != NULL)
	{
		feData = (void *)esMakeFEData(parent, child, URL_DELETE_METHOD);
		if (feData != NULL)
		{
			urls->method = URL_DELETE_METHOD;
			urls->fe_data = (void *)feData;
			NET_GetURL(urls, FO_PRESENT,
				(MWContext *)gRDFMWContext(),
				es_GetUrlExitFunc);
		}
		else
		{
			NET_FreeURLStruct(urls);
		}
	}
}



void
possiblyAccessES(RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
   if ((ESFTPRT(u)) && 
       (s == gCoreVocab->RDF_parent) && (containerp(u))) {
     char* id =  resourceID(u);
     readRDFFile((resourceType(u) == ES_RT ? &id[4] : id), u, false);
   }
}



void
parseNextESFTPLine (RDFFile f, char* line)
{
  int16 i1, i2;
  char url[100];
  RDF_Resource ru;
  PRBool directoryp;
   
  if (f->fileType == FTP_RT) {
	  if (!startsWith("201", line)) return;
	  line = &line[5];
  }
  i1 = charSearch(' ', line);
  if (i1 == -1) return;
  i2 = charSearch(' ', &line[i1+1]) + i1 + 1;
  line[i1] = '\0';
  directoryp = 0;
  if (strlen(line) > 64) return;
  if (resourceType(f->top) == ES_RT) {
    directoryp = startsWith("Director", &line[i1+1]);
     sprintf(url, "nes:%s%s%s", f->url, line, (directoryp ? "/" : ""));
  } else {
    int i3 = charSearch(' ', &line[i1+i2+1]);
    directoryp = startsWith("Directory", &line[i1+i2+i3+2]);

/* this is bad as files can be of zero-length!
    if ((charSearch('.', line) ==-1) && (startsWith("0", &line[i1+1]))) directoryp = 1;
*/

    sprintf(url, "%s%s%s", f->url, line, (directoryp ? "/" : ""));
   }
  ru = RDF_GetResource(NULL, url, 1);
  setResourceType(ru, resourceType(f->top));
  if (directoryp) setContainerp(ru, 1);
/*
  remoteStoreAdd(gRemoteStore, ru, gCoreVocab->RDF_name, NET_UnEscape(line), RDF_STRING_TYPE, 1);
*/
  remoteStoreAdd(gRemoteStore, ru, gCoreVocab->RDF_parent, f->top, RDF_RESOURCE_TYPE, 1);
}



int
parseNextESFTPBlob(NET_StreamClass *stream, char* blob, int32 size)
{
  RDFFile f;
  int32 n, last, m;
  n = last = 0;

  f = (RDFFile)stream->data_object;
  if (f == NULL ||  size < 0) {
    return MK_INTERRUPTED;
  }

  while (n < size) {
    char c = blob[n];
    m = 0;
    memset(f->line, '\0', f->lineSize);
    if (f->holdOver[0] != '\0') {
      memcpy(f->line, f->holdOver, strlen(f->holdOver));
      m = strlen(f->holdOver);
      memset(f->holdOver, '\0', RDF_BUF_SIZE);
    }
    while ((m < f->lineSize) && (c != '\r') && (c != '\n') && (n < size)) {
      f->line[m] = c;
      m++;
      n++;
      c = blob[n];
    }
    n++;
    if (m > 0) {
      if ((c == '\n') || (c == '\r')) {
	last = n;
	parseNextESFTPLine(f, f->line);
      } else if (size > last) {
	memcpy(f->holdOver, f->line, m);
      }
    }
  }
  return(size);
}

