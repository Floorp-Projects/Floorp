/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
        if (PL_OPT_BAD == os) {
            continue;
        }
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


int main(int argc, char **argv)
{
    PRIntn rv;

    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */

/* i2l.c */
