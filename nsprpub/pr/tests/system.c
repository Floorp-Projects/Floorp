/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

int main(int argc, char **argv)
{
    PRStatus rv;
    PRSysInfo cmd;
    PRFileDesc *output = PR_GetSpecialFD(PR_StandardOutput);

    char *info = (char*)PR_Calloc(SYS_INFO_BUFFER_LENGTH, 1);
    for (cmd = PR_SI_HOSTNAME; cmd <= PR_SI_ARCHITECTURE; Incr(&cmd))
    {
        rv = PR_GetSystemInfo(cmd, info, SYS_INFO_BUFFER_LENGTH);
        if (PR_SUCCESS == rv) {
            PR_fprintf(output, "%s: %s\n", tag[cmd], info);
        }
        else {
            PL_FPrintError(output, tag[cmd]);
        }
    }
    PR_DELETE(info);

    PR_fprintf(output, "Host page size is %d\n", PR_GetPageSize());
    PR_fprintf(output, "Page shift is %d\n", PR_GetPageShift());
    PR_fprintf(output, "Memory map alignment is %ld\n", PR_GetMemMapAlignment());
    PR_fprintf(output, "Number of processors is: %d\n", PR_GetNumberOfProcessors());
    PR_fprintf(output, "Physical memory size is: %llu\n", PR_GetPhysicalMemorySize());

    return 0;
}  /* main */

/* system.c */
