/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file is used by not only Linux but also other glibc systems
 * such as GNU/Hurd and GNU/k*BSD.
 */

#ifndef nspr_linux_defs_h___
#define nspr_linux_defs_h___

#include "prthread.h"

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH  "linux"
#define _PR_SI_SYSNAME  "LINUX"
#ifdef __powerpc64__
#define _PR_SI_ARCHITECTURE "ppc64"
#elif defined(__powerpc__)
#define _PR_SI_ARCHITECTURE "ppc"
#elif defined(__alpha)
#define _PR_SI_ARCHITECTURE "alpha"
#elif defined(__ia64__)
#define _PR_SI_ARCHITECTURE "ia64"
#elif defined(__x86_64__)
#define _PR_SI_ARCHITECTURE "x86-64"
#elif defined(__mc68000__)
#define _PR_SI_ARCHITECTURE "m68k"
#elif defined(__sparc__) && defined(__arch64__)
#define _PR_SI_ARCHITECTURE "sparc64"
#elif defined(__sparc__)
#define _PR_SI_ARCHITECTURE "sparc"
#elif defined(__i386__)
#define _PR_SI_ARCHITECTURE "x86"
#elif defined(__mips__)
#define _PR_SI_ARCHITECTURE "mips"
#elif defined(__arm__)
#define _PR_SI_ARCHITECTURE "arm"
#elif defined(__aarch64__)
#define _PR_SI_ARCHITECTURE "aarch64"
#elif defined(__hppa__)
#define _PR_SI_ARCHITECTURE "hppa"
#elif defined(__s390x__)
#define _PR_SI_ARCHITECTURE "s390x"
#elif defined(__s390__)
#define _PR_SI_ARCHITECTURE "s390"
#elif defined(__sh__)
#define _PR_SI_ARCHITECTURE "sh"
#elif defined(__avr32__)
#define _PR_SI_ARCHITECTURE "avr32"
#elif defined(__m32r__)
#define _PR_SI_ARCHITECTURE "m32r"
#elif defined(__or1k__)
#define _PR_SI_ARCHITECTURE "or1k"
#elif defined(__riscv) && (__riscv_xlen == 32)
#define _PR_SI_ARCHITECTURE "riscv32"
#elif defined(__riscv) && (__riscv_xlen == 64)
#define _PR_SI_ARCHITECTURE "riscv64"
#elif defined(__e2k__)
#define _PR_SI_ARCHITECTURE "e2k"
#elif defined(__arc__)
#define _PR_SI_ARCHITECTURE "arc"
#elif defined(__nios2__)
#define _PR_SI_ARCHITECTURE "nios2"
#elif defined(__microblaze__)
#define _PR_SI_ARCHITECTURE "microblaze"
#elif defined(__nds32__)
#define _PR_SI_ARCHITECTURE "nds32"
#elif defined(__xtensa__)
#define _PR_SI_ARCHITECTURE "xtensa"
#else
#error "Unknown CPU architecture"
#endif
#define PR_DLL_SUFFIX       ".so"

#define _PR_VMBASE              0x30000000
#define _PR_STACK_VMBASE    0x50000000
#define _MD_DEFAULT_STACK_SIZE  65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#if defined(__aarch64__) || defined(__mips__)
#define _MD_MINIMUM_STACK_SIZE  0x20000
#endif

#undef  HAVE_STACK_GROWING_UP

/*
 * Elf linux supports dl* functions
 */
#define HAVE_DLL
#define USE_DLFCN
#if defined(ANDROID)
#define NO_DLOPEN_NULL
#endif

#if defined(__FreeBSD_kernel__) || defined(__GNU__)
#define _PR_HAVE_SOCKADDR_LEN
#endif

#if defined(__i386__)
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
extern PRInt32 _PR_x86_AtomicIncrement(PRInt32 *val);
#define _MD_ATOMIC_INCREMENT          _PR_x86_AtomicIncrement
extern PRInt32 _PR_x86_AtomicDecrement(PRInt32 *val);
#define _MD_ATOMIC_DECREMENT          _PR_x86_AtomicDecrement
extern PRInt32 _PR_x86_AtomicAdd(PRInt32 *ptr, PRInt32 val);
#define _MD_ATOMIC_ADD                _PR_x86_AtomicAdd
extern PRInt32 _PR_x86_AtomicSet(PRInt32 *val, PRInt32 newval);
#define _MD_ATOMIC_SET                _PR_x86_AtomicSet
#endif

#if defined(__ia64__)
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
extern PRInt32 _PR_ia64_AtomicIncrement(PRInt32 *val);
#define _MD_ATOMIC_INCREMENT          _PR_ia64_AtomicIncrement
extern PRInt32 _PR_ia64_AtomicDecrement(PRInt32 *val);
#define _MD_ATOMIC_DECREMENT          _PR_ia64_AtomicDecrement
extern PRInt32 _PR_ia64_AtomicAdd(PRInt32 *ptr, PRInt32 val);
#define _MD_ATOMIC_ADD                _PR_ia64_AtomicAdd
extern PRInt32 _PR_ia64_AtomicSet(PRInt32 *val, PRInt32 newval);
#define _MD_ATOMIC_SET                _PR_ia64_AtomicSet
#endif

#if defined(__x86_64__)
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
extern PRInt32 _PR_x86_64_AtomicIncrement(PRInt32 *val);
#define _MD_ATOMIC_INCREMENT          _PR_x86_64_AtomicIncrement
extern PRInt32 _PR_x86_64_AtomicDecrement(PRInt32 *val);
#define _MD_ATOMIC_DECREMENT          _PR_x86_64_AtomicDecrement
extern PRInt32 _PR_x86_64_AtomicAdd(PRInt32 *ptr, PRInt32 val);
#define _MD_ATOMIC_ADD                _PR_x86_64_AtomicAdd
extern PRInt32 _PR_x86_64_AtomicSet(PRInt32 *val, PRInt32 newval);
#define _MD_ATOMIC_SET                _PR_x86_64_AtomicSet
#endif

#if defined(__or1k__)
#if defined(__GNUC__)
/* Use GCC built-in functions */
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
#define _MD_ATOMIC_INCREMENT(ptr) __sync_add_and_fetch(ptr, 1)
#define _MD_ATOMIC_DECREMENT(ptr) __sync_sub_and_fetch(ptr, 1)
#define _MD_ATOMIC_ADD(ptr, i) __sync_add_and_fetch(ptr, i)
#define _MD_ATOMIC_SET(ptr, nv) __sync_lock_test_and_set(ptr, nv)
#endif
#endif

#if defined(__powerpc__) && !defined(__powerpc64__)
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
extern PRInt32 _PR_ppc_AtomicIncrement(PRInt32 *val);
#define _MD_ATOMIC_INCREMENT          _PR_ppc_AtomicIncrement
extern PRInt32 _PR_ppc_AtomicDecrement(PRInt32 *val);
#define _MD_ATOMIC_DECREMENT          _PR_ppc_AtomicDecrement
extern PRInt32 _PR_ppc_AtomicAdd(PRInt32 *ptr, PRInt32 val);
#define _MD_ATOMIC_ADD                _PR_ppc_AtomicAdd
extern PRInt32 _PR_ppc_AtomicSet(PRInt32 *val, PRInt32 newval);
#define _MD_ATOMIC_SET                _PR_ppc_AtomicSet
#endif

#if defined(__powerpc64__)
#if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
/* Use GCC built-in functions */
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
#define _MD_ATOMIC_INCREMENT(ptr) __sync_add_and_fetch(ptr, 1)
#define _MD_ATOMIC_DECREMENT(ptr) __sync_sub_and_fetch(ptr, 1)
#define _MD_ATOMIC_ADD(ptr, i) __sync_add_and_fetch(ptr, i)
#define _MD_ATOMIC_SET(ptr, nv) __sync_lock_test_and_set(ptr, nv)
#endif
#endif

#if defined(__mips__) && defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)
/* Use GCC built-in functions */
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
#define _MD_ATOMIC_INCREMENT(ptr) __sync_add_and_fetch(ptr, 1)
#define _MD_ATOMIC_DECREMENT(ptr) __sync_sub_and_fetch(ptr, 1)
#define _MD_ATOMIC_ADD(ptr, i) __sync_add_and_fetch(ptr, i)
#define _MD_ATOMIC_SET(ptr, nv) __sync_lock_test_and_set(ptr, nv)
#endif

#if defined(__alpha)
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
#define _MD_ATOMIC_ADD(ptr, i) ({               \
    PRInt32 __atomic_tmp, __atomic_ret;   \
    __asm__ __volatile__(                       \
    "1: ldl_l   %[ret], %[val]          \n"     \
    "   addl    %[ret], %[inc], %[tmp]  \n"     \
    "   addl    %[ret], %[inc], %[ret]  \n"     \
    "   stl_c   %[tmp], %[val]          \n"     \
    "   beq     %[tmp], 2f              \n"     \
    ".subsection 2                      \n"     \
    "2: br      1b                      \n"     \
    ".previous"                                 \
    : [ret] "=&r" (__atomic_ret),               \
      [tmp] "=&r" (__atomic_tmp),               \
      [val] "=m" (*ptr)                         \
    : [inc] "Ir" (i), "m" (*ptr));              \
    __atomic_ret;                               \
})
#define _MD_ATOMIC_INCREMENT(ptr) _MD_ATOMIC_ADD(ptr, 1)
#define _MD_ATOMIC_DECREMENT(ptr) ({            \
    PRInt32 __atomic_tmp, __atomic_ret;   \
    __asm__ __volatile__(                       \
    "1: ldl_l   %[ret], %[val]          \n"     \
    "   subl    %[ret], 1, %[tmp]       \n"     \
    "   subl    %[ret], 1, %[ret]       \n"     \
    "   stl_c   %[tmp], %[val]          \n"     \
    "   beq     %[tmp], 2f              \n"     \
    ".subsection 2                      \n"     \
    "2: br      1b                      \n"     \
    ".previous"                                 \
    : [ret] "=&r" (__atomic_ret),               \
      [tmp] "=&r" (__atomic_tmp),               \
      [val] "=m" (*ptr)                         \
    : "m" (*ptr));                              \
    __atomic_ret;                               \
})
#define _MD_ATOMIC_SET(ptr, n) ({               \
    PRInt32 __atomic_tmp, __atomic_ret;   \
    __asm__ __volatile__(                       \
    "1: ldl_l   %[ret], %[val]          \n"     \
    "   mov     %[newval], %[tmp]       \n"     \
    "   stl_c   %[tmp], %[val]          \n"     \
    "   beq     %[tmp], 2f              \n"     \
    ".subsection 2                      \n"     \
    "2: br      1b                      \n"     \
    ".previous"                                 \
    : [ret] "=&r" (__atomic_ret),               \
      [tmp] "=&r"(__atomic_tmp),                \
      [val] "=m" (*ptr)                         \
    : [newval] "Ir" (n), "m" (*ptr));           \
    __atomic_ret;                               \
})
#endif

#if defined(__arm__) || defined(__aarch64__)
#if defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)
/* Use GCC built-in functions */
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()

#define _MD_ATOMIC_INCREMENT(ptr) __sync_add_and_fetch(ptr, 1)
#define _MD_ATOMIC_DECREMENT(ptr) __sync_sub_and_fetch(ptr, 1)
#define _MD_ATOMIC_SET(ptr, nv) __sync_lock_test_and_set(ptr, nv)
#define _MD_ATOMIC_ADD(ptr, i) __sync_add_and_fetch(ptr, i)

#elif defined(_PR_ARM_KUSER)
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()

/*
 * The kernel provides this helper function at a fixed address with a fixed
 * ABI signature, directly callable from user space.
 *
 * Definition:
 * Atomically store newval in *ptr if *ptr is equal to oldval.
 * Return zero if *ptr was changed or non-zero if no exchange happened.
 */
typedef int (__kernel_cmpxchg_t)(int oldval, int newval, volatile int *ptr);
#define __kernel_cmpxchg (*(__kernel_cmpxchg_t *)0xffff0fc0)

#define _MD_ATOMIC_INCREMENT(ptr) _MD_ATOMIC_ADD(ptr, 1)
#define _MD_ATOMIC_DECREMENT(ptr) _MD_ATOMIC_ADD(ptr, -1)

static inline PRInt32 _MD_ATOMIC_ADD(PRInt32 *ptr, PRInt32 n)
{
    PRInt32 ov, nv;
    volatile PRInt32 *vp = ptr;

    do {
        ov = *vp;
        nv = ov + n;
    } while (__kernel_cmpxchg(ov, nv, vp));

    return nv;
}

static inline PRInt32 _MD_ATOMIC_SET(PRInt32 *ptr, PRInt32 nv)
{
    PRInt32 ov;
    volatile PRInt32 *vp = ptr;

    do {
        ov = *vp;
    } while (__kernel_cmpxchg(ov, nv, vp));

    return ov;
}
#endif
#endif /* __arm__ */

#define USE_SETJMP
#if (defined(__GLIBC__) && __GLIBC__ >= 2) || defined(ANDROID)
#define _PR_POLL_AVAILABLE
#endif
#undef _PR_USE_POLL
#define _PR_STAT_HAS_ONLY_ST_ATIME
#if defined(__alpha) || defined(__ia64__)
#define _PR_HAVE_LARGE_OFF_T
#elif (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 1) \
    || defined(ANDROID)
#define _PR_HAVE_OFF64_T
#else
#define _PR_NO_LARGE_FILES
#endif
#if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 1) \
    || defined(ANDROID)
#define _PR_INET6
#define _PR_HAVE_INET_NTOP
#define _PR_HAVE_GETHOSTBYNAME2
#define _PR_HAVE_GETADDRINFO
#define _PR_INET6_PROBE
#endif
#ifndef ANDROID
#define _PR_HAVE_SYSV_SEMAPHORES
#define PR_HAVE_SYSV_NAMED_SHARED_MEMORY
#endif
/* Android has gethostbyname_r but not gethostbyaddr_r or gethostbyname2_r. */
#if (__GLIBC__ >= 2) && defined(_PR_PTHREADS)
#define _PR_HAVE_GETHOST_R
#define _PR_HAVE_GETHOST_R_INT
#endif

#ifdef _PR_PTHREADS

extern void _MD_CleanupBeforeExit(void);
#define _MD_CLEANUP_BEFORE_EXIT _MD_CleanupBeforeExit

#else  /* ! _PR_PTHREADS */

#include <setjmp.h>

#define PR_CONTEXT_TYPE sigjmp_buf

#define CONTEXT(_th) ((_th)->md.context)

#ifdef __powerpc__
/*
 * PowerPC based MkLinux
 *
 * On the PowerPC, the new style jmp_buf isn't used until glibc
 * 2.1.
 */
#if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 1)
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[JB_GPR1]
#else
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[0].__misc[0]
#endif /* glibc 2.1 or later */
#define _MD_SET_FP(_t, val)
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) ((void *) 0)
/* aix = 64, macos = 70 */
#define PR_NUM_GCREGS  64

#elif defined(__alpha)
/* Alpha based Linux */

#if defined(__GLIBC__) && __GLIBC__ >= 2
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[JB_SP]
#define _MD_SET_FP(_t, val)
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) ((void *) 0)
#define _MD_SP_TYPE long int
#else
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[0].__sp
#define _MD_SET_FP(_t, val)
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) ((void *) 0)
#define _MD_SP_TYPE __ptr_t
#endif /* defined(__GLIBC__) && __GLIBC__ >= 2 */

/* XXX not sure if this is correct, or maybe it should be 17? */
#define PR_NUM_GCREGS 9

#elif defined(__ia64__)

#define _MD_GET_SP(_t)      ((long *)((_t)->md.context[0].__jmpbuf)[0])
#define _MD_SET_FP(_t, val)
#define _MD_GET_SP_PTR(_t)  &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t)  ((void *) 0)
#define _MD_SP_TYPE         long int

#define PR_NUM_GCREGS       _JBLEN

#elif defined(__mc68000__)
/* m68k based Linux */

/*
 * On the m68k, glibc still uses the old style sigjmp_buf, even
 * in glibc 2.0.7.
 */
#if defined(__GLIBC__) && __GLIBC__ >= 2
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[0].__sp
#define _MD_SET_FP(_t, val)
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) ((void *) 0)
#define _MD_SP_TYPE int
#else
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[0].__sp
#define _MD_SET_FP(_t, val)
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) ((void *) 0)
#define _MD_SP_TYPE __ptr_t
#endif /* defined(__GLIBC__) && __GLIBC__ >= 2 */

/* XXX not sure if this is correct, or maybe it should be 17? */
#define PR_NUM_GCREGS 9

#elif defined(__sparc__)
/* Sparc */
#if defined(__GLIBC__) && __GLIBC__ >= 2
/*
 * You need glibc2-2.0.7-25 or later. The libraries that came with
 * Red Hat 5.1 are not new enough, but they are in 5.2.
 */
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[JB_SP]
#define _MD_SET_FP(_t, val) ((_t)->md.context[0].__jmpbuf[JB_FP] = val)
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) (&(_t)->md.context[0].__jmpbuf[JB_FP])
#define _MD_SP_TYPE int
#else
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[0].__fp
#define _MD_SET_FP(_t, val)
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) ((void *) 0)
#define _MD_SP_TYPE __ptr_t
#endif /* defined(__GLIBC__) && __GLIBC__ >= 2 */

#elif defined(__i386__)
/* Intel based Linux */
#if defined(__GLIBC__) && __GLIBC__ >= 2
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[JB_SP]
#define _MD_SET_FP(_t, val) ((_t)->md.context[0].__jmpbuf[JB_BP] = val)
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) (&(_t)->md.context[0].__jmpbuf[JB_BP])
#define _MD_SP_TYPE int
#else
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[0].__sp
#define _MD_SET_FP(_t, val) ((_t)->md.context[0].__jmpbuf[0].__bp = val)
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) &((_t)->md.context[0].__jmpbuf[0].__bp)
#define _MD_SP_TYPE __ptr_t
#endif /* defined(__GLIBC__) && __GLIBC__ >= 2 */
#define PR_NUM_GCREGS   6

#elif defined(__mips__)
/* Linux/MIPS */
#if defined(__GLIBC__) && __GLIBC__ >= 2
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[0].__sp
#define _MD_SET_FP(_t, val) ((_t)->md.context[0].__jmpbuf[0].__fp = (val))
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) (&(_t)->md.context[0].__jmpbuf[0].__fp)
#define _MD_SP_TYPE __ptr_t
#else
#error "Linux/MIPS pre-glibc2 not supported yet"
#endif /* defined(__GLIBC__) && __GLIBC__ >= 2 */

#elif defined(__arm__)
/* ARM/Linux */
#if defined(__GLIBC__) && __GLIBC__ >= 2
#ifdef __ARM_EABI__
/* EABI */
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[8]
#define _MD_SET_FP(_t, val) ((_t)->md.context[0].__jmpbuf[7] = (val))
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) (&(_t)->md.context[0].__jmpbuf[7])
#define _MD_SP_TYPE __ptr_t
#else /* __ARM_EABI__ */
/* old ABI */
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[20]
#define _MD_SET_FP(_t, val) ((_t)->md.context[0].__jmpbuf[19] = (val))
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) (&(_t)->md.context[0].__jmpbuf[19])
#define _MD_SP_TYPE __ptr_t
#endif /* __ARM_EABI__ */
#else
#error "ARM/Linux pre-glibc2 not supported yet"
#endif /* defined(__GLIBC__) && __GLIBC__ >= 2 */

#elif defined(__sh__)
/* SH/Linux */
#if defined(__GLIBC__) && __GLIBC__ >= 2
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[7]
#define _MD_SET_FP(_t, val) ((_t)->md.context[0].__jmpbuf[6] = (val))
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) (&(_t)->md.context[0].__jmpbuf[6])
#define _MD_SP_TYPE __ptr_t
#else
#error "SH/Linux pre-glibc2 not supported yet"
#endif /* defined(__GLIBC__) && __GLIBC__ >= 2 */

#elif defined(__m32r__)
/* Linux/M32R */
#if defined(__GLIBC__) && __GLIBC__ >= 2
#define _MD_GET_SP(_t) (_t)->md.context[0].__jmpbuf[0].__regs[JB_SP]
#define _MD_SET_FP(_t, val) ((_t)->md.context[0].__jmpbuf[0].__regs[JB_FP] = (val))
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) (&(_t)->md.context[0].__jmpbuf[0].__regs[JB_FP])
#define _MD_SP_TYPE __ptr_t
#else
#error "Linux/M32R pre-glibc2 not supported yet"
#endif /* defined(__GLIBC__) && __GLIBC__ >= 2 */

#else

#error "Unknown CPU architecture"

#endif /*__powerpc__*/

/*
** Initialize a thread context to run "_main()" when started
*/
#ifdef __powerpc__

#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)  \
{  \
    *status = PR_TRUE;  \
    if (sigsetjmp(CONTEXT(_thread), 1)) {  \
        _main();  \
    }  \
    _MD_GET_SP(_thread) = (unsigned char*) ((_sp) - 128); \
    _thread->md.sp = _MD_GET_SP_PTR(_thread); \
    _thread->md.fp = _MD_GET_FP_PTR(_thread); \
    _MD_SET_FP(_thread, 0); \
}

#elif defined(__mips__)

#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)  \
{  \
    *status = PR_TRUE;  \
    (void) sigsetjmp(CONTEXT(_thread), 1);  \
    _thread->md.context[0].__jmpbuf[0].__pc = (__ptr_t) _main;  \
    _MD_GET_SP(_thread) = (_MD_SP_TYPE) ((_sp) - 64); \
    _thread->md.sp = _MD_GET_SP_PTR(_thread); \
    _thread->md.fp = _MD_GET_FP_PTR(_thread); \
    _MD_SET_FP(_thread, 0); \
}

#else

#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)  \
{  \
    *status = PR_TRUE;  \
    if (sigsetjmp(CONTEXT(_thread), 1)) {  \
        _main();  \
    }  \
    _MD_GET_SP(_thread) = (_MD_SP_TYPE) ((_sp) - 64); \
    _thread->md.sp = _MD_GET_SP_PTR(_thread); \
    _thread->md.fp = _MD_GET_FP_PTR(_thread); \
    _MD_SET_FP(_thread, 0); \
}

#endif /*__powerpc__*/

#define _MD_SWITCH_CONTEXT(_thread)  \
    if (!sigsetjmp(CONTEXT(_thread), 1)) {  \
    (_thread)->md.errcode = errno;  \
    _PR_Schedule();  \
    }

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread) \
{   \
    errno = (_thread)->md.errcode;  \
    _MD_SET_CURRENT_THREAD(_thread);  \
    siglongjmp(CONTEXT(_thread), 1);  \
}

/* Machine-dependent (MD) data structures */

struct _MDThread {
    PR_CONTEXT_TYPE context;
    void *sp;
    void *fp;
    int id;
    int errcode;
};

struct _MDThreadStack {
    PRInt8 notused;
};

struct _MDLock {
    PRInt8 notused;
};

struct _MDSemaphore {
    PRInt8 notused;
};

struct _MDCVar {
    PRInt8 notused;
};

struct _MDSegment {
    PRInt8 notused;
};

/*
 * md-specific cpu structure field
 */
#include <sys/time.h>  /* for FD_SETSIZE */
#define _PR_MD_MAX_OSFD FD_SETSIZE

struct _MDCPU_Unix {
    PRCList ioQ;
    PRUint32 ioq_timeout;
    PRInt32 ioq_max_osfd;
    PRInt32 ioq_osfd_cnt;
#ifndef _PR_USE_POLL
    fd_set fd_read_set, fd_write_set, fd_exception_set;
    PRInt16 fd_read_cnt[_PR_MD_MAX_OSFD],fd_write_cnt[_PR_MD_MAX_OSFD],
            fd_exception_cnt[_PR_MD_MAX_OSFD];
#else
    struct pollfd *ioq_pollfds;
    int ioq_pollfds_size;
#endif  /* _PR_USE_POLL */
};

#define _PR_IOQ(_cpu)           ((_cpu)->md.md_unix.ioQ)
#define _PR_ADD_TO_IOQ(_pq, _cpu) PR_APPEND_LINK(&_pq.links, &_PR_IOQ(_cpu))
#define _PR_FD_READ_SET(_cpu)       ((_cpu)->md.md_unix.fd_read_set)
#define _PR_FD_READ_CNT(_cpu)       ((_cpu)->md.md_unix.fd_read_cnt)
#define _PR_FD_WRITE_SET(_cpu)      ((_cpu)->md.md_unix.fd_write_set)
#define _PR_FD_WRITE_CNT(_cpu)      ((_cpu)->md.md_unix.fd_write_cnt)
#define _PR_FD_EXCEPTION_SET(_cpu)  ((_cpu)->md.md_unix.fd_exception_set)
#define _PR_FD_EXCEPTION_CNT(_cpu)  ((_cpu)->md.md_unix.fd_exception_cnt)
#define _PR_IOQ_TIMEOUT(_cpu)       ((_cpu)->md.md_unix.ioq_timeout)
#define _PR_IOQ_MAX_OSFD(_cpu)      ((_cpu)->md.md_unix.ioq_max_osfd)
#define _PR_IOQ_OSFD_CNT(_cpu)      ((_cpu)->md.md_unix.ioq_osfd_cnt)
#define _PR_IOQ_POLLFDS(_cpu)       ((_cpu)->md.md_unix.ioq_pollfds)
#define _PR_IOQ_POLLFDS_SIZE(_cpu)  ((_cpu)->md.md_unix.ioq_pollfds_size)

#define _PR_IOQ_MIN_POLLFDS_SIZE(_cpu)  32

struct _MDCPU {
    struct _MDCPU_Unix md_unix;
};

#define _MD_INIT_LOCKS()
#define _MD_NEW_LOCK(lock) PR_SUCCESS
#define _MD_FREE_LOCK(lock)
#define _MD_LOCK(lock)
#define _MD_UNLOCK(lock)
#define _MD_INIT_IO()
#define _MD_IOQ_LOCK()
#define _MD_IOQ_UNLOCK()

extern PRStatus _MD_InitializeThread(PRThread *thread);

#define _MD_INIT_RUNNING_CPU(cpu)       _MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD                 _MD_InitializeThread
#define _MD_EXIT_THREAD(thread)
#define _MD_SUSPEND_THREAD(thread)      _MD_suspend_thread
#define _MD_RESUME_THREAD(thread)       _MD_resume_thread
#define _MD_CLEAN_THREAD(_thread)

extern PRStatus _MD_CREATE_THREAD(
    PRThread *thread,
    void (*start) (void *),
    PRThreadPriority priority,
    PRThreadScope scope,
    PRThreadState state,
    PRUint32 stackSize);
extern void _MD_SET_PRIORITY(struct _MDThread *thread, PRUintn newPri);
extern PRStatus _MD_WAIT(PRThread *, PRIntervalTime timeout);
extern PRStatus _MD_WAKEUP_WAITER(PRThread *);
extern void _MD_YIELD(void);

#endif /* ! _PR_PTHREADS */

extern void _MD_EarlyInit(void);

#define _MD_EARLY_INIT                  _MD_EarlyInit
#define _MD_FINAL_INIT                  _PR_UnixInit
#define _PR_HAVE_CLOCK_MONOTONIC

/*
 * We wrapped the select() call.  _MD_SELECT refers to the built-in,
 * unwrapped version.
 */
#define _MD_SELECT __select

#ifdef _PR_POLL_AVAILABLE
#include <sys/poll.h>
extern int __syscall_poll(struct pollfd *ufds, unsigned long int nfds,
                          int timeout);
#define _MD_POLL __syscall_poll
#endif

/* For writev() */
#include <sys/uio.h>

extern void _MD_linux_map_sendfile_error(int err);

#endif /* nspr_linux_defs_h___ */
