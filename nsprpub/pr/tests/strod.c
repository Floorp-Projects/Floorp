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
#include "prio.h"
#include "prprf.h"
#include "prdtoa.h"
#include "plgetopt.h"

#include <stdlib.h>

static void Help(void)
{
    PRFileDesc *err = PR_GetSpecialFD(PR_StandardError);
    PR_fprintf(err, "Usage: /.strod [-n n] [-l n] [-h]\n");
    PR_fprintf(err, "\t-n n Number to translate    (default: 1234567890123456789)\n");
    PR_fprintf(err, "\t-l n Times to loop the test (default: 1)\n");
    PR_fprintf(err, "\t-h   This message and nothing else\n");
}  /* Help */

static PRIntn PR_CALLBACK RealMain(PRIntn argc, char **argv)
{
    PLOptStatus os;
    PRIntn loops = 1;
    PRFloat64 answer;
    const char *number = "1234567890123456789";
    PRFileDesc *err = PR_GetSpecialFD(PR_StandardError);
    PLOptState *opt = PL_CreateOptState(argc, argv, "hc:l:");

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'n':  /* number to translate */
            number = opt->value;
            break;
        case 'l':  /* number of times to run the tests */
            loops = atoi(opt->value);
            break;
        case 'h':  /* user wants some guidance */
            Help();  /* so give him an earful */
            return 2;  /* but not a lot else */
            break;
         default:
            break;
        }
    }
    PL_DestroyOptState(opt);

    PR_fprintf(err, "Settings\n");
    PR_fprintf(err, "\tNumber to translate %s\n", number);
    PR_fprintf(err, "\tLoops to run test: %d\n", loops);

    while (loops--)
    {
        answer = PR_strtod(number, NULL);
        PR_fprintf(err, "Translation = %20.0f\n", answer);
    }
    return 0;
}



PRIntn main(PRIntn argc, char *argv[])
{
    PRIntn rv;
    
    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */
