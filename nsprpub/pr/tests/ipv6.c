/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "prio.h"
#include "prmem.h"
#include "prsystem.h"
#include "prnetdb.h"
#include "prprf.h"

#include "plerror.h"
#include "plgetopt.h"
#include "obsolete/probslet.h"

#include <string.h>

#define DNS_BUFFER 100
#define ADDR_BUFFER 100
#define HOST_BUFFER 1024
#define PROTO_BUFFER 1500

static PRFileDesc *err = NULL;

static void Help(void)
{
    PR_fprintf(err, "Usage: [-t s] [-s] [-h]\n");
    PR_fprintf(err, "\t<nul>    Name of host to lookup          (default: self)\n");
    PR_fprintf(err, "\t-6       First turn on IPv6 capability   (default: FALSE)\n");
    PR_fprintf(err, "\t-h       This message and nothing else\n");
}  /* Help */

static void DumpAddr(const PRNetAddr* address, const char *msg)
{
    PRUint32 *word = (PRUint32*)address;
    PRUint32 addr_len = sizeof(PRNetAddr);
    PR_fprintf(err, "%s[%d]\t", msg, PR_NETADDR_SIZE(address));
    while (addr_len > 0)
    {
        PR_fprintf(err, " %08x", *word++);
        addr_len -= sizeof(PRUint32);
    }
    PR_fprintf(err, "\n");
}  /* DumpAddr */

static PRStatus PrintAddress(const PRNetAddr* address)
{
    PRNetAddr translation;
    char buffer[ADDR_BUFFER];
    PRStatus rv = PR_NetAddrToString(address, buffer, sizeof(buffer));
    if (PR_FAILURE == rv) PL_FPrintError(err, "PR_NetAddrToString");
    else
    {
        PR_fprintf(err, "\t%s\n", buffer);
        memset(&translation, 0, sizeof(translation));
        rv = PR_StringToNetAddr(buffer, &translation);
        if (PR_FAILURE == rv) PL_FPrintError(err, "PR_StringToNetAddr");
        else
        {
            PRSize addr_len = PR_NETADDR_SIZE(address);
            if (0 != memcmp(address, &translation, addr_len))
            {
                PR_fprintf(err, "Address translations do not match\n");
                DumpAddr(address, "original");
                DumpAddr(&translation, "translate");
                rv = PR_FAILURE;
            }
        }
    }
    return rv;
}  /* PrintAddress */

PRIntn main(PRIntn argc, char **argv)
{
    PRStatus rv;
    PLOptStatus os;
    PRHostEnt host;
    PRProtoEnt proto;
    PRBool ipv6 = PR_FALSE;
    const char *name = NULL;
    PRBool failed = PR_FALSE;
    PLOptState *opt = PL_CreateOptState(argc, argv, "h6");

    err = PR_GetSpecialFD(PR_StandardError);

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 0:  /* Name of host to lookup */
            name = opt->value;
            break;
        case '6':  /* Turn on IPv6 mode */
            ipv6 = PR_TRUE;
            break;
        case 'h':  /* user wants some guidance */
         default:
            Help();  /* so give him an earful */
            return 2;  /* but not a lot else */
        }
    }
    PL_DestroyOptState(opt);

    if (ipv6)
    {
        rv = PR_SetIPv6Enable(ipv6);
        if (PR_FAILURE == rv)
        {
            failed = PR_TRUE;
            PL_FPrintError(err, "PR_SetIPv6Enable");
        }
    }

    {
        if (NULL == name)
        {
            char *me = (char*)PR_MALLOC(DNS_BUFFER);
            rv = PR_GetSystemInfo(PR_SI_HOSTNAME, me, DNS_BUFFER);
            if (PR_FAILURE == rv)
            {
                failed = PR_TRUE;
                PL_FPrintError(err, "PR_GetHostName");
                return 2;
            }
            name = me;  /* just leak the storage */
        }
    }

    {
        char buffer[HOST_BUFFER];
        PR_fprintf(err, "Translating the name %s ...", name);

        rv = PR_GetHostByName(name, buffer, sizeof(buffer), &host);
        if (PR_FAILURE == rv)
        {
            failed = PR_TRUE;
            PL_FPrintError(err, "PR_GetHostByName");
        }
        else
        {
            PRIntn index = 0;
            PRNetAddr address;
            PR_fprintf(err, "success .. enumerating results\n");
            do
            {
                index = PR_EnumerateHostEnt(index, &host, 0, &address);
                if (index > 0) PrintAddress(&address);
                else if (-1 == index)
                {
                    failed = PR_TRUE;
                    PL_FPrintError(err, "PR_EnumerateHostEnt");
                }
            } while (index > 0);
        }
    }


    {
        char buffer[PROTO_BUFFER];
        /*
        ** Get Proto by name/number
        */
        rv = PR_GetProtoByName("tcp", &buffer[1], sizeof(buffer) - 1, &proto);
        rv = PR_GetProtoByNumber(6, &buffer[3], sizeof(buffer) - 3, &proto);
    }

    return (failed) ? 1 : 0;
}
