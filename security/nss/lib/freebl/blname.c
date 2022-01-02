/*
 *  blname.c - determine the freebl library name.
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(FREEBL_LOWHASH)
static const char* default_name =
    SHLIB_PREFIX "freeblpriv" SHLIB_VERSION "." SHLIB_SUFFIX;
#else
static const char* default_name =
    SHLIB_PREFIX "freebl" SHLIB_VERSION "." SHLIB_SUFFIX;
#endif

/* getLibName() returns the name of the library to load. */

#if defined(SOLARIS) && defined(__sparc)
#include <stddef.h>
#include <strings.h>
#include <sys/systeminfo.h>

#if defined(NSS_USE_64)

const static char fpu_hybrid_shared_lib[] = "libfreebl_64fpu_3.so";
const static char int_hybrid_shared_lib[] = "libfreebl_64int_3.so";
const static char non_hybrid_shared_lib[] = "libfreebl_64fpu_3.so";

const static char int_hybrid_isa[] = "sparcv9";
const static char fpu_hybrid_isa[] = "sparcv9+vis";

#else

const static char fpu_hybrid_shared_lib[] = "libfreebl_32fpu_3.so";
const static char int_hybrid_shared_lib[] = "libfreebl_32int64_3.so";
/* This was for SPARC V8, now obsolete. */
const static char* const non_hybrid_shared_lib = NULL;

const static char int_hybrid_isa[] = "sparcv8plus";
const static char fpu_hybrid_isa[] = "sparcv8plus+vis";

#endif

static const char*
getLibName(void)
{
    char* found_int_hybrid;
    char* found_fpu_hybrid;
    long buflen;
    char buf[256];

    buflen = sysinfo(SI_ISALIST, buf, sizeof buf);
    if (buflen <= 0)
        return NULL;
    /* sysinfo output is always supposed to be NUL terminated, but ... */
    if (buflen < sizeof buf)
        buf[buflen] = '\0';
    else
        buf[(sizeof buf) - 1] = '\0';
    /* The ISA list is a space separated string of names of ISAs and
     * ISA extensions, in order of decreasing performance.
     * There are two different ISAs with which NSS's crypto code can be
     * accelerated. If both are in the list, we take the first one.
     * If one is in the list, we use it, and if neither then we use
     * the base unaccelerated code.
     */
    found_int_hybrid = strstr(buf, int_hybrid_isa);
    found_fpu_hybrid = strstr(buf, fpu_hybrid_isa);
    if (found_fpu_hybrid &&
        (!found_int_hybrid ||
         (found_int_hybrid - found_fpu_hybrid) >= 0)) {
        return fpu_hybrid_shared_lib;
    }
    if (found_int_hybrid) {
        return int_hybrid_shared_lib;
    }
    return non_hybrid_shared_lib;
}

#elif defined(HPUX) && !defined(NSS_USE_64) && !defined(__ia64)
#include <unistd.h>

/* This code tests to see if we're running on a PA2.x CPU.
** It returns true (1) if so, and false (0) otherwise.
*/
static const char*
getLibName(void)
{
    long cpu = sysconf(_SC_CPU_VERSION);
    return (cpu == CPU_PA_RISC2_0)
               ? "libfreebl_32fpu_3.sl"
               : "libfreebl_32int_3.sl";
}
#else
/* default case, for platforms/ABIs that have only one freebl shared lib. */
static const char*
getLibName(void)
{
    return default_name;
}
#endif
