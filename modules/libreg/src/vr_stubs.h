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
/* vr_stubs.h
 *
 * XP code stubs for stand-alone registry library
 *
 */

#ifndef _VR_STUBS_H_
#define _VR_STUBS_H_

#include <errno.h>

#ifdef XP_MAC
#include <stat.h>
#else
#if defined(BSDI) && !defined(BSDI_2)
#include <sys/types.h>
#endif
#include <sys/stat.h>
#endif

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#ifdef XP_MAC
#define EMFILE 23
#define EBADF  24
#endif

#define XP_FILE_READ             "r"
#define XP_FILE_READ_BIN         "rb"
#define XP_FILE_WRITE            "w"
#define XP_FILE_WRITE_BIN        "wb"
#define XP_FILE_UPDATE           "r+"
#define XP_FILE_TRUNCATE         "w+"
#ifdef SUNOS4
/* XXX SunOS4 hack -- make this universal by using r+b and w+b */
#define XP_FILE_UPDATE_BIN       "r+"
#define XP_FILE_TRUNCATE_BIN     "w+"
#else
#define XP_FILE_UPDATE_BIN       "rb+"
#define XP_FILE_TRUNCATE_BIN     "wb+"
#endif

#define XP_FileSeek(file,offset,whence) fseek ((file), (offset), (whence))
#define XP_FileRead(dest,count,file)    fread ((dest), 1, (count), (file))
#define XP_FileWrite(src,count,file)    fwrite ((src), 1, (count), (file))
#define XP_FileTell(file)               ftell(file)
#define XP_FileFlush(file)              fflush(file)
#define XP_FileClose(file)              fclose(file)

#define XP_FileOpen(name,type,mode)     VR_StubOpen((mode))

#define XP_BEGIN_PROTOS
#define XP_END_PROTOS

#define XP_ASSERT(x)        ((void)0)
#define XP_STRCPY(a,b)      strcpy((a),(b))
#define XP_STRLEN(x)        strlen(x)
#define XP_SPRINTF          sprintf
#define XP_FREE(x)          free((x))
#define XP_ALLOC(x)         malloc((x))
#define XP_FREEIF(x)        if ((x)) free((x))
#define XP_STRCMP(x,y)      strcmp((x),(y))
#define XP_STRNCMP(x,y,n)   strncmp((x),(y),(n))
#define XP_STRDUP(s)        strdup((s))
#define XP_MEMCPY(d, s, l)	memcpy((d), (s), (l))
#define XP_MEMSET(d, c, l)      memset((d), (c), (l))

#ifdef XP_PC
#define XP_STRCASECMP(x,y)  strcmpi((x),(y))

#ifdef XP_OS2
#define XP_STRNCASECMP(x,y,n) strnicmp((x),(y),(n))
#else
#define XP_STRNCASECMP(x,y,n) strncmpi((x),(y),(n))
#endif

#endif

#ifdef XP_MAC
#define XP_STRCASECMP(x,y)  strcasecmp((x),(y))
#define XP_STRNCASECMP(x,y,n) strncasecmp((x),(y),(n))
extern int strcasecmp(const char *str1, const char *str2);
extern int strncasecmp(const char *str1, const char *str2, int length);
#endif

#ifdef XP_UNIX
#define XP_STRCASECMP	strcasecomp   /* from libxp.a */
#endif

typedef FILE          * XP_File;

typedef long            int32;
typedef unsigned long   uint32;
typedef short           int16;
typedef unsigned short  uint16;
typedef unsigned char   uint8;

typedef char            Bool;
typedef int             XP_Bool;

#ifdef XP_WIN
 typedef struct _stat   XP_StatStruct;
 #define XP_Stat(file,data,type)     _stat((file),(data))
#else
 typedef struct stat    XP_StatStruct;
#define  XP_Stat(file,data,type)     stat((file),(data))
#endif

extern XP_File VR_StubOpen (const char * mode);


#endif /* _VR_STUBS_H_ */
