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
   This file implements utility routines for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "fs2rdf.h"
#include "utils.h"
#include "nlcstore.h"
#include "vocabint.h"


	/* globals */
PRBool rdfDBInited = 0;
PLHashTable*  resourceHash = 0;
PLHashTable*  dataSourceHash = 0;
RDFT gRemoteStore = 0;
RDFT gSessionDB = 0;

	/* externs */
extern	char	*profileDirURL;

#define INITIAL_RESOURCE_LIST_SIZE 50
#define INITIAL_ASSERTION_LIST_SIZE 100
#define GROW_LIST_INCR 50



int
compareStrings(char *s1, char *s2)
{
   return XP_STRCASECMP(s1, s2);
}



RDF_Resource
getMCFFrtop (char* furl)
{
  char* url = getBaseURL(furl);
  RDF_Resource r;
  r = RDF_GetResource(NULL, url, 1);
  freeMem(url);
  return r;
}



RDFFile
makeRDFFile (char* url, RDF_Resource top, PRBool localp)
{
  RDFFile ans = (RDFFile)getMem(sizeof(struct RDF_FileStruct));
  /*  ans->rdf = rdf; */
  ans->url = getBaseURL(url);
  ans->top = top;
  ans->localp = localp;
  if (!localp)  ans->refreshingp = PR_TRUE;
  initRDFFile(ans);
  return ans;
}



void
initRDFFile (RDFFile ans)
{
  char* url = ans->url;
  ans->rtop = getMCFFrtop(url);
  ans->line = (char*)getMem(RDF_BUF_SIZE);
  ans->currentSlot = (char*)getMem(100);
  ans->resourceList = (RDF_Resource*)getMem(INITIAL_RESOURCE_LIST_SIZE * sizeof(RDF_Resource));
  ans->assertionList = (Assertion*)getMem(INITIAL_ASSERTION_LIST_SIZE * sizeof(Assertion));
  ans->resourceListSize = INITIAL_RESOURCE_LIST_SIZE;
  ans->assertionListSize = INITIAL_ASSERTION_LIST_SIZE;
  ans->holdOver = (char*)getMem(RDF_BUF_SIZE);
  ans->depth = 1;
  ans->lastItem = ans->stack[0] = ans->top;
  ans->locked = ans->localp;
  ans->lineSize = LINE_SIZE;
  ans->tv = true;
  ans->resourceCount = 0;
  ans->assertionCount = 0;
}


void
addToResourceList (RDFFile f, RDF_Resource u)
{
  if (f->resourceListSize == f->resourceCount) {
    RDF_Resource* newResourceList = (RDF_Resource*)getMem(sizeof(RDF_Resource)*(f->resourceListSize + GROW_LIST_INCR));
    RDF_Resource* old = f->resourceList;
    memcpy((char*)newResourceList, (char*)f->resourceList, sizeof(RDF_Resource)*f->resourceListSize);
    f->resourceList = newResourceList;
    f->resourceListSize = f->resourceListSize + GROW_LIST_INCR;
    freeMem(old);
  }
  *(f->resourceList + f->resourceCount++) = u;
}



void
addToAssertionList (RDFFile f, Assertion as)
{
  if (f->assertionListSize == f->assertionCount) {
    Assertion* newAssertionList = (Assertion*)getMem(sizeof(RDF_Resource)*(f->assertionListSize + GROW_LIST_INCR));
    Assertion* old = f->assertionList;
    memcpy((char*)newAssertionList, (char*)f->assertionList, sizeof(RDF_Resource)*f->assertionListSize);
    f->assertionList = newAssertionList;
    f->assertionListSize = f->assertionListSize + GROW_LIST_INCR;
    freeMem(old);
  }
  *(f->assertionList + f->assertionCount++) = as;
}



void
removeFromAssertionList(RDFFile f, Assertion as)
{
	/* XXX implement */
}



void
ht_fprintf(PRFileDesc *file, const char *fmt, ...) 
{
    va_list ap;
    char *buf;
    va_start(ap, fmt);
    buf = PR_smprintf(fmt, ap);
    va_end(ap);
    if(buf) {
      	PR_Write(file, buf, strlen(buf));
	free(buf);
    }
}



void
ht_rjcprintf(PRFileDesc *file, const char *fmt, const char *data)
{
	char	*buf;

	buf = PR_smprintf(fmt, data);
	if(buf) {
		PR_Write(file, buf, strlen(buf));
		free(buf);
	}
}


char *
makeDBURL(char* name)
{
  char		*ans;
  size_t		s;
  
  if (profileDirURL == NULL) return NULL;
  if ((ans = (char*) getMem(strlen(profileDirURL) + strlen(name) + 3)) != NULL) {
    s = strlen(profileDirURL);
    memcpy(ans, profileDirURL, s);
    if (ans[s-1] != '/') {
      ans[s++] = '/';
    }
    memcpy(&ans[s], name, strlen(name));
    
#ifdef	XP_WIN
    if (ans[9] == '|') ans[9] = ':';
#endif
  }
  return(ans);
}


PLHashNumber
idenHash (const void *key)
{
	return (PLHashNumber)key;
}



int
idenEqual (const void *v1, const void *v2)
{
	return (v1 == v2);
}



PRBool
inverseTV (PRBool tv)
{
  if (tv == true) {
    return false;
  } else return true;
}



char *
append2Strings (const char* str1, const char* str2)
{
  int32 l1 = strlen(str1);
  int32 len = l1 + strlen(str2);
  char* ans = (char*) getMem(len+1);
  memcpy(ans, str1, l1);
  memcpy(&ans[l1], str2, len-l1);
  return ans;
}



char *
convertString2UTF8AndAppend (int16 charSetID, const char* str1, const char* str2)
{
  char* utf8str1 = (char*) INTL_ConvertLineWithoutAutoDetect(charSetID,
		CS_UTF8, (unsigned char*) str1, strlen(str1));
  char* newString = append2Strings( utf8str1, str2);
  XP_FREEIF(utf8str1);
  return newString;
}



void
stringAppendBase (char* dest, const char* addition)
{
  int32 l1 = strlen(dest);
  int32 l2 = strlen(addition);
  int32 l3 = charSearch('#', addition);
  if (l3 != -1) l2 = l3;
  memcpy(&dest[l1], addition, l2);
}



void
stringAppend (char* dest, const char* addition)
{
  int32 l1 = strlen(dest);
  int32 l2 = strlen(addition);
  memcpy(&dest[l1], addition, l2);
}



int16
charSearch (const char c, const char* data)
{
  char* ch = strchr(data, c);

  if (ch) {
    return (ch - data);
  } else {
    return -1;
  }
 
}



PRBool
endsWith (const char* pattern, const char* uuid)
{
  short l1 = strlen(pattern);
  short l2 = strlen(uuid);
  short index;
  
  if (l2 < l1) return false;
  
  for (index = 1; index <= l1; index++) {
    if (toupper(pattern[l1-index]) != toupper(uuid[l2-index])) return false;
  }
  
  return true;
}



PR_PUBLIC_API(PRBool)
startsWith (const char* pattern, const char* uuid)
{
  short l1 = strlen(pattern);
  short l2 = strlen(uuid);
  if (l2 < l1) return false;
  return (XP_STRNCASECMP(pattern, uuid, l1)  == 0);
}



PRBool
substring (const char* pattern, const char* data)
{
  char *p = XP_STRCASESTR(data, pattern);
  return p != NULL;
}



int16
revCharSearch (const char c, const char* data)
{
  char *p = strrchr(data, c);
  return p ? p-data : -1;
}



PRBool
urlEquals (const char* url1, const char* url2)
{
  int16 n1 = charSearch('#', url1);
  int16 n2 = charSearch('#',  url2);
  if ((n1 == -1) && (n2 == -1)) {    
    return (strcmp(url1, url2) == 0);
  } else if ((n2 == -1) && (n1 > 0)) {
    return ((strlen(url2) == (size_t)(n1)) && (strncmp(url1, url2, n1) == 0));
  } else if ((n1 == -1) && (size_t) (n2 > 0)) {
    return ((strlen(url1) == (size_t)(n2)) && (strncmp(url1, url2, n2) == 0));
  } else return 0;
}



PRBool
isSeparator (RDF_Resource r)
{
  return (startsWith("separator", resourceID(r)) || startsWith("nc:separator", resourceID(r))) ;
}



char *
getBaseURL (const char* url)
{
  int n = charSearch('#' , url);
  char* ans;
  if (n == -1) return copyString(url);
  if (n == 0) return NULL;
  ans = getMem(n+1);
  memcpy(ans, url, n);
  return ans;
}



void
setContainerp (RDF_Resource r, PRBool val)
{
  if (val) {
    r->flags |= CONTAINER_FLAG;
  } else {
    r->flags &= (~CONTAINER_FLAG);
  }
}



PRBool
containerp (RDF_Resource r)
{
  return (r->flags & CONTAINER_FLAG);
}



void
setLockedp (RDF_Resource r, PRBool val)
{
  if (val) {
    r->flags |= LOCKED_FLAG;
  } else {
    r->flags &= (~LOCKED_FLAG);
  }
}



PRBool
lockedp (RDF_Resource r)
{
  return (r->flags & LOCKED_FLAG);
}



uint8
resourceType (RDF_Resource r)
{
 return r->type;
}



void
setResourceType (RDF_Resource r, uint8 val)
{
  r->type = val;
}



char *
resourceID(RDF_Resource r)
{
  return r->url;
 }

char* opTypeToString (RDF_EventType opType) {
  switch (opType) {
    case RDF_ASSERT_NOTIFY :
      return "Assert";
	case RDF_INSERT_NOTIFY :
      return "Insert";
	case RDF_DELETE_NOTIFY :
      return "Unassert";
  }
  return "Unknown Op";
}


void traceNotify (char* event, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type) {
#ifdef DEBUG_guha1    
      char* traceLine = getMem(1000);
      if (type == RDF_INT_TYPE) {
        sprintf(traceLine, "%s  %s(%s, %i)\n", 
              event, resourceID(s), resourceID(u), (int) v);
      } else if (type == RDF_STRING_TYPE){
        sprintf(traceLine, "%s %s(%s, \"%s\")\n", 
                event,  resourceID(s), resourceID(u), (char*) v);
      } else if (type == RDF_RESOURCE_TYPE) {
        sprintf(traceLine, "%s %s(%s, %s)\n", 
                event,  resourceID(s), resourceID(u), resourceID((RDF_Resource)v));
      } else {
        sprintf(traceLine, "%s <gubbish>\n", event);
      }
      FE_Trace(traceLine);
      freeMem(traceLine);    
#endif
}


char *
makeResourceName (RDF_Resource node)
{
	char		*name = NULL;

	name =  resourceID(node);
	if (startsWith("http:", resourceID(node)))
	{
		name = &name[7];
	}
	else if (startsWith("file:", resourceID(node)))
	{
		name = &name[FS_URL_OFFSET];
	}
	else
	{
		if ((name = getResourceDefaultName(node)) == NULL)
		{
			name = resourceID(node);
		}
	}
	return ((name != NULL) ? copyString(name) : NULL);
}


#define kLeft1BitMask  0x80
#define kLeft2BitsMask 0xC0
#define kLeft3BitsMask 0xE0
#define kLeft4BitsMask 0xF0
#define kLeft5BitsMask 0xF8
#define kLeft6BitsMask 0xFC
#define kLeft7BitsMask 0xFE

#define k2BytesLeadByte kLeft2BitsMask
#define k3BytesLeadByte kLeft3BitsMask
#define k4BytesLeadByte kLeft4BitsMask
#define k5BytesLeadByte kLeft5BitsMask
#define k6BytesLeadByte kLeft6BitsMask
#define kTrialByte      kLeft1BitMask

#define UTF8_1Byte(c) ( 0 == ((c) & kLeft1BitMask))
#define UTF8_2Bytes(c) ( k2BytesLeadByte == ((c) & kLeft3BitsMask))
#define UTF8_3Bytes(c) ( k3BytesLeadByte == ((c) & kLeft4BitsMask))
#define UTF8_4Bytes(c) ( k4BytesLeadByte == ((c) & kLeft5BitsMask))
#define UTF8_5Bytes(c) ( k5BytesLeadByte == ((c) & kLeft6BitsMask))
#define UTF8_6Bytes(c) ( k6BytesLeadByte == ((c) & kLeft7BitsMask))
#define UTF8_ValidTrialByte(c) ( kTrialByte == ((c) & kLeft2BitsMask))


PRBool IsUTF8String(const char* utf8)
{
	if(NULL == utf8)
		return TRUE;
	return IsUTF8Text(utf8, strlen(utf8));
}
PRBool IsUTF8Text(const char* utf8, int32 len)
{
   int32 i;
   int32 j;
   int32 clen;
   for(i =0; i < len; i += clen)
   {
      if(UTF8_1Byte(utf8[i]))
      {
        clen = 1;
      } else if(UTF8_2Bytes(utf8[i])) {
        clen = 2;
	/* No enough trail bytes */
        if( (i + clen) > len) 
	  return FALSE;
	/* 0000 0000 - 0000 007F : should encode in less bytes */
        if(0 ==  (utf8[i] & 0x1E )) 
          return FALSE;
      } else if(UTF8_3Bytes(utf8[i])) {
        clen = 3;
	/* No enough trail bytes */
        if( (i + clen) > len) 
	  return FALSE;
	/* a single Surrogate should not show in 3 bytes UTF8, instead, the pair should be intepreted
	   as one single UCS4 char and encoded UTF8 in 4 bytes */
        if((0xED == utf8[i] ) && (0xA0 ==  (utf8[i+1] & 0xA0 ) )) 
          return FALSE;
	/* 0000 0000 - 0000 07FF : should encode in less bytes */
        if((0 ==  (utf8[i] & 0x0F )) && (0 ==  (utf8[i+1] & 0x20 ) )) 
          return FALSE;
      } else if(UTF8_4Bytes(utf8[i])) {
        clen = 4;
	/* No enough trail bytes */
        if( (i + clen) > len) 
	  return FALSE;
	/* 0000 0000 - 0000 FFFF : should encode in less bytes */
        if((0 ==  (utf8[i] & 0x07 )) && (0 ==  (utf8[i+1] & 0x30 )) ) 
          return FALSE;
      } else if(UTF8_5Bytes(utf8[i])) {
        clen = 5;
	/* No enough trail bytes */
        if( (i + clen) > len) 
	  return FALSE;
	/* 0000 0000 - 001F FFFF : should encode in less bytes */
        if((0 ==  (utf8[i] & 0x03 )) && (0 ==  (utf8[i+1] & 0x38 )) ) 
          return FALSE;
      } else if(UTF8_6Bytes(utf8[i])) {
        clen = 6;
	/* No enough trail bytes */
        if( (i + clen) > len) 
	  return FALSE;
	/* 0000 0000 - 03FF FFFF : should encode in less bytes */
        if((0 ==  (utf8[i] & 0x01 )) && (0 ==  (utf8[i+1] & 0x3E )) ) 
          return FALSE;
      } else {
        return FALSE;
      }
      for(j = 1; j<clen ;j++)
      {
	if(! UTF8_ValidTrialByte(utf8[i+j])) /* Trail bytes invalid */
	  return FALSE;
      }
   }
   return TRUE;
}


PR_PUBLIC_API(char *)
RDF_GetResourceName(RDF rdf, RDF_Resource node)
{
  char* name = RDF_GetSlotValue(rdf, node, gCoreVocab->RDF_name, RDF_STRING_TYPE, false, true);
  XP_ASSERT( IsUTF8String(name));
  if (name != NULL) return name;
  name = makeResourceName(node);
  XP_ASSERT( IsUTF8String(name));
  return name;
}


#ifdef MOZILLA_CLIENT
PR_PUBLIC_API(RDF_Resource)
RDFUtil_GetFirstInstance (RDF_Resource type, char* defaultURL)
{

  RDF_Resource bmk = nlocalStoreGetSlotValue(gLocalStore, type,
					     gCoreVocab->RDF_instanceOf,
					     RDF_RESOURCE_TYPE, true, true);
  if (bmk == NULL) {
    /* bmk = RDF_GetResource(NULL, defaultURL, 1); */
    bmk = createContainer(defaultURL);
    nlocalStoreAssert(gLocalStore, bmk, gCoreVocab->RDF_instanceOf, 
		      type, RDF_RESOURCE_TYPE, 1);
  }
  return bmk;
}


PR_PUBLIC_API(void)
RDFUtil_SetFirstInstance (RDF_Resource type, RDF_Resource item)
{
  RDF_Resource bmk = nlocalStoreGetSlotValue(gLocalStore, type,
					     gCoreVocab->RDF_instanceOf,
					     RDF_RESOURCE_TYPE, true, true);
  if (bmk) {
    nlocalStoreUnassert(gLocalStore, bmk,  gCoreVocab->RDF_instanceOf, 
			type,	 RDF_RESOURCE_TYPE);
  }
  if (item) {
    nlocalStoreAssert(gLocalStore, item, gCoreVocab->RDF_instanceOf, 
			type, RDF_RESOURCE_TYPE, true);
  }
}



PR_PUBLIC_API(RDF_Resource)
RDFUtil_GetQuickFileFolder()
{
  return RDFUtil_GetFirstInstance(gNavCenter->RDF_BookmarkFolderCategory, "NC:Bookmarks");
}



PR_PUBLIC_API(void)
RDFUtil_SetQuickFileFolder(RDF_Resource container)
{
  RDFUtil_SetFirstInstance(gNavCenter->RDF_BookmarkFolderCategory, container);
}


RDF_Resource gPTFolder = NULL;
PR_PUBLIC_API(RDF_Resource)
RDFUtil_GetPTFolder()
{
  if (gPTFolder) return gPTFolder;
 return (gPTFolder =  RDFUtil_GetFirstInstance(gNavCenter->RDF_PersonalToolbarFolderCategory, "PersonalToolbar"));
}



PR_PUBLIC_API(void)
RDFUtil_SetPTFolder(RDF_Resource container)
{
  /*
  RDFUtil_SetFirstInstance( gNavCenter->RDF_PersonalToolbarFolderCategory, container);
  */
}



PR_PUBLIC_API(RDF_Resource)
RDFUtil_GetNewBookmarkFolder()
{
  return RDFUtil_GetFirstInstance(gNavCenter->RDF_NewBookmarkFolderCategory, "NC:Bookmarks");
}



PR_PUBLIC_API(void)
RDFUtil_SetNewBookmarkFolder(RDF_Resource container)
{
  RDFUtil_SetFirstInstance(gNavCenter->RDF_NewBookmarkFolderCategory, container);
}



PR_PUBLIC_API(RDF_Resource)
RDFUtil_GetDefaultSelectedView()
{
  return RDFUtil_GetFirstInstance(gNavCenter->RDF_DefaultSelectedView, "selectedView");
}



PR_PUBLIC_API(void)
RDFUtil_SetDefaultSelectedView(RDF_Resource container)
{
  RDFUtil_SetFirstInstance(gNavCenter->RDF_DefaultSelectedView, container);
}



/* I am putting the cookies stuff here for now */

RDFT gCookieStore = 0;

PUBLIC void
NET_InitRDFCookieResources (void) ;



PR_PUBLIC_API(void)
RDF_AddCookieResource(char* name, char* path, char* host, char* expires, char* value, 
                      PRBool isDomain, PRBool secure) {
  char* url = getMem(strlen(name) + strlen(host) + strlen(path) + 10);
  RDF_Resource ru;
  RDF_Resource hostUnit = RDF_GetResource(NULL, host, 0);
  if (!hostUnit) {
    hostUnit = RDF_GetResource(NULL, host, 1);
    setContainerp(hostUnit, 1);
    setResourceType(hostUnit, COOKIE_RT);
    remoteStoreAdd(gCookieStore, hostUnit, gCoreVocab->RDF_parent, gNavCenter->RDF_Cookies, 
                   RDF_RESOURCE_TYPE, 1);  
  }
  sprintf(url, "cookie:%s!%s!%s", host, path,  name);
  ru = RDF_GetResource(NULL, url, 1);
  setResourceType(ru, COOKIE_RT);
  remoteStoreAdd(gCookieStore, ru, gCoreVocab->RDF_name, name, RDF_STRING_TYPE, 1);
  remoteStoreAdd(gCookieStore, ru, gNavCenter->cookieDomain, (void *)((isDomain) ? 1:0), RDF_INT_TYPE, 1);
  remoteStoreAdd(gCookieStore, ru, gNavCenter->cookieValue, value, RDF_STRING_TYPE, 1);
  remoteStoreAdd(gCookieStore, ru, gNavCenter->cookieHost, host, RDF_STRING_TYPE, 1);
  remoteStoreAdd(gCookieStore, ru, gNavCenter->cookiePath, path, RDF_STRING_TYPE, 1);
  remoteStoreAdd(gCookieStore, ru, gNavCenter->cookieSecure, (void *)((secure) ? 1:0), RDF_INT_TYPE, 1);
  remoteStoreAdd(gCookieStore, ru, gNavCenter->cookieExpires, expires, RDF_STRING_TYPE, 1);
  remoteStoreAdd(gCookieStore, ru, gCoreVocab->RDF_parent, hostUnit, RDF_RESOURCE_TYPE, 1);  
}



PRBool
CookieUnassert (RDFT r, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type) {
  if (resourceType(u) == COOKIE_RT) {
    /* delete the cookie */
    /* NET_DeleteCookie(resourceID(u)); */
    remoteStoreRemove(r, u, s, v, type);
    return 1;
  } else return 0;
}



RDF_Cursor
CookieGetSlotValues(RDFT rdf, RDF_Resource u, RDF_Resource s,
	RDF_ValueType type, PRBool inversep, PRBool tv)
{
	RDF_Cursor	c = NULL;

	if ((resourceType(u) == COOKIE_RT) && (s == gNavCenter->RDF_Command) &&
		(type == RDF_RESOURCE_TYPE) && (inversep) && (tv))
	{
		if ((c = (RDF_Cursor)getMem(sizeof(struct RDF_CursorStruct))) != NULL)
		{
			c->u = u;
			c->s = s;
			c->type = type;
			c->inversep = inversep;
			c->tv = tv;
			c->count = 0;
			c->pdata = NULL;
		}
		return(c);
	}
	else
	{
		c = remoteStoreGetSlotValues(rdf, u, s, type, inversep, tv);
	}
	return(c);
}



#define	COOKIE_CMD_PREFIX	"Command:Cookie:"
#define	COOKIE_CMD_HOSTS	"Command:Cookie:TBD"	/* actual cmd name */

void *
CookieGetNextValue(RDFT rdf, RDF_Cursor c)
{

	void		*data = NULL;

	if (c != NULL)
	{
		if ((resourceType(c->u) == COOKIE_RT) &&
			(c->s == gNavCenter->RDF_Command) &&
			(c->type == RDF_RESOURCE_TYPE))
		{
			/* return cookie commands here */
			switch(c->count)
			{
				case	0:
				if (containerp(c->u))
				{
					data = (void *)RDF_GetResource(NULL,
						COOKIE_CMD_HOSTS, true);
				}
				break;
			}
          		c->count++;
		}
		else
		{
			data = remoteStoreNextValue(rdf, c);
		}
	}
	return(data);
}



RDF_Error
CookieDisposeCursor(RDFT rdf, RDF_Cursor c)
{
	RDF_Error	err=0;

	if (c != NULL)
	{
		if ((c->s == gNavCenter->RDF_Command) &&
			(c->type == RDF_RESOURCE_TYPE))
		{
			if (c->pdata != NULL)
			{
				/* free private data */
			}
			freeMem(c);
			err = 0;
		}
		else
		{
			err = remoteStoreDisposeCursor(rdf, c);
		}
	}
	return(err);
}



PRBool
CookieAssert(RDFT rdf, RDF_Resource u, RDF_Resource s, void *v,
		RDF_ValueType type, PRBool tv)
{
	PRBool		retVal = false;

	if ((resourceType(u) == COOKIE_RT) &&
		(s == gNavCenter->RDF_Command) &&
		(type == RDF_RESOURCE_TYPE) &&
		(v != NULL) && (tv))
	{
       RDF_Resource vu = (RDF_Resource)v;
		retVal = true;
		/* handle command in 'v' on 'u' */
		if (startsWith(COOKIE_CMD_PREFIX, resourceID(vu)))
		{
			if (!strcmp(resourceID(vu), COOKIE_CMD_HOSTS))
			{
				/* do whatever is appropriate for the cmd */
			}
		}
	}
	return(retVal);
}



void *
CookieGetSlotValue(RDFT rdf, RDF_Resource u, RDF_Resource s,
	RDF_ValueType type, PRBool inversep, PRBool tv)
{
	void		*data = NULL;

	if ((startsWith(COOKIE_CMD_PREFIX, resourceID(u))) &&
		(s == gCoreVocab->RDF_name) &&
		(type == RDF_STRING_TYPE) && (!inversep) && (tv))
	{
		data = copyString(resourceID(u) + strlen(COOKIE_CMD_PREFIX));
	}
	else
	{
		data = remoteStoreGetSlotValue(rdf, u, s, type, inversep, tv);
	}
	return(data);
}



RDFT
MakeCookieStore (char* url)
{
  if (startsWith("rdf:CookieStore", url)) {
    if (gCookieStore == 0) {
      RDFT ntr = NewRemoteStore(url);
      ntr->assert = CookieAssert;
      ntr->unassert = CookieUnassert;
      ntr->getSlotValue = CookieGetSlotValue;
      ntr->getSlotValues = CookieGetSlotValues;
      ntr->hasAssertion = remoteStoreHasAssertion;
      ntr->nextValue = CookieGetNextValue;
      ntr->disposeCursor = CookieDisposeCursor;
      gCookieStore = ntr;
      /*      NET_InitRDFCookieResources (    ) ; */
      return ntr;
    } else return gCookieStore;
  } else return NULL;
}
#endif /* MOZILLA_CLIENT */
