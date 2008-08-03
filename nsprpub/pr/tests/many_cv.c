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

static PRInt32 Random(void)
{
    PRInt32 ran = rand() >> 16;
    return ran;
}  /* Random */

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
            PRInt32 ran = Random() % 8;
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


PRIntn main(PRIntn argc, char **argv)
{
    PRIntn rv;
    
    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */
