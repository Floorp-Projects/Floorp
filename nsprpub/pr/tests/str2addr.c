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
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
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

/*
 * File: str2addr.c
 * Description: a test for PR_StringToNetAddr
 */

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>

/* Address string to convert */
#define DEFAULT_IPV4_ADDR_STR "207.200.73.41"

/* Expected conversion result, in network byte order */
static unsigned char default_ipv4_addr[] = {207, 200, 73, 41};

int main(int argc, char **argv)
{
    PRNetAddr addr;
    const char *addrStr;
    unsigned char *bytes;
    int idx;

    addrStr = DEFAULT_IPV4_ADDR_STR;
    if (PR_StringToNetAddr(addrStr, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_StringToNetAddr failed\n");
        exit(1);
    }
    if (addr.inet.family != PR_AF_INET) {
        fprintf(stderr, "addr.inet.family should be %d but is %d\n",
                PR_AF_INET, addr.inet.family);
        exit(1);
    }
    bytes = (unsigned char *) &addr.inet.ip;
    for (idx = 0; idx < 4; idx++) {
        if (bytes[idx] != default_ipv4_addr[idx]) {
            fprintf(stderr, "byte %d of IPv4 addr should be %d but is %d\n",
                    idx, default_ipv4_addr[idx], bytes[idx]);
            exit(1);
        }
    }

    printf("PASS\n");
    return 0;
}
