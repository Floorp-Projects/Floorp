/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Daniel Veditz <dveditz@netscape.com>
 */
/* vr_stubs.h
 *
 * XP code stubs for stand-alone registry library
 *
 */

#ifndef _VR_STUBS_H_
#define _VR_STUBS_H_

#ifdef STANDALONE_REGISTRY

#include <errno.h>
#include <string.h>
#ifdef XP_MAC
#include "macstdlibextras.h"  /* For strcasecmp and strncasecmp */
#endif

#else

#include "prio.h"
#include "prmem.h"
#include "plstr.h"

#endif /* STANDALONE_REGISTRY*/

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

#if defined(__cplusplus)
# define XP_CPLUSPLUS
# define XP_IS_CPLUSPLUS 1
#else
# define XP_IS_CPLUSPLUS 0
#endif

#if defined(XP_CPLUSPLUS)
# define XP_BEGIN_PROTOS extern "C" {
# define XP_END_PROTOS }
#else
# define XP_BEGIN_PROTOS
# define XP_END_PROTOS
#endif


#ifdef STANDALONE_REGISTRY

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

#define XP_FileSeek(file,offset,whence) fseek((file), (offset), (whence))
#define XP_FileRead(dest,count,file)    fread((dest), 1, (count), (file))
#define XP_FileWrite(src,count,file)    fwrite((src), 1, (count), (file))
#define XP_FileTell(file)               ftell(file)
#define XP_FileFlush(file)              fflush(file)
#define XP_FileClose(file)              fclose(file)

#define XP_ASSERT(x)        ((void)0)

#define XP_STRCAT(a,b)      strcat((a),(b))
#define XP_ATOI             atoi
#define XP_STRCPY(a,b)      strcpy((a),(b))
#define XP_STRLEN(x)        strlen(x)
#define XP_SPRINTF          sprintf
#define XP_FREE(x)          free((x))
#define XP_ALLOC(x)         malloc((x))
#define XP_FREEIF(x)        if ((x)) free((x))
#define XP_STRCMP(x,y)      strcmp((x),(y))
#define XP_STRNCMP(x,y,n)   strncmp((x),(y),(n))
#define XP_STRDUP(s)        strdup((s))
#define XP_MEMCPY(d, s, l)  memcpy((d), (s), (l))
#define XP_MEMSET(d, c, l)  memset((d), (c), (l))

#define PR_Lock(a)          ((void)0)
#define PR_Unlock(a)        ((void)0)

#ifdef XP_PC
  #define XP_STRCASECMP(x,y)  stricmp((x),(y))
  #define XP_STRNCASECMP(x,y,n) strnicmp((x),(y),(n))
#else
  #define XP_STRCASECMP(x,y)  strcasecmp((x),(y))
  #define XP_STRNCASECMP(x,y,n) strncasecmp((x),(y),(n))
#endif /*XP_PC*/

typedef FILE          * XP_File;

#else /* if not standalone, use NSPR */

#define XP_FILE_READ             PR_RDONLY, 0644
#define XP_FILE_READ_BIN         PR_RDONLY, 0644
#define XP_FILE_WRITE            PR_WRONLY, 0644
#define XP_FILE_WRITE_BIN        PR_WRONLY, 0644
#define XP_FILE_UPDATE           PR_RDWR|PR_CREATE_FILE, 0644
#define XP_FILE_TRUNCATE         (PR_WRONLY | PR_TRUNCATE), 0644

#define XP_FILE_UPDATE_BIN       PR_RDWR|PR_CREATE_FILE, 0644
#define XP_FILE_TRUNCATE_BIN     (PR_RDWR | PR_TRUNCATE), 0644

#ifdef SEEK_SET
    #undef SEEK_SET
    #undef SEEK_CUR
    #undef SEEK_END
    #define SEEK_SET PR_SEEK_SET
    #define SEEK_CUR PR_SEEK_CUR
    #define SEEK_END PR_SEEK_END
#endif
/*
** Note that PR_Seek returns the offset (if successful) and -1 otherwise.  So
** to make this code work
**           if (XP_FileSeek(fh, offset, SEEK_SET) != 0)  { error handling }
** we return 1 if PR_Seek() returns a negative value, and 0 otherwise
*/
#define XP_FileSeek(file,offset,whence) (PR_Seek((file), (offset), (whence)) < 0)
#define XP_FileRead(dest,count,file)    PR_Read((file), (dest), (count))
#define XP_FileWrite(src,count,file)    PR_Write((file), (src), (count))
#define XP_FileTell(file)               PR_Seek(file, 0, PR_SEEK_CUR)
#define XP_FileFlush(file)              PR_Sync(file)
#define XP_FileClose(file)              PR_Close(file)

#define XP_ASSERT(x)        PR_ASSERT((x))

#define XP_STRCAT(a,b)      PL_strcat((a),(b))
#define XP_ATOI             PL_atoi
#define XP_STRCPY(a,b)      PL_strcpy((a),(b))
#define XP_STRLEN(x)        PL_strlen(x)
#define XP_SPRINTF          sprintf
#define XP_FREE(x)          PR_Free((x))
#define XP_ALLOC(x)         PR_Malloc((x))
#define XP_FREEIF(x)        PR_FREEIF(x)
#define XP_STRCMP(x,y)      PL_strcmp((x),(y))
#define XP_STRNCMP(x,y,n)   PL_strncmp((x),(y),(n))
#define XP_STRDUP(s)        PL_strdup((s))
#define XP_MEMCPY(d, s, l)  memcpy((d), (s), (l))
#define XP_MEMSET(d, c, l)  memset((d), (c), (l))

#define XP_STRCASECMP(x,y)  PL_strcasecmp((x),(y))
#define XP_STRNCASECMP(x,y,n) PL_strncasecmp((x),(y),(n))

typedef PRFileDesc* XP_File;

#endif /*STANDALONE_REGISTRY*/

#ifdef STANDALONE_REGISTRY /* included from prmon.h otherwise */
#include "prtypes.h"

#if 0
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
#endif /* XP_MAC */
#endif /* 0 */
#endif /*STANDALONE_REGISTRY*/

#ifdef XP_PC
 typedef struct _stat   XP_StatStruct;
 #define XP_Stat(file,data,type)     _stat((file),(data))
#else
 typedef struct stat    XP_StatStruct;
 #define  XP_Stat(file,data,type)     stat((file),(data))
#endif /*XP_PC*/

#ifdef XP_MAC
 extern int nr_RenameFile(char *from, char *to);
#else
    XP_BEGIN_PROTOS
    #define nr_RenameFile(from, to)    rename((from), (to))
    XP_END_PROTOS
#endif



XP_BEGIN_PROTOS

extern char* globalRegName;
extern char* verRegName;

extern void vr_findGlobalRegName();
extern char* vr_findVerRegName();


#ifdef STANDALONE_REGISTRY /* included from prmon.h otherwise */

extern XP_File vr_fileOpen(const char *name, const char * mode);

#if !defined(XP_PC) && !(defined(__GLIBC__) && __GLIBC__ >= 2)
extern char * strdup(const char * s);
#endif


#else
#define vr_fileOpen PR_Open
#endif /* STANDALONE_REGISTRY */

XP_END_PROTOS

#endif /* _VR_STUBS_H_ */
