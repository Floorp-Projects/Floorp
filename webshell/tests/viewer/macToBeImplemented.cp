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

// Unimplemented code temporarily lives here

#include "fe_proto.h"
#include "xp_file.h"
#include "prlog.h"


/* Netlib utility routine, should be ripped out */
void	FE_FileType(char * path, Bool * useDefault, char ** fileType, char ** encoding)
{
	if ((path == NULL) || (fileType == NULL) || (encoding == NULL))
		return;

	*useDefault = TRUE;
	*fileType = NULL;
	*encoding = NULL;
}

#include "mcom_db.h"
DB * dbopen(const char *fname, int flags,int mode, DBTYPE type, const void *openinfo)
{
	PR_ASSERT(FALSE);
	return NULL;
}

char * XP_FileReadLine(char * dest, int32 bufferSize, XP_File file)
{
	PR_ASSERT(FALSE);
	return NULL;
}

#include "il_strm.h"

/* Given the first few bytes of a stream, identify the image format */
static int
il_type(int suspected_type, const char *buf, int32 len)
{
	int i;

	if (len >= 4 && !strncmp(buf, "GIF8", 4)) 
	{
		return IL_GIF;
	}

  	/* for PNG */
	if (len >= 4 && ((unsigned char)buf[0]==0x89 &&
		             (unsigned char)buf[1]==0x50 &&
					 (unsigned char)buf[2]==0x4E &&
					 (unsigned char)buf[3]==0x47))
	{ 
		return IL_PNG;
	}


	/* JFIF files start with SOI APP0 but older files can start with SOI DQT
	 * so we test for SOI followed by any marker, i.e. FF D8 FF
	 * this will also work for SPIFF JPEG files if they appear in the future.
	 *
	 * (JFIF is 0XFF 0XD8 0XFF 0XE0 <skip 2> 0X4A 0X46 0X49 0X46 0X00)
     */
	if (len >= 3 &&
	   ((unsigned char)buf[0])==0xFF &&
	   ((unsigned char)buf[1])==0xD8 &&
	   ((unsigned char)buf[2])==0xFF)
	{
		return IL_JPEG;
	}

	/* no simple test for XBM vs, say, XPM so punt for now */
	if (len >= 8 && !strncmp(buf, "#define ", 8) ) 
	{
        /* Don't contradict the given type, since this ID isn't definitive */
        if ((suspected_type == IL_UNKNOWN) || (suspected_type == IL_XBM))
            return IL_XBM;
	}

	if (len < 35) 
	{
		/* ILTRACE(1,("il: too few bytes to determine type")); */
		return suspected_type;
	}

	/* all the servers return different formats so root around */
	for (i=0; i<28; i++)
	{
		if (!strncmp(&buf[i], "Not Fou", 7))
			return IL_NOTFOUND;
	}
	
	return suspected_type;
}

/*
 *	determine what kind of image data we are dealing with
 */
extern "C"
int IL_Type(const char *buf, int32 len)
{
    return il_type(IL_UNKNOWN, buf, len);
}

/* Set limit on approximate size, in bytes, of all pixmap storage used
   by the imagelib.  */
extern "C" 
void IL_SetCacheSize(uint32 new_size)
{
}
