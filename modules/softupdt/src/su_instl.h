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
/* su_instl.h
 * 
 * created by atotic, 1/22/97
 */

#include "xp_mcom.h"

#define REBOOT_NEEDED 999
#define SU_SUCCESS    0

XP_BEGIN_PROTOS

/* Immediately executes the file 
 * returns errors if encountered
 */
int FE_ExecuteFile( const char * filename, const char * cmdline );
#ifdef XP_PC
void    PR_CALLBACK SU_AlertCallback(void *dummy);
void FE_ScheduleRenameUtility(void);
#endif

#ifdef XP_UNIX
int FE_CopyFile (const char *in, const char *out);
#endif

XP_END_PROTOS

