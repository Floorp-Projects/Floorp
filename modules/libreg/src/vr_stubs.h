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

#ifdef XP_UNIX
#define XP_FileOpen(name,type,mode)     VR_StubOpen((name), (mode))
#else
#define XP_FileOpen(name,type,mode)     VR_StubOpen((mode))
#endif

#define XP_BEGIN_PROTOS
#define XP_END_PROTOS

#define XP_ASSERT(x)        ((void)0)
#define XP_STRCAT(a,b)      strcat((a),(b))
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
  #define XP_STRCASECMP(x,y)  stricmp((x),(y))
  #define XP_STRNCASECMP(x,y,n) strnicmp((x),(y),(n))
#else
  #define XP_STRCASECMP(x,y)  strcasecmp((x),(y))
  #define XP_STRNCASECMP(x,y,n) strncasecmp((x),(y),(n))
#endif

typedef FILE          * XP_File;

#ifdef STANDALONE_REGISTRY /* included from prmon.h otherwise */
typedef long            int32;
typedef unsigned long   uint32;
typedef short           int16;
typedef unsigned short  uint16;
typedef unsigned char   uint8;

#ifdef XP_MAC
#include <Types.h>
	typedef char BOOL;
    typedef char Bool;
    typedef char XP_Bool;
#elif defined(XP_PC)
    typedef int Bool;
    typedef int XP_Bool;
#else
    /*  XP_UNIX: X11/Xlib.h "define"s Bool to be int. This is really lame
     *  (that's what typedef is for losers). So.. in lieu of a #undef Bool
     *  here (Xlib still needs ints for Bool-typed parameters) people have
     *  been #undef-ing Bool before including this file.
     *  Can we just #undef Bool here? <mailto:mcafee> (help from djw, converse)
     */

    typedef char Bool;
    typedef char XP_Bool;
#endif
#endif /* STANDALONE_REGISTRY */

#ifdef XP_PC
 typedef struct _stat   VR_StatStruct;
 #define VR_Stat(file,data)     _stat((file),(data))
#else
 typedef struct stat    VR_StatStruct;
#define  VR_Stat(file,data)     stat((file),(data))
#endif



#if defined(__cplusplus)
# define XP_BEGIN_PROTOS extern "C" {
# define XP_END_PROTOS }
#else
# define XP_BEGIN_PROTOS
# define XP_END_PROTOS
#endif

XP_BEGIN_PROTOS

#ifdef XP_UNIX
extern XP_File VR_StubOpen (const char *name, const char * mode);
#else
extern XP_File VR_StubOpen (const char * mode);
#endif

#ifdef XP_MAC
extern int strcasecmp(const char *str1, const char *str2);
extern int strncasecmp(const char *str1, const char *str2, int length);
extern char * strdup(const char *str);
#endif

XP_END_PROTOS

#endif /* _VR_STUBS_H_ */
