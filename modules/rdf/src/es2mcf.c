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

/* 
   This file implements FTP support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "es2mcf.h"
#include "glue.h"
#include "ht.h"
#include "utils.h"


	/* externs */
extern	RDF	gNCDB;
#define ESFTPRT(x) ((resourceType((RDF_Resource)x) == ES_RT) || (resourceType((RDF_Resource)x) == FTP_RT))



RDFT
MakeESFTPStore (char* url)
{
  RDFT ntr = NewRemoteStore(url);
  ntr->assert = ESAssert;
  ntr->unassert = ESUnassert;
  ntr->possiblyAccessFile =  ESFTPPossiblyAccessFile;
  return ntr;
}



void
ESFTPPossiblyAccessFile (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
  if (((resourceType(u) == ES_RT) || (resourceType(u) == FTP_RT)) &&
	(s == gCoreVocab->RDF_parent) && (containerp(u))) {
    char* id = resourceID(u);
    readRDFFile((resourceType(u) == ES_RT ? &id[4] : id), u, false, rdf);
   }
}



PRBool
ESAssert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
		   RDF_ValueType type, PRBool tv)
{
	PRBool		retVal;
  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char*) v)));	

  if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE)  &&
      (ESFTPRT((RDF_Resource)v)) &&
      (tv) && (containerp((RDF_Resource)v))) {
    ESAddChild(rdf, (RDF_Resource)v, u);
    retVal = PR_TRUE;
  } else {
    retVal = PR_FALSE;
  }
  return(retVal);
}



PRBool
ESUnassert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
		   RDF_ValueType type)
{
	PRBool		retVal;

  XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char*) v)));	
  if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE)  &&
      (ESFTPRT((RDF_Resource)v)) &&
      (containerp((RDF_Resource)v))) {
    ESRemoveChild(rdf, (RDF_Resource)v, u);
    retVal = PR_TRUE;
  } else {
    retVal = PR_FALSE;
  }
  return(retVal);
}



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
ESAddChild (RDFT rdf, RDF_Resource parent, RDF_Resource child)
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
				(MWContext *)gRDFMWContext(rdf),
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
ESRemoveChild (RDFT rdf, RDF_Resource parent, RDF_Resource child)
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
				(MWContext *)gRDFMWContext(rdf),
				es_GetUrlExitFunc);
		}
		else
		{
			NET_FreeURLStruct(urls);
		}
	}
}



void
parseNextESFTPLine (RDFFile f, char* line)
{
	PRBool		directoryp;
	RDF_Resource	ru;
	char		*token, *url;
	int16		loop, tokenNum = 0;
	int32		val;

	if (f->fileType != FTP_RT && f->fileType != ES_RT)	return;

	/* work around bug where linefeeds are actually encoded as %0A */
	if (endsWith("%0A", line))	line[strlen(line)-3] = '\0';

	if ((token = strtok(line, " ")) != NULL)
	{
		/* skip 1st token (the numeric command) */
		token = strtok(NULL, " \t");
	}

	if (startsWith("200:", line))
	{
		while (token != NULL)
		{
			while ((token != NULL) && (tokenNum < RDF_MAX_NUM_FILE_TOKENS))
			{
				if (!strcmp(token, "Filename"))
				{
					f->tokens[f->numFileTokens].token = gCoreVocab->RDF_name;
					f->tokens[f->numFileTokens].type = RDF_STRING_TYPE;
					f->tokens[f->numFileTokens].tokenNum = tokenNum;
					++(f->numFileTokens);
				}
				else if (!strcmp(token, "Content-Length"))
				{
					f->tokens[f->numFileTokens].token = gWebData->RDF_size;
					f->tokens[f->numFileTokens].type = RDF_INT_TYPE;
					f->tokens[f->numFileTokens].tokenNum = tokenNum;
					++(f->numFileTokens);
				}
				else if (!strcmp(token, "File-type"))
				{
					f->tokens[f->numFileTokens].token = gWebData->RDF_description;
					f->tokens[f->numFileTokens].type = RDF_STRING_TYPE;
					f->tokens[f->numFileTokens].tokenNum = tokenNum;
					++(f->numFileTokens);
				}
				else if (!strcmp(token, "Last-Modified"))
				{
					f->tokens[f->numFileTokens].token = gWebData->RDF_lastModifiedDate;
					f->tokens[f->numFileTokens].type = RDF_STRING_TYPE;
					f->tokens[f->numFileTokens].tokenNum = tokenNum;
					++(f->numFileTokens);
				}
/*
				else if (!strcmp(token, "Permissions"))
				{
					f->tokens[f->numFileTokens].token = NULL;
					f->tokens[f->numFileTokens].type = RDF_STRING_TYPE;
					f->tokens[f->numFileTokens].tokenNum = tokenNum;
					++(f->numFileTokens);
				}
*/
				++tokenNum;
				token = strtok(NULL, " \t");
			}
		}
	}
	else if (startsWith("201:", line))
	{
		directoryp = false;
		while (token != NULL)
		{
			for (loop=0; loop<f->numFileTokens; loop++)
			{
				if (tokenNum == f->tokens[loop].tokenNum)
				{
					f->tokens[loop].data = strdup(token);
					if (f->tokens[loop].token == gWebData->RDF_description)
					{
						if (startsWith("Directory", token) ||
							startsWith("Sym-Directory", token))
						{
							directoryp = true;
						}
					}
				}
			}
			++tokenNum;
			token = strtok(NULL, " \t");
		}

		ru = NULL;
		for (loop=0; loop<f->numFileTokens; loop++)
		{
			/* find name, create resource from it */

			if (f->tokens[loop].token == gCoreVocab->RDF_name)
			{
				if (resourceType(f->top) == ES_RT)
				{
					url = PR_smprintf("nes:%s%s%s", f->url, f->tokens[loop].data, (directoryp) ? "/":"");
				}
				else
				{
					url = PR_smprintf("%s%s%s", f->url, f->tokens[loop].data, (directoryp) ? "/":"");
				}
				if (url != NULL)
				{
					if ((ru = RDF_GetResource(NULL, url, 1)) != NULL)
					{
						setResourceType(ru, resourceType(f->top));
						if (directoryp == true)
							{
							setContainerp(ru, 1);
						}
					XP_FREE(url);
					}
				}
				break;				
			}
		}
		if (ru != NULL)
		{
			for (loop=0; loop<f->numFileTokens; loop++)
			{
				if (f->tokens[loop].data != NULL)
				{
					switch(f->tokens[loop].type)
					{
						case	RDF_STRING_TYPE:
						addSlotValue(f, ru, f->tokens[loop].token,
							unescapeURL(f->tokens[loop].data),
							f->tokens[loop].type, NULL);
						break;
	
						case	RDF_INT_TYPE:
						if (directoryp == false)
						{
							sscanf(f->tokens[loop].data, "%lu", &val);
							if (val != 0)
							{
								addSlotValue(f, ru, f->tokens[loop].token,
									(void *)val, f->tokens[loop].type, NULL);
							}
						}
						break;
					}
				}
			}
			addSlotValue(f, ru, gCoreVocab->RDF_parent, f->top, RDF_RESOURCE_TYPE, NULL);
		}
		
	}
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
