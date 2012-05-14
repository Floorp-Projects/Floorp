/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prio.h"
#include "prenv.h"
#include "prmem.h"
#include "prlink.h"
#include "prsystem.h"
#include "prnetdb.h"
#include "prprf.h"
#include "prvrsion.h"

#include "plerror.h"
#include "plgetopt.h"
#include "obsolete/probslet.h"

#include <string.h>

#define DNS_BUFFER 100
#define ADDR_BUFFER 100
#define HOST_BUFFER 1024
#define PROTO_BUFFER 1500

#define NETADDR_SIZE(addr) \
    (PR_AF_INET == (addr)->raw.family ? \
    sizeof((addr)->inet) : sizeof((addr)->ipv6))

static PRFileDesc *err = NULL;

static void Help(void)
{
    PR_fprintf(err, "Usage: [-V] [-h]\n");
    PR_fprintf(err, "\t<nul>    Name of host to lookup          (default: self)\n");
    PR_fprintf(err, "\t-V       Display runtime version info    (default: FALSE)\n");
    PR_fprintf(err, "\t-h       This message and nothing else\n");
}  /* Help */

static void DumpAddr(const PRNetAddr* address, const char *msg)
{
    PRUint32 *word = (PRUint32*)address;
    PRUint32 addr_len = sizeof(PRNetAddr);
    PR_fprintf(err, "%s[%d]\t", msg, NETADDR_SIZE(address));
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
            PRSize addr_len = NETADDR_SIZE(address);
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

int main(int argc, char **argv)
{
    PRStatus rv;
    PLOptStatus os;
    PRHostEnt host;
    PRProtoEnt proto;
    const char *name = NULL;
    PRBool failed = PR_FALSE, version = PR_FALSE;
    PLOptState *opt = PL_CreateOptState(argc, argv, "Vh");

    err = PR_GetSpecialFD(PR_StandardError);

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 0:  /* Name of host to lookup */
            name = opt->value;
            break;
         case 'V':  /* Do version discovery */
            version = PR_TRUE;
            break;
        case 'h':  /* user wants some guidance */
         default:
            Help();  /* so give him an earful */
            return 2;  /* but not a lot else */
        }
    }
    PL_DestroyOptState(opt);

    if (version)
    {
#if defined(WINNT)
#define NSPR_LIB "libnspr4"
#else
#define NSPR_LIB "nspr4"
#endif
        const PRVersionDescription *version_info;
        char *nspr_path = PR_GetEnv("LD_LIBRARY_PATH");
        char *nspr_name = PR_GetLibraryName(nspr_path, NSPR_LIB);
        PRLibrary *runtime = PR_LoadLibrary(nspr_name);
        if (NULL == runtime)
            PL_FPrintError(err, "PR_LoadLibrary");
        else
        {
            versionEntryPointType versionPoint = (versionEntryPointType)
                PR_FindSymbol(runtime, "libVersionPoint");
            if (NULL == versionPoint)
                PL_FPrintError(err, "PR_FindSymbol");
            else
            {
                char buffer[100];
                PRExplodedTime exploded;
                version_info = versionPoint();
                (void)PR_fprintf(err, "Runtime library version information\n");
                PR_ExplodeTime(
                    version_info->buildTime, PR_GMTParameters, &exploded);
                (void)PR_FormatTime(
                    buffer, sizeof(buffer), "%d %b %Y %H:%M:%S", &exploded);
                (void)PR_fprintf(err, "  Build time: %s GMT\n", buffer);
                (void)PR_fprintf(
                    err, "  Build time: %s\n", version_info->buildTimeString);
                (void)PR_fprintf(
                    err, "  %s V%u.%u.%u (%s%s%s)\n",
                    version_info->description,
                    version_info->vMajor,
                    version_info->vMinor,
                    version_info->vPatch,
                    (version_info->beta ? " beta " : ""),
                    (version_info->debug ? " debug " : ""),
                    (version_info->special ? " special" : ""));
                (void)PR_fprintf(err, "  filename: %s\n", version_info->filename);
                (void)PR_fprintf(err, "  security: %s\n", version_info->security);
                (void)PR_fprintf(err, "  copyright: %s\n", version_info->copyright);
                (void)PR_fprintf(err, "  comment: %s\n", version_info->comment);
            }
        }
        if (NULL != nspr_name) PR_FreeLibraryName(nspr_name);
    }

    {
        if (NULL == name)
        {
            char *me = (char*)PR_MALLOC(DNS_BUFFER);
            rv = PR_GetSystemInfo(PR_SI_HOSTNAME, me, DNS_BUFFER);
            if (PR_FAILURE == rv)
            {
                failed = PR_TRUE;
                PL_FPrintError(err, "PR_GetSystemInfo");
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
            memset(&address, 0, sizeof(PRNetAddr));
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
