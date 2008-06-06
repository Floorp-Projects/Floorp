/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

#include "primpl.h"
#include "prsystem.h"
#include "prprf.h"
#include "prlong.h"

#if defined(BEOS)
#include <kernel/OS.h>
#endif

#if defined(OS2)
#define INCL_DOS
#define INCL_DOSMISC
#include <os2.h>
/* define the required constant if it is not already defined in the headers */
#ifndef QSV_NUMPROCESSORS
#define QSV_NUMPROCESSORS 26
#endif
#endif

/* BSD-derived systems use sysctl() to get the number of processors */
#if defined(BSDI) || defined(FREEBSD) || defined(NETBSD) \
    || defined(OPENBSD) || defined(DARWIN)
#define _PR_HAVE_SYSCTL
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#if defined(DARWIN)
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#endif

#if defined(HPUX)
#include <sys/mpctl.h>
#include <sys/pstat.h>
#endif

#if defined(XP_UNIX)
#include <unistd.h>
#include <sys/utsname.h>
#endif

#if defined(AIX)
#include <cf.h>
#include <sys/cfgodm.h>
#endif

#if defined(WIN32)
/* This struct is not present in VC6 headers, so declare it here */
typedef struct {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    DWORDLONG ullTotalPhys;
    DWORDLONG ullAvailPhys;
    DWORDLONG ullToalPageFile;
    DWORDLONG ullAvailPageFile;
    DWORDLONG ullTotalVirtual;
    DWORDLONG ullAvailVirtual;
    DWORDLONG ullAvailExtendedVirtual;
} PR_MEMORYSTATUSEX;

/* Typedef for dynamic lookup of GlobalMemoryStatusEx(). */
typedef BOOL (WINAPI *GlobalMemoryStatusExFn)(PR_MEMORYSTATUSEX *);
#endif

PR_IMPLEMENT(char) PR_GetDirectorySeparator(void)
{
    return PR_DIRECTORY_SEPARATOR;
}  /* PR_GetDirectorySeparator */

/*
** OBSOLETE -- the function name is misspelled.
*/
PR_IMPLEMENT(char) PR_GetDirectorySepartor(void)
{
#if defined(DEBUG)
    static PRBool warn = PR_TRUE;
    if (warn) {
        warn = _PR_Obsolete("PR_GetDirectorySepartor()",
                "PR_GetDirectorySeparator()");
    }
#endif
    return PR_GetDirectorySeparator();
}  /* PR_GetDirectorySepartor */

PR_IMPLEMENT(char) PR_GetPathSeparator(void)
{
    return PR_PATH_SEPARATOR;
}  /* PR_GetPathSeparator */

PR_IMPLEMENT(PRStatus) PR_GetSystemInfo(PRSysInfo cmd, char *buf, PRUint32 buflen)
{
    PRUintn len = 0;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    switch(cmd)
    {
      case PR_SI_HOSTNAME:
      case PR_SI_HOSTNAME_UNTRUNCATED:
        if (PR_FAILURE == _PR_MD_GETHOSTNAME(buf, (PRUintn)buflen))
            return PR_FAILURE;

        if (cmd == PR_SI_HOSTNAME_UNTRUNCATED)
            break;
        /*
         * On some platforms a system does not have a hostname and
         * its IP address is returned instead.   The following code
         * should be skipped on those platforms.
         */
#ifndef _PR_GET_HOST_ADDR_AS_NAME
        /* Return the unqualified hostname */
            while (buf[len] && (len < buflen)) {
                if (buf[len] == '.') {
                    buf[len] = '\0';
                    break;
                }
                len += 1;
            }    
#endif
         break;

      case PR_SI_SYSNAME:
        /* Return the operating system name */
#if defined(XP_UNIX) || defined(WIN32)
        if (PR_FAILURE == _PR_MD_GETSYSINFO(cmd, buf, (PRUintn)buflen))
            return PR_FAILURE;
#else
        (void)PR_snprintf(buf, buflen, _PR_SI_SYSNAME);
#endif
        break;

      case PR_SI_RELEASE:
        /* Return the version of the operating system */
#if defined(XP_UNIX) || defined(WIN32)
        if (PR_FAILURE == _PR_MD_GETSYSINFO(cmd, buf, (PRUintn)buflen))
            return PR_FAILURE;
#endif
#if defined(XP_OS2)
        {
            ULONG os2ver[2] = {0};
            DosQuerySysInfo(QSV_VERSION_MINOR, QSV_VERSION_REVISION,
                            &os2ver, sizeof(os2ver));
            /* Formatting for normal usage (2.11, 3.0, 4.0, 4.5); officially,
               Warp 4 is version 2.40.00, WSeB 2.45.00 */
            if (os2ver[0] < 30)
              (void)PR_snprintf(buf, buflen, "%s%lu",
                                "2.", os2ver[0]);
            else if (os2ver[0] < 45)
              (void)PR_snprintf(buf, buflen, "%lu%s%lu",
                                os2ver[0]/10, ".", os2ver[1]);
            else
              (void)PR_snprintf(buf, buflen, "%.1f",
                                os2ver[0]/10.0);
        }
#endif /* OS2 */
        break;

      case PR_SI_ARCHITECTURE:
        /* Return the architecture of the machine (ie. x86, mips, alpha, ...)*/
        (void)PR_snprintf(buf, buflen, _PR_SI_ARCHITECTURE);
        break;
	  default:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
			return PR_FAILURE;
    }
    return PR_SUCCESS;
}

/*
** PR_GetNumberOfProcessors()
** 
** Implementation notes:
**   Every platform does it a bit different.
**     numCpus is the returned value.
**   for each platform's "if defined" section
**     declare your local variable
**     do your thing, assign to numCpus
**   order of the if defined()s may be important,
**     especially for unix variants. Do platform
**     specific implementations before XP_UNIX.
** 
*/
PR_IMPLEMENT(PRInt32) PR_GetNumberOfProcessors( void )
{
    PRInt32     numCpus;
#if defined(WIN32)
    SYSTEM_INFO     info;

    GetSystemInfo( &info );
    numCpus = info.dwNumberOfProcessors;
#elif defined(XP_MAC)
/* Hard-code the number of processors to 1 on the Mac
** MacOS/9 will always be 1. The MPProcessors() call is for
** MacOS/X, when issued. Leave it commented out for now. */
/*  numCpus = MPProcessors(); */
    numCpus = 1;
#elif defined(BEOS)
    system_info sysInfo;

    get_system_info(&sysInfo);
    numCpus = sysInfo.cpu_count;
#elif defined(OS2)
    DosQuerySysInfo( QSV_NUMPROCESSORS, QSV_NUMPROCESSORS, &numCpus, sizeof(numCpus));
#elif defined(_PR_HAVE_SYSCTL)
    int mib[2];
    int rc;
    size_t len = sizeof(numCpus);

    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    rc = sysctl( mib, 2, &numCpus, &len, NULL, 0 );
    if ( -1 == rc )  {
        numCpus = -1; /* set to -1 for return value on error */
        _PR_MD_MAP_DEFAULT_ERROR( _MD_ERRNO() );
    }
#elif defined(HPUX)
    numCpus = mpctl( MPC_GETNUMSPUS, 0, 0 );
    if ( numCpus < 1 )  {
        numCpus = -1; /* set to -1 for return value on error */
        _PR_MD_MAP_DEFAULT_ERROR( _MD_ERRNO() );
    }
#elif defined(IRIX)
    numCpus = sysconf( _SC_NPROC_ONLN );
#elif defined(RISCOS)
    numCpus = 1;
#elif defined(XP_UNIX)
    numCpus = sysconf( _SC_NPROCESSORS_ONLN );
#else
#error "An implementation is required"
#endif
    return(numCpus);
} /* end PR_GetNumberOfProcessors() */

/*
** PR_GetPhysicalMemorySize()
** 
** Implementation notes:
**   Every platform does it a bit different.
**     bytes is the returned value.
**   for each platform's "if defined" section
**     declare your local variable
**     do your thing, assign to bytes.
** 
*/
PR_IMPLEMENT(PRUint64) PR_GetPhysicalMemorySize(void)
{
    PRUint64 bytes = 0;

#if defined(LINUX) || defined(SOLARIS)

    long pageSize = sysconf(_SC_PAGESIZE);
    long pageCount = sysconf(_SC_PHYS_PAGES);
    bytes = (PRUint64) pageSize * pageCount;

#elif defined(HPUX)

    struct pst_static info;
    int result = pstat_getstatic(&info, sizeof(info), 1, 0);
    if (result == 1)
        bytes = (PRUint64) info.physical_memory * info.page_size;

#elif defined(DARWIN)

    struct host_basic_info hInfo;
    mach_msg_type_number_t count;

    int result = host_info(mach_host_self(),
                           HOST_BASIC_INFO,
                           (host_info_t) &hInfo,
                           &count);
    if (result == KERN_SUCCESS)
        bytes = hInfo.memory_size;

#elif defined(WIN32)

    /* Try to use the newer GlobalMemoryStatusEx API for Windows 2000+. */
    GlobalMemoryStatusExFn globalMemory = (GlobalMemoryStatusExFn) NULL;
    HMODULE module = GetModuleHandle("kernel32.dll");

    if (module) {
        globalMemory = (GlobalMemoryStatusExFn)GetProcAddress(module, "GlobalMemoryStatusEx");

        if (globalMemory) {
            PR_MEMORYSTATUSEX memStat;
            memStat.dwLength = sizeof(memStat);

            if (globalMemory(&memStat))
                bytes = memStat.ullTotalPhys;
        }
    }

    if (!bytes) {
        /* Fall back to the older API. */
        MEMORYSTATUS memStat;
        memset(&memStat, 0, sizeof(memStat));
        GlobalMemoryStatus(&memStat);
        bytes = memStat.dwTotalPhys;
    }

#elif defined(OS2)

    ULONG ulPhysMem;
    DosQuerySysInfo(QSV_TOTPHYSMEM,
                    QSV_TOTPHYSMEM,
                    &ulPhysMem,
                    sizeof(ulPhysMem));
    bytes = ulPhysMem;

#elif defined(AIX)

    if (odm_initialize() == 0) {
        int how_many;
        struct CuAt *obj = getattr("sys0", "realmem", 0, &how_many);
        if (obj != NULL) {
            bytes = (PRUint64) atoi(obj->value) * 1024;
            free(obj);
        }
        odm_terminate();
    }

#else

    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);

#endif

    return bytes;
} /* end PR_GetPhysicalMemorySize() */
