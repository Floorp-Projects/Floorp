/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "blapii.h"
#include "mpi.h"
#include "secerr.h"
#include "prtypes.h"
#include "prinit.h"
#include "prenv.h"

#if defined(_MSC_VER) && !defined(_M_IX86)
#include <intrin.h> /* for _xgetbv() */
#endif

static PRCallOnceType coFreeblInit;

/* State variables. */
static PRBool aesni_support_ = PR_FALSE;
static PRBool clmul_support_ = PR_FALSE;
static PRBool avx_support_ = PR_FALSE;

#ifdef NSS_X86_OR_X64
/*
 * Adapted from the example code in "How to detect New Instruction support in
 * the 4th generation Intel Core processor family" by Max Locktyukhin.
 *
 * XGETBV:
 *   Reads an extended control register (XCR) specified by ECX into EDX:EAX.
 */
static PRBool
check_xcr0_ymm()
{
    PRUint32 xcr0;
#if defined(_MSC_VER)
#if defined(_M_IX86)
    __asm {
        mov ecx, 0
        xgetbv
        mov xcr0, eax
    }
#else
    xcr0 = (PRUint32)_xgetbv(0); /* Requires VS2010 SP1 or later. */
#endif /* _M_IX86 */
#else  /* _MSC_VER */
    /* Old OSX compilers don't support xgetbv. Use byte form. */
    __asm__(".byte 0x0F, 0x01, 0xd0"
            : "=a"(xcr0)
            : "c"(0)
            : "%edx");
#endif /* _MSC_VER */
    /* Check if xmm and ymm state are enabled in XCR0. */
    return (xcr0 & 6) == 6;
}

#define ECX_AESNI (1 << 25)
#define ECX_CLMUL (1 << 1)
#define ECX_XSAVE (1 << 26)
#define ECX_OSXSAVE (1 << 27)
#define ECX_AVX (1 << 28)
#define AVX_BITS (ECX_XSAVE | ECX_OSXSAVE | ECX_AVX)

void
CheckX86CPUSupport()
{
    unsigned long eax, ebx, ecx, edx;
    char *disable_hw_aes = PR_GetEnvSecure("NSS_DISABLE_HW_AES");
    char *disable_pclmul = PR_GetEnvSecure("NSS_DISABLE_PCLMUL");
    char *disable_avx = PR_GetEnvSecure("NSS_DISABLE_AVX");
    freebl_cpuid(1, &eax, &ebx, &ecx, &edx);
    aesni_support_ = (PRBool)((ecx & ECX_AESNI) != 0 && disable_hw_aes == NULL);
    clmul_support_ = (PRBool)((ecx & ECX_CLMUL) != 0 && disable_pclmul == NULL);
    /* For AVX we check AVX, OSXSAVE, and XSAVE
     * as well as XMM and YMM state. */
    avx_support_ = (PRBool)((ecx & AVX_BITS) == AVX_BITS) && check_xcr0_ymm() &&
                   disable_avx == NULL;
}
#endif /* NSS_X86_OR_X64 */

PRBool
aesni_support()
{
    return aesni_support_;
}
PRBool
clmul_support()
{
    return clmul_support_;
}
PRBool
avx_support()
{
    return avx_support_;
}

static PRStatus
FreeblInit(void)
{
#ifdef NSS_X86_OR_X64
    CheckX86CPUSupport();
#endif
    return PR_SUCCESS;
}

SECStatus
BL_Init()
{
    if (PR_CallOnce(&coFreeblInit, FreeblInit) != PR_SUCCESS) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    RSA_Init();

    return SECSuccess;
}
