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

    return 0;
}  /* main */

/* system.c */
