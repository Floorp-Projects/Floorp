/*
 * PKCS #11 FIPS Power-Up Self Test.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: fipstest.c,v 1.31 2012/06/28 17:55:06 rrelyea%redhat.com Exp $ */

#ifndef NSS_FIPS_DISABLED

#include "seccomon.h"
#include "lgdb.h"
#include "blapi.h"

/*
 * different platforms have different ways of calling and initial entry point
 * when the dll/.so is loaded. Most platforms support either a posix pragma
 * or the GCC attribute. Some platforms suppor a pre-defined name, and some
 * platforms have a link line way of invoking this function.
 */

/* The pragma */
#if defined(USE_INIT_PRAGMA)
#pragma init(lg_startup_tests)
#endif

/* GCC Attribute */
#if defined(__GNUC__) && !defined(NSS_NO_INIT_SUPPORT)
#define INIT_FUNCTION __attribute__((constructor))
#else
#define INIT_FUNCTION
#endif

static void INIT_FUNCTION lg_startup_tests(void);

/* Windows pre-defined entry */
#if defined(XP_WIN) && !defined(NSS_NO_INIT_SUPPORT)
#include <windows.h>

BOOL WINAPI
DllMain(
    HINSTANCE hinstDLL, // handle to DLL module
    DWORD fdwReason,    // reason for calling function
    LPVOID lpReserved)  // reserved
{
    // Perform actions based on the reason for calling.
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            // Initialize once for each new process.
            // Return FALSE to fail DLL load.
            lg_startup_tests();
            break;

        case DLL_THREAD_ATTACH:
            // Do thread-specific initialization.
            break;

        case DLL_THREAD_DETACH:
            // Do thread-specific cleanup.
            break;

        case DLL_PROCESS_DETACH:
            // Perform any necessary cleanup.
            break;
    }
    return TRUE; // Successful DLL_PROCESS_ATTACH.
}
#endif

static PRBool lg_self_tests_ran = PR_FALSE;
static PRBool lg_self_tests_success = PR_FALSE;

static void
lg_local_function(void)
{
}

/*
 * This function is called at dll load time, the code tha makes this
 * happen is platform specific on defined above.
 */
static void
lg_startup_tests(void)
{
    const char *libraryName = LG_LIB_NAME;

    PORT_Assert(!lg_self_tests_ran);
    PORT_Assert(!lg_self_tests_success);
    lg_self_tests_ran = PR_TRUE;
    lg_self_tests_success = PR_FALSE; /* just in case */

    /* no self tests required for the legacy db, only the integrity check */
    /* check the integrity of our shared library */
    if (!BLAPI_SHVerify(libraryName, (PRFuncPtr)&lg_local_function)) {
        /* something is wrong with the library, fail without enabling
         * the fips token */
        return;
    }
    /* FIPS product has been installed and is functioning, allow
     * the module to operate in fips mode */
    lg_self_tests_success = PR_TRUE;
}

PRBool
lg_FIPSEntryOK()
{
#ifdef NSS_NO_INIT_SUPPORT
    /* this should only be set on platforms that can't handle one of the INIT
     * schemes.  This code allows those platforms to continue to function,
     * though they don't meet the strict NIST requirements. If NO_INIT_SUPPORT
     * is not set, and init support has not been properly enabled, softken
     * will always fail because of the test below */
    if (!lg_self_tests_ran) {
        lg_startup_tests();
    }
#endif
    return lg_self_tests_success;
}

#endif /* NSS_FIPS_DISABLED */
