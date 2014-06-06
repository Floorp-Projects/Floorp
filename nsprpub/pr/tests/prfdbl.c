/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This is a simple test of the PR_fprintf() function for doubles.
 */

#include "prprf.h"

int main()
{
    double pi = 3.1415926;
    double e = 2.71828;
    double root2 = 1.414;
    double zero = 0.0;
    double nan = zero / zero;

    PR_fprintf(PR_STDOUT, "pi is %f.\n", pi);
    PR_fprintf(PR_STDOUT, "e is %f.\n", e);
    PR_fprintf(PR_STDOUT, "The square root of 2 is %f.\n", root2);
    PR_fprintf(PR_STDOUT, "NaN is %f.\n", nan);

    PR_fprintf(PR_STDOUT, "pi is %301f.\n", pi);
    PR_fprintf(PR_STDOUT, "e is %65416.123f.\n", e);
    PR_fprintf(PR_STDOUT, "e is %0000000000000000000065416.123f.\n", e);
    PR_fprintf(PR_STDOUT, "NaN is %1024.1f.\n", nan);
    return 0;
}
