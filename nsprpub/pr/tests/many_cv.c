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

#include "prinit.h"
#include "prprf.h"
#include "prthread.h"
#include "prcvar.h"
#include "prlock.h"
#include "prlog.h"
#include "prmem.h"

#include "plgetopt.h"

#include <stdlib.h>

static void Help(void)
{
    PRFileDesc *err = PR_GetSpecialFD(PR_StandardError);
    PR_fprintf(err, "many_cv usage: [-c n] [-l n] [-h]\n");
    PR_fprintf(err, "\t-c n Number of conditions per lock       (default: 10)\n");
    PR_fprintf(err, "\t-l n Number of times to loop the test    (default:  1)\n");
    PR_fprintf(err, "\t-h   This message and nothing else\n");
}  /* Help */

static PRIntn PR_CALLBACK RealMain( PRIntn argc, char **argv )
{
    PLOptStatus os;
    PRIntn index, nl;
    PRLock *ml = NULL;
    PRCondVar **cv = NULL;
    PRIntn loops = 1, cvs = 10;
    PRFileDesc *err = PR_GetSpecialFD(PR_StandardError);
    PLOptState *opt = PL_CreateOptState(argc, argv, "hc:l:");

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'c':  /* number of CVs to association with lock */
            cvs = atoi(opt->value);
            break;
        case 'l':  /* number of times to run the tests */
            loops = atoi(opt->value);
            break;
        case 'h':  /* user wants some guidance */
         default:
            Help();  /* so give him an earful */
            return 2;  /* but not a lot else */
        }
    }
    PL_DestroyOptState(opt);

    PR_fprintf(err, "Settings\n");
    PR_fprintf(err, "\tConditions / lock: %d\n", cvs);
    PR_fprintf(err, "\tLoops to run test: %d\n", loops);

    ml = PR_NewLock();
    PR_ASSERT(NULL != ml);

    cv = (PRCondVar**)PR_CALLOC(sizeof(PRCondVar*) * cvs);
    PR_ASSERT(NULL != cv);

    for (index = 0; index < cvs; ++index)
    {
        cv[index] = PR_NewCondVar(ml);
        PR_ASSERT(NULL != cv[index]);
    }

    for (index = 0; index < loops; ++index)
    {
        PR_Lock(ml);
        for (nl = 0; nl < cvs; ++nl)
            PR_NotifyCondVar(cv[nl]);
        PR_Unlock(ml);
    }

    for (index = 0; index < cvs; ++index)
        PR_DestroyCondVar(cv[index]);

    PR_DestroyLock(ml);
    
    printf("PASS\n");

    return 0;
}


PRIntn main(PRIntn argc, char **argv)
{
    PRIntn rv;
    
    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */
