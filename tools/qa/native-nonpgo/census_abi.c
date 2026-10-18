/* SPDX-License-Identifier: MPL-2.0 */
/* Header-derived layout only. Process snapshots stay in the Python caller. */
#define _DARWIN_C_SOURCE 1
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#define SIGNED(member) _Generic(((struct kinfo_proc *)0)->member, \
    char: CHAR_MIN < 0, signed char: 1, short: 1, int: 1, long: 1, long long: 1, default: 0)
#define FIELD(label, member) \
    printf("\"%s\":{\"offset\":%zu,\"bytes\":%zu,\"signed\":%s}", label, \
           offsetof(struct kinfo_proc, member), \
           sizeof(((struct kinfo_proc *)0)->member), SIGNED(member) ? "true" : "false")

int main(void) {
    const uint16_t order = 1;
    _Static_assert(sizeof(void *) == 8, "LP64 required");
    _Static_assert(sizeof(pid_t) == 4, "32-bit PID required");
    _Static_assert(sizeof(uid_t) == 4, "32-bit UID required");
    _Static_assert(KERN_PROC_PID == 1, "self selector required");
    _Static_assert(KERN_PROC_PGRP == 2, "group selector required");
    printf("{\"schema\":1,\"pointerBytes\":%zu,\"recordBytes\":%zu,"
           "\"byteOrder\":\"%s\",\"mib\":[%d,%d,%d,0],\"fields\":{",
           sizeof(void *), sizeof(struct kinfo_proc),
           *(const unsigned char *)&order == 1 ? "little" : "big",
           CTL_KERN, KERN_PROC, KERN_PROC_ALL);
    FIELD("pid", kp_proc.p_pid); printf(",");
    FIELD("startSec", kp_proc.p_starttime.tv_sec); printf(",");
    FIELD("startUsec", kp_proc.p_starttime.tv_usec); printf(",");
    FIELD("state", kp_proc.p_stat); printf(",");
    FIELD("uid", kp_eproc.e_ucred.cr_uid); printf(",");
    FIELD("ruid", kp_eproc.e_pcred.p_ruid);
    printf("}}\n");
    return 0;
}
