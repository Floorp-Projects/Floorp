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

#include "primpl.h"
#include "prsystem.h"
#include "prprf.h"

#if defined(XP_UNIX)
#include <unistd.h>
#include <sys/utsname.h>
#endif

PR_IMPLEMENT(char) PR_GetDirectorySeparator()
{
    return PR_DIRECTORY_SEPARATOR;
}  /* PR_GetDirectorySeparator */

/*
** OBSOLETE -- the function name is misspelled.
*/
PR_IMPLEMENT(char) PR_GetDirectorySepartor()
{
#if defined(DEBUG)
    static PRBool warn = PR_TRUE;
    if (warn) {
        warn = _PR_Obsolete("PR_GetDirectorySepartor()",
                "PR_GetDirectorySeparator()");
    }
#endif
    return PR_GetDirectorySeparator();
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
#if defined(XP_OS2)
        {
            ULONG os2ver[2] = {0};
            DosQuerySysInfo(QSV_VERSION_MINOR, QSV_VERSION_REVISION,
                            &os2ver, sizeof(os2ver));
            /* Formatting for normal usage (2.11, 3.0, 4.0); officially,
               Warp 4 is version 2.40.00 */
            if (os2ver[0] < 30)
              (void)PR_snprintf(buf, buflen, "%s%lu",
                                "2.", os2ver[0]);
            else
              (void)PR_snprintf(buf, buflen, "%lu%s%lu",
                                os2ver[0]/10, ".", os2ver[1]);
        }
#endif /* OS2 */
        break;

      case PR_SI_ARCHITECTURE:
        /* Return the architecture of the machine (ie. x86, mips, alpha, ...)*/
        (void)PR_snprintf(buf, buflen, _PR_SI_ARCHITECTURE);
        break;
    }
    return PR_SUCCESS;
}
