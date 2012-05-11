/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prinit.h"
#include "prprf.h"
#include "prthread.h"
#include "prcvar.h"
#include "prlock.h"
#include "prlog.h"
#include "prmem.h"

#include "primpl.h"

#include "plgetopt.h"

#include <stdlib.h>

static PRInt32 RandomNum(void)
{
    PRInt32 ran = rand() >> 16;
    return ran;
}  /* RandomNum */

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
    PRBool stats = PR_FALSE;
    PRIntn nc, loops = 1, cvs = 10;
    PRFileDesc *err = PR_GetSpecialFD(PR_StandardError);
    PLOptState *opt = PL_CreateOptState(argc, argv, "hsc:l:");

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 's':  /* number of CVs to association with lock */
            stats = PR_TRUE;
            break;
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
        {
            PRInt32 ran = RandomNum() % 8;
            if (0 == ran) PR_NotifyAllCondVar(cv[nl]);
            else for (nc = 0; nc < ran; ++nc)
                PR_NotifyCondVar(cv[nl]);
        }
        PR_Unlock(ml);
    }

    for (index = 0; index < cvs; ++index)
        PR_DestroyCondVar(cv[index]);

    PR_DELETE(cv);

    PR_DestroyLock(ml);
    
    printf("PASS\n");

    PT_FPrintStats(err, "\nPThread Statistics\n");
    return 0;
}


int main(int argc, char **argv)
{
    PRIntn rv;
    
    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */
