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

#include "primpl.h"
#include "prsystem.h"
#include "prprf.h"

#if defined(XP_UNIX)
#include <unistd.h>
#include <sys/utsname.h>
#endif

PR_IMPLEMENT(char) PR_GetDirectorySepartor()
{
    return PR_DIRECTORY_SEPARATOR;
}  /* PR_GetDirectorySepartor */

PR_IMPLEMENT(PRStatus) PR_GetSystemInfo(PRSysInfo cmd, char *buf, PRUint32 buflen)
{
    PRUintn len = 0;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    switch(cmd)
    {
      case PR_SI_HOSTNAME:
        if (PR_FAILURE == _PR_MD_GETHOSTNAME(buf, (PRUintn)buflen))
            return PR_FAILURE;
        /* Return the unqualified hostname */
            while (buf[len] && (len < buflen)) {
                if (buf[len] == '.') {
                    buf[len] = '\0';
                    break;
                }
                len += 1;
            }    
         break;

      case PR_SI_SYSNAME:
        /* Return the operating system name */
        (void)PR_snprintf(buf, buflen, _PR_SI_SYSNAME);
        break;

      case PR_SI_RELEASE:
        /* Return the version of the operating system */
#if defined(XP_UNIX)
        {
            struct utsname info;
            uname(&info);
            (void)PR_snprintf(buf, buflen, info.release);
        }
#endif
        break;

      case PR_SI_ARCHITECTURE:
        /* Return the architecture of the machine (ie. x86, mips, alpha, ...)*/
        (void)PR_snprintf(buf, buflen, _PR_SI_ARCHITECTURE);
        break;
    }
    return PR_SUCCESS;
}
