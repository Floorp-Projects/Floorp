/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

#include <stdlib.h>

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
#ifndef XP_OS2_EMX
extern char *optarg;
extern int optind;
extern int getopt(int argc, char **argv, char *spec);
#endif
PR_END_EXTERN_C


/*
** Definitions of directory structures amd functions
** These definitions are from:
**      <dirent.h>
*/
#ifdef XP_OS2_EMX
#include <sys/types.h>
#endif
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>          /* O_BINARY */

#ifdef OS2
extern PRStatus _MD_OS2GetHostName(char *name, PRUint32 namelen);
#define _MD_GETHOSTNAME _MD_OS2GetHostName
#else
extern PRStatus _MD_WindowsGetHostName(char *name, PRUint32 namelen);
#define _MD_GETHOSTNAME _MD_WindowsGetHostName
#endif

#endif /* prpcos_h___ */
