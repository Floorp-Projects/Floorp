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
   This file implements file system support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "fs2rdf.h"
#include "glue.h"

	/* external string references in allxpstr */
extern	int	RDF_UNABLETODELETEFILE, RDF_UNABLETODELETEFOLDER;



char *
getVolume(int volNum)
{
	char		*buffer = NULL;
#ifdef XP_WIN
	UINT driveType;
#endif

#ifdef	XP_MAC
	ParamBlockRec		pb;
	Str32			str;
#endif

	if ((buffer = getMem(64L)) == NULL)
	{
		return(NULL);
	}

#ifdef	XP_MAC	
	pb.volumeParam.ioCompletion = NULL;
	pb.volumeParam.ioVolIndex = volNum + 1;
	pb.volumeParam.ioNamePtr = (void *)str;
	if (!PBGetVInfo(&pb,FALSE))	{
		str[1+(unsigned)str[0]] = '\0';
		strcpy(buffer, "file:///");
		strcat(buffer, NET_Escape((char *)&str[1], URL_XALPHAS));  /* URL_PATH */
		strcat(buffer,"/");
		return(buffer);
		}
#endif

#ifdef	XP_WIN
	/* Windows-specific method for getting drive info without touching the drives (Dave H.) */
	sprintf(buffer, "%c:\\", volNum + 'A');
	driveType = GetDriveType(buffer);
	if (driveType != DRIVE_UNKNOWN && driveType != DRIVE_NO_ROOT_DIR)
	{
		sprintf( buffer, "file:///%c:/", volNum + 'A');
		return buffer;
	}
#endif

	if (buffer != NULL)
	{
		freeMem(buffer);
	}
	return(NULL);
}



void
buildVolumeList(RDF_Resource fs)
{
	RDF_Resource	vol;
	char		*volName;
	int		volNum=0;

	while (volNum < 26) {
	   if ((volName = getVolume(volNum++)) != NULL)  {
	
		  if ((vol = RDF_GetResource(NULL, volName, 1)) != NULL)
		  {
			setContainerp(vol, 1);
			setResourceType(vol, LFS_RT);
			remoteStoreAdd(gRemoteStore, vol, gCoreVocab->RDF_parent,
					fs, RDF_RESOURCE_TYPE, 1);
		  }
	   }
	}
}



PRDir *
OpenDir(char *name)
{
  if (startsWith("file:///", name)) {
    /* return PR_OpenDir(&name[FS_URL_OFFSET]); */
    return CallPROpenDirUsingFileURL(name);
  } else return(NULL);
}



RDFT
MakeLFSStore (char* url)
{
  int		volNum=0;
  RDFT ntr = (RDFT)getMem(sizeof(struct RDF_TranslatorStruct));
  ntr->assert = NULL;
  ntr->unassert = fsUnassert;
  ntr->getSlotValue = fsGetSlotValue;
  ntr->getSlotValues = fsGetSlotValues;
  ntr->hasAssertion = fsHasAssertion;
  ntr->nextValue = fsNextValue;
  ntr->disposeCursor = fsDisposeCursor;
  buildVolumeList(gNavCenter->RDF_LocalFiles);
  ntr->url = copyString(url);
  return ntr;
}



PRBool
fsRemoveFile(char *filePathname, PRBool justCheckWriteAccess)
{
	PRBool		errFlag = true;

	if (justCheckWriteAccess == true)
	{
		errFlag = ((!CallPRWriteAccessFileUsingFileURL(filePathname)) ? false:true);
	}
	else
	{
		if (!CallPRWriteAccessFileUsingFileURL(filePathname))
		{
			if (!CallPRDeleteFileUsingFileURL(filePathname))
			{
				errFlag = false;
			}
		}
		if (errFlag == true)
		{
			FE_Alert(((MWContext *)gRDFMWContext()),
				PR_smprintf(XP_GetString(RDF_UNABLETODELETEFILE),
				filePathname));
		}
	}
	return(errFlag);
}



PRBool
fsRemoveDir(char *filePathname, PRBool justCheckWriteAccess)
{
	PRBool		dirFlag, errFlag = false;
	PRDir		*d, *tempD;
	PRDirEntry	*de;
	char		*base, *dirStr, *fileStr;
	int		len, n = PR_SKIP_BOTH;

	if (filePathname == NULL)			return(true);
	if ((d = OpenDir(filePathname)) == NULL)	return(true);

	while (de = PR_ReadDir(d, n++))
	{
		base = NET_Escape(de->name, URL_XALPHAS);
		if (base == NULL)	return(true);
		fileStr = append2Strings(filePathname, base);

		XP_FREE(base);
		if (fileStr == NULL)	return(true);
		dirFlag = (((tempD = OpenDir(fileStr)) != NULL)	? true:false);
		if (tempD != NULL)
		{

			PR_CloseDir(tempD);
		}

		if (dirFlag)
		{
			dirStr = append2Strings(fileStr, "/");
			errFlag = fsRemoveDir(dirStr, justCheckWriteAccess);
			if (dirStr != NULL)	freeMem(dirStr);
		}
		else
		{
			errFlag = fsRemoveFile(fileStr, justCheckWriteAccess);
		}

		freeMem(fileStr);
		if (errFlag == true)	break;
	}


	PR_CloseDir(d);

	if (errFlag == false)
	{
		if ((dirStr = copyString(filePathname)) != NULL)
		{
			len = strlen(dirStr)-1;
			if (dirStr[len] == '/')
			{
				dirStr[len] = '\0';
			}
			if (justCheckWriteAccess == true)
			{
				errFlag = fsRemoveFile(dirStr, justCheckWriteAccess);
			}
			else
			{
				errFlag = (CallPR_RmDirUsingFileURL(dirStr) != 0);

				if (errFlag == true)
				{
					FE_Alert(((MWContext *)gRDFMWContext()),
						PR_smprintf(XP_GetString(RDF_UNABLETODELETEFOLDER),
						filePathname));
				}
			}
			freeMem(dirStr);
		}
	}

	return(errFlag);
}



PRBool
fsUnassert (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type)
{
	char		*filePathname;
	PRBool		retVal = false;

	if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE) &&
		(resourceType(u) == LFS_RT) && (containerp(v)) && 
		(resourceType((RDF_Resource)v) == LFS_RT) &&
		(startsWith(resourceID((RDF_Resource)v),  resourceID(u))))
	{
		filePathname =  resourceID((RDF_Resource)u);
		if (filePathname[strlen(filePathname)-1] == '/')
		{
			if (!fsRemoveDir(filePathname, true))
			{
				if (!fsRemoveDir(filePathname, false))
				{
					retVal = true;
				}
			}
		}
		else
		{
			if (!fsRemoveFile(filePathname, true))
			{
				if (!fsRemoveFile(filePathname, false))
				{
					retVal = true;
				}
			}
		}

		if (retVal == true)
		{
			sendNotifications2(mcf, RDF_DELETE_NOTIFY,
				u, s, v, type, true);
		}
		
	/* XXX hack: always return true (even if file/dir deletion fails)
		     to prevent an assertion going into the localstore,
		     which would hide a file/dir that still exists */

		retVal = true;
	}

	return(retVal);
}



PRBool
fsHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
		       RDF_ValueType type, PRBool tv)
{
	PRBool		ans = 0;
	PRDir		*d;
	PRDirEntry	*de;
	char		*filePathname, *name;
	int		len, n=0;

	if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE) &&
		(resourceType(u) == LFS_RT) && (containerp(v)) && 
		(resourceType((RDF_Resource)v) == LFS_RT) &&
		(startsWith(resourceID((RDF_Resource)v),  resourceID(u))))
	{
		d = OpenDir(resourceID((RDF_Resource)v));
		if ((filePathname = copyString(resourceID(u))) == NULL)
		{
			return(0);
		}

		len = strlen(filePathname)-1;
		if (filePathname[len] == '/')
		{
			filePathname[len] = '\0';
		}
		name = unescapeURL(&filePathname[ 1 + revCharSearch(XP_DIRECTORY_SEPARATOR,
				filePathname)]);
		if (name == NULL)
		{
			freeMem(filePathname);
			return(0);
		}


		while (de = PR_ReadDir(d, n++))
		{
			if (strcmp(name, de->name) == 0)
			{
				ans = 1;
				break;
			}
		}

		PR_CloseDir(d);
		XP_FREE(name);
		freeMem(filePathname);
		return ans;
	}
	else
	{
		return(0);
	}
}



void *
fsGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv)
{
  if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE) && (fsUnitp(u)) && tv) {
    if (inversep) {
      char* filePathname =  resourceID(u);
      size_t n = revCharSearch(XP_DIRECTORY_SEPARATOR, filePathname);
      char nname[100];
      PRDir *d;
      PRBool ans = 0;
      memcpy((char*)nname, filePathname, n);
      d = OpenDir(nname);
      ans = (d != NULL);
      PR_CloseDir(d);
      if (ans) {
	RDF_Resource r = RDF_GetResource(NULL, nname, 1);
	setResourceType(r, LFS_RT);
	setContainerp(r, 1);
	return r;
      } else return NULL;
    } else {
      PRDir* d = OpenDir(resourceID(u));
      PRDirEntry* de = PR_ReadDir(d, PR_SKIP_BOTH);
      if (de != NULL) {
	char nname[512];


	sprintf(nname, "%s%c%s",  resourceID(u), XP_DIRECTORY_SEPARATOR, de->name);
	PR_CloseDir(d);
	return CreateFSUnit(nname, false);
      } else {
	PR_CloseDir(d);
	return NULL;
      }
    }
  } else if ((s == gCoreVocab->RDF_name) && (type == RDF_STRING_TYPE) && (tv) && (fsUnitp(u))) {
    char *pathname, *name = NULL;
    int16 n,len;

  	if (pathname = copyString(resourceID(u))) {
  	  len = strlen(pathname);
  	  if (pathname[len-1] == '/')  pathname[--len] = '\0';
  	  n = revCharSearch('/', pathname);
  	  name = unescapeURL(&pathname[n+1]);
  	  freeMem(pathname);
  	}
    return(name);
  } else if ((s == gWebData->RDF_size) && (type == RDF_INT_TYPE) && (tv) && (fsUnitp(u))) {
    PRFileInfo fn;
    PRStatus err;
    char* filePathname =  resourceID(u);
    err=PR_GetFileInfo(&filePathname[FS_URL_OFFSET], &fn);
    if (err != -1) return (void *)fn.size;
    else return NULL;
  } else if ((s == gWebData->RDF_lastModifiedDate) && (type == RDF_INT_TYPE) && (tv) && (fsUnitp(u))) {

    PRFileInfo fn;
    PRStatus err;
    char* filePathname =  resourceID(u);
    err = PR_GetFileInfo(&filePathname[FS_URL_OFFSET], &fn);
    if (err != -1)
	{
		PRTime		oneMillion, dateVal;
		int32		modifyTime;

	LL_I2L(oneMillion, PR_USEC_PER_SEC);
	LL_DIV(dateVal, fn.modifyTime, oneMillion);
	LL_L2I(modifyTime, dateVal);
	return (void *)modifyTime;
	}
    else

	return NULL;
  } else if ((s == gWebData->RDF_creationDate) && (type == RDF_INT_TYPE) && (tv) && (fsUnitp(u))) {

    PRFileInfo fn;
    PRStatus err;
    char* filePathname =  resourceID(u);
    err = PR_GetFileInfo(&filePathname[FS_URL_OFFSET], &fn);
    if (err != -1)
	{
		PRTime		oneMillion, dateVal;
		uint32		creationTime;

	LL_I2L(oneMillion, PR_USEC_PER_SEC);
	LL_DIV(dateVal, fn.creationTime, oneMillion);
	LL_L2I(creationTime, dateVal);
	return (void *)creationTime;
	}
    else

	return NULL;
  }
  return NULL;
}



PRBool
fileDirectoryp(RDF_Resource u)
{
  if (startsWith("file:",  resourceID(u))) {
    PRDir *d = OpenDir(resourceID(u));
    PRBool ans = (d != NULL);
    if (ans) PR_CloseDir(d);
    return ans;
  } else return 0;
}



RDF_Cursor
fsGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, 
				     RDF_ValueType type,  PRBool inversep, PRBool tv)
{
  if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE) && (fsUnitp(u))
      && (inversep) && (tv)) {
    
    PRDir *d = OpenDir(resourceID(u));
    RDF_Cursor c;
    if (d == NULL) return NULL;
    c = (RDF_Cursor) getMem(sizeof(struct RDF_CursorStruct));
    c->u = u;
    c->count = PR_SKIP_BOTH;
    c->pdata = d;
    c->type = type;
    return c;
  } else return NULL;
}



void *
fsNextValue (RDFT rdf, RDF_Cursor c)
{
    PRFileInfo fn;
    char *encoded = NULL;

  if (c == NULL) {
    return NULL;
  } else {

    PRDirEntry* de = PR_ReadDir((PRDir*)c->pdata, c->count++);
    if (de == NULL) {

      PR_CloseDir((PRDir*)c->pdata);
      c->pdata = NULL;
      return NULL;
    } else {
      char nname[512], *base;
      PRBool isDirectoryFlag = false, sep = ((resourceID(c->u))[strlen(resourceID(c->u))-1] == XP_DIRECTORY_SEPARATOR);
      int len;


      base = NET_Escape(de->name, URL_XALPHAS);		/* URL_PATH */
      if (base != NULL)	{
        if (sep) {
        sprintf(nname, "%s%s",  resourceID(c->u), base);
        } else
        sprintf(nname, "%s/%s",  resourceID(c->u), base);
        XP_FREE(base);
      }

      encoded = unescapeURL(&nname[FS_URL_OFFSET]);
      if (encoded != NULL) {
#ifdef  XP_WIN
            if (encoded[1] == '|') encoded[1] = ':';
#endif

	    PR_GetFileInfo(encoded, &fn);
		if (fn.type == PR_FILE_DIRECTORY)	{
		    isDirectoryFlag = TRUE; 
 			len=strlen(nname);
			nname[len] = '/';
			nname[len+1] = '\0';
			}
		freeMem(encoded);
      }
      return CreateFSUnit(nname, isDirectoryFlag);
    } 
  }
}



RDF_Error
fsDisposeCursor (RDFT rdf, RDF_Cursor c)
{
  if (c != NULL) {
    if (c->pdata) PR_CloseDir((PRDir*)c->pdata);
    freeMem(c);
  }
  return 0;
}



RDF_Resource
CreateFSUnit (char* nname, PRBool isDirectoryFlag)
{
  char *name;
  PRBool newName = 0;
  RDF_Resource existing;
  if (startsWith("file:///", nname)) {
    name = nname;
  } else {
    name = (char*) getMem(strlen(nname) + 8);
    memcpy(name,  "file:///", 8);
    memcpy(&name[8], nname, strlen(nname));
    newName = 1;
  }
  existing = RDF_GetResource(NULL, name, 0);

  if (existing != NULL) {
    if (newName) freeMem(name);
    return existing;
  } else {
    existing = RDF_GetResource(NULL, name, 1);
    setResourceType(existing, LFS_RT);
    setContainerp(existing, isDirectoryFlag);
    if (newName) freeMem(name);
    return existing;
  }
}
