/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "prio.h"
#include "prmem.h"
#include "prprf.h"
#include "prsystem.h"

#include "plerror.h"

static char *tag[] =
{
    "PR_SI_HOSTNAME",
    "PR_SI_SYSNAME",
    "PR_SI_RELEASE",
    "PR_SI_ARCHITECTURE"
};

static PRSysInfo Incr(PRSysInfo *cmd)
{
    PRIntn tmp = (PRIntn)*cmd + 1;
    *cmd = (PRSysInfo)tmp;
    return (PRSysInfo)tmp;
}  /* Incr */

PRIntn main(PRIntn argc, char **argv)
{
    PRStatus rv;
    PRSysInfo cmd;
    PRFileDesc *output = PR_GetSpecialFD(PR_StandardOutput);

    char *info = (char*)PR_Calloc(SYS_INFO_BUFFER_LENGTH, 1);
    for (cmd = PR_SI_HOSTNAME; cmd <= PR_SI_ARCHITECTURE; Incr(&cmd))
    {
        rv = PR_GetSystemInfo(cmd, info, SYS_INFO_BUFFER_LENGTH);
        if (PR_SUCCESS == rv) PR_fprintf(output, "%s: %s\n", tag[cmd], info);
        else PL_FPrintError(output, tag[cmd]);
    }
    PR_DELETE(info);

    PR_fprintf(output, "Host page size is %d\n", PR_GetPageSize());
    PR_fprintf(output, "Page shift is %d\n", PR_GetPageShift());
    PR_fprintf(output, "Number of processors is: %d\n", PR_GetNumberOfProcessors());
    PR_fprintf(output, "Physical memory size is: %llu\n", PR_GetPhysicalMemorySize());

    return 0;
}  /* main */

/* system.c */
