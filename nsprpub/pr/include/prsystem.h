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

#ifndef prsystem_h___
#define prsystem_h___

/*
** API to NSPR functions returning system info.
*/
#include "prtypes.h"

PR_BEGIN_EXTERN_C

/*
** Get the host' directory separator.
**  Pathnames are then assumed to be of the form:
**      [<sep><root_component><sep>]*(<component><sep>)<leaf_name>
*/

PR_EXTERN(char) PR_GetDirectorySeparator(void);

/*
** OBSOLETE -- the function name is misspelled.
** Use PR_GetDirectorySeparator instead.
*/

PR_EXTERN(char) PR_GetDirectorySepartor(void);


/* Types of information available via PR_GetSystemInfo(...) */
typedef enum {
    PR_SI_HOSTNAME,
    PR_SI_SYSNAME,
    PR_SI_RELEASE,
    PR_SI_ARCHITECTURE
} PRSysInfo;


/*
** If successful returns a null termintated string in 'buf' for
** the information indicated in 'cmd'. If unseccussful the reason for
** the failure can be retrieved from PR_GetError().
**
** The buffer is allocated by the caller and should be at least
** SYS_INFO_BUFFER_LENGTH bytes in length.
*/

#define SYS_INFO_BUFFER_LENGTH 256

PR_EXTERN(PRStatus) PR_GetSystemInfo(PRSysInfo cmd, char *buf, PRUint32 buflen);

/*
** Return the number of bytes in a page
*/
PR_EXTERN(PRInt32) PR_GetPageSize(void);

/*
** Return log2 of the size of a page
*/
PR_EXTERN(PRInt32) PR_GetPageShift(void);

PR_END_EXTERN_C

#endif /* prsystem_h___ */
