/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef prpcos_h___
#define prpcos_h___

#define PR_DLL_SUFFIX       ".dll"

#include <stdlib.h>

#define DIRECTORY_SEPARATOR         '\\'
#define DIRECTORY_SEPARATOR_STR     "\\"
#define PATH_SEPARATOR              ';'

/*
** Routines for processing command line arguments
*/
PR_BEGIN_EXTERN_C
#ifndef XP_OS2
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
#ifdef XP_OS2
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
extern PRStatus _MD_WindowsGetSysInfo(PRSysInfo cmd, char *name, PRUint32 namelen);
#define _MD_GETSYSINFO _MD_WindowsGetSysInfo
#endif

#endif /* prpcos_h___ */
