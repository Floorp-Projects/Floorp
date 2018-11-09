/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessRedirect.h"

#include "mozilla/Maybe.h"

#include "HashTable.h"
#include "Lock.h"
#include "MemorySnapshot.h"
#include "ProcessRecordReplay.h"
#include "ProcessRewind.h"
#include "base/eintr_wrapper.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>

#include <bsm/audit.h>
#include <bsm/audit_session.h>
#include <mach/clock.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#include <sys/attr.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include <Carbon/Carbon.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <objc/objc-runtime.h>

namespace mozilla {
namespace recordreplay {

// Specify every function that is being redirected. MACRO is invoked with the
// function's name, followed by any hooks associated with the redirection for
// saving its output or adding a preamble.
#define FOR_EACH_REDIRECTION(MACRO)                              \
  /* System call wrappers */                                     \
  MACRO(kevent, RR_SaveRvalHadErrorNegative<RR_WriteBuffer<3, 4, struct kevent>>, \
        nullptr, nullptr, Preamble_WaitForever)                  \
  MACRO(kevent64, RR_SaveRvalHadErrorNegative<RR_WriteBuffer<3, 4, struct kevent64_s>>) \
  MACRO(mprotect, nullptr, Preamble_mprotect)                    \
  MACRO(mmap, nullptr, Preamble_mmap)                            \
  MACRO(munmap, nullptr, Preamble_munmap)                        \
  MACRO(read, RR_SaveRvalHadErrorNegative<RR_WriteBufferViaRval<1, 2>>, \
        nullptr, nullptr, Preamble_SetError<EIO>)                \
  MACRO(__read_nocancel, RR_SaveRvalHadErrorNegative<RR_WriteBufferViaRval<1, 2>>) \
  MACRO(pread, RR_SaveRvalHadErrorNegative<RR_WriteBufferViaRval<1, 2>>) \
  MACRO(write, RR_SaveRvalHadErrorNegative,                      \
        nullptr, nullptr, MiddlemanPreamble_write)               \
  MACRO(__write_nocancel, RR_SaveRvalHadErrorNegative)           \
  MACRO(open, RR_SaveRvalHadErrorNegative)                       \
  MACRO(__open_nocancel, RR_SaveRvalHadErrorNegative)            \
  MACRO(recv, RR_SaveRvalHadErrorNegative<RR_WriteBufferViaRval<1, 2>>) \
  MACRO(recvmsg, RR_SaveRvalHadErrorNegative<RR_recvmsg>,        \
        nullptr, nullptr, Preamble_WaitForever)                  \
  MACRO(sendmsg, RR_SaveRvalHadErrorNegative,                    \
        nullptr, nullptr, MiddlemanPreamble_sendmsg)             \
  MACRO(shm_open, RR_SaveRvalHadErrorNegative)                   \
  MACRO(socket, RR_SaveRvalHadErrorNegative)                     \
  MACRO(kqueue, RR_SaveRvalHadErrorNegative)                     \
  MACRO(pipe, RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<0, 2 * sizeof(int)>>, \
        nullptr, nullptr, Preamble_SetError)                     \
  MACRO(close, RR_SaveRvalHadErrorNegative)                      \
  MACRO(__close_nocancel, RR_SaveRvalHadErrorNegative)           \
  MACRO(mkdir, RR_SaveRvalHadErrorNegative)                      \
  MACRO(dup, RR_SaveRvalHadErrorNegative)                        \
  MACRO(access, RR_SaveRvalHadErrorNegative)                     \
  MACRO(lseek, RR_SaveRvalHadErrorNegative)                      \
  MACRO(socketpair, RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<3, 2 * sizeof(int)>>) \
  MACRO(fileport_makeport,                                       \
        RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<1, sizeof(size_t)>>) \
  MACRO(getsockopt, RR_SaveRvalHadErrorNegative<RR_getsockopt>)  \
  MACRO(gettimeofday, RR_SaveRvalHadErrorNegative<RR_Compose<    \
                        RR_WriteOptionalBufferFixedSize<0, sizeof(struct timeval)>, \
                        RR_WriteOptionalBufferFixedSize<1, sizeof(struct timezone)>>>, \
        nullptr, nullptr, Preamble_PassThrough)                  \
  MACRO(getuid, RR_ScalarRval)                                   \
  MACRO(geteuid, RR_ScalarRval)                                  \
  MACRO(getgid, RR_ScalarRval)                                   \
  MACRO(getegid, RR_ScalarRval)                                  \
  MACRO(issetugid, RR_ScalarRval)                                \
  MACRO(__gettid, RR_ScalarRval)                                 \
  MACRO(getpid, nullptr, Preamble_getpid)                        \
  MACRO(fcntl, RR_SaveRvalHadErrorNegative, Preamble_fcntl, nullptr, MiddlemanPreamble_fcntl) \
  MACRO(getattrlist, RR_SaveRvalHadErrorNegative<RR_WriteBuffer<2, 3>>) \
  MACRO(fstat$INODE64,                                           \
        RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<1, sizeof(struct stat)>>, \
        nullptr, nullptr, Preamble_SetError)                     \
  MACRO(lstat$INODE64,                                           \
        RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<1, sizeof(struct stat)>>, \
        nullptr, nullptr, Preamble_SetError)                     \
  MACRO(stat$INODE64,                                            \
        RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<1, sizeof(struct stat)>>, \
        nullptr, nullptr, Preamble_SetError)                     \
  MACRO(statfs$INODE64,                                          \
        RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<1, sizeof(struct statfs)>>, \
        nullptr, nullptr, Preamble_SetError)                     \
  MACRO(fstatfs$INODE64,                                         \
        RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<1, sizeof(struct statfs)>>, \
        nullptr, nullptr, Preamble_SetError)                     \
  MACRO(readlink, RR_SaveRvalHadErrorNegative<RR_WriteBuffer<1, 2>>) \
  MACRO(__getdirentries64, RR_SaveRvalHadErrorNegative<RR_Compose< \
                             RR_WriteBuffer<1, 2>,               \
                             RR_WriteBufferFixedSize<3, sizeof(size_t)>>>) \
  MACRO(getdirentriesattr, RR_SaveRvalHadErrorNegative<RR_Compose< \
                             RR_WriteBufferFixedSize<1, sizeof(struct attrlist)>, \
                             RR_WriteBuffer<2, 3>,               \
                             RR_WriteBufferFixedSize<4, sizeof(size_t)>, \
                             RR_WriteBufferFixedSize<5, sizeof(size_t)>, \
                             RR_WriteBufferFixedSize<6, sizeof(size_t)>>>) \
  MACRO(getrusage,                                               \
        RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<1, sizeof(struct rusage)>>, \
        nullptr, nullptr, Preamble_PassThrough)                  \
  MACRO(__getrlimit,                                             \
        RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<1, sizeof(struct rlimit)>>) \
  MACRO(__setrlimit, RR_SaveRvalHadErrorNegative)                \
  MACRO(sigprocmask,                                             \
        RR_SaveRvalHadErrorNegative<RR_WriteOptionalBufferFixedSize<2, sizeof(sigset_t)>>, \
        nullptr, nullptr, Preamble_PassThrough)                  \
  MACRO(sigaltstack,                                             \
        RR_SaveRvalHadErrorNegative<RR_WriteOptionalBufferFixedSize<2, sizeof(stack_t)>>) \
  MACRO(sigaction,                                               \
        RR_SaveRvalHadErrorNegative<RR_WriteOptionalBufferFixedSize<2, sizeof(struct sigaction)>>) \
  MACRO(__pthread_sigmask,                                       \
        RR_SaveRvalHadErrorNegative<RR_WriteOptionalBufferFixedSize<2, sizeof(sigset_t)>>) \
  MACRO(__fsgetpath, RR_SaveRvalHadErrorNegative<RR_WriteBuffer<0, 1>>) \
  MACRO(__disable_threadsignal, nullptr, Preamble___disable_threadsignal) \
  MACRO(__sysctl, RR_SaveRvalHadErrorNegative<RR___sysctl>)      \
  MACRO(__mac_syscall, RR_SaveRvalHadErrorNegative)              \
  MACRO(getaudit_addr,                                           \
        RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<0, sizeof(auditinfo_addr_t)>>) \
  MACRO(umask, RR_ScalarRval)                                    \
  MACRO(__select, RR_SaveRvalHadErrorNegative<RR_Compose<        \
                    RR_WriteBufferFixedSize<1, sizeof(fd_set)>,  \
                    RR_WriteBufferFixedSize<2, sizeof(fd_set)>,  \
                    RR_WriteBufferFixedSize<3, sizeof(fd_set)>,  \
                    RR_WriteOptionalBufferFixedSize<4, sizeof(timeval)>>>, \
        nullptr, nullptr, Preamble_WaitForever)                  \
  MACRO(__process_policy, RR_SaveRvalHadErrorNegative)           \
  MACRO(__kdebug_trace, RR_SaveRvalHadErrorNegative)             \
  MACRO(guarded_kqueue_np,                                       \
        RR_SaveRvalHadErrorNegative<RR_WriteBufferFixedSize<0, sizeof(size_t)>>) \
  MACRO(csops, RR_SaveRvalHadErrorNegative<RR_WriteBuffer<2, 3>>) \
  MACRO(__getlogin, RR_SaveRvalHadErrorNegative<RR_WriteBuffer<0, 1>>) \
  MACRO(__workq_kernreturn, nullptr, Preamble___workq_kernreturn) \
  MACRO(start_wqthread, nullptr, Preamble_start_wqthread)        \
  /* pthreads interface functions */                             \
  MACRO(pthread_cond_wait, nullptr, Preamble_pthread_cond_wait)  \
  MACRO(pthread_cond_timedwait, nullptr, Preamble_pthread_cond_timedwait) \
  MACRO(pthread_cond_timedwait_relative_np, nullptr, Preamble_pthread_cond_timedwait_relative_np) \
  MACRO(pthread_create, nullptr, Preamble_pthread_create)        \
  MACRO(pthread_join, nullptr, Preamble_pthread_join)            \
  MACRO(pthread_mutex_init, nullptr, Preamble_pthread_mutex_init) \
  MACRO(pthread_mutex_destroy, nullptr, Preamble_pthread_mutex_destroy) \
  MACRO(pthread_mutex_lock, nullptr, Preamble_pthread_mutex_lock) \
  MACRO(pthread_mutex_trylock, nullptr, Preamble_pthread_mutex_trylock) \
  MACRO(pthread_mutex_unlock, nullptr, Preamble_pthread_mutex_unlock) \
  /* C Library functions */                                      \
  MACRO(dlclose, nullptr, Preamble_Veto<0>)                      \
  MACRO(dlopen, nullptr, Preamble_PassThrough)                   \
  MACRO(dlsym, nullptr, Preamble_PassThrough)                    \
  MACRO(fclose, RR_SaveRvalHadErrorNegative)                     \
  MACRO(fopen, RR_SaveRvalHadErrorZero)                          \
  MACRO(fread, RR_Compose<RR_ScalarRval, RR_fread>)              \
  MACRO(fseek, RR_SaveRvalHadErrorNegative)                      \
  MACRO(ftell, RR_SaveRvalHadErrorNegative)                      \
  MACRO(fwrite, RR_ScalarRval)                                   \
  MACRO(getenv, RR_CStringRval, Preamble_getenv, nullptr, Preamble_Veto<0>) \
  MACRO(localtime_r, RR_SaveRvalHadErrorZero<RR_Compose<         \
                       RR_WriteBufferFixedSize<1, sizeof(struct tm)>, \
                       RR_RvalIsArgument<1>>>)                   \
  MACRO(gmtime_r, RR_SaveRvalHadErrorZero<RR_Compose<            \
                    RR_WriteBufferFixedSize<1, sizeof(struct tm)>, \
                    RR_RvalIsArgument<1>>>)                      \
  MACRO(localtime, nullptr, Preamble_localtime)                  \
  MACRO(gmtime, nullptr, Preamble_gmtime)                        \
  MACRO(mktime, RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<0, sizeof(struct tm)>>) \
  MACRO(setlocale, RR_CStringRval)                               \
  MACRO(strftime, RR_Compose<RR_ScalarRval, RR_WriteBufferViaRval<0, 1, 1>>) \
  MACRO(arc4random, RR_ScalarRval)                               \
  MACRO(mach_absolute_time, RR_ScalarRval, Preamble_mach_absolute_time, \
        nullptr, Preamble_PassThrough)                           \
  MACRO(mach_msg, RR_Compose<RR_ScalarRval, RR_WriteBuffer<0, 3>>, \
        nullptr, nullptr, Preamble_WaitForever)                  \
  MACRO(mach_timebase_info,                                      \
        RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<0, sizeof(mach_timebase_info_data_t)>>) \
  MACRO(mach_vm_allocate, nullptr, Preamble_mach_vm_allocate)    \
  MACRO(mach_vm_deallocate, nullptr, Preamble_mach_vm_deallocate) \
  MACRO(mach_vm_protect, nullptr, Preamble_mach_vm_protect)      \
  MACRO(realpath,                                                \
        RR_SaveRvalHadErrorZero<RR_Compose<RR_CStringRval,       \
                                           RR_WriteOptionalBufferFixedSize<1, PATH_MAX>>>) \
  MACRO(realpath$DARWIN_EXTSN,                                   \
        RR_SaveRvalHadErrorZero<RR_Compose<RR_CStringRval,       \
                                           RR_WriteOptionalBufferFixedSize<1, PATH_MAX>>>) \
  /* By passing through events when initializing the sandbox, we ensure both */ \
  /* that we actually initialize the process sandbox while replaying as well as */ \
  /* while recording, and that any activity in these calls does not interfere */ \
  /* with the replay. */                                         \
  MACRO(sandbox_init, nullptr, Preamble_PassThrough)             \
  MACRO(sandbox_init_with_parameters, nullptr, Preamble_PassThrough) \
  /* Make sure events are passed through here so that replaying processes can */ \
  /* inspect their own threads. */                               \
  MACRO(task_threads, nullptr, Preamble_PassThrough)             \
  MACRO(vm_copy, nullptr, Preamble_vm_copy)                      \
  MACRO(vm_purgable_control, nullptr, Preamble_vm_purgable_control) \
  MACRO(tzset)                                                   \
  /* NSPR functions */                                           \
  MACRO(PL_NewHashTable, nullptr, Preamble_PL_NewHashTable)      \
  MACRO(PL_HashTableDestroy, nullptr, Preamble_PL_HashTableDestroy) \
  /* Objective C functions */                                    \
  MACRO(class_getClassMethod, RR_ScalarRval)                     \
  MACRO(class_getInstanceMethod, RR_ScalarRval)                  \
  MACRO(method_exchangeImplementations)                          \
  MACRO(objc_autoreleasePoolPop)                                 \
  MACRO(objc_autoreleasePoolPush, RR_ScalarRval)                 \
  MACRO(objc_msgSend, RR_objc_msgSend, Preamble_objc_msgSend,    \
        Middleman_objc_msgSend, MiddlemanPreamble_objc_msgSend)  \
  /* Cocoa functions */                                          \
  MACRO(AcquireFirstMatchingEventInQueue, RR_ScalarRval)         \
  MACRO(CFArrayAppendValue)                                      \
  MACRO(CFArrayCreate, RR_ScalarRval, nullptr, Middleman_CFArrayCreate) \
  MACRO(CFArrayGetCount, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CFArrayGetValueAtIndex, RR_ScalarRval, nullptr, Middleman_CFArrayGetValueAtIndex) \
  MACRO(CFArrayRemoveValueAtIndex)                               \
  MACRO(CFAttributedStringCreate, RR_ScalarRval, nullptr,        \
        Middleman_Compose<Middleman_CFTypeArg<1>, Middleman_CFTypeArg<2>, Middleman_CreateCFTypeRval>) \
  MACRO(CFBundleCopyExecutableURL, RR_ScalarRval)                \
  MACRO(CFBundleCopyInfoDictionaryForURL, RR_ScalarRval)         \
  MACRO(CFBundleCreate, RR_ScalarRval)                           \
  MACRO(CFBundleGetBundleWithIdentifier, RR_ScalarRval)          \
  MACRO(CFBundleGetDataPointerForName, nullptr, Preamble_VetoIfNotPassedThrough<0>) \
  MACRO(CFBundleGetFunctionPointerForName, nullptr, Preamble_VetoIfNotPassedThrough<0>) \
  MACRO(CFBundleGetIdentifier, RR_ScalarRval)                    \
  MACRO(CFBundleGetInfoDictionary, RR_ScalarRval)                \
  MACRO(CFBundleGetMainBundle, RR_ScalarRval)                    \
  MACRO(CFBundleGetValueForInfoDictionaryKey, RR_ScalarRval)     \
  MACRO(CFDataGetBytePtr, RR_CFDataGetBytePtr, nullptr, Middleman_CFDataGetBytePtr) \
  MACRO(CFDataGetLength, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CFDateFormatterCreate, RR_ScalarRval)                    \
  MACRO(CFDateFormatterGetFormat, RR_ScalarRval)                 \
  MACRO(CFDictionaryAddValue, nullptr, nullptr,                  \
        Middleman_Compose<Middleman_UpdateCFTypeArg<0>,          \
                          Middleman_CFTypeArg<1>,                \
                          Middleman_CFTypeArg<2>>)               \
  MACRO(CFDictionaryCreate, RR_ScalarRval, nullptr, Middleman_CFDictionaryCreate) \
  MACRO(CFDictionaryCreateMutable, RR_ScalarRval, nullptr, Middleman_CreateCFTypeRval) \
  MACRO(CFDictionaryCreateMutableCopy, RR_ScalarRval, nullptr,   \
        Middleman_Compose<Middleman_CFTypeArg<2>, Middleman_CreateCFTypeRval>) \
  MACRO(CFDictionaryGetValue, RR_ScalarRval, nullptr,            \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CFTypeArg<1>, Middleman_CFTypeRval>) \
  MACRO(CFDictionaryGetValueIfPresent,                           \
        RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<2, sizeof(const void*)>>, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_CFTypeArg<1>,                \
                          Middleman_CFTypeOutputArg<2>>)         \
  MACRO(CFDictionaryReplaceValue, nullptr, nullptr,              \
        Middleman_Compose<Middleman_UpdateCFTypeArg<0>,          \
                          Middleman_CFTypeArg<1>,                \
                          Middleman_CFTypeArg<2>>)               \
  MACRO(CFEqual, RR_ScalarRval, nullptr,                         \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CFTypeArg<1>>) \
  MACRO(CFGetTypeID, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CFLocaleCopyCurrent, RR_ScalarRval)                      \
  MACRO(CFLocaleCopyPreferredLanguages, RR_ScalarRval)           \
  MACRO(CFLocaleCreate, RR_ScalarRval)                           \
  MACRO(CFLocaleGetIdentifier, RR_ScalarRval)                    \
  MACRO(CFNotificationCenterAddObserver, nullptr, Preamble_CFNotificationCenterAddObserver) \
  MACRO(CFNotificationCenterGetLocalCenter, RR_ScalarRval)       \
  MACRO(CFNotificationCenterRemoveObserver)                      \
  MACRO(CFNumberCreate, RR_ScalarRval, nullptr, Middleman_CFNumberCreate) \
  MACRO(CFNumberGetValue, RR_Compose<RR_ScalarRval, RR_CFNumberGetValue>, nullptr, \
        Middleman_CFNumberGetValue)                              \
  MACRO(CFNumberIsFloatType, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CFPreferencesAppValueIsForced, RR_ScalarRval)            \
  MACRO(CFPreferencesCopyAppValue, RR_ScalarRval)                \
  MACRO(CFPreferencesCopyValue, RR_ScalarRval)                   \
  MACRO(CFPropertyListCreateFromXMLData, RR_ScalarRval)          \
  MACRO(CFPropertyListCreateWithStream, RR_ScalarRval)           \
  MACRO(CFReadStreamClose)                                       \
  MACRO(CFReadStreamCreateWithFile, RR_ScalarRval)               \
  MACRO(CFReadStreamOpen, RR_ScalarRval)                         \
  /* Don't handle release/retain calls explicitly in the middleman, all resources */ \
  /* will be cleaned up when its calls are reset. */             \
  MACRO(CFRelease, RR_ScalarRval, nullptr, nullptr, Preamble_Veto<0>) \
  MACRO(CFRetain, RR_ScalarRval, nullptr, nullptr, MiddlemanPreamble_CFRetain) \
  MACRO(CFRunLoopAddSource)                                      \
  MACRO(CFRunLoopGetCurrent, RR_ScalarRval)                      \
  MACRO(CFRunLoopRemoveSource)                                   \
  MACRO(CFRunLoopSourceCreate, RR_ScalarRval, Preamble_CFRunLoopSourceCreate) \
  MACRO(CFRunLoopSourceInvalidate)                               \
  MACRO(CFRunLoopSourceSignal)                                   \
  MACRO(CFRunLoopWakeUp)                                         \
  MACRO(CFStringAppendCharacters)                                \
  MACRO(CFStringCompare, RR_ScalarRval, nullptr,                 \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CFTypeArg<1>>) \
  MACRO(CFStringCreateArrayBySeparatingStrings, RR_ScalarRval)   \
  MACRO(CFStringCreateMutable, RR_ScalarRval)                    \
  MACRO(CFStringCreateWithBytes, RR_ScalarRval, nullptr,         \
        Middleman_Compose<Middleman_Buffer<1, 2>, Middleman_CreateCFTypeRval>) \
  MACRO(CFStringCreateWithBytesNoCopy, RR_ScalarRval)            \
  MACRO(CFStringCreateWithCharactersNoCopy, RR_ScalarRval, nullptr, \
        Middleman_Compose<Middleman_Buffer<1, 2, UniChar>, Middleman_CreateCFTypeRval>) \
  MACRO(CFStringCreateWithCString, RR_ScalarRval)                \
  MACRO(CFStringCreateWithFormat, RR_ScalarRval)                 \
  /* Argument indexes are off by one here as the CFRange argument uses two slots. */ \
  MACRO(CFStringGetBytes, RR_Compose<                            \
                            RR_ScalarRval,                       \
                            RR_WriteOptionalBuffer<6, 7>,        \
                            RR_WriteOptionalBufferFixedSize<8, sizeof(CFIndex)>>) \
  /* Argument indexes are off by one here as the CFRange argument uses two slots. */ \
  /* We also need to specify the argument register with the range's length here. */ \
  MACRO(CFStringGetCharacters, RR_WriteBuffer<3, 2, UniChar>, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_WriteBuffer<3, 2, UniChar>>) \
  MACRO(CFStringGetCString, RR_Compose<RR_ScalarRval, RR_WriteBuffer<1, 2>>) \
  MACRO(CFStringGetCStringPtr, nullptr, Preamble_VetoIfNotPassedThrough<0>) \
  MACRO(CFStringGetIntValue, RR_ScalarRval)                      \
  MACRO(CFStringGetLength, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CFStringGetMaximumSizeForEncoding, RR_ScalarRval)        \
  MACRO(CFStringHasPrefix, RR_ScalarRval)                        \
  MACRO(CFStringTokenizerAdvanceToNextToken, RR_ScalarRval)      \
  MACRO(CFStringTokenizerCreate, RR_ScalarRval)                  \
  MACRO(CFStringTokenizerGetCurrentTokenRange, RR_ComplexScalarRval) \
  MACRO(CFURLCreateFromFileSystemRepresentation, RR_ScalarRval)  \
  MACRO(CFURLCreateFromFSRef, RR_ScalarRval)                     \
  MACRO(CFURLCreateWithFileSystemPath, RR_ScalarRval)            \
  MACRO(CFURLCreateWithString, RR_ScalarRval)                    \
  MACRO(CFURLGetFileSystemRepresentation, RR_Compose<RR_ScalarRval, RR_WriteBuffer<2, 3>>) \
  MACRO(CFURLGetFSRef, RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<1, sizeof(FSRef)>>) \
  MACRO(CFUUIDCreate, RR_ScalarRval, nullptr, Middleman_CreateCFTypeRval) \
  MACRO(CFUUIDCreateString, RR_ScalarRval)                       \
  MACRO(CFUUIDGetUUIDBytes, RR_ComplexScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CGAffineTransformConcat, RR_OversizeRval<sizeof(CGAffineTransform)>) \
  MACRO(CGBitmapContextCreateImage, RR_ScalarRval)               \
  MACRO(CGBitmapContextCreateWithData,                           \
        RR_Compose<RR_ScalarRval, RR_CGBitmapContextCreateWithData>, nullptr, \
        Middleman_CGBitmapContextCreateWithData)                 \
  MACRO(CGBitmapContextGetBytesPerRow, RR_ScalarRval)            \
  MACRO(CGBitmapContextGetHeight, RR_ScalarRval)                 \
  MACRO(CGBitmapContextGetWidth, RR_ScalarRval)                  \
  MACRO(CGColorRelease, RR_ScalarRval)                           \
  MACRO(CGColorSpaceCopyICCProfile, RR_ScalarRval)               \
  MACRO(CGColorSpaceCreateDeviceGray, RR_ScalarRval, nullptr, Middleman_CreateCFTypeRval) \
  MACRO(CGColorSpaceCreateDeviceRGB, RR_ScalarRval, nullptr, Middleman_CreateCFTypeRval) \
  MACRO(CGColorSpaceCreatePattern, RR_ScalarRval)                \
  MACRO(CGColorSpaceRelease, RR_ScalarRval)                      \
  MACRO(CGContextBeginTransparencyLayerWithRect)                 \
  MACRO(CGContextClipToRects, RR_ScalarRval, nullptr,            \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_Buffer<1, 2, CGRect>>) \
  MACRO(CGContextConcatCTM, nullptr, nullptr,                    \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_StackArgumentData<sizeof(CGAffineTransform)>>) \
  MACRO(CGContextDrawImage, RR_FlushCGContext<0>)                \
  MACRO(CGContextEndTransparencyLayer)                           \
  MACRO(CGContextFillRect, RR_FlushCGContext<0>, nullptr,        \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_StackArgumentData<sizeof(CGRect)>, \
                          Middleman_FlushCGContext<0>>)          \
  MACRO(CGContextGetClipBoundingBox, RR_OversizeRval<sizeof(CGRect)>) \
  MACRO(CGContextGetCTM, RR_OversizeRval<sizeof(CGAffineTransform)>) \
  MACRO(CGContextGetType, RR_ScalarRval)                         \
  MACRO(CGContextGetUserSpaceToDeviceSpaceTransform, RR_OversizeRval<sizeof(CGAffineTransform)>) \
  MACRO(CGContextRestoreGState, nullptr, Preamble_CGContextRestoreGState, \
        Middleman_UpdateCFTypeArg<0>)                            \
  MACRO(CGContextSaveGState, nullptr, nullptr, Middleman_UpdateCFTypeArg<0>) \
  MACRO(CGContextSetAllowsFontSubpixelPositioning, nullptr, nullptr, \
        Middleman_UpdateCFTypeArg<0>)                            \
  MACRO(CGContextSetAllowsFontSubpixelQuantization, nullptr, nullptr, \
        Middleman_UpdateCFTypeArg<0>)                            \
  MACRO(CGContextSetAlpha, nullptr, nullptr, Middleman_UpdateCFTypeArg<0>) \
  MACRO(CGContextSetBaseCTM, nullptr, nullptr,                   \
        Middleman_Compose<Middleman_UpdateCFTypeArg<0>,          \
                          Middleman_StackArgumentData<sizeof(CGAffineTransform)>>) \
  MACRO(CGContextSetCTM, nullptr, nullptr,                       \
        Middleman_Compose<Middleman_UpdateCFTypeArg<0>,          \
                          Middleman_StackArgumentData<sizeof(CGAffineTransform)>>) \
  MACRO(CGContextSetGrayFillColor, nullptr, nullptr, Middleman_UpdateCFTypeArg<0>) \
  MACRO(CGContextSetRGBFillColor)                                \
  MACRO(CGContextSetShouldAntialias, nullptr, nullptr, Middleman_UpdateCFTypeArg<0>) \
  MACRO(CGContextSetShouldSmoothFonts, nullptr, nullptr, Middleman_UpdateCFTypeArg<0>) \
  MACRO(CGContextSetShouldSubpixelPositionFonts, nullptr, nullptr, Middleman_UpdateCFTypeArg<0>) \
  MACRO(CGContextSetShouldSubpixelQuantizeFonts, nullptr, nullptr, Middleman_UpdateCFTypeArg<0>) \
  MACRO(CGContextSetTextDrawingMode, nullptr, nullptr, Middleman_UpdateCFTypeArg<0>) \
  MACRO(CGContextSetTextMatrix, nullptr, nullptr,                \
        Middleman_Compose<Middleman_UpdateCFTypeArg<0>,          \
                          Middleman_StackArgumentData<sizeof(CGAffineTransform)>>) \
  MACRO(CGContextScaleCTM, nullptr, nullptr, Middleman_UpdateCFTypeArg<0>) \
  MACRO(CGContextTranslateCTM, nullptr, nullptr, Middleman_UpdateCFTypeArg<0>) \
  MACRO(CGDataProviderCreateWithData, RR_Compose<RR_ScalarRval, RR_CGDataProviderCreateWithData>, \
        nullptr, Middleman_CGDataProviderCreateWithData)         \
  MACRO(CGDataProviderRelease, nullptr, nullptr, nullptr, Preamble_Veto<0>) \
  MACRO(CGDisplayCopyColorSpace, RR_ScalarRval)                  \
  MACRO(CGDisplayIOServicePort, RR_ScalarRval)                   \
  MACRO(CGEventSourceCounterForEventType, RR_ScalarRval)         \
  MACRO(CGFontCopyTableForTag, RR_ScalarRval, nullptr,           \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CreateCFTypeRval>) \
  MACRO(CGFontCopyTableTags, RR_ScalarRval, nullptr,             \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CreateCFTypeRval>) \
  MACRO(CGFontCopyVariations, RR_ScalarRval, nullptr,            \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CreateCFTypeRval>) \
  MACRO(CGFontCreateCopyWithVariations, RR_ScalarRval)           \
  MACRO(CGFontCreateWithDataProvider, RR_ScalarRval, nullptr,    \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CreateCFTypeRval>) \
  MACRO(CGFontCreateWithFontName, RR_ScalarRval, nullptr,        \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CreateCFTypeRval>) \
  MACRO(CGFontCreateWithPlatformFont, RR_ScalarRval)             \
  MACRO(CGFontGetAscent, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CGFontGetCapHeight, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CGFontGetDescent, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CGFontGetFontBBox, RR_OversizeRval<sizeof(CGRect)>, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<1>, Middleman_OversizeRval<sizeof(CGRect)>>) \
  MACRO(CGFontGetGlyphAdvances, RR_Compose<RR_ScalarRval, RR_WriteBuffer<3, 2, int>>, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_Buffer<1, 2, CGGlyph>,       \
                          Middleman_WriteBuffer<3, 2, int>>)     \
  MACRO(CGFontGetGlyphBBoxes, RR_Compose<RR_ScalarRval, RR_WriteBuffer<3, 2, CGRect>>, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_Buffer<1, 2, CGGlyph>,       \
                          Middleman_WriteBuffer<3, 2, CGRect>>)  \
  MACRO(CGFontGetGlyphPath, RR_ScalarRval)                       \
  MACRO(CGFontGetLeading, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CGFontGetUnitsPerEm, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CGFontGetXHeight, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CGImageGetHeight, RR_ScalarRval)                         \
  MACRO(CGImageGetWidth, RR_ScalarRval)                          \
  MACRO(CGImageRelease, RR_ScalarRval)                           \
  MACRO(CGMainDisplayID, RR_ScalarRval)                          \
  MACRO(CGPathAddPath)                                           \
  MACRO(CGPathApply, nullptr, Preamble_CGPathApply)              \
  MACRO(CGPathContainsPoint, RR_ScalarRval)                      \
  MACRO(CGPathCreateMutable, RR_ScalarRval)                      \
  MACRO(CGPathGetBoundingBox, RR_OversizeRval<sizeof(CGRect)>)   \
  MACRO(CGPathGetCurrentPoint, RR_ComplexFloatRval)              \
  MACRO(CGPathIsEmpty, RR_ScalarRval)                            \
  MACRO(CGSSetDebugOptions, RR_ScalarRval)                       \
  MACRO(CGSShutdownServerConnections)                            \
  MACRO(CTFontCopyFamilyName, RR_ScalarRval, nullptr,            \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CreateCFTypeRval>) \
  MACRO(CTFontCopyFeatures, RR_ScalarRval, nullptr,              \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CreateCFTypeRval>) \
  MACRO(CTFontCopyFontDescriptor, RR_ScalarRval, nullptr,        \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CreateCFTypeRval>) \
  MACRO(CTFontCopyGraphicsFont, RR_ScalarRval, nullptr,          \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CreateCFTypeRval>) \
  MACRO(CTFontCopyTable, RR_ScalarRval, nullptr,                 \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CreateCFTypeRval>) \
  MACRO(CTFontCopyVariationAxes, RR_ScalarRval, nullptr,         \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CreateCFTypeRval>) \
  MACRO(CTFontCreateForString, RR_ScalarRval, nullptr,           \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CFTypeArg<1>, Middleman_CreateCFTypeRval>) \
  MACRO(CTFontCreatePathForGlyph, RR_ScalarRval, nullptr,        \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_BufferFixedSize<2, sizeof(CGAffineTransform)>, \
                          Middleman_CreateCFTypeRval>)           \
  MACRO(CTFontCreateWithFontDescriptor, RR_ScalarRval, nullptr,  \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_BufferFixedSize<1, sizeof(CGAffineTransform)>, \
                          Middleman_CreateCFTypeRval>)                 \
  MACRO(CTFontCreateWithGraphicsFont, RR_ScalarRval, nullptr,    \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_BufferFixedSize<1, sizeof(CGAffineTransform)>, \
                          Middleman_CFTypeArg<2>,                \
                          Middleman_CreateCFTypeRval>)                 \
  MACRO(CTFontCreateWithName, RR_ScalarRval, nullptr,            \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_BufferFixedSize<1, sizeof(CGAffineTransform)>, \
                          Middleman_CreateCFTypeRval>)                 \
  MACRO(CTFontDescriptorCopyAttribute, RR_ScalarRval, nullptr,   \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CFTypeArg<1>, Middleman_CreateCFTypeRval>) \
  MACRO(CTFontDescriptorCreateCopyWithAttributes, RR_ScalarRval, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CFTypeArg<1>, Middleman_CreateCFTypeRval>) \
  MACRO(CTFontDescriptorCreateMatchingFontDescriptors, RR_ScalarRval, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CFTypeArg<1>, Middleman_CreateCFTypeRval>) \
  MACRO(CTFontDescriptorCreateWithAttributes, RR_ScalarRval, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CreateCFTypeRval>) \
  MACRO(CTFontDrawGlyphs, RR_FlushCGContext<4>, nullptr,         \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_CFTypeArg<4>,                \
                          Middleman_Buffer<1, 3, CGGlyph>,       \
                          Middleman_Buffer<2, 3, CGPoint>,       \
                          Middleman_FlushCGContext<4>>)          \
  MACRO(CTFontGetAdvancesForGlyphs,                              \
        RR_Compose<RR_FloatRval, RR_WriteOptionalBuffer<3, 4, CGSize>>, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_Buffer<2, 4, CGGlyph>,       \
                          Middleman_WriteBuffer<3, 4, CGSize>>)  \
  MACRO(CTFontGetAscent, RR_FloatRval, nullptr, Middleman_CFTypeArg<0>)   \
  MACRO(CTFontGetBoundingBox, RR_OversizeRval<sizeof(CGRect)>, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<1>, Middleman_OversizeRval<sizeof(CGRect)>>) \
  MACRO(CTFontGetBoundingRectsForGlyphs,                         \
        /* Argument indexes here are off by one due to the oversize rval. */ \
        RR_Compose<RR_OversizeRval<sizeof(CGRect)>,              \
                   RR_WriteOptionalBuffer<4, 5, CGRect>>, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<1>,                \
                          Middleman_Buffer<3, 5, CGGlyph>,       \
                          Middleman_OversizeRval<sizeof(CGRect)>, \
                          Middleman_WriteBuffer<4, 5, CGRect>>)  \
  MACRO(CTFontGetCapHeight, RR_FloatRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CTFontGetDescent, RR_FloatRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CTFontGetGlyphCount, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CTFontGetGlyphsForCharacters,                            \
        RR_Compose<RR_ScalarRval, RR_WriteBuffer<2, 3, CGGlyph>>, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_Buffer<1, 3, UniChar>,       \
                          Middleman_WriteBuffer<2, 3, CGGlyph>>) \
  MACRO(CTFontGetLeading, RR_FloatRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CTFontGetSize, RR_FloatRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CTFontGetSymbolicTraits, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CTFontGetUnderlinePosition, RR_FloatRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CTFontGetUnderlineThickness, RR_FloatRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CTFontGetUnitsPerEm, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CTFontGetXHeight, RR_FloatRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CTFontManagerCopyAvailableFontFamilyNames, RR_ScalarRval) \
  MACRO(CTFontManagerRegisterFontsForURLs, RR_ScalarRval)        \
  MACRO(CTFontManagerSetAutoActivationSetting)                   \
  MACRO(CTLineCreateWithAttributedString, RR_ScalarRval, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CreateCFTypeRval>) \
  MACRO(CTLineGetGlyphRuns, RR_ScalarRval, nullptr,              \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CFTypeRval>) \
  MACRO(CTRunGetAttributes, RR_ScalarRval, nullptr,              \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CFTypeRval>) \
  MACRO(CTRunGetGlyphCount, RR_ScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  MACRO(CTRunGetGlyphsPtr, RR_CTRunGetElements<CGGlyph, CTRunGetGlyphs>, nullptr, \
        Middleman_CTRunGetElements<CGGlyph, CTRunGetGlyphs>)     \
  MACRO(CTRunGetPositionsPtr, RR_CTRunGetElements<CGPoint, CTRunGetPositions>, nullptr, \
        Middleman_CTRunGetElements<CGPoint, CTRunGetPositions>)  \
  MACRO(CTRunGetStringIndicesPtr, RR_CTRunGetElements<CFIndex, CTRunGetStringIndices>, nullptr, \
        Middleman_CTRunGetElements<CFIndex, CTRunGetStringIndices>) \
  MACRO(CTRunGetStringRange, RR_ComplexScalarRval, nullptr, Middleman_CFTypeArg<0>) \
  /* Argument indexes are off by one here as the CFRange argument uses two slots. */ \
  MACRO(CTRunGetTypographicBounds,                               \
        RR_Compose<RR_FloatRval,                                 \
                   RR_WriteOptionalBufferFixedSize<3, sizeof(CGFloat)>, \
                   RR_WriteOptionalBufferFixedSize<4, sizeof(CGFloat)>, \
                   RR_WriteOptionalBufferFixedSize<5, sizeof(CGFloat)>>, nullptr, \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_WriteBufferFixedSize<3, sizeof(CGFloat)>, \
                          Middleman_WriteBufferFixedSize<4, sizeof(CGFloat)>, \
                          Middleman_WriteBufferFixedSize<5, sizeof(CGFloat)>>) \
  MACRO(CUIDraw, nullptr, nullptr,                               \
        Middleman_Compose<Middleman_CFTypeArg<0>,                \
                          Middleman_CFTypeArg<1>,                \
                          Middleman_CFTypeArg<2>,                \
                          Middleman_StackArgumentData<sizeof(CGRect)>>) \
  MACRO(FSCompareFSRefs, RR_ScalarRval)                          \
  MACRO(FSGetVolumeInfo, RR_Compose<                             \
                           RR_ScalarRval,                        \
                           RR_WriteBufferFixedSize<5, sizeof(HFSUniStr255)>, \
                           RR_WriteBufferFixedSize<6, sizeof(FSRef)>>) \
  MACRO(FSFindFolder, RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<3, sizeof(FSRef)>>) \
  MACRO(Gestalt, RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<1, sizeof(SInt32)>>) \
  MACRO(GetEventClass, RR_ScalarRval)                            \
  MACRO(GetCurrentEventQueue, RR_ScalarRval)                     \
  MACRO(GetCurrentProcess,                                       \
        RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<0, sizeof(ProcessSerialNumber)>>) \
  MACRO(GetEventAttributes, RR_ScalarRval)                       \
  MACRO(GetEventDispatcherTarget, RR_ScalarRval)                 \
  MACRO(GetEventKind, RR_ScalarRval)                             \
  MACRO(HIThemeDrawButton, RR_ScalarRval)                        \
  MACRO(HIThemeDrawFrame, RR_ScalarRval)                         \
  MACRO(HIThemeDrawGroupBox, RR_ScalarRval)                      \
  MACRO(HIThemeDrawGrowBox, RR_ScalarRval)                       \
  MACRO(HIThemeDrawMenuBackground, RR_ScalarRval)                \
  MACRO(HIThemeDrawMenuItem, RR_ScalarRval)                      \
  MACRO(HIThemeDrawMenuSeparator, RR_ScalarRval)                 \
  MACRO(HIThemeDrawSeparator, RR_ScalarRval)                     \
  MACRO(HIThemeDrawTabPane, RR_ScalarRval)                       \
  MACRO(HIThemeDrawTrack, RR_ScalarRval)                         \
  MACRO(HIThemeGetGrowBoxBounds,                                 \
        RR_Compose<RR_ScalarRval, RR_WriteBufferFixedSize<2, sizeof(HIRect)>>) \
  MACRO(HIThemeSetFill, RR_ScalarRval)                           \
  MACRO(IORegistryEntrySearchCFProperty, RR_ScalarRval)          \
  MACRO(LSCopyAllHandlersForURLScheme, RR_ScalarRval)            \
  MACRO(LSCopyApplicationForMIMEType,                            \
        RR_Compose<RR_ScalarRval, RR_WriteOptionalBufferFixedSize<2, sizeof(CFURLRef)>>) \
  MACRO(LSCopyItemAttribute,                                     \
        RR_Compose<RR_ScalarRval, RR_WriteOptionalBufferFixedSize<3, sizeof(CFTypeRef)>>) \
  MACRO(LSCopyKindStringForMIMEType,                             \
        RR_Compose<RR_ScalarRval, RR_WriteOptionalBufferFixedSize<1, sizeof(CFStringRef)>>) \
  MACRO(LSGetApplicationForInfo, RR_Compose<                     \
                                   RR_ScalarRval,                \
                                   RR_WriteOptionalBufferFixedSize<4, sizeof(FSRef)>, \
                                   RR_WriteOptionalBufferFixedSize<5, sizeof(CFURLRef)>>) \
  MACRO(LSGetApplicationForURL, RR_Compose<                      \
                                   RR_ScalarRval,                \
                                   RR_WriteOptionalBufferFixedSize<2, sizeof(FSRef)>, \
                                   RR_WriteOptionalBufferFixedSize<3, sizeof(CFURLRef)>>) \
  MACRO(NSClassFromString, RR_ScalarRval, nullptr,               \
        Middleman_Compose<Middleman_CFTypeArg<0>, Middleman_CFTypeRval>) \
  MACRO(NSRectFill)                                              \
  MACRO(NSSearchPathForDirectoriesInDomains, RR_ScalarRval)      \
  MACRO(NSSetFocusRingStyle, RR_ScalarRval)                      \
  MACRO(NSTemporaryDirectory, RR_ScalarRval)                     \
  MACRO(OSSpinLockLock, nullptr, Preamble_OSSpinLockLock)        \
  MACRO(ReleaseEvent, RR_ScalarRval)                             \
  MACRO(RemoveEventFromQueue, RR_ScalarRval)                     \
  MACRO(RetainEvent, RR_ScalarRval)                              \
  MACRO(SCDynamicStoreCopyProxies, RR_ScalarRval)                \
  MACRO(SCDynamicStoreCreate, RR_ScalarRval)                     \
  MACRO(SCDynamicStoreCreateRunLoopSource, RR_ScalarRval)        \
  MACRO(SCDynamicStoreKeyCreateProxies, RR_ScalarRval)           \
  MACRO(SCDynamicStoreSetNotificationKeys, RR_ScalarRval)        \
  MACRO(SendEventToEventTarget, RR_ScalarRval)                   \
  /* These are not public APIs, but other redirected functions may be aliases for */ \
  /* these which are dynamically installed on the first call in a way that our */ \
  /* redirection mechanism doesn't completely account for. */    \
  MACRO(SLDisplayCopyColorSpace, RR_ScalarRval)                  \
  MACRO(SLDisplayIOServicePort, RR_ScalarRval)                   \
  MACRO(SLEventSourceCounterForEventType, RR_ScalarRval)         \
  MACRO(SLMainDisplayID, RR_ScalarRval)                          \
  MACRO(SLSSetDenyWindowServerConnections, RR_ScalarRval)        \
  MACRO(SLSShutdownServerConnections)

#define MAKE_CALL_EVENT(aName, ...)  CallEvent_ ##aName ,

enum CallEvent {                                \
  FOR_EACH_REDIRECTION(MAKE_CALL_EVENT)         \
  CallEvent_Count                               \
};

#undef MAKE_CALL_EVENT

///////////////////////////////////////////////////////////////////////////////
// Callbacks
///////////////////////////////////////////////////////////////////////////////

enum CallbackEvent {
  CallbackEvent_CFRunLoopPerformCallBack,
  CallbackEvent_CGPathApplierFunction
};

typedef void (*CFRunLoopPerformCallBack)(void*);

static void
CFRunLoopPerformCallBackWrapper(void* aInfo)
{
  RecordReplayCallback(CFRunLoopPerformCallBack, &aInfo);
  rrc.mFunction(aInfo);

  // Make sure we service any callbacks that have been posted for the main
  // thread whenever the main thread's message loop has any activity.
  PauseMainThreadAndServiceCallbacks();
}

static size_t
CGPathElementPointCount(CGPathElement* aElement)
{
  switch (aElement->type) {
  case kCGPathElementCloseSubpath:         return 0;
  case kCGPathElementMoveToPoint:
  case kCGPathElementAddLineToPoint:       return 1;
  case kCGPathElementAddQuadCurveToPoint:  return 2;
  case kCGPathElementAddCurveToPoint:      return 3;
  default: MOZ_CRASH();
  }
}

static void
CGPathApplierFunctionWrapper(void* aInfo, CGPathElement* aElement)
{
  RecordReplayCallback(CGPathApplierFunction, &aInfo);

  CGPathElement replayElement;
  if (IsReplaying()) {
    aElement = &replayElement;
  }

  aElement->type = (CGPathElementType) RecordReplayValue(aElement->type);

  size_t npoints = CGPathElementPointCount(aElement);
  if (IsReplaying()) {
    aElement->points = new CGPoint[npoints];
  }
  RecordReplayBytes(aElement->points, npoints * sizeof(CGPoint));

  rrc.mFunction(aInfo, aElement);

  if (IsReplaying()) {
    delete[] aElement->points;
  }
}

void
ReplayInvokeCallback(size_t aCallbackId)
{
  MOZ_RELEASE_ASSERT(IsReplaying());
  switch (aCallbackId) {
  case CallbackEvent_CFRunLoopPerformCallBack:
    CFRunLoopPerformCallBackWrapper(nullptr);
    break;
  case CallbackEvent_CGPathApplierFunction:
    CGPathApplierFunctionWrapper(nullptr, nullptr);
    break;
  default:
    MOZ_CRASH();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Middleman Call Helpers
///////////////////////////////////////////////////////////////////////////////

// Inputs that originate from static data in the replaying process itself
// rather than from previous middleman calls.
enum class ObjCInputKind {
  StaticClass,
  ConstantString,
};

// Internal layout of a constant compile time CFStringRef.
struct CFConstantString {
  Class mClass;
  size_t mStuff;
  char* mData;
  size_t mLength;
};

// Capture an Objective C or CoreFoundation input to a call, which may come
// either from another middleman call, or from static data in the replaying
// process.
static void
Middleman_ObjCInput(MiddlemanCallContext& aCx, id* aThingPtr)
{
  MOZ_RELEASE_ASSERT(aCx.AccessPreface());

  if (Middleman_SystemInput(aCx, (const void**) aThingPtr)) {
    // This value came from a previous middleman call.
    return;
  }

  MOZ_RELEASE_ASSERT(aCx.AccessInput());

  if (aCx.mPhase == MiddlemanCallPhase::ReplayInput) {
    // Try to determine where this object came from.

    // List of the Objective C classes which messages might be sent to directly.
    static const char* gStaticClasses[] = {
      "NSAutoreleasePool",
      "NSColor",
      "NSDictionary",
      "NSFont",
      "NSFontManager",
      "NSNumber",
      "NSString",
      "NSWindow",
    };

    // Watch for messages sent to particular classes.
    for (const char* className : gStaticClasses) {
      Class cls = objc_lookUpClass(className);
      if (cls == (Class) *aThingPtr) {
        aCx.WriteInputScalar((size_t) ObjCInputKind::StaticClass);
        size_t len = strlen(className) + 1;
        aCx.WriteInputScalar(len);
        aCx.WriteInputBytes(className, len);
        return;
      }
    }

    // Watch for constant compile time strings baked into the generated code or
    // stored in system libraries. Be careful when accessing the pointer as in
    // the case where a middleman call hook for a function is missing the
    // pointer could have originated from the recording and its address may not
    // be mapped. In this case we would rather gracefully recover and fail to
    // paint, instead of crashing.
    if (MemoryRangeIsTracked(*aThingPtr, sizeof(CFConstantString))) {
      CFConstantString* str = (CFConstantString*) *aThingPtr;
      if (str->mClass == objc_lookUpClass("__NSCFConstantString") &&
          str->mLength <= 4096 && // Sanity check.
          MemoryRangeIsTracked(str->mData, str->mLength)) {
        InfallibleVector<UniChar> buffer;
        NS_ConvertUTF8toUTF16 converted(str->mData, str->mLength);
        aCx.WriteInputScalar((size_t) ObjCInputKind::ConstantString);
        aCx.WriteInputScalar(str->mLength);
        aCx.WriteInputBytes(converted.get(), str->mLength * sizeof(UniChar));
        return;
      }
    }

    aCx.MarkAsFailed();
    return;
  }

  switch ((ObjCInputKind) aCx.ReadInputScalar()) {
  case ObjCInputKind::StaticClass: {
    size_t len = aCx.ReadInputScalar();
    UniquePtr<char[]> className(new char[len]);
    aCx.ReadInputBytes(className.get(), len);
    *aThingPtr = (id) objc_lookUpClass(className.get());
    break;
  }
  case ObjCInputKind::ConstantString: {
    size_t len = aCx.ReadInputScalar();
    UniquePtr<UniChar[]> contents(new UniChar[len]);
    aCx.ReadInputBytes(contents.get(), len * sizeof(UniChar));
    *aThingPtr = (id) CFStringCreateWithCharacters(kCFAllocatorDefault, contents.get(), len);
    break;
  }
  default:
    MOZ_CRASH();
  }
}

template <size_t Argument>
static void
Middleman_CFTypeArg(MiddlemanCallContext& aCx)
{
  if (aCx.AccessPreface()) {
    auto& object = aCx.mArguments->Arg<Argument, id>();
    Middleman_ObjCInput(aCx, &object);
  }
}

static void
Middleman_CFTypeOutput(MiddlemanCallContext& aCx, CFTypeRef* aOutput, bool aOwnsReference)
{
  Middleman_SystemOutput(aCx, (const void**) aOutput);

  if (*aOutput) {
    switch (aCx.mPhase) {
    case MiddlemanCallPhase::MiddlemanOutput:
      if (!aOwnsReference) {
        CFRetain(*aOutput);
      }
      break;
    case MiddlemanCallPhase::MiddlemanRelease:
      CFRelease(*aOutput);
      break;
    default:
      break;
    }
  }
}

// For APIs using the 'Get' rule: no reference is held on the returned value.
static void
Middleman_CFTypeRval(MiddlemanCallContext& aCx)
{
  auto& rval = aCx.mArguments->Rval<CFTypeRef>();
  Middleman_CFTypeOutput(aCx, &rval, /* aOwnsReference = */ false);
}

// For APIs using the 'Create' rule: a reference is held on the returned
// value which must be released.
static void
Middleman_CreateCFTypeRval(MiddlemanCallContext& aCx)
{
  auto& rval = aCx.mArguments->Rval<CFTypeRef>();
  Middleman_CFTypeOutput(aCx, &rval, /* aOwnsReference = */ true);
}

template <size_t Argument>
static void
Middleman_CFTypeOutputArg(MiddlemanCallContext& aCx)
{
  Middleman_WriteBufferFixedSize<Argument, sizeof(const void*)>(aCx);

  auto arg = aCx.mArguments->Arg<Argument, const void**>();
  Middleman_CFTypeOutput(aCx, arg, /* aOwnsReference = */ false);
}

// For APIs whose result will be released by the middleman's autorelease pool.
static void
Middleman_AutoreleaseCFTypeRval(MiddlemanCallContext& aCx)
{
  auto& rval = aCx.mArguments->Rval<const void*>();
  Middleman_SystemOutput(aCx, &rval);
}

// For functions which have an input CFType value and also have side effects on
// that value, this associates the call with its own input value so that this
// will be treated as a dependent for any future calls using the value.
template <size_t Argument>
static void
Middleman_UpdateCFTypeArg(MiddlemanCallContext& aCx)
{
  auto arg = aCx.mArguments->Arg<Argument, const void*>();

  Middleman_CFTypeArg<Argument>(aCx);
  Middleman_SystemOutput(aCx, &arg, /* aUpdating = */ true);
}

template <int Error = EAGAIN>
static PreambleResult
Preamble_SetError(CallArguments* aArguments)
{
  aArguments->Rval<ssize_t>() = -1;
  errno = Error;
  return PreambleResult::Veto;
}

///////////////////////////////////////////////////////////////////////////////
// system call redirections
///////////////////////////////////////////////////////////////////////////////

static void
RR_recvmsg(Stream& aEvents, CallArguments* aArguments, ErrorType* aError)
{
  auto& msg = aArguments->Arg<1, struct msghdr*>();

  aEvents.CheckInput(msg->msg_iovlen);
  for (int i = 0; i < msg->msg_iovlen; i++) {
    aEvents.CheckInput(msg->msg_iov[i].iov_len);
  }

  aEvents.RecordOrReplayValue(&msg->msg_flags);
  aEvents.RecordOrReplayValue(&msg->msg_controllen);
  aEvents.RecordOrReplayBytes(msg->msg_control, msg->msg_controllen);

  size_t nbytes = aArguments->Rval<size_t>();
  for (int i = 0; nbytes && i < msg->msg_iovlen; i++) {
    struct iovec* iov = &msg->msg_iov[i];
    size_t iovbytes = nbytes <= iov->iov_len ? nbytes : iov->iov_len;
    aEvents.RecordOrReplayBytes(iov->iov_base, iovbytes);
    nbytes -= iovbytes;
  }
  MOZ_RELEASE_ASSERT(nbytes == 0);
}

static PreambleResult
MiddlemanPreamble_sendmsg(CallArguments* aArguments)
{
  // Silently pretend that sends succeed after diverging from the recording.
  size_t totalSize = 0;
  auto msg = aArguments->Arg<1, msghdr*>();
  for (int i = 0; i < msg->msg_iovlen; i++) {
    totalSize += msg->msg_iov[i].iov_len;
  }
  aArguments->Rval<size_t>() = totalSize;
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_mprotect(CallArguments* aArguments)
{
  // Ignore any mprotect calls that occur after saving a checkpoint.
  if (!HasSavedCheckpoint()) {
    return PreambleResult::PassThrough;
  }
  aArguments->Rval<ssize_t>() = 0;
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_mmap(CallArguments* aArguments)
{
  auto& address = aArguments->Arg<0, void*>();
  auto& size = aArguments->Arg<1, size_t>();
  auto& prot = aArguments->Arg<2, size_t>();
  auto& flags = aArguments->Arg<3, size_t>();
  auto& fd = aArguments->Arg<4, size_t>();
  auto& offset = aArguments->Arg<5, size_t>();

  MOZ_RELEASE_ASSERT(address == PageBase(address));

  // Make sure that fixed mappings do not interfere with snapshot state.
  if (flags & MAP_FIXED) {
    CheckFixedMemory(address, RoundupSizeToPageBoundary(size));
  }

  void* memory = nullptr;
  if ((flags & MAP_ANON) || (IsReplaying() && !AreThreadEventsPassedThrough())) {
    // Get an anonymous mapping for the result.
    if (flags & MAP_FIXED) {
      // For fixed allocations, make sure this memory region is mapped and zero.
      if (!HasSavedCheckpoint()) {
        // Make sure this memory region is writable.
        OriginalCall(mprotect, int, address, size, PROT_READ | PROT_WRITE | PROT_EXEC);
      }
      memset(address, 0, size);
      memory = address;
    } else {
      memory = AllocateMemoryTryAddress(address, RoundupSizeToPageBoundary(size),
                                        MemoryKind::Tracked);
    }
  } else {
    // We have to call mmap itself, which can change memory protection flags
    // for memory that is already allocated. If we haven't saved a checkpoint
    // then this is no problem, but after saving a checkpoint we have to make
    // sure that protection flags are what we expect them to be.
    int newProt = HasSavedCheckpoint() ? (PROT_READ | PROT_EXEC) : prot;
    memory = OriginalCall(mmap, void*, address, size, newProt, flags, fd, offset);

    if (flags & MAP_FIXED) {
      MOZ_RELEASE_ASSERT(memory == address);
      RestoreWritableFixedMemory(memory, RoundupSizeToPageBoundary(size));
    } else if (memory && memory != (void*)-1) {
      RegisterAllocatedMemory(memory, RoundupSizeToPageBoundary(size), MemoryKind::Tracked);
    }
  }

  if (!(flags & MAP_ANON) && !AreThreadEventsPassedThrough()) {
    // Include the data just mapped in the recording.
    MOZ_RELEASE_ASSERT(memory && memory != (void*)-1);
    RecordReplayBytes(memory, size);
  }

  aArguments->Rval<void*>() = memory;
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_munmap(CallArguments* aArguments)
{
  auto& address = aArguments->Arg<0, void*>();
  auto& size = aArguments->Arg<1, size_t>();

  DeallocateMemory(address, size, MemoryKind::Tracked);
  aArguments->Rval<ssize_t>() = 0;
  return PreambleResult::Veto;
}

static PreambleResult
MiddlemanPreamble_write(CallArguments* aArguments)
{
  // Silently pretend that writes succeed after diverging from the recording.
  aArguments->Rval<size_t>() = aArguments->Arg<2, size_t>();
  return PreambleResult::Veto;
}

static void
RR_getsockopt(Stream& aEvents, CallArguments* aArguments, ErrorType* aError)
{
  auto& optval = aArguments->Arg<3, void*>();
  auto& optlen = aArguments->Arg<4, int*>();

  // The initial length has already been clobbered while recording, but make
  // sure there is enough room for the copy while replaying.
  int initLen = *optlen;
  aEvents.RecordOrReplayValue(optlen);
  MOZ_RELEASE_ASSERT(*optlen <= initLen);

  aEvents.RecordOrReplayBytes(optval, *optlen);
}

static PreambleResult
Preamble_getpid(CallArguments* aArguments)
{
  if (!AreThreadEventsPassedThrough()) {
    // Don't involve the recording with getpid calls, so that this can be used
    // after diverging from the recording.
    aArguments->Rval<size_t>() = GetRecordingPid();
    return PreambleResult::Veto;
  }
  return PreambleResult::Redirect;
}

static PreambleResult
Preamble_fcntl(CallArguments* aArguments)
{
  // We don't record any outputs for fcntl other than its return value, but
  // some commands have an output parameter they write additional data to.
  // Handle this by only allowing a limited set of commands to be used when
  // events are not passed through and we are recording/replaying the outputs.
  if (AreThreadEventsPassedThrough()) {
    return PreambleResult::Redirect;
  }

  auto& cmd = aArguments->Arg<1, size_t>();
  switch (cmd) {
  case F_GETFL:
  case F_SETFL:
  case F_GETFD:
  case F_SETFD:
  case F_NOCACHE:
  case F_SETLK:
  case F_SETLKW:
    break;
  default:
    MOZ_CRASH();
  }
  return PreambleResult::Redirect;
}

static PreambleResult
MiddlemanPreamble_fcntl(CallArguments* aArguments)
{
  auto& cmd = aArguments->Arg<1, size_t>();
  switch (cmd) {
  case F_SETFL:
  case F_SETFD:
  case F_SETLK:
  case F_SETLKW:
    break;
  default:
    MOZ_CRASH();
  }
  aArguments->Rval<ssize_t>() = 0;
  return PreambleResult::Veto;
}

static PreambleResult
Preamble___disable_threadsignal(CallArguments* aArguments)
{
  // __disable_threadsignal is called when a thread finishes. During replay a
  // terminated thread can cause problems such as changing access bits on
  // tracked memory behind the scenes.
  //
  // Ideally, threads will never try to finish when we are replaying, since we
  // are supposed to have control over all threads in the system and only spawn
  // threads which will run forever. Unfortunately, GCD might have already
  // spawned threads before we were able to install our redirections, so use a
  // fallback here to keep these threads from terminating.
  if (IsReplaying()) {
    Thread::WaitForeverNoIdle();
  }
  return PreambleResult::PassThrough;
}

static void
RR___sysctl(Stream& aEvents, CallArguments* aArguments, ErrorType* aError)
{
  auto& old = aArguments->Arg<2, void*>();
  auto& oldlenp = aArguments->Arg<3, size_t*>();

  aEvents.CheckInput((old ? 1 : 0) | (oldlenp ? 2 : 0));
  if (oldlenp) {
    aEvents.RecordOrReplayValue(oldlenp);
  }
  if (old) {
    aEvents.RecordOrReplayBytes(old, *oldlenp);
  }
}

static PreambleResult
Preamble___workq_kernreturn(CallArguments* aArguments)
{
  // Busy-wait until initialization is complete.
  while (!gInitialized) {
    ThreadYield();
  }

  // Make sure we know this thread exists.
  Thread::Current();

  RecordReplayInvokeCall(CallEvent___workq_kernreturn, aArguments);
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_start_wqthread(CallArguments* aArguments)
{
  // When replaying we don't want system threads to run, but by the time we
  // initialize the record/replay system GCD has already started running.
  // Use this redirection to watch for new threads being spawned by GCD, and
  // suspend them immediately.
  if (IsReplaying()) {
    Thread::WaitForeverNoIdle();
  }

  RecordReplayInvokeCall(CallEvent_start_wqthread, aArguments);
  return PreambleResult::Veto;
}

///////////////////////////////////////////////////////////////////////////////
// pthreads redirections
///////////////////////////////////////////////////////////////////////////////

static void
DirectLockMutex(pthread_mutex_t* aMutex)
{
  AutoPassThroughThreadEvents pt;
  ssize_t rv = OriginalCall(pthread_mutex_lock, ssize_t, aMutex);
  MOZ_RELEASE_ASSERT(rv == 0);
}

static void
DirectUnlockMutex(pthread_mutex_t* aMutex)
{
  AutoPassThroughThreadEvents pt;
  ssize_t rv = OriginalCall(pthread_mutex_unlock, ssize_t, aMutex);
  MOZ_RELEASE_ASSERT(rv == 0);
}

// Handle a redirection which releases a mutex, waits in some way for a cvar,
// and reacquires the mutex before returning.
static ssize_t
WaitForCvar(pthread_mutex_t* aMutex, pthread_cond_t* aCond, bool aRecordReturnValue,
            const std::function<ssize_t()>& aCallback)
{
  Lock* lock = Lock::Find(aMutex);
  if (!lock) {
    if (IsReplaying() && !AreThreadEventsPassedThrough()) {
      Thread* thread = Thread::Current();
      if (thread->MaybeWaitForCheckpointSave([=]() { pthread_mutex_unlock(aMutex); })) {
        // We unlocked the mutex while the thread idled, so don't wait on the
        // condvar: the state the thread is waiting on may have changed and it
        // might not want to continue waiting. Returning immediately means this
        // this is a spurious wakeup, which is allowed by the pthreads API and
        // should be handled correctly by callers.
        pthread_mutex_lock(aMutex);
        return 0;
      }
      thread->NotifyUnrecordedWait([=]() {
          pthread_mutex_lock(aMutex);
          pthread_cond_broadcast(aCond);
          pthread_mutex_unlock(aMutex);
        });
    }

    AutoEnsurePassThroughThreadEvents pt;
    return aCallback();
  }
  ssize_t rv = 0;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    rv = aCallback();
  } else {
    DirectUnlockMutex(aMutex);
  }
  lock->Exit();
  lock->Enter();
  if (IsReplaying()) {
    DirectLockMutex(aMutex);
  }
  if (aRecordReturnValue) {
    return RecordReplayValue(rv);
  }
  MOZ_RELEASE_ASSERT(rv == 0);
  return 0;
}

static PreambleResult
Preamble_pthread_cond_wait(CallArguments* aArguments)
{
  auto& cond = aArguments->Arg<0, pthread_cond_t*>();
  auto& mutex = aArguments->Arg<1, pthread_mutex_t*>();
  aArguments->Rval<ssize_t>() =
    WaitForCvar(mutex, cond, false,
                [=]() { return OriginalCall(pthread_cond_wait, ssize_t, cond, mutex); });
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_pthread_cond_timedwait(CallArguments* aArguments)
{
  auto& cond = aArguments->Arg<0, pthread_cond_t*>();
  auto& mutex = aArguments->Arg<1, pthread_mutex_t*>();
  auto& timeout = aArguments->Arg<2, timespec*>();
  aArguments->Rval<ssize_t>() =
    WaitForCvar(mutex, cond, true,
                [=]() { return OriginalCall(pthread_cond_timedwait, ssize_t,
                                            cond, mutex, timeout); });
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_pthread_cond_timedwait_relative_np(CallArguments* aArguments)
{
  auto& cond = aArguments->Arg<0, pthread_cond_t*>();
  auto& mutex = aArguments->Arg<1, pthread_mutex_t*>();
  auto& timeout = aArguments->Arg<2, timespec*>();
  aArguments->Rval<ssize_t>() =
    WaitForCvar(mutex, cond, true,
                [=]() { return OriginalCall(pthread_cond_timedwait_relative_np, ssize_t,
                                            cond, mutex, timeout); });
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_pthread_create(CallArguments* aArguments)
{
  if (AreThreadEventsPassedThrough()) {
    return PreambleResult::Redirect;
  }

  auto& token = aArguments->Arg<0, pthread_t*>();
  auto& attr = aArguments->Arg<1, const pthread_attr_t*>();
  auto& start = aArguments->Arg<2, void*>();
  auto& startArg = aArguments->Arg<3, void*>();

  int detachState;
  int rv = pthread_attr_getdetachstate(attr, &detachState);
  MOZ_RELEASE_ASSERT(rv == 0);

  *token = Thread::StartThread((Thread::Callback) start, startArg,
                               detachState == PTHREAD_CREATE_JOINABLE);
  if (!*token) {
    // Don't create new threads after diverging from the recording.
    MOZ_RELEASE_ASSERT(HasDivergedFromRecording());
    return Preamble_SetError(aArguments);
  }

  aArguments->Rval<ssize_t>() = 0;
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_pthread_join(CallArguments* aArguments)
{
  if (AreThreadEventsPassedThrough()) {
    return PreambleResult::Redirect;
  }

  auto& token = aArguments->Arg<0, pthread_t>();
  auto& ptr = aArguments->Arg<1, void**>();

  Thread* thread = Thread::GetByNativeId(token);
  thread->Join();

  *ptr = nullptr;
  aArguments->Rval<ssize_t>() = 0;
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_pthread_mutex_init(CallArguments* aArguments)
{
  auto& mutex = aArguments->Arg<0, pthread_mutex_t*>();
  auto& attr = aArguments->Arg<1, pthread_mutexattr_t*>();

  Lock::New(mutex);
  aArguments->Rval<ssize_t>() = OriginalCall(pthread_mutex_init, ssize_t, mutex, attr);
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_pthread_mutex_destroy(CallArguments* aArguments)
{
  auto& mutex = aArguments->Arg<0, pthread_mutex_t*>();

  Lock::Destroy(mutex);
  aArguments->Rval<ssize_t>() = OriginalCall(pthread_mutex_destroy, ssize_t, mutex);
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_pthread_mutex_lock(CallArguments* aArguments)
{
  auto& mutex = aArguments->Arg<0, pthread_mutex_t*>();

  Lock* lock = Lock::Find(mutex);
  if (!lock) {
    AutoEnsurePassThroughThreadEventsUseStackPointer pt;
    aArguments->Rval<ssize_t>() = OriginalCall(pthread_mutex_lock, ssize_t, mutex);
    return PreambleResult::Veto;
  }
  ssize_t rv = 0;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    rv = OriginalCall(pthread_mutex_lock, ssize_t, mutex);
  }
  rv = RecordReplayValue(rv);
  MOZ_RELEASE_ASSERT(rv == 0 || rv == EDEADLK);
  if (rv == 0) {
    lock->Enter();
    if (IsReplaying()) {
      DirectLockMutex(mutex);
    }
  }
  aArguments->Rval<ssize_t>() = rv;
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_pthread_mutex_trylock(CallArguments* aArguments)
{
  auto& mutex = aArguments->Arg<0, pthread_mutex_t*>();

  Lock* lock = Lock::Find(mutex);
  if (!lock) {
    AutoEnsurePassThroughThreadEvents pt;
    aArguments->Rval<ssize_t>() = OriginalCall(pthread_mutex_trylock, ssize_t, mutex);
    return PreambleResult::Veto;
  }
  ssize_t rv = 0;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    rv = OriginalCall(pthread_mutex_trylock, ssize_t, mutex);
  }
  rv = RecordReplayValue(rv);
  MOZ_RELEASE_ASSERT(rv == 0 || rv == EBUSY);
  if (rv == 0) {
    lock->Enter();
    if (IsReplaying()) {
      DirectLockMutex(mutex);
    }
  }
  aArguments->Rval<ssize_t>() = rv;
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_pthread_mutex_unlock(CallArguments* aArguments)
{
  auto& mutex = aArguments->Arg<0, pthread_mutex_t*>();

  Lock* lock = Lock::Find(mutex);
  if (!lock) {
    AutoEnsurePassThroughThreadEventsUseStackPointer pt;
    aArguments->Rval<ssize_t>() = OriginalCall(pthread_mutex_unlock, ssize_t, mutex);
    return PreambleResult::Veto;
  }
  lock->Exit();
  DirectUnlockMutex(mutex);
  aArguments->Rval<ssize_t>() = 0;
  return PreambleResult::Veto;
}

///////////////////////////////////////////////////////////////////////////////
// stdlib redirections
///////////////////////////////////////////////////////////////////////////////

static void
RR_fread(Stream& aEvents, CallArguments* aArguments, ErrorType* aError)
{
  auto& buf = aArguments->Arg<0, void*>();
  auto& elemSize = aArguments->Arg<1, size_t>();
  auto& capacity = aArguments->Arg<2, size_t>();
  auto& rval = aArguments->Rval<size_t>();

  aEvents.CheckInput(elemSize);
  aEvents.CheckInput(capacity);
  MOZ_RELEASE_ASSERT(rval <= capacity);
  aEvents.RecordOrReplayBytes(buf, rval * elemSize);
}

static PreambleResult
Preamble_getenv(CallArguments* aArguments)
{
  // Ignore attempts to get environment variables that might be fetched in a
  // racy way.
  auto env = aArguments->Arg<0, const char*>();

  // The JPEG library can fetch configuration information from the environment
  // in a way that can run non-deterministically on different threads.
  if (strncmp(env, "JSIMD_", 6) == 0) {
    aArguments->Rval<char*>() = nullptr;
    return PreambleResult::Veto;
  }

  return PreambleResult::Redirect;
}

static struct tm gGlobalTM;

// localtime behaves the same as localtime_r, except it is not reentrant.
// For simplicity, define this in terms of localtime_r.
static PreambleResult
Preamble_localtime(CallArguments* aArguments)
{
  aArguments->Rval<struct tm*>() = localtime_r(aArguments->Arg<0, const time_t*>(), &gGlobalTM);
  return PreambleResult::Veto;
}

// The same concern here applies as for localtime.
static PreambleResult
Preamble_gmtime(CallArguments* aArguments)
{
  aArguments->Rval<struct tm*>() = gmtime_r(aArguments->Arg<0, const time_t*>(), &gGlobalTM);
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_mach_absolute_time(CallArguments* aArguments)
{
  // This function might be called through OSSpinLock while setting gTlsThreadKey.
  Thread* thread = Thread::GetByStackPointer(&thread);
  if (!thread || thread->PassThroughEvents()) {
    aArguments->Rval<uint64_t>() = OriginalCall(mach_absolute_time, uint64_t);
    return PreambleResult::Veto;
  }
  return PreambleResult::Redirect;
}

static PreambleResult
Preamble_mach_vm_allocate(CallArguments* aArguments)
{
  auto& address = aArguments->Arg<1, void**>();
  auto& size = aArguments->Arg<2, size_t>();
  *address = AllocateMemory(size, MemoryKind::Tracked);
  aArguments->Rval<size_t>() = KERN_SUCCESS;
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_mach_vm_deallocate(CallArguments* aArguments)
{
  auto& address = aArguments->Arg<1, void*>();
  auto& size = aArguments->Arg<2, size_t>();
  DeallocateMemory(address, size, MemoryKind::Tracked);
  aArguments->Rval<size_t>() = KERN_SUCCESS;
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_mach_vm_protect(CallArguments* aArguments)
{
  // Ignore any mach_vm_protect calls that occur after saving a checkpoint, as for mprotect.
  if (!HasSavedCheckpoint()) {
    return PreambleResult::PassThrough;
  }
  aArguments->Rval<size_t>() = KERN_SUCCESS;
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_vm_purgable_control(CallArguments* aArguments)
{
  // Never allow purging of volatile memory, to simplify memory snapshots.
  auto& state = aArguments->Arg<3, int*>();
  *state = VM_PURGABLE_NONVOLATILE;
  aArguments->Rval<size_t>() = KERN_SUCCESS;
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_vm_copy(CallArguments* aArguments)
{
  // Asking the kernel to copy memory doesn't work right if the destination is
  // non-writable, so do the copy manually.
  auto& src = aArguments->Arg<1, void*>();
  auto& size = aArguments->Arg<2, size_t>();
  auto& dest = aArguments->Arg<3, void*>();
  memcpy(dest, src, size);
  aArguments->Rval<size_t>() = KERN_SUCCESS;
  return PreambleResult::Veto;
}

///////////////////////////////////////////////////////////////////////////////
// NSPR redirections
///////////////////////////////////////////////////////////////////////////////

// Even though NSPR is compiled as part of firefox, it is easier to just
// redirect this stuff than get record/replay related changes into the source.

static PreambleResult
Preamble_PL_NewHashTable(CallArguments* aArguments)
{
  auto& keyHash = aArguments->Arg<1, PLHashFunction>();
  auto& keyCompare = aArguments->Arg<2, PLHashComparator>();
  auto& valueCompare = aArguments->Arg<3, PLHashComparator>();
  auto& allocOps = aArguments->Arg<4, const PLHashAllocOps*>();
  auto& allocPriv = aArguments->Arg<5, void*>();

  GeneratePLHashTableCallbacks(&keyHash, &keyCompare, &valueCompare, &allocOps, &allocPriv);
  return PreambleResult::PassThrough;
}

static PreambleResult
Preamble_PL_HashTableDestroy(CallArguments* aArguments)
{
  auto& table = aArguments->Arg<0, PLHashTable*>();

  void* priv = table->allocPriv;
  OriginalCall(PL_HashTableDestroy, void, table);
  DestroyPLHashTableCallbacks(priv);
  return PreambleResult::Veto;
}

///////////////////////////////////////////////////////////////////////////////
// Objective C redirections
///////////////////////////////////////////////////////////////////////////////

// From Foundation.h, which has lots of Objective C declarations and can't be
// included here :(
struct NSFastEnumerationState
{
  unsigned long state;
  id* itemsPtr;
  unsigned long* mutationsPtr;
  unsigned long extra[5];
};

// Emulation of NSFastEnumeration on arrays does not replay any exceptions
// thrown by mutating the array while it is being iterated over.
static unsigned long gNeverChange;

static PreambleResult
Preamble_objc_msgSend(CallArguments* aArguments)
{
  if (!AreThreadEventsPassedThrough()) {
    auto message = aArguments->Arg<1, const char*>();

    // Watch for some top level NSApplication messages that can cause Gecko
    // events to be processed.
    if (!strcmp(message, "run") ||
        !strcmp(message, "nextEventMatchingMask:untilDate:inMode:dequeue:"))
    {
      PassThroughThreadEventsAllowCallbacks([&]() {
          RecordReplayInvokeCall(CallEvent_objc_msgSend, aArguments);
        });
      RecordReplayBytes(&aArguments->Rval<size_t>(), sizeof(size_t));
      return PreambleResult::Veto;
    }
  }
  return PreambleResult::Redirect;
}

static void
RR_objc_msgSend(Stream& aEvents, CallArguments* aArguments, ErrorType* aError)
{
  auto& object = aArguments->Arg<0, id>();
  auto& message = aArguments->Arg<1, const char*>();

  aEvents.CheckInput(message);

  aEvents.RecordOrReplayValue(&aArguments->Rval<size_t>());
  aEvents.RecordOrReplayBytes(&aArguments->FloatRval(), sizeof(double));

  // Do some extra recording on messages that return additional state.

  if (!strcmp(message, "countByEnumeratingWithState:objects:count:")) {
    auto& state = aArguments->Arg<2, NSFastEnumerationState*>();
    auto& rval = aArguments->Rval<size_t>();
    if (IsReplaying()) {
      state->itemsPtr = NewLeakyArray<id>(rval);
      state->mutationsPtr = &gNeverChange;
    }
    aEvents.RecordOrReplayBytes(state->itemsPtr, rval * sizeof(id));
  }

  if (!strcmp(message, "getCharacters:")) {
    size_t len = 0;
    if (IsRecording()) {
      AutoPassThroughThreadEvents pt;
      len = CFStringGetLength((CFStringRef) object);
    }
    aEvents.RecordOrReplayValue(&len);
    aEvents.RecordOrReplayBytes(aArguments->Arg<2, void*>(), len * sizeof(wchar_t));
  }

  if (!strcmp(message, "UTF8String") || !strcmp(message, "cStringUsingEncoding:")) {
    auto& rval = aArguments->Rval<char*>();
    size_t len = IsRecording() ? strlen(rval) : 0;
    aEvents.RecordOrReplayValue(&len);
    if (IsReplaying()) {
      rval = NewLeakyArray<char>(len + 1);
    }
    aEvents.RecordOrReplayBytes(rval, len + 1);
  }
}

static PreambleResult
MiddlemanPreamble_objc_msgSend(CallArguments* aArguments)
{
  auto message = aArguments->Arg<1, const char*>();

  // Ignore uses of NSAutoreleasePool after diverging from the recording.
  // These are not performed in the middleman because the middleman has its
  // own autorelease pool, and because the middleman can process calls from
  // multiple threads which will cause these messages to behave differently.
  if (!strcmp(message, "alloc") ||
      !strcmp(message, "drain") ||
      !strcmp(message, "init") ||
      !strcmp(message, "release")) {
    // Fake a return value in case the caller null checks it.
    aArguments->Rval<size_t>() = 1;
    return PreambleResult::Veto;
  }

  // Other messages will be handled by Middleman_objc_msgSend.
  return PreambleResult::Redirect;
}

static void
Middleman_PerformSelector(MiddlemanCallContext& aCx)
{
  Middleman_CString<2>(aCx);
  Middleman_CFTypeArg<3>(aCx);

  // The behavior of performSelector:withObject: varies depending on the
  // selector used, so use a whitelist here.
  if (aCx.mPhase == MiddlemanCallPhase::ReplayPreface) {
    auto str = aCx.mArguments->Arg<2, const char*>();
    if (strcmp(str, "appearanceNamed:")) {
      aCx.MarkAsFailed();
      return;
    }
  }

  Middleman_AutoreleaseCFTypeRval(aCx);
}

static void
Middleman_DictionaryWithObjects(MiddlemanCallContext& aCx)
{
  Middleman_Buffer<2, 4, const void*>(aCx);
  Middleman_Buffer<3, 4, const void*>(aCx);

  if (aCx.AccessPreface()) {
    auto objects = aCx.mArguments->Arg<2, const void**>();
    auto keys = aCx.mArguments->Arg<3, const void**>();
    auto count = aCx.mArguments->Arg<4, CFIndex>();

    for (CFIndex i = 0; i < count; i++) {
      Middleman_ObjCInput(aCx, (id*) &objects[i]);
      Middleman_ObjCInput(aCx, (id*) &keys[i]);
    }
  }

  Middleman_AutoreleaseCFTypeRval(aCx);
}

static void
Middleman_NSStringGetCharacters(MiddlemanCallContext& aCx)
{
  auto string = aCx.mArguments->Arg<0, CFStringRef>();
  auto& buffer = aCx.mArguments->Arg<2, void*>();

  if (aCx.mPhase == MiddlemanCallPhase::MiddlemanInput) {
    size_t len = CFStringGetLength(string);
    buffer = aCx.AllocateBytes(len * sizeof(UniChar));
  }

  if (aCx.AccessOutput()) {
    size_t len =
      (aCx.mPhase == MiddlemanCallPhase::MiddlemanOutput) ? CFStringGetLength(string) : 0;
    aCx.ReadOrWriteOutputBytes(&len, sizeof(len));
    if (aCx.mReplayOutputIsOld) {
      buffer = aCx.AllocateBytes(len * sizeof(UniChar));
    }
    aCx.ReadOrWriteOutputBytes(buffer, len * sizeof(UniChar));
  }
}

struct ObjCMessageInfo
{
  const char* mMessage;
  MiddlemanCallFn mMiddlemanCall;
};

// All Objective C messages that can be called in the middleman, and hooks for
// capturing any inputs and outputs other than the object and message.
static ObjCMessageInfo gObjCMiddlemanCallMessages[] = {
  { "performSelector:withObject:", Middleman_PerformSelector },
  { "respondsToSelector:", Middleman_CString<2> },

  // NSArray
  { "count" },
  { "objectAtIndex:", Middleman_AutoreleaseCFTypeRval },

  // NSColor
  { "currentControlTint" },

  // NSDictionary
  { "dictionaryWithObjects:forKeys:count:", Middleman_DictionaryWithObjects },

  // NSFont
  { "boldSystemFontOfSize:", Middleman_AutoreleaseCFTypeRval },
  { "controlContentFontOfSize:", Middleman_AutoreleaseCFTypeRval },
  { "familyName", Middleman_AutoreleaseCFTypeRval },
  { "fontDescriptor", Middleman_AutoreleaseCFTypeRval },
  { "menuBarFontOfSize:", Middleman_AutoreleaseCFTypeRval },
  { "pointSize" },
  { "smallSystemFontSize" },
  { "systemFontOfSize:", Middleman_AutoreleaseCFTypeRval },
  { "toolTipsFontOfSize:", Middleman_AutoreleaseCFTypeRval },
  { "userFontOfSize:", Middleman_AutoreleaseCFTypeRval },

  // NSFontManager
  { "availableMembersOfFontFamily:", Middleman_Compose<Middleman_CFTypeArg<2>, Middleman_AutoreleaseCFTypeRval> },
  { "sharedFontManager", Middleman_AutoreleaseCFTypeRval },

  // NSNumber
  { "numberWithBool:", Middleman_AutoreleaseCFTypeRval },
  { "unsignedIntValue" },

  // NSString
  { "getCharacters:", Middleman_NSStringGetCharacters },
  { "hasSuffix:", Middleman_CFTypeArg<2> },
  { "isEqualToString:", Middleman_CFTypeArg<2> },
  { "length" },
  { "rangeOfString:options:", Middleman_CFTypeArg<2> },
  { "stringWithCharacters:length:",
    Middleman_Compose<Middleman_Buffer<2, 3, UniChar>, Middleman_AutoreleaseCFTypeRval> },

  // NSWindow
  { "coreUIRenderer", Middleman_AutoreleaseCFTypeRval },

  // UIFontDescriptor
  { "symbolicTraits" },
};

static void
Middleman_objc_msgSend(MiddlemanCallContext& aCx)
{
  auto& object = aCx.mArguments->Arg<0, id>();
  auto message = aCx.mArguments->Arg<1, const char*>();

  for (const ObjCMessageInfo& info : gObjCMiddlemanCallMessages) {
    if (!strcmp(info.mMessage, message)) {
      if (aCx.AccessPreface()) {
        Middleman_ObjCInput(aCx, &object);
      }
      if (info.mMiddlemanCall && !aCx.mFailed) {
        info.mMiddlemanCall(aCx);
      }
      return;
    }
  }

  if (aCx.mPhase == MiddlemanCallPhase::ReplayPreface) {
    aCx.MarkAsFailed();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Cocoa redirections
///////////////////////////////////////////////////////////////////////////////

static void
Middleman_CFArrayCreate(MiddlemanCallContext& aCx)
{
  Middleman_Buffer<1, 2, const void*>(aCx);

  if (aCx.AccessPreface()) {
    auto values = aCx.mArguments->Arg<1, const void**>();
    auto numValues = aCx.mArguments->Arg<2, CFIndex>();
    auto& callbacks = aCx.mArguments->Arg<3, const CFArrayCallBacks*>();

    // For now we only support creating arrays with CFType elements.
    if (aCx.mPhase == MiddlemanCallPhase::MiddlemanInput) {
      callbacks = &kCFTypeArrayCallBacks;
    } else {
      MOZ_RELEASE_ASSERT(callbacks == &kCFTypeArrayCallBacks);
    }

    for (CFIndex i = 0; i < numValues; i++) {
      Middleman_ObjCInput(aCx, (id*) &values[i]);
    }
  }

  Middleman_CreateCFTypeRval(aCx);
}

static void
Middleman_CFArrayGetValueAtIndex(MiddlemanCallContext& aCx)
{
  Middleman_CFTypeArg<0>(aCx);

  auto array = aCx.mArguments->Arg<0, id>();

  // We can't probe the array to see what callbacks it uses, so look at where
  // it came from to see whether its elements should be treated as CFTypes.
  MiddlemanCall* call = LookupMiddlemanCall(array);
  bool isCFTypeRval = false;
  if (call) {
    switch (call->mCallId) {
    case CallEvent_CTLineGetGlyphRuns:
    case CallEvent_CTFontCopyVariationAxes:
    case CallEvent_CTFontDescriptorCreateMatchingFontDescriptors:
      isCFTypeRval = true;
      break;
    default:
      break;
    }
  }

  if (isCFTypeRval) {
    Middleman_CFTypeRval(aCx);
  }
}

static void
RR_CFDataGetBytePtr(Stream& aEvents, CallArguments* aArguments, ErrorType* aError)
{
  auto& rval = aArguments->Rval<UInt8*>();

  size_t len = 0;
  if (IsRecording()) {
    len = OriginalCall(CFDataGetLength, size_t, aArguments->Arg<0, CFDataRef>());
  }
  aEvents.RecordOrReplayValue(&len);
  if (IsReplaying()) {
    rval = NewLeakyArray<UInt8>(len);
  }
  aEvents.RecordOrReplayBytes(rval, len);
}

static void
Middleman_CFDataGetBytePtr(MiddlemanCallContext& aCx)
{
  Middleman_CFTypeArg<0>(aCx);

  auto data = aCx.mArguments->Arg<0, CFDataRef>();
  auto& buffer = aCx.mArguments->Rval<void*>();

  if (aCx.AccessOutput()) {
    size_t len = (aCx.mPhase == MiddlemanCallPhase::MiddlemanOutput) ? CFDataGetLength(data) : 0;
    aCx.ReadOrWriteOutputBytes(&len, sizeof(len));
    if (aCx.mPhase == MiddlemanCallPhase::ReplayOutput) {
      buffer = aCx.AllocateBytes(len);
    }
    aCx.ReadOrWriteOutputBytes(buffer, len);
  }
}

static void
Middleman_CFDictionaryCreate(MiddlemanCallContext& aCx)
{
  Middleman_Buffer<1, 3, const void*>(aCx);
  Middleman_Buffer<2, 3, const void*>(aCx);

  if (aCx.AccessPreface()) {
    auto keys = aCx.mArguments->Arg<1, const void**>();
    auto values = aCx.mArguments->Arg<2, const void**>();
    auto numValues = aCx.mArguments->Arg<3, CFIndex>();
    auto& keyCallbacks = aCx.mArguments->Arg<4, const CFDictionaryKeyCallBacks*>();
    auto& valueCallbacks = aCx.mArguments->Arg<5, const CFDictionaryValueCallBacks*>();

    // For now we only support creating dictionaries with CFType keys and values.
    if (aCx.mPhase == MiddlemanCallPhase::MiddlemanInput) {
      keyCallbacks = &kCFTypeDictionaryKeyCallBacks;
      valueCallbacks = &kCFTypeDictionaryValueCallBacks;
    } else {
      MOZ_RELEASE_ASSERT(keyCallbacks == &kCFTypeDictionaryKeyCallBacks);
      MOZ_RELEASE_ASSERT(valueCallbacks == &kCFTypeDictionaryValueCallBacks);
    }

    for (CFIndex i = 0; i < numValues; i++) {
      Middleman_ObjCInput(aCx, (id*) &keys[i]);
      Middleman_ObjCInput(aCx, (id*) &values[i]);
    }
  }

  Middleman_CreateCFTypeRval(aCx);
}

static void DummyCFNotificationCallback(CFNotificationCenterRef aCenter, void* aObserver,
                                        CFStringRef aName, const void* aObject,
                                        CFDictionaryRef aUserInfo)
{
  // FIXME
  //MOZ_CRASH();
}

static PreambleResult
Preamble_CFNotificationCenterAddObserver(CallArguments* aArguments)
{
  auto& callback = aArguments->Arg<2, CFNotificationCallback>();
  if (!AreThreadEventsPassedThrough()) {
    callback = DummyCFNotificationCallback;
  }
  return PreambleResult::Redirect;
}

static size_t
CFNumberTypeBytes(CFNumberType aType)
{
  switch (aType) {
  case kCFNumberSInt8Type: return sizeof(int8_t);
  case kCFNumberSInt16Type: return sizeof(int16_t);
  case kCFNumberSInt32Type: return sizeof(int32_t);
  case kCFNumberSInt64Type: return sizeof(int64_t);
  case kCFNumberFloat32Type: return sizeof(float);
  case kCFNumberFloat64Type: return sizeof(double);
  case kCFNumberCharType: return sizeof(char);
  case kCFNumberShortType: return sizeof(short);
  case kCFNumberIntType: return sizeof(int);
  case kCFNumberLongType: return sizeof(long);
  case kCFNumberLongLongType: return sizeof(long long);
  case kCFNumberFloatType: return sizeof(float);
  case kCFNumberDoubleType: return sizeof(double);
  case kCFNumberCFIndexType: return sizeof(CFIndex);
  case kCFNumberNSIntegerType: return sizeof(long);
  case kCFNumberCGFloatType: return sizeof(CGFloat);
  default: MOZ_CRASH();
  }
}

static void
Middleman_CFNumberCreate(MiddlemanCallContext& aCx)
{
  if (aCx.AccessPreface()) {
    auto numberType = aCx.mArguments->Arg<1, CFNumberType>();
    auto& valuePtr = aCx.mArguments->Arg<2, void*>();
    aCx.ReadOrWritePrefaceBuffer(&valuePtr, CFNumberTypeBytes(numberType));
  }

  Middleman_CreateCFTypeRval(aCx);
}

static void
RR_CFNumberGetValue(Stream& aEvents, CallArguments* aArguments, ErrorType* aError)
{
  auto& type = aArguments->Arg<1, CFNumberType>();
  auto& value = aArguments->Arg<2, void*>();

  aEvents.CheckInput(type);
  aEvents.RecordOrReplayBytes(value, CFNumberTypeBytes(type));
}

static void
Middleman_CFNumberGetValue(MiddlemanCallContext& aCx)
{
  Middleman_CFTypeArg<0>(aCx);

  auto& buffer = aCx.mArguments->Arg<2, void*>();
  auto type = aCx.mArguments->Arg<1, CFNumberType>();
  aCx.ReadOrWriteOutputBuffer(&buffer, CFNumberTypeBytes(type));
}

static PreambleResult
MiddlemanPreamble_CFRetain(CallArguments* aArguments)
{
  aArguments->Rval<size_t>() = aArguments->Arg<0, size_t>();
  return PreambleResult::Veto;
}

static PreambleResult
Preamble_CFRunLoopSourceCreate(CallArguments* aArguments)
{
  if (!AreThreadEventsPassedThrough()) {
    auto& cx = aArguments->Arg<2, CFRunLoopSourceContext*>();

    RegisterCallbackData(BitwiseCast<void*>(cx->perform));
    RegisterCallbackData(cx->info);

    if (IsRecording()) {
      CallbackWrapperData* wrapperData = new CallbackWrapperData(cx->perform, cx->info);
      cx->perform = CFRunLoopPerformCallBackWrapper;
      cx->info = wrapperData;
    }
  }
  return PreambleResult::Redirect;
}

struct ContextDataInfo
{
  CGContextRef mContext;
  void* mData;
  size_t mDataSize;

  ContextDataInfo(CGContextRef aContext, void* aData, size_t aDataSize)
    : mContext(aContext), mData(aData), mDataSize(aDataSize)
  {}
};

static StaticInfallibleVector<ContextDataInfo> gContextData;

static void
RR_CGBitmapContextCreateWithData(Stream& aEvents, CallArguments* aArguments, ErrorType* aError)
{
  auto& data = aArguments->Arg<0, void*>();
  auto& height = aArguments->Arg<2, size_t>();
  auto& bytesPerRow = aArguments->Arg<4, size_t>();
  auto& rval = aArguments->Rval<CGContextRef>();

  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());

  // When replaying, Middleman_CGBitmapContextCreateWithData will take care of
  // updating gContextData with the right context pointer (after being mangled
  // in Middleman_SystemOutput).
  if (IsRecording()) {
    gContextData.emplaceBack(rval, data, height * bytesPerRow);
  }
}

static void
Middleman_CGBitmapContextCreateWithData(MiddlemanCallContext& aCx)
{
  Middleman_CFTypeArg<5>(aCx);
  Middleman_StackArgumentData<3 * sizeof(size_t)>(aCx);
  Middleman_CreateCFTypeRval(aCx);

  auto& data = aCx.mArguments->Arg<0, void*>();
  auto height = aCx.mArguments->Arg<2, size_t>();
  auto bytesPerRow = aCx.mArguments->Arg<4, size_t>();
  auto rval = aCx.mArguments->Rval<CGContextRef>();

  if (aCx.mPhase == MiddlemanCallPhase::MiddlemanInput) {
    data = aCx.AllocateBytes(height * bytesPerRow);
  }

  if ((aCx.mPhase == MiddlemanCallPhase::ReplayPreface && !HasDivergedFromRecording()) ||
      (aCx.AccessOutput() && !aCx.mReplayOutputIsOld)) {
    gContextData.emplaceBack(rval, data, height * bytesPerRow);
  }
}

template <size_t ContextArgument>
static void
RR_FlushCGContext(Stream& aEvents, CallArguments* aArguments, ErrorType* aError)
{
  auto& context = aArguments->Arg<ContextArgument, CGContextRef>();

  for (int i = gContextData.length() - 1; i >= 0; i--) {
    if (context == gContextData[i].mContext) {
      aEvents.RecordOrReplayBytes(gContextData[i].mData, gContextData[i].mDataSize);
      return;
    }
  }
  MOZ_CRASH("RR_FlushCGContext");
}

template <size_t ContextArgument>
static void
Middleman_FlushCGContext(MiddlemanCallContext& aCx)
{
  auto context = aCx.mArguments->Arg<ContextArgument, CGContextRef>();

  // Update the contents of the target buffer in the middleman process to match
  // the current contents in the replaying process.
  if (aCx.AccessInput()) {
    for (int i = gContextData.length() - 1; i >= 0; i--) {
      if (context == gContextData[i].mContext) {
        aCx.ReadOrWriteInputBytes(gContextData[i].mData, gContextData[i].mDataSize);
        return;
      }
    }
    MOZ_CRASH("Middleman_FlushCGContext");
  }

  // After performing the call, the buffer in the replaying process is updated
  // to match any data written by the middleman.
  if (aCx.AccessOutput()) {
    for (int i = gContextData.length() - 1; i >= 0; i--) {
      if (context == gContextData[i].mContext) {
        aCx.ReadOrWriteOutputBytes(gContextData[i].mData, gContextData[i].mDataSize);
        return;
      }
    }
    MOZ_CRASH("Middleman_FlushCGContext");
  }
}

static PreambleResult
Preamble_CGContextRestoreGState(CallArguments* aArguments)
{
  return IsRecording() ? PreambleResult::PassThrough : PreambleResult::Veto;
}

static void
RR_CGDataProviderCreateWithData(Stream& aEvents, CallArguments* aArguments, ErrorType* aError)
{
  auto& info = aArguments->Arg<0, void*>();
  auto& data = aArguments->Arg<1, const void*>();
  auto& size = aArguments->Arg<2, size_t>();
  auto& releaseData = aArguments->Arg<3, CGDataProviderReleaseDataCallback>();

  if (IsReplaying() && releaseData) {
    // Immediately release the data, since there is no data provider to do it for us.
    releaseData(info, data, size);
  }
}

static void
ReleaseDataCallback(void*, const void* aData, size_t)
{
  free((void*) aData);
}

static void
Middleman_CGDataProviderCreateWithData(MiddlemanCallContext& aCx)
{
  Middleman_Buffer<1, 2>(aCx);
  Middleman_CreateCFTypeRval(aCx);

  auto& info = aCx.mArguments->Arg<0, void*>();
  auto& data = aCx.mArguments->Arg<1, const void*>();
  auto& size = aCx.mArguments->Arg<2, size_t>();
  auto& releaseData = aCx.mArguments->Arg<3, CGDataProviderReleaseDataCallback>();

  // Make a copy of the data that won't be released the next time middleman
  // calls are reset, in case CoreGraphics decides to hang onto the data
  // provider after that point.
  if (aCx.mPhase == MiddlemanCallPhase::MiddlemanInput) {
    void* newData = malloc(size);
    memcpy(newData, data, size);
    data = newData;
    releaseData = ReleaseDataCallback;
  }

  // Immediately release the data in the replaying process.
  if (aCx.mPhase == MiddlemanCallPhase::ReplayInput && releaseData) {
    releaseData(info, data, size);
  }
}

static PreambleResult
Preamble_CGPathApply(CallArguments* aArguments)
{
  if (AreThreadEventsPassedThrough()) {
    return PreambleResult::Redirect;
  }

  auto& path = aArguments->Arg<0, CGPathRef>();
  auto& data = aArguments->Arg<1, void*>();
  auto& function = aArguments->Arg<2, CGPathApplierFunction>();

  RegisterCallbackData(BitwiseCast<void*>(function));
  RegisterCallbackData(data);
  PassThroughThreadEventsAllowCallbacks([&]() {
      CallbackWrapperData wrapperData(function, data);
      OriginalCall(CGPathApply, void, path, &wrapperData, CGPathApplierFunctionWrapper);
    });
  RemoveCallbackData(data);

  return PreambleResult::Veto;
}

// Note: We only redirect CTRunGetGlyphsPtr, not CTRunGetGlyphs. The latter may
// be implemented with a loop that jumps back into the code we overwrite with a
// jump, a pattern which ProcessRedirect.cpp does not handle. For the moment,
// Gecko code only calls CTRunGetGlyphs if CTRunGetGlyphsPtr fails, so make
// sure that CTRunGetGlyphsPtr always succeeds when it is being recorded.
// The same concerns apply to CTRunGetPositionsPtr and CTRunGetStringIndicesPtr,
// so they are all handled here.
template <typename ElemType, void (*GetElemsFn)(CTRunRef, CFRange, ElemType*)>
static void
RR_CTRunGetElements(Stream& aEvents, CallArguments* aArguments, ErrorType* aError)
{
  auto& run = aArguments->Arg<0, CTRunRef>();
  auto& rval = aArguments->Rval<ElemType*>();

  size_t count;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    count = CTRunGetGlyphCount(run);
    if (!rval) {
      rval = NewLeakyArray<ElemType>(count);
      GetElemsFn(run, CFRangeMake(0, 0), rval);
    }
  }
  aEvents.RecordOrReplayValue(&count);
  if (IsReplaying()) {
    rval = NewLeakyArray<ElemType>(count);
  }
  aEvents.RecordOrReplayBytes(rval, count * sizeof(ElemType));
}

template <typename ElemType, void (*GetElemsFn)(CTRunRef, CFRange, ElemType*)>
static void
Middleman_CTRunGetElements(MiddlemanCallContext& aCx)
{
  Middleman_CFTypeArg<0>(aCx);

  if (aCx.AccessOutput()) {
    auto run = aCx.mArguments->Arg<0, CTRunRef>();
    auto& rval = aCx.mArguments->Rval<ElemType*>();

    size_t count = 0;
    if (IsMiddleman()) {
      count = CTRunGetGlyphCount(run);
      if (!rval) {
        rval = (ElemType*) aCx.AllocateBytes(count * sizeof(ElemType));
        GetElemsFn(run, CFRangeMake(0, 0), rval);
      }
    }
    aCx.ReadOrWriteOutputBytes(&count, sizeof(count));
    if (IsReplaying()) {
      rval = (ElemType*) aCx.AllocateBytes(count * sizeof(ElemType));
    }
    aCx.ReadOrWriteOutputBytes(rval, count * sizeof(ElemType));
  }
}

static PreambleResult
Preamble_OSSpinLockLock(CallArguments* aArguments)
{
  auto& lock = aArguments->Arg<0, OSSpinLock*>();

  // These spin locks never need to be recorded, but they are used by malloc
  // and can end up calling other recorded functions like mach_absolute_time,
  // so make sure events are passed through here. Note that we don't have to
  // redirect OSSpinLockUnlock, as it doesn't have these issues.
  AutoEnsurePassThroughThreadEventsUseStackPointer pt;
  OriginalCall(OSSpinLockLock, void, lock);

  return PreambleResult::Veto;
}

///////////////////////////////////////////////////////////////////////////////
// Redirection generation
///////////////////////////////////////////////////////////////////////////////

#define MAKE_REDIRECTION_ENTRY(aName, ...)          \
  { #aName, nullptr, nullptr, __VA_ARGS__ },

Redirection gRedirections[] = {
  FOR_EACH_REDIRECTION(MAKE_REDIRECTION_ENTRY)
  { }
};

#undef MAKE_REDIRECTION_ENTRY

///////////////////////////////////////////////////////////////////////////////
// Direct system call API
///////////////////////////////////////////////////////////////////////////////

const char*
SymbolNameRaw(void* aPtr)
{
  Dl_info info;
  return (dladdr(aPtr, &info) && info.dli_sname) ? info.dli_sname : "???";
}

void*
DirectAllocateMemory(void* aAddress, size_t aSize)
{
  void* res = OriginalCall(mmap, void*,
                           aAddress, aSize, PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_ANON | MAP_PRIVATE, -1, 0);
  MOZ_RELEASE_ASSERT(res && res != (void*)-1);
  return res;
}

void
DirectDeallocateMemory(void* aAddress, size_t aSize)
{
  ssize_t rv = OriginalCall(munmap, int, aAddress, aSize);
  MOZ_RELEASE_ASSERT(rv >= 0);
}

void
DirectWriteProtectMemory(void* aAddress, size_t aSize, bool aExecutable,
                         bool aIgnoreFailures /* = false */)
{
  ssize_t rv = OriginalCall(mprotect, int, aAddress, aSize,
                            PROT_READ | (aExecutable ? PROT_EXEC : 0));
  MOZ_RELEASE_ASSERT(aIgnoreFailures || rv == 0);
}

void
DirectUnprotectMemory(void* aAddress, size_t aSize, bool aExecutable,
                      bool aIgnoreFailures /* = false */)
{
  ssize_t rv = OriginalCall(mprotect, int, aAddress, aSize,
                            PROT_READ | PROT_WRITE | (aExecutable ? PROT_EXEC : 0));
  MOZ_RELEASE_ASSERT(aIgnoreFailures || rv == 0);
}

void
DirectSeekFile(FileHandle aFd, uint64_t aOffset)
{
  static_assert(sizeof(uint64_t) == sizeof(off_t), "off_t should have 64 bits");
  ssize_t rv = HANDLE_EINTR(OriginalCall(lseek, int, aFd, aOffset, SEEK_SET));
  MOZ_RELEASE_ASSERT(rv >= 0);
}

FileHandle
DirectOpenFile(const char* aFilename, bool aWriting)
{
  int flags = aWriting ? (O_WRONLY | O_CREAT | O_TRUNC) : O_RDONLY;
  int perms = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  int fd = HANDLE_EINTR(OriginalCall(open, int, aFilename, flags, perms));
  MOZ_RELEASE_ASSERT(fd > 0);
  return fd;
}

void
DirectCloseFile(FileHandle aFd)
{
  ssize_t rv = HANDLE_EINTR(OriginalCall(close, int, aFd));
  MOZ_RELEASE_ASSERT(rv >= 0);
}

void
DirectDeleteFile(const char* aFilename)
{
  ssize_t rv = unlink(aFilename);
  MOZ_RELEASE_ASSERT(rv >= 0 || errno == ENOENT);
}

void
DirectWrite(FileHandle aFd, const void* aData, size_t aSize)
{
  ssize_t rv = HANDLE_EINTR(OriginalCall(write, int, aFd, aData, aSize));
  MOZ_RELEASE_ASSERT((size_t) rv == aSize);
}

void
DirectPrint(const char* aString)
{
  DirectWrite(STDERR_FILENO, aString, strlen(aString));
}

size_t
DirectRead(FileHandle aFd, void* aData, size_t aSize)
{
  // Clear the memory in case it is write protected by the memory snapshot
  // mechanism.
  memset(aData, 0, aSize);
  ssize_t rv = HANDLE_EINTR(OriginalCall(read, int, aFd, aData, aSize));
  MOZ_RELEASE_ASSERT(rv >= 0);
  return (size_t) rv;
}

void
DirectCreatePipe(FileHandle* aWriteFd, FileHandle* aReadFd)
{
  int fds[2];
  ssize_t rv = OriginalCall(pipe, int, fds);
  MOZ_RELEASE_ASSERT(rv >= 0);
  *aWriteFd = fds[1];
  *aReadFd = fds[0];
}

static double gAbsoluteToNanosecondsRate;

void
InitializeCurrentTime()
{
  AbsoluteTime time = { 1000000, 0 };
  Nanoseconds rate = AbsoluteToNanoseconds(time);
  MOZ_RELEASE_ASSERT(!rate.hi);
  gAbsoluteToNanosecondsRate = rate.lo / 1000000.0;
}

double
CurrentTime()
{
  return OriginalCall(mach_absolute_time, int64_t) * gAbsoluteToNanosecondsRate / 1000.0;
}

void
DirectSpawnThread(void (*aFunction)(void*), void* aArgument)
{
  MOZ_RELEASE_ASSERT(IsMiddleman() || AreThreadEventsPassedThrough());

  pthread_attr_t attr;
  int rv = pthread_attr_init(&attr);
  MOZ_RELEASE_ASSERT(rv == 0);

  rv = pthread_attr_setstacksize(&attr, 2 * 1024 * 1024);
  MOZ_RELEASE_ASSERT(rv == 0);

  rv = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  MOZ_RELEASE_ASSERT(rv == 0);

  pthread_t pthread;
  rv = OriginalCall(pthread_create, int, &pthread, &attr, (void* (*)(void*)) aFunction, aArgument);
  MOZ_RELEASE_ASSERT(rv == 0);

  rv = pthread_attr_destroy(&attr);
  MOZ_RELEASE_ASSERT(rv == 0);
}

} // recordreplay
} // mozilla
