/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "fe_proto.h"
#include "xp_file.h"
#include "prlog.h"
#include <Aliases.h>
#include "FullPath.h"
#include "prmem.h"

#define USE_MSL

#ifdef USE_MSL
PR_BEGIN_EXTERN_C
PR_EXTERN(OSErr) ConvertUnixPathToMacPath(const char *, char **);
PR_END_EXTERN_C
#endif

//
// XP_File routines
// The implementation has not been thorougly tested
// It attempts to use NSPR for all file i/o
//

char * WH_FileName (const char *name, XP_FileType type)
{
	if (type != xpURL)
	{
		//PR_ASSERT(FALSE);
		return NULL;
	}
	else
		return name ? strdup(name) : NULL;
}


// XP_FileOpen
//
//		This is a ¥TEMPORARY¥ version. We will soon use NSPR for all file I/O.
//
//		NOTE: Like NSPR, we use ConvertUnixPathToMacPath() to support
//		some special paths (for instance "/usr/local/netscape/bin" to 
//		designate the application folder) but unlike NSPR, this function
//		also support aliases, although through a very slow process
//		which should be optimized in a final implementation (currently,
//		we convert a fullpath to a fSpec back-and-forth).
//
XP_File XP_FileOpen( const char* name, XP_FileType type,
					 const XP_FilePerm inPermissions )
{
	if (type != xpURL)
	{
		//PR_ASSERT(FALSE);
		return NULL;
	}

#ifdef USE_MSL
	// use MSL like the other functions in xp_file.h
	char *macFileName = NULL;
    OSErr err = ConvertUnixPathToMacPath(name, &macFileName);
	if (err != noErr)
		return NULL;

	// resolve an alias if this was one
	FSSpec	fileSpec;
	err = FSpLocationFromFullPath(strlen(macFileName), macFileName, &fileSpec);
	if (err == noErr)
	{
		Boolean crap;
		err = ResolveAliasFile(&fileSpec, true, &crap, &crap);
		if (err == noErr)
		{
			short	fullPathLength;
			Handle	fullPath;
			err = FSpGetFullPath(&fileSpec, &fullPathLength, &fullPath);
			if (err == noErr)
			{
				macFileName = (char*)malloc(fullPathLength + 1);
				BlockMoveData(*fullPath, macFileName, fullPathLength);
				macFileName[fullPathLength] = '\0';
				::DisposeHandle(fullPath);
			}
		}
	}

	XP_File theFile = (XP_File)fopen(macFileName, inPermissions);
	PR_FREEIF(macFileName);
	return theFile;

#else
	// use NSPR as is would be nice to do one day
	PRIntn permissions = 0;
	for ( char * p = inPermissions; *p; p++)
	{
		switch (*p)	{
			case 'r':
				// NSPR r, w, and rw flags are mutually exclusive
				if ( permissions & PR_WRONLY )
				{
					permissions &= 0xF8;
					permissions |= PR_RDWR;
				}
				else
					permissions |= PR_RDONLY;
				break;
			case 'w':
				permissions |= PR_CREATE_FILE;
				if ( permissions & PR_RDONLY )
				{
					permissions &= 0xF8;
					permissions |= PR_RDWR;
				}
				else
					permissions |= PR_WRONLY;
				break;
			case 'b':
				break;
			case 'a':
				PR_ASSERT(FALSE);	// NSPR does not do file open with ASCII?
				break;
			case '+':
				permissions |= PR_APPEND;
				break;
		}
	}
	return (XP_File)PR_Open(name, 0x600,  permissions);
#endif // USE_MSL
}

int XP_FileRemove( const char* name, XP_FileType type )
{
	if (type != xpURL)
	{
		PR_ASSERT(FALSE);
		return -1;
	}
	return PR_Delete(name);
}

XP_Dir XP_OpenDir( const char* name, XP_FileType type )
{
	if (type != xpURL)
	{
		PR_ASSERT(FALSE);
		return NULL;
	}
	return (XP_Dir)PR_OpenDir(name);
}

XP_DirEntryStruct * XP_ReadDir(XP_Dir dir)
{
	PRDirEntry * prDirEntry  = PR_ReadDir((PRDir*)dir, PR_SKIP_NONE);
	static XP_DirEntryStruct dirStruct;
	if (prDirEntry)
	{
		strcpy(dirStruct.d_name, prDirEntry->name);
		return &dirStruct;
	}
	else
		return NULL;
}

void XP_CloseDir( XP_Dir dir )
{
	PR_CloseDir( (PRDir *)dir );
}

int XP_Stat( const char* name, XP_StatStruct * outStat,  XP_FileType type )
{
	if (type != xpURL)
	{
		PR_ASSERT(FALSE);
		return -1;
	}
	memset(outStat, sizeof(XP_StatStruct), 1);
	PRFileInfo info;
	if ( PR_GetFileInfo( name, &info) == PR_SUCCESS)
	{
		outStat->st_size = info.size;
		PRInt64 million, seconds;
		LL_I2L( million, 1000000);
		LL_DIV(seconds, info.creationTime, million);
		LL_L2I(outStat->st_ctime, seconds);
		LL_L2I(outStat->st_ctime , info.modifyTime);
		LL_DIV(seconds, info.modifyTime, seconds);
		LL_L2I(outStat->st_mtime, seconds);
		if ( info.type == PR_FILE_DIRECTORY)
			outStat->st_mode = S_IFDIR;
		else if ( info.type == PR_FILE_FILE )
			outStat->st_mode = S_IFREG;
		else if ( info.type == PR_FILE_OTHER )
			outStat->st_mode = S_IFLNK;
		else
			PR_ASSERT("NSPR has returned unexpected file type");
		return 0;
	}	
	else return -1;
}

/* Needs to deal with both CR, CRLF, and LF end-of-line */
extern char * XP_FileReadLine(char * dest, int32 bufferSize, XP_File file)
{
	char * retBuf = fgets(dest, bufferSize, file);
	if (retBuf == NULL)	/* EOF */
		return NULL;
	char * LFoccurence = (char *)strchr(retBuf, LF);
	if (LFoccurence != NULL)	/* We have hit LF before CR,  */
	{
		fseek(file, -(strlen(retBuf) - (LFoccurence  - retBuf))+1, SEEK_CUR);
		LFoccurence[1] = 0;
	}
	else	/* We have CR, check if the next character is LF */
	{
		int c;

		c = fgetc(file);

		if (c == EOF)
			;	/* Do nothing, end of file */
		else if (c == LF)	/* Swallow CR/LF */
		{
			int len = strlen(retBuf);
			if (len < bufferSize)	/* Append LF to our buffer if we can */
			{
				retBuf[len++] = LF;
				retBuf[len] = 0;
			}
		}
		else	/* No LF, just clean up the seek */
		{
			fseek(file, -1, SEEK_CUR);
		}
	}
	return retBuf;
}

#include "ufilemgr.h"
/* Netlib utility routine, should be ripped out */
void	FE_FileType(char * path, 
					Bool * useDefault, 
					char ** fileType, 
					char ** encoding)
{
	if ((path == NULL) || (fileType == NULL) || (encoding == NULL))
		return;

	*useDefault = TRUE;
	*fileType = NULL;
	*encoding = NULL;

	char *pathPart = NET_ParseURL( path, GET_PATH_PART);
	if (pathPart == NULL)
		return;

	FSSpec spec;
	OSErr err = CFileMgr::FSSpecFromLocalUnixPath(pathPart, &spec);	// Skip file://
	XP_FREE(pathPart);

#if 0
#include "umimemap.h"
#include "uprefd.h"
#include "InternetConfig.h"
	CMimeMapper * mapper = CPrefs::sMimeTypes.FindMimeType(spec);
	if (mapper != NULL)
	{
		*useDefault = FALSE;
		*fileType = XP_STRDUP(mapper->GetMimeName());
	}
	else
	{
		FInfo		fndrInfo;
		OSErr err = FSpGetFInfo( &spec, &fndrInfo );
		if ( (err != noErr) || (fndrInfo.fdType == 'TEXT') )
			*fileType = XP_STRDUP(APPLICATION_OCTET_STREAM);
		else
		{
			// Time to call IC to see if it knows anything
			ICMapEntry ICMapper;
			
			ICError  error = 0;
			CStr255 fileName( spec.name );
			error = CInternetConfigInterface::GetInternetConfigFileMapping(
					fndrInfo.fdType, fndrInfo.fdCreator,  fileName ,  &ICMapper );	
			if( error != icPrefNotFoundErr && StrLength(ICMapper.MIME_type) )
			{
				*useDefault = FALSE;
				CStr255 mimeName( ICMapper.MIME_type );
				*fileType = XP_STRDUP( mimeName );
			}
			else
			{
				// That failed try using the creator type		
				mapper = CPrefs::sMimeTypes.FindCreator(fndrInfo.fdCreator);
				if( mapper)
				{
					*useDefault = FALSE;
					*fileType = XP_STRDUP(mapper->GetMimeName());
				}
				else
				{
					// don't have a mime mapper
					*fileType = XP_STRDUP(APPLICATION_OCTET_STREAM);
				}
			}
		}
	}
#endif
}
