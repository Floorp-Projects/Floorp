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
static PRBool ssse3_support_ = PR_FALSE;
static PRBool arm_neon_support_ = PR_FALSE;
static PRBool arm_aes_support_ = PR_FALSE;
static PRBool arm_sha1_support_ = PR_FALSE;
static PRBool arm_sha2_support_ = PR_FALSE;
static PRBool arm_pmull_support_ = PR_FALSE;

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
#define ECX_SSSE3 (1 << 9)
#define AVX_BITS (ECX_XSAVE | ECX_OSXSAVE | ECX_AVX)

void
CheckX86CPUSupport()
{
    unsigned long eax, ebx, ecx, edx;
    char *disable_hw_aes = PR_GetEnvSecure("NSS_DISABLE_HW_AES");
    char *disable_pclmul = PR_GetEnvSecure("NSS_DISABLE_PCLMUL");
    char *disable_avx = PR_GetEnvSecure("NSS_DISABLE_AVX");
    char *disable_ssse3 = PR_GetEnvSecure("NSS_DISABLE_SSSE3");
    freebl_cpuid(1, &eax, &ebx, &ecx, &edx);
    aesni_support_ = (PRBool)((ecx & ECX_AESNI) != 0 && disable_hw_aes == NULL);
    clmul_support_ = (PRBool)((ecx & ECX_CLMUL) != 0 && disable_pclmul == NULL);
    /* For AVX we check AVX, OSXSAVE, and XSAVE
     * as well as XMM and YMM state. */
    avx_support_ = (PRBool)((ecx & AVX_BITS) == AVX_BITS) && check_xcr0_ymm() &&
                   disable_avx == NULL;
    ssse3_support_ = (PRBool)((ecx & ECX_SSSE3) != 0 &&
                              disable_ssse3 == NULL);
}
#endif /* NSS_X86_OR_X64 */

/* clang-format off */
#if (defined(__aarch64__) || defined(__arm__)) && !defined(__ANDROID__)
#ifndef __has_include
#define __has_include(x) 0
#endif
#if (__has_include(<sys/auxv.h>) || defined(__linux__)) && \
    defined(__GNUC__) && __GNUC__ >= 2 && defined(__ELF__)
#include <sys/auxv.h>
extern unsigned long getauxval(unsigned long type) __attribute__((weak));
#else
static unsigned long (*getauxval)(unsigned long) = NULL;
#define AT_HWCAP2 0
#define AT_HWCAP 0
#endif /* defined(__GNUC__) && __GNUC__ >= 2 && defined(__ELF__)*/
#endif /* (defined(__aarch64__) || defined(__arm__)) && !defined(__ANDROID__) */
/* clang-format on */

#if defined(__aarch64__) && !defined(__ANDROID__)
// Defines from hwcap.h in Linux kernel - ARM64
#ifndef HWCAP_AES
#define HWCAP_AES (1 << 3)
#endif
#ifndef HWCAP_PMULL
#define HWCAP_PMULL (1 << 4)
#endif
#ifndef HWCAP_SHA1
#define HWCAP_SHA1 (1 << 5)
#endif
#ifndef HWCAP_SHA2
#define HWCAP_SHA2 (1 << 6)
#endif

void
CheckARMSupport()
{
    char *disable_arm_neon = PR_GetEnvSecure("NSS_DISABLE_ARM_NEON");
    char *disable_hw_aes = PR_GetEnvSecure("NSS_DISABLE_HW_AES");
    if (getauxval) {
        long hwcaps = getauxval(AT_HWCAP);
        arm_aes_support_ = hwcaps & HWCAP_AES && disable_hw_aes == NULL;
        arm_pmull_support_ = hwcaps & HWCAP_PMULL;
        arm_sha1_support_ = hwcaps & HWCAP_SHA1;
        arm_sha2_support_ = hwcaps & HWCAP_SHA2;
    }
    /* aarch64 must support NEON. */
    arm_neon_support_ = disable_arm_neon == NULL;
}
#endif /* defined(__aarch64__) && !defined(__ANDROID__) */

#if defined(__arm__) && !defined(__ANDROID__)
// Defines from hwcap.h in Linux kernel - ARM
/*
 * HWCAP flags - for elf_hwcap (in kernel) and AT_HWCAP
 */
#ifndef HWCAP_NEON
#define HWCAP_NEON (1 << 12)
#endif

/*
 * HWCAP2 flags - for elf_hwcap2 (in kernel) and AT_HWCAP2
 */
#ifndef HWCAP2_AES
#define HWCAP2_AES (1 << 0)
#endif
#ifndef HWCAP2_PMULL
#define HWCAP2_PMULL (1 << 1)
#endif
#ifndef HWCAP2_SHA1
#define HWCAP2_SHA1 (1 << 2)
#endif
#ifndef HWCAP2_SHA2
#define HWCAP2_SHA2 (1 << 3)
#endif

void
CheckARMSupport()
{
    char *disable_arm_neon = PR_GetEnvSecure("NSS_DISABLE_ARM_NEON");
    char *disable_hw_aes = PR_GetEnvSecure("NSS_DISABLE_HW_AES");
    if (getauxval) {
        long hwcaps = getauxval(AT_HWCAP2);
        arm_aes_support_ = hwcaps & HWCAP2_AES && disable_hw_aes == NULL;
        arm_pmull_support_ = hwcaps & HWCAP2_PMULL;
        arm_sha1_support_ = hwcaps & HWCAP2_SHA1;
        arm_sha2_support_ = hwcaps & HWCAP2_SHA2;
        arm_neon_support_ = hwcaps & HWCAP_NEON && disable_arm_neon == NULL;
    }
}
#endif /* defined(__arm__) && !defined(__ANDROID__) */

// Enable when Firefox can use it.
// #if defined(__ANDROID__) && (defined(__arm__) || defined(__aarch64__))
// #include <cpu-features.h>
// void
// CheckARMSupport()
// {
//     char *disable_arm_neon = PR_GetEnvSecure("NSS_DISABLE_ARM_NEON");
//     char *disable_hw_aes = PR_GetEnvSecure("NSS_DISABLE_HW_AES");
//     AndroidCpuFamily family = android_getCpuFamily();
//     uint64_t features = android_getCpuFeatures();
//     if (family == ANDROID_CPU_FAMILY_ARM64) {
//         arm_aes_support_ = features & ANDROID_CPU_ARM64_FEATURE_AES &&
//                            disable_hw_aes == NULL;
//         arm_pmull_support_ = features & ANDROID_CPU_ARM64_FEATURE_PMULL;
//         arm_sha1_support_ = features & ANDROID_CPU_ARM64_FEATURE_SHA1;
//         arm_sha2_support_ = features & ANDROID_CPU_ARM64_FEATURE_SHA2;
//         arm_neon_support_ = disable_arm_neon == NULL;
//     }
//     if (family == ANDROID_CPU_FAMILY_ARM) {
//         arm_aes_support_ = features & ANDROID_CPU_ARM_FEATURE_AES &&
//                            disable_hw_aes == NULL;
//         arm_pmull_support_ = features & ANDROID_CPU_ARM_FEATURE_PMULL;
//         arm_sha1_support_ = features & ANDROID_CPU_ARM_FEATURE_SHA1;
//         arm_sha2_support_ = features & ANDROID_CPU_ARM_FEATURE_SHA2;
//         arm_neon_support_ = hwcaps & ANDROID_CPU_ARM_FEATURE_NEON &&
//                             disable_arm_neon == NULL;
//     }
// }
// #endif /* defined(__ANDROID__) && (defined(__arm__) || defined(__aarch64__)) */

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
PRBool
ssse3_support()
{
    return ssse3_support_;
}
PRBool
arm_neon_support()
{
    return arm_neon_support_;
}
PRBool
arm_aes_support()
{
    return arm_aes_support_;
}
PRBool
arm_pmull_support()
{
    return arm_pmull_support_;
}
PRBool
arm_sha1_support()
{
    return arm_sha1_support_;
}
PRBool
arm_sha2_support()
{
    return arm_sha2_support_;
}

static PRStatus
FreeblInit(void)
{
#ifdef NSS_X86_OR_X64
    CheckX86CPUSupport();
#elif (defined(__aarch64__) || defined(__arm__)) && !defined(__ANDROID__)
    CheckARMSupport();
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
