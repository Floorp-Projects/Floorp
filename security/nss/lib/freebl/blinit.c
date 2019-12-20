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

#if defined(_WIN64) && defined(__aarch64__)
#include <windows.h>
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
static PRBool ppc_crypto_support_ = PR_FALSE;

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
#if defined(__aarch64__) || defined(__arm__)
#ifndef __has_include
#define __has_include(x) 0
#endif
#if (__has_include(<sys/auxv.h>) || defined(__linux__)) && \
    defined(__GNUC__) && __GNUC__ >= 2 && defined(__ELF__)
/* This might be conflict with host compiler */
#if !defined(__ANDROID__)
#include <sys/auxv.h>
#endif
extern unsigned long getauxval(unsigned long type) __attribute__((weak));
#else
static unsigned long (*getauxval)(unsigned long) = NULL;
#endif /* defined(__GNUC__) && __GNUC__ >= 2 && defined(__ELF__)*/

#ifndef AT_HWCAP2
#define AT_HWCAP2 26
#endif
#ifndef AT_HWCAP
#define AT_HWCAP 16
#endif

#endif /* defined(__aarch64__) || defined(__arm__) */
/* clang-format on */

#if defined(__aarch64__)
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
    char *disable_pmull = PR_GetEnvSecure("NSS_DISABLE_PMULL");
#if defined(_WIN64)
    BOOL arm_crypto_support = IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
    arm_aes_support_ = arm_crypto_support && disable_hw_aes == NULL;
    arm_pmull_support_ = arm_crypto_support && disable_pmull == NULL;
    arm_sha1_support_ = arm_crypto_support;
    arm_sha2_support_ = arm_crypto_support;
#else
    if (getauxval) {
        long hwcaps = getauxval(AT_HWCAP);
        arm_aes_support_ = hwcaps & HWCAP_AES && disable_hw_aes == NULL;
        arm_pmull_support_ = hwcaps & HWCAP_PMULL && disable_pmull == NULL;
        arm_sha1_support_ = hwcaps & HWCAP_SHA1;
        arm_sha2_support_ = hwcaps & HWCAP_SHA2;
    }
#endif
    /* aarch64 must support NEON. */
    arm_neon_support_ = disable_arm_neon == NULL;
}
#endif /* defined(__aarch64__) */

#if defined(__arm__)
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

PRBool
GetNeonSupport()
{
    char *disable_arm_neon = PR_GetEnvSecure("NSS_DISABLE_ARM_NEON");
    if (disable_arm_neon) {
        return PR_FALSE;
    }
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    // Compiler generates NEON instruction as default option.
    // If no getauxval, compiler generate NEON instruction by default,
    // we should allow NOEN support.
    return PR_TRUE;
#elif !defined(__ANDROID__)
    // Android's cpu-features.c detects features by the following logic
    //
    // - Call getauxval(AT_HWCAP)
    // - Parse /proc/self/auxv if getauxval is nothing or returns 0
    // - Parse /proc/cpuinfo if both cannot detect features
    //
    // But we don't use it for Android since Android document
    // (https://developer.android.com/ndk/guides/cpu-features) says
    // one problem with AT_HWCAP sometimes devices (Nexus 4 and emulator)
    // are mistaken for IDIV.
    if (getauxval) {
        return (getauxval(AT_HWCAP) & HWCAP_NEON);
    }
#endif /* defined(__ARM_NEON) || defined(__ARM_NEON__) */
    return PR_FALSE;
}

#ifdef __linux__
static long
ReadCPUInfoForHWCAP2()
{
    FILE *cpuinfo;
    char buf[512];
    char *p;
    long hwcap2 = 0;

    cpuinfo = fopen("/proc/cpuinfo", "r");
    if (!cpuinfo) {
        return 0;
    }
    while (fgets(buf, 511, cpuinfo)) {
        if (!memcmp(buf, "Features", 8)) {
            p = strstr(buf, " aes");
            if (p && (p[4] == ' ' || p[4] == '\n')) {
                hwcap2 |= HWCAP2_AES;
            }
            p = strstr(buf, " sha1");
            if (p && (p[5] == ' ' || p[5] == '\n')) {
                hwcap2 |= HWCAP2_SHA1;
            }
            p = strstr(buf, " sha2");
            if (p && (p[5] == ' ' || p[5] == '\n')) {
                hwcap2 |= HWCAP2_SHA2;
            }
            p = strstr(buf, " pmull");
            if (p && (p[6] == ' ' || p[6] == '\n')) {
                hwcap2 |= HWCAP2_PMULL;
            }
            break;
        }
    }

    fclose(cpuinfo);
    return hwcap2;
}
#endif /* __linux__ */

void
CheckARMSupport()
{
    char *disable_hw_aes = PR_GetEnvSecure("NSS_DISABLE_HW_AES");
    if (getauxval) {
        // Android's cpu-features.c uses AT_HWCAP2 for newer features.
        // AT_HWCAP2 is implemented on newer devices / kernel, so we can trust
        // it since cpu-features.c doesn't have workaround / fallback.
        // Also, AT_HWCAP2 is supported by glibc 2.18+ on Linux/arm, If
        // AT_HWCAP2 isn't supported by glibc or Linux kernel, getauxval will
        // returns 0.
        long hwcaps = getauxval(AT_HWCAP2);
#ifdef __linux__
        if (!hwcaps) {
            // Some ARMv8 devices may not implement AT_HWCAP2. So we also
            // read /proc/cpuinfo if AT_HWCAP2 is 0.
            hwcaps = ReadCPUInfoForHWCAP2();
        }
#endif
        arm_aes_support_ = hwcaps & HWCAP2_AES && disable_hw_aes == NULL;
        arm_pmull_support_ = hwcaps & HWCAP2_PMULL;
        arm_sha1_support_ = hwcaps & HWCAP2_SHA1;
        arm_sha2_support_ = hwcaps & HWCAP2_SHA2;
    }
    arm_neon_support_ = GetNeonSupport();
}
#endif /* defined(__arm__) */

// Enable when Firefox can use it for Android API 16 and 17.
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
PRBool
ppc_crypto_support()
{
    return ppc_crypto_support_;
}

#if defined(__powerpc__)

#include <sys/auxv.h>

// Defines from cputable.h in Linux kernel - PPC, letting us build on older kernels
#ifndef PPC_FEATURE2_VEC_CRYPTO
#define PPC_FEATURE2_VEC_CRYPTO 0x02000000
#endif

static void
CheckPPCSupport()
{
    char *disable_hw_crypto = PR_GetEnvSecure("NSS_DISABLE_PPC_GHASH");

    long hwcaps = getauxval(AT_HWCAP2);

    ppc_crypto_support_ = hwcaps & PPC_FEATURE2_VEC_CRYPTO && disable_hw_crypto == NULL;
}

#endif /* __powerpc__ */

static PRStatus
FreeblInit(void)
{
#ifdef NSS_X86_OR_X64
    CheckX86CPUSupport();
#elif (defined(__aarch64__) || defined(__arm__))
    CheckARMSupport();
#elif (defined(__powerpc__))
    CheckPPCSupport();
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
