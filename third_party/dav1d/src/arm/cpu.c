/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Janne Grunau
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "common/attributes.h"

#include "src/arm/cpu.h"

#if defined(HAVE_GETAUXVAL) || defined(HAVE_ELF_AUX_INFO)
#include <sys/auxv.h>

#if ARCH_AARCH64

#define HWCAP_AARCH64_ASIMDDP (1 << 20)
#define HWCAP_AARCH64_SVE     (1 << 22)
#define HWCAP2_AARCH64_SVE2   (1 << 1)
#define HWCAP2_AARCH64_I8MM   (1 << 13)

COLD unsigned dav1d_get_cpu_flags_arm(void) {
#ifdef HAVE_GETAUXVAL
    unsigned long hw_cap = getauxval(AT_HWCAP);
    unsigned long hw_cap2 = getauxval(AT_HWCAP2);
#else
    unsigned long hw_cap = 0;
    unsigned long hw_cap2 = 0;
    elf_aux_info(AT_HWCAP, &hw_cap, sizeof(hw_cap));
    elf_aux_info(AT_HWCAP2, &hw_cap2, sizeof(hw_cap2));
#endif

    unsigned flags = DAV1D_ARM_CPU_FLAG_NEON;
    flags |= (hw_cap & HWCAP_AARCH64_ASIMDDP) ? DAV1D_ARM_CPU_FLAG_DOTPROD : 0;
    flags |= (hw_cap2 & HWCAP2_AARCH64_I8MM) ? DAV1D_ARM_CPU_FLAG_I8MM : 0;
    flags |= (hw_cap & HWCAP_AARCH64_SVE) ? DAV1D_ARM_CPU_FLAG_SVE : 0;
    flags |= (hw_cap2 & HWCAP2_AARCH64_SVE2) ? DAV1D_ARM_CPU_FLAG_SVE2 : 0;
    return flags;
}
#else  /* !ARCH_AARCH64 */

#ifndef HWCAP_ARM_NEON
#define HWCAP_ARM_NEON    (1 << 12)
#endif
#define HWCAP_ARM_ASIMDDP (1 << 24)
#define HWCAP_ARM_I8MM    (1 << 27)

COLD unsigned dav1d_get_cpu_flags_arm(void) {
#ifdef HAVE_GETAUXVAL
    unsigned long hw_cap = getauxval(AT_HWCAP);
#else
    unsigned long hw_cap = 0;
    elf_aux_info(AT_HWCAP, &hw_cap, sizeof(hw_cap));
#endif

    unsigned flags = (hw_cap & HWCAP_ARM_NEON) ? DAV1D_ARM_CPU_FLAG_NEON : 0;
    flags |= (hw_cap & HWCAP_ARM_ASIMDDP) ? DAV1D_ARM_CPU_FLAG_DOTPROD : 0;
    flags |= (hw_cap & HWCAP_ARM_I8MM) ? DAV1D_ARM_CPU_FLAG_I8MM : 0;
    return flags;
}
#endif /* ARCH_AARCH64 */

#elif defined(__APPLE__)
#include <sys/sysctl.h>

static int have_feature(const char *feature) {
    int supported = 0;
    size_t size = sizeof(supported);
    if (sysctlbyname(feature, &supported, &size, NULL, 0) != 0) {
        return 0;
    }
    return supported;
}

COLD unsigned dav1d_get_cpu_flags_arm(void) {
    unsigned flags = DAV1D_ARM_CPU_FLAG_NEON;
    if (have_feature("hw.optional.arm.FEAT_DotProd"))
        flags |= DAV1D_ARM_CPU_FLAG_DOTPROD;
    if (have_feature("hw.optional.arm.FEAT_I8MM"))
        flags |= DAV1D_ARM_CPU_FLAG_I8MM;
    /* No SVE and SVE2 feature detection available on Apple platforms. */
    return flags;
}

#elif defined(_WIN32)
#include <windows.h>

COLD unsigned dav1d_get_cpu_flags_arm(void) {
    unsigned flags = DAV1D_ARM_CPU_FLAG_NEON;
#ifdef PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE
    if (IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE))
        flags |= DAV1D_ARM_CPU_FLAG_DOTPROD;
#endif
    /* No I8MM or SVE feature detection available on Windows at the time of
     * writing. */
    return flags;
}

#elif defined(__ANDROID__)
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static unsigned parse_proc_cpuinfo(const char *flag) {
    FILE *file = fopen("/proc/cpuinfo", "r");
    if (!file)
        return 0;

    char line_buffer[120];
    const char *line;

    size_t flaglen = strlen(flag);
    while ((line = fgets(line_buffer, sizeof(line_buffer), file))) {
        // check all occurances as whole words
        const char *found = line;
        while ((found = strstr(found, flag))) {
            if ((found == line_buffer || !isgraph(found[-1])) &&
                (isspace(found[flaglen]) || feof(file))) {
                fclose(file);
                return 1;
            }
            found += flaglen;
        }
        // if line is incomplete seek back to avoid splitting the search
        // string into two buffers
        if (!strchr(line, '\n') && strlen(line) > flaglen) {
            // use fseek since the 64 bit fseeko is only available since
            // Android API level 24 and meson defines _FILE_OFFSET_BITS
            // by default 64
            if (fseek(file, -flaglen, SEEK_CUR))
                break;
        }
    }

    fclose(file);

    return 0;
}

COLD unsigned dav1d_get_cpu_flags_arm(void) {
    unsigned flags = parse_proc_cpuinfo("neon") ? DAV1D_ARM_CPU_FLAG_NEON : 0;
    flags |= parse_proc_cpuinfo("asimd") ? DAV1D_ARM_CPU_FLAG_NEON : 0;
    flags |= parse_proc_cpuinfo("asimddp") ? DAV1D_ARM_CPU_FLAG_DOTPROD : 0;
    flags |= parse_proc_cpuinfo("i8mm") ? DAV1D_ARM_CPU_FLAG_I8MM : 0;
#if ARCH_AARCH64
    flags |= parse_proc_cpuinfo("sve") ? DAV1D_ARM_CPU_FLAG_SVE : 0;
    flags |= parse_proc_cpuinfo("sve2") ? DAV1D_ARM_CPU_FLAG_SVE2 : 0;
#endif /* ARCH_AARCH64 */
    return flags;
}

#else  /* Unsupported OS */

COLD unsigned dav1d_get_cpu_flags_arm(void) {
    return 0;
}

#endif
