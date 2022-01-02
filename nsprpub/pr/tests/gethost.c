/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * File: gethost.c
 *
 * Description: tests various functions in prnetdb.h
 *
 * Usage: gethost [-6] [hostname]
 */

#include "prio.h"
#include "prnetdb.h"
#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_HOST_NAME "mozilla.org"

static void Help(void)
{
    fprintf(stderr, "Usage: gethost [-h] [hostname]\n");
    fprintf(stderr, "\t-h          help\n");
    fprintf(stderr, "\thostname    Name of host    (default: %s)\n",
            DEFAULT_HOST_NAME);
}  /* Help */

/*
 * Prints the contents of a PRHostEnt structure
 */
void PrintHostent(const PRHostEnt *he)
{
    int i;
    int j;

    printf("h_name: %s\n", he->h_name);
    for (i = 0; he->h_aliases[i]; i++) {
        printf("h_aliases[%d]: %s\n", i, he->h_aliases[i]);
    }
    printf("h_addrtype: %d\n", he->h_addrtype);
    printf("h_length: %d\n", he->h_length);
    for (i = 0; he->h_addr_list[i]; i++) {
        printf("h_addr_list[%d]: ", i);
        for (j = 0; j < he->h_length; j++) {
            if (j != 0) {
                printf(".");
            }
            printf("%u", (unsigned char)he->h_addr_list[i][j]);
        }
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    const char *hostName = DEFAULT_HOST_NAME;
    PRHostEnt he, reversehe;
    char buf[PR_NETDB_BUF_SIZE];
    char reversebuf[PR_NETDB_BUF_SIZE];
    PRIntn idx;
    PRNetAddr addr;
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "h");

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt))) {
        if (PL_OPT_BAD == os) {
            continue;
        }
        switch (opt->option) {
            case 0:  /* naked */
                hostName = opt->value;
                break;
            case 'h':  /* Help message */
            default:
                Help();
                return 2;
        }
    }
    PL_DestroyOptState(opt);

    if (PR_GetHostByName(hostName, buf, sizeof(buf), &he) == PR_FAILURE) {
        fprintf(stderr, "PR_GetHostByName failed\n");
        exit(1);
    }
    PrintHostent(&he);
    idx = 0;
    while (1) {
        idx = PR_EnumerateHostEnt(idx, &he, 0, &addr);
        if (idx == -1) {
            fprintf(stderr, "PR_EnumerateHostEnt failed\n");
            exit(1);
        }
        if (idx == 0) {
            break;    /* normal loop termination */
        }
        printf("reverse lookup\n");
        if (PR_GetHostByAddr(&addr, reversebuf, sizeof(reversebuf),
                             &reversehe) == PR_FAILURE) {
            fprintf(stderr, "PR_GetHostByAddr failed\n");
            exit(1);
        }
        PrintHostent(&reversehe);
    }

    printf("PR_GetIPNodeByName with PR_AF_INET\n");
    if (PR_GetIPNodeByName(hostName, PR_AF_INET, PR_AI_DEFAULT,
                           buf, sizeof(buf), &he) == PR_FAILURE) {
        fprintf(stderr, "PR_GetIPNodeByName failed\n");
        exit(1);
    }
    PrintHostent(&he);
    printf("PR_GetIPNodeByName with PR_AF_INET6\n");
    if (PR_GetIPNodeByName(hostName, PR_AF_INET6, PR_AI_DEFAULT,
                           buf, sizeof(buf), &he) == PR_FAILURE) {
        fprintf(stderr, "PR_GetIPNodeByName failed\n");
        exit(1);
    }
    PrintHostent(&he);
    idx = 0;
    printf("PR_GetHostByAddr with PR_AF_INET6\n");
    while (1) {
        idx = PR_EnumerateHostEnt(idx, &he, 0, &addr);
        if (idx == -1) {
            fprintf(stderr, "PR_EnumerateHostEnt failed\n");
            exit(1);
        }
        if (idx == 0) {
            break;    /* normal loop termination */
        }
        printf("reverse lookup\n");
        if (PR_GetHostByAddr(&addr, reversebuf, sizeof(reversebuf),
                             &reversehe) == PR_FAILURE) {
            fprintf(stderr, "PR_GetHostByAddr failed\n");
            exit(1);
        }
        PrintHostent(&reversehe);
    }
    printf("PR_GetHostByAddr with PR_AF_INET6 done\n");

    PR_StringToNetAddr("::1", &addr);
    if (PR_IsNetAddrType(&addr, PR_IpAddrV4Mapped) == PR_TRUE) {
        fprintf(stderr, "addr should not be ipv4 mapped address\n");
        exit(1);
    }
    if (PR_IsNetAddrType(&addr, PR_IpAddrLoopback) == PR_FALSE) {
        fprintf(stderr, "addr should be loopback address\n");
        exit(1);
    }

    PR_StringToNetAddr("127.0.0.1", &addr);
    if (PR_IsNetAddrType(&addr, PR_IpAddrLoopback) == PR_FALSE) {
        fprintf(stderr, "addr should be loopback address\n");
        exit(1);
    }
    PR_StringToNetAddr("::FFFF:127.0.0.1", &addr);
    if (PR_IsNetAddrType(&addr, PR_IpAddrV4Mapped) == PR_FALSE) {
        fprintf(stderr, "addr should be ipv4 mapped address\n");
        exit(1);
    }
    if (PR_IsNetAddrType(&addr, PR_IpAddrLoopback) == PR_FALSE) {
        fprintf(stderr, "addr should be loopback address\n");
        exit(1);
    }

    if (PR_InitializeNetAddr(PR_IpAddrAny, 0, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_InitializeNetAddr failed\n");
        exit(1);
    }
    if (PR_IsNetAddrType(&addr, PR_IpAddrAny) == PR_FALSE) {
        fprintf(stderr, "addr should be unspecified address\n");
        exit(1);
    }
    if (PR_InitializeNetAddr(PR_IpAddrLoopback, 0, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_InitializeNetAddr failed\n");
        exit(1);
    }
    if (PR_IsNetAddrType(&addr, PR_IpAddrLoopback) == PR_FALSE) {
        fprintf(stderr, "addr should be loopback address\n");
        exit(1);
    }

    if (PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET, 0, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_SetNetAddr failed\n");
        exit(1);
    }
    if (PR_IsNetAddrType(&addr, PR_IpAddrAny) == PR_FALSE) {
        fprintf(stderr, "addr should be unspecified address\n");
        exit(1);
    }
    if (PR_SetNetAddr(PR_IpAddrLoopback, PR_AF_INET, 0, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_SetNetAddr failed\n");
        exit(1);
    }
    if (PR_IsNetAddrType(&addr, PR_IpAddrLoopback) == PR_FALSE) {
        fprintf(stderr, "addr should be loopback address\n");
        exit(1);
    }

    addr.inet.family = PR_AF_INET;
    addr.inet.port = 0;
    addr.inet.ip = PR_htonl(PR_INADDR_ANY);
    if (PR_IsNetAddrType(&addr, PR_IpAddrAny) == PR_FALSE) {
        fprintf(stderr, "addr should be unspecified address\n");
        exit(1);
    }
    {
        char buf[256];
        PR_NetAddrToString(&addr, buf, 256);
        printf("IPv4 INADDRANY: %s\n", buf);
    }
    addr.inet.family = PR_AF_INET;
    addr.inet.port = 0;
    addr.inet.ip = PR_htonl(PR_INADDR_LOOPBACK);
    if (PR_IsNetAddrType(&addr, PR_IpAddrLoopback) == PR_FALSE) {
        fprintf(stderr, "addr should be loopback address\n");
        exit(1);
    }
    {
        char buf[256];
        PR_NetAddrToString(&addr, buf, 256);
        printf("IPv4 LOOPBACK: %s\n", buf);
    }

    if (PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET6, 0, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_SetNetAddr failed\n");
        exit(1);
    }
    if (PR_IsNetAddrType(&addr, PR_IpAddrAny) == PR_FALSE) {
        fprintf(stderr, "addr should be unspecified address\n");
        exit(1);
    }
    {
        char buf[256];
        PR_NetAddrToString(&addr, buf, 256);
        printf("IPv6 INADDRANY: %s\n", buf);
    }
    if (PR_SetNetAddr(PR_IpAddrLoopback, PR_AF_INET6, 0, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_SetNetAddr failed\n");
        exit(1);
    }
    if (PR_IsNetAddrType(&addr, PR_IpAddrLoopback) == PR_FALSE) {
        fprintf(stderr, "addr should be loopback address\n");
        exit(1);
    }
    {
        char buf[256];
        PR_NetAddrToString(&addr, buf, 256);
        printf("IPv6 LOOPBACK: %s\n", buf);
    }
    {
        PRIPv6Addr v6addr;
        char tmp_buf[256];

        PR_SetNetAddr(PR_IpAddrLoopback, PR_AF_INET, 0, &addr);

        PR_ConvertIPv4AddrToIPv6(addr.inet.ip, &v6addr);
        PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET6, 0, &addr);
        addr.ipv6.ip = v6addr;
        PR_NetAddrToString(&addr, tmp_buf, 256);
        printf("IPv4-mapped IPv6 LOOPBACK: %s\n", tmp_buf);
    }
    printf("PASS\n");
    return 0;
}
