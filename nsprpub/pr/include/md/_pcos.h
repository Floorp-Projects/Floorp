/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
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
extern PRStatus _MD_WindowsGetSysInfo(PRSysInfo cmd, char *name, PRUint32 namelen);
#define _MD_GETSYSINFO _MD_WindowsGetSysInfo
#endif

#endif /* prpcos_h___ */
