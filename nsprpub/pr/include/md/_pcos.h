/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef prpcos_h___
#define prpcos_h___

#define PR_DLL_SUFFIX		".dll"

#ifndef OS2
#include <windows.h>
#endif
#include <setjmp.h>
#include <stdlib.h>

#define USE_SETJMP
#define DIRECTORY_SEPARATOR         '\\'
#define DIRECTORY_SEPARATOR_STR     "\\"
#define PATH_SEPARATOR              ';'

#ifdef WIN16
#define GCPTR __far
#else
#define GCPTR
#endif

/*
** Routines for processing command line arguments
*/
PR_BEGIN_EXTERN_C

extern char *optarg;
extern int optind;
extern int getopt(int argc, char **argv, char *spec);

PR_END_EXTERN_C

#define gcmemcpy(a,b,c) memcpy(a,b,c)


/*
** Definitions of directory structures amd functions
** These definitions are from:
**      <dirent.h>
*/
#ifdef MOZ_BITS
#include <time.h>
#endif
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>          /* O_BINARY */

typedef int PROSFD;

/*
** Undo the macro define in the Microsoft header files...
*/
#ifndef XP_OS2 /* Uh... this seems a bit insane in itself to we OS/2 folk */
#ifdef _stat
#undef _stat
#endif
#endif

#ifdef OS2
extern PRStatus _MD_OS2GetHostName(char *name, PRUint32 namelen);
#define _MD_GETHOSTNAME _MD_OS2GetHostName
#else
extern PRStatus _MD_WindowsGetHostName(char *name, PRUint32 namelen);
#define _MD_GETHOSTNAME _MD_WindowsGetHostName
#endif

#endif /* prpcos_h___ */
