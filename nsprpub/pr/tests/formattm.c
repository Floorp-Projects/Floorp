/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A test program for PR_FormatTime and PR_FormatTimeUSEnglish */

#include "prtime.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    char buffer[256];
    char small_buffer[8];
    PRTime now;
    PRExplodedTime tod;

    now = PR_Now();
    PR_ExplodeTime(now, PR_LocalTimeParameters, &tod);

    if (PR_FormatTime(buffer, sizeof(buffer),
                      "%a %b %d %H:%M:%S %Z %Y", &tod) != 0) {
        printf("%s\n", buffer);
    } else {
        fprintf(stderr, "PR_FormatTime(buffer) failed\n");
        return 1;
    }

    small_buffer[0] = '?';
    if (PR_FormatTime(small_buffer, sizeof(small_buffer),
                      "%a %b %d %H:%M:%S %Z %Y", &tod) == 0) {
        if (small_buffer[0] != '\0') {
            fprintf(stderr, "PR_FormatTime(small_buffer) did not output "
                    "an empty string on failure\n");
            return 1;
        }
        printf("%s\n", small_buffer);
    } else {
        fprintf(stderr, "PR_FormatTime(small_buffer) succeeded "
                "unexpectedly\n");
        return 1;
    }

    (void)PR_FormatTimeUSEnglish(buffer, sizeof(buffer),
                                 "%a %b %d %H:%M:%S %Z %Y", &tod);
    printf("%s\n", buffer);

    return 0;
}
