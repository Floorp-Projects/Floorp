/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prnetdb.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const char *testaddrs[] = {
    "::", "::",
    "::1", "::1",
    "::ffff", "::ffff",
    "::1:0", "::0.1.0.0",
    "::127.0.0.1", "::127.0.0.1",
    "::FFFF:127.0.0.1", "::ffff:127.0.0.1",
    "::FFFE:9504:3501", "::fffe:9504:3501",
    "0:0:1:0:35c:0:0:0", "0:0:1:0:35c::",
    "0:0:3f4c:0:0:4552:0:0", "::3f4c:0:0:4552:0:0",
    "0:0:1245:0:0:0:0567:0", "0:0:1245::567:0", 
    "0:1:2:3:4:5:6:7", "0:1:2:3:4:5:6:7", 
    "1:2:3:0:4:5:6:7", "1:2:3:0:4:5:6:7", 
    "1:2:3:4:5:6:7:0", "1:2:3:4:5:6:7:0", 
    "1:2:3:4:5:6:7:8", "1:2:3:4:5:6:7:8", 
    "1:2:3:4:5:6::7", "1:2:3:4:5:6:0:7", 
    0
};

const char *badaddrs[] = {
    "::.1.2.3",
    "ffff::.1.2.3",
    "1:2:3:4:5:6:7::8",
    "1:2:3:4:5:6::7:8",
    "::ff99.2.3.4",
    0
};

int failed_already = 0;

int main(int argc, char **argv)
{
    const char **nexttestaddr = testaddrs;
    const char **nextbadaddr = badaddrs;
    const char *in, *expected_out;
    PRNetAddr addr;
    char buf[256];
    PRStatus rv;

    while ((in = *nexttestaddr++) != 0) {
	expected_out = *nexttestaddr++;
	rv = PR_StringToNetAddr(in, &addr);
	if (rv) {
	    printf("cannot convert %s to addr: %d\n", in, rv);
            failed_already = 1;
	    continue;
	}
	rv = PR_NetAddrToString(&addr, buf, sizeof(buf));
	if (rv) {
	    printf("cannot convert %s back to string: %d\n", in, rv);
            failed_already = 1;
	    continue;
	}
	if (strcmp(buf, expected_out)) {
            /* This is not necessarily an error */
	    printf("%s expected %s got %s\n", in, expected_out, buf);
	}
    }
    while ((in = *nextbadaddr++) != 0) {
        if (PR_StringToNetAddr(in, &addr) == PR_SUCCESS) {
            printf("converted bad addr %s\n", in);
            failed_already = 1;
        }
    }
    if (failed_already) {
        printf("FAIL\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
