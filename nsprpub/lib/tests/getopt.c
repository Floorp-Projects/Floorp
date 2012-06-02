/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nspr.h"
#include "plgetopt.h"



static const PLLongOpt optArray[] = {
    { "longa", 'a'        , PR_TRUE  },
    { "longb", 'b'        , PR_TRUE  },
    { "longc", 'c'        , PR_FALSE },
    { "longd", 'd' | 0x100, PR_TRUE  },
    { "longe", 'e' | 0x100, PR_FALSE },
    {    NULL,                       }
};

int
main(int argc, char **argv) 
{
    PLOptState *opt;
    PLOptStatus ostat;

    opt = PL_CreateLongOptState(argc, argv, "a:b:c", optArray);

    while (PL_OPT_OK == (ostat = PL_GetNextOpt(opt))) {
	if (opt->option == 0 && opt->longOptIndex < 0) 
	    printf("Positional parameter: \"%s\"\n", opt->value);
	else
	    printf("%s option: %x (\'%c\', index %d), argument: \"%s\"\n",
		   (ostat == PL_OPT_BAD) ? "BAD" : "GOOD",
		   opt->longOption, opt->option ? opt->option : ' ',
		   opt->longOptIndex, opt->value);

    }
    printf("last result was %s\n", (ostat == PL_OPT_BAD) ? "BAD" : "EOL");
    PL_DestroyOptState(opt);
    return 0;
}
