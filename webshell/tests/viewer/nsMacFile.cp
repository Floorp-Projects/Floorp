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

//
// XP_File routines
// The implementation has not been thorougly tested
// It attempts to use NSPR for all file i/o
//

char * WH_FileName (const char *name, XP_FileType type)
{
	if (type != xpURL)
	{
		PR_ASSERT(FALSE);
		return NULL;
	}
	else
		return name ? strdup(name) : NULL;
}

XP_File XP_FileOpen( const char* name, XP_FileType type,
					 const XP_FilePerm inPermissions )
{
	if (type != xpURL)
	{
		PR_ASSERT(FALSE);
		return NULL;
	}
	PRIntn permissions = 0;
	for ( char * p = inPermissions; p; p++)
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
