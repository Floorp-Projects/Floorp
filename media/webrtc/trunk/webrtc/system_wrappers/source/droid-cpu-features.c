/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <sys/system_properties.h>
#ifdef __arm__
#include <machine/cpu-features.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "webrtc/system_wrappers/source/droid-cpu-features.h"

static  pthread_once_t     g_once;
static  AndroidCpuFamily   g_cpuFamily;
static  uint64_t           g_cpuFeatures;
static  int                g_cpuCount;

static const int  android_cpufeatures_debug = 0;

#ifdef __arm__
#  define DEFAULT_CPU_FAMILY  ANDROID_CPU_FAMILY_ARM
#elif defined __i386__
#  define DEFAULT_CPU_FAMILY  ANDROID_CPU_FAMILY_X86
#else
#  define DEFAULT_CPU_FAMILY  ANDROID_CPU_FAMILY_UNKNOWN
#endif

#define  D(...) \
    do { \
        if (android_cpufeatures_debug) { \
            printf(__VA_ARGS__); fflush(stdout); \
        } \
    } while (0)

#ifdef __i386__
static __inline__ void x86_cpuid(int func, int values[4])
{
    int a, b, c, d;
    /* We need to preserve ebx since we're compiling PIC code */
    /* this means we can't use "=b" for the second output register */
    __asm__ __volatile__ ( \
      "push %%ebx\n"
      "cpuid\n" \
      "mov %1, %%ebx\n"
      "pop %%ebx\n"
      : "=a" (a), "=r" (b), "=c" (c), "=d" (d) \
      : "a" (func) \
    );
    values[0] = a;
    values[1] = b;
    values[2] = c;
    values[3] = d;
}
#endif

/* Read the content of /proc/cpuinfo into a user-provided buffer.
 * Return the length of the data, or -1 on error. Does *not*
 * zero-terminate the content. Will not read more
 * than 'buffsize' bytes.
 */
static int
read_file(const char*  pathname, char*  buffer, size_t  buffsize)
{
    int  fd, len;

    fd = open(pathname, O_RDONLY);
    if (fd < 0)
        return -1;

    do {
        len = read(fd, buffer, buffsize);
    } while (len < 0 && errno == EINTR);

    close(fd);

    return len;
}

/* Extract the content of a the first occurence of a given field in
 * the content of /proc/cpuinfo and return it as a heap-allocated
 * string that must be freed by the caller.
 *
 * Return NULL if not found
 */
static char*
extract_cpuinfo_field(char* buffer, int buflen, const char* field)
{
    int  fieldlen = strlen(field);
    char* bufend = buffer + buflen;
    char* result = NULL;
    int len, ignore;
    const char *p, *q;

    /* Look for first field occurence, and ensures it starts the line.
     */
    p = buffer;
    bufend = buffer + buflen;
    for (;;) {
        p = memmem(p, bufend-p, field, fieldlen);
        if (p == NULL)
            goto EXIT;

        if (p == buffer || p[-1] == '\n')
            break;

        p += fieldlen;
    }

    /* Skip to the first column followed by a space */
    p += fieldlen;
    p  = memchr(p, ':', bufend-p);
    if (p == NULL || p[1] != ' ')
        goto EXIT;

    /* Find the end of the line */
    p += 2;
    q = memchr(p, '\n', bufend-p);
    if (q == NULL)
        q = bufend;

    /* Copy the line into a heap-allocated buffer */
    len = q-p;
    result = malloc(len+1);
    if (result == NULL)
        goto EXIT;

    memcpy(result, p, len);
    result[len] = '\0';

EXIT:
    return result;
}

/* Count the number of occurences of a given field prefix in /proc/cpuinfo.
 */
static int
count_cpuinfo_field(char* buffer, int buflen, const char* field)
{
    int fieldlen = strlen(field);
    const char* p = buffer;
    const char* bufend = buffer + buflen;
    const char* q;
    int count = 0;

    for (;;) {
        const char* q;

        p = memmem(p, bufend-p, field, fieldlen);
        if (p == NULL)
            break;

        /* Ensure that the field is at the start of a line */
        if (p > buffer && p[-1] != '\n') {
            p += fieldlen;
            continue;
        }


        /* skip any whitespace */
        q = p + fieldlen;
        while (q < bufend && (*q == ' ' || *q == '\t'))
            q++;

        /* we must have a colon now */
        if (q < bufend && *q == ':') {
            count += 1;
            q ++;
        }
        p = q;
    }

    return count;
}

/* Like strlen(), but for constant string literals */
#define STRLEN_CONST(x)  ((sizeof(x)-1)


/* Checks that a space-separated list of items contains one given 'item'.
 * Returns 1 if found, 0 otherwise.
 */
static int
has_list_item(const char* list, const char* item)
{
    const char*  p = list;
    int itemlen = strlen(item);

    if (list == NULL)
        return 0;

    while (*p) {
        const char*  q;

        /* skip spaces */
        while (*p == ' ' || *p == '\t')
            p++;

        /* find end of current list item */
        q = p;
        while (*q && *q != ' ' && *q != '\t')
            q++;

        if (itemlen == q-p && !memcmp(p, item, itemlen))
            return 1;

        /* skip to next item */
        p = q;
    }
    return 0;
}


static void
android_cpuInit(void)
{
    char cpuinfo[4096];
    int  cpuinfo_len;

    g_cpuFamily   = DEFAULT_CPU_FAMILY;
    g_cpuFeatures = 0;
    g_cpuCount    = 1;

    cpuinfo_len = read_file("/proc/cpuinfo", cpuinfo, sizeof cpuinfo);
    D("cpuinfo_len is (%d):\n%.*s\n", cpuinfo_len,
      cpuinfo_len >= 0 ? cpuinfo_len : 0, cpuinfo);

    if (cpuinfo_len < 0)  /* should not happen */ {
        return;
    }

    /* Count the CPU cores, the value may be 0 for single-core CPUs */
    g_cpuCount = count_cpuinfo_field(cpuinfo, cpuinfo_len, "processor");
    if (g_cpuCount == 0) {
        g_cpuCount = count_cpuinfo_field(cpuinfo, cpuinfo_len, "Processor");
        if (g_cpuCount == 0) {
            g_cpuCount = 1;
        }
    }

    D("found cpuCount = %d\n", g_cpuCount);

#ifdef __ARM_ARCH__
    {
        char*  features = NULL;
        char*  architecture = NULL;

        /* Extract architecture from the "CPU Architecture" field.
         * The list is well-known, unlike the the output of
         * the 'Processor' field which can vary greatly.
         *
         * See the definition of the 'proc_arch' array in
         * $KERNEL/arch/arm/kernel/setup.c and the 'c_show' function in
         * same file.
         */
        char* cpuArch = extract_cpuinfo_field(cpuinfo, cpuinfo_len, "CPU architecture");

        if (cpuArch != NULL) {
            char*  end;
            long   archNumber;
            int    hasARMv7 = 0;

            D("found cpuArch = '%s'\n", cpuArch);

            /* read the initial decimal number, ignore the rest */
            archNumber = strtol(cpuArch, &end, 10);

            /* Here we assume that ARMv8 will be upwards compatible with v7
                * in the future. Unfortunately, there is no 'Features' field to
                * indicate that Thumb-2 is supported.
                */
            if (end > cpuArch && archNumber >= 7) {
                hasARMv7 = 1;
            }

            /* Unfortunately, it seems that certain ARMv6-based CPUs
             * report an incorrect architecture number of 7!
             *
             * See http://code.google.com/p/android/issues/detail?id=10812
             *
             * We try to correct this by looking at the 'elf_format'
             * field reported by the 'Processor' field, which is of the
             * form of "(v7l)" for an ARMv7-based CPU, and "(v6l)" for
             * an ARMv6-one.
             */
            if (hasARMv7) {
                char* cpuProc = extract_cpuinfo_field(cpuinfo, cpuinfo_len,
                                                      "Processor");
                if (cpuProc != NULL) {
                    D("found cpuProc = '%s'\n", cpuProc);
                    if (has_list_item(cpuProc, "(v6l)")) {
                        D("CPU processor and architecture mismatch!!\n");
                        hasARMv7 = 0;
                    }
                    free(cpuProc);
                }
            }

            if (hasARMv7) {
                g_cpuFeatures |= ANDROID_CPU_ARM_FEATURE_ARMv7;
            }

            /* The LDREX / STREX instructions are available from ARMv6 */
            if (archNumber >= 6) {
                g_cpuFeatures |= ANDROID_CPU_ARM_FEATURE_LDREX_STREX;
            }

            free(cpuArch);
        }

        /* Extract the list of CPU features from 'Features' field */
        char* cpuFeatures = extract_cpuinfo_field(cpuinfo, cpuinfo_len, "Features");

        if (cpuFeatures != NULL) {

            D("found cpuFeatures = '%s'\n", cpuFeatures);

            if (has_list_item(cpuFeatures, "vfpv3"))
                g_cpuFeatures |= ANDROID_CPU_ARM_FEATURE_VFPv3;

            else if (has_list_item(cpuFeatures, "vfpv3d16"))
                g_cpuFeatures |= ANDROID_CPU_ARM_FEATURE_VFPv3;

            if (has_list_item(cpuFeatures, "neon")) {
                /* Note: Certain kernels only report neon but not vfpv3
                    *       in their features list. However, ARM mandates
                    *       that if Neon is implemented, so must be VFPv3
                    *       so always set the flag.
                    */
                g_cpuFeatures |= ANDROID_CPU_ARM_FEATURE_NEON |
                                 ANDROID_CPU_ARM_FEATURE_VFPv3;
            }
            free(cpuFeatures);
        }
    }
#endif /* __ARM_ARCH__ */

#ifdef __i386__
    g_cpuFamily = ANDROID_CPU_FAMILY_X86;

    int regs[4];

/* According to http://en.wikipedia.org/wiki/CPUID */
#define VENDOR_INTEL_b  0x756e6547
#define VENDOR_INTEL_c  0x6c65746e
#define VENDOR_INTEL_d  0x49656e69

    x86_cpuid(0, regs);
    int vendorIsIntel = (regs[1] == VENDOR_INTEL_b &&
                         regs[2] == VENDOR_INTEL_c &&
                         regs[3] == VENDOR_INTEL_d);

    x86_cpuid(1, regs);
    if ((regs[2] & (1 << 9)) != 0) {
        g_cpuFeatures |= ANDROID_CPU_X86_FEATURE_SSSE3;
    }
    if ((regs[2] & (1 << 23)) != 0) {
        g_cpuFeatures |= ANDROID_CPU_X86_FEATURE_POPCNT;
    }
    if (vendorIsIntel && (regs[2] & (1 << 22)) != 0) {
        g_cpuFeatures |= ANDROID_CPU_X86_FEATURE_MOVBE;
    }
#endif
}


AndroidCpuFamily
android_getCpuFamily(void)
{
    pthread_once(&g_once, android_cpuInit);
    return g_cpuFamily;
}


uint64_t
android_getCpuFeatures(void)
{
    pthread_once(&g_once, android_cpuInit);
    return g_cpuFeatures;
}


int
android_getCpuCount(void)
{
    pthread_once(&g_once, android_cpuInit);
    return g_cpuCount;
}
