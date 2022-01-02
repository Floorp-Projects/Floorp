/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
