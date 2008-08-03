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
 * Portions created by the Initial Developer are Copyright (C) 2000
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

int main()
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
