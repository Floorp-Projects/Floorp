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

#include <stdlib.h>

#include "prio.h"
#include "prinit.h"
#include "prprf.h"
#include "prlong.h"

#include "plerror.h"
#include "plgetopt.h"

typedef union Overlay_i
{
    PRInt32 i;
    PRInt64 l;
} Overlay_i;

typedef union Overlay_u
{
    PRUint32 i;
    PRUint64 l;
} Overlay_u;

static PRFileDesc *err = NULL;

static void Help(void)
{
    PR_fprintf(err, "Usage: -i n | -u n | -h\n");
    PR_fprintf(err, "\t-i n treat following number as signed integer\n");
    PR_fprintf(err, "\t-u n treat following number as unsigned integer\n");
    PR_fprintf(err, "\t-h   This message and nothing else\n");
}  /* Help */

static PRIntn PR_CALLBACK RealMain(PRIntn argc, char **argv)
{
    Overlay_i si;
    Overlay_u ui;
    PLOptStatus os;
    PRBool bsi = PR_FALSE, bui = PR_FALSE;
    PLOptState *opt = PL_CreateOptState(argc, argv, "hi:u:");
    err = PR_GetSpecialFD(PR_StandardError);

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'i':  /* signed integer */
            si.i = (PRInt32)atoi(opt->value);
            bsi = PR_TRUE;
            break;
        case 'u':  /* unsigned */
            ui.i = (PRUint32)atoi(opt->value);
            bui = PR_TRUE;
            break;
        case 'h':  /* user wants some guidance */
         default:
            Help();  /* so give him an earful */
            return 2;  /* but not a lot else */
        }
    }
    PL_DestroyOptState(opt);

#if defined(HAVE_LONG_LONG)
    PR_fprintf(err, "We have long long\n");
#else
    PR_fprintf(err, "We don't have long long\n");
#endif

    if (bsi)
    {
        PR_fprintf(err, "Converting %ld: ", si.i);
        LL_I2L(si.l, si.i);
        PR_fprintf(err, "%lld\n", si.l);
    }

    if (bui)
    {
        PR_fprintf(err, "Converting %lu: ", ui.i);
        LL_I2L(ui.l, ui.i);
        PR_fprintf(err, "%llu\n", ui.l);
    }
    return 0;

}  /* main */


PRIntn main(PRIntn argc, char *argv[])
{
    PRIntn rv;
    
    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */

/* i2l.c */
