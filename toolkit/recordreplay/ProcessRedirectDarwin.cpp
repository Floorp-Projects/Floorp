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

#define FOR_EACH_REDIRECTION(MACRO)             \
  /* System call wrappers */                    \
  MACRO(kevent)                                 \
  MACRO(kevent64)                               \
  MACRO(madvise)                                \
  MACRO(mprotect)                               \
  MACRO(mmap)                                   \
  MACRO(munmap)                                 \
  MACRO(read)                                   \
  MACRO(__read_nocancel)                        \
  MACRO(pread)                                  \
  MACRO(write)                                  \
  MACRO(__write_nocancel)                       \
  MACRO(open)                                   \
  MACRO(__open_nocancel)                        \
  MACRO(recv)                                   \
  MACRO(recvmsg)                                \
  MACRO(sendmsg)                                \
  MACRO(shm_open)                               \
  MACRO(socket)                                 \
  MACRO(kqueue)                                 \
  MACRO(pipe)                                   \
  MACRO(close)                                  \
  MACRO(__close_nocancel)                       \
  MACRO(mkdir)                                  \
  MACRO(dup)                                    \
  MACRO(access)                                 \
  MACRO(lseek)                                  \
  MACRO(socketpair)                             \
  MACRO(fileport_makeport)                      \
  MACRO(getsockopt)                             \
  MACRO(gettimeofday)                           \
  MACRO(getuid)                                 \
  MACRO(geteuid)                                \
  MACRO(getgid)                                 \
  MACRO(getegid)                                \
  MACRO(issetugid)                              \
  MACRO(__gettid)                               \
  MACRO(getpid)                                 \
  MACRO(fcntl)                                  \
  MACRO(fstat$INODE64)                          \
  MACRO(lstat$INODE64)                          \
  MACRO(stat$INODE64)                           \
  MACRO(getattrlist)                            \
  MACRO(statfs$INODE64)                         \
  MACRO(fstatfs$INODE64)                        \
  MACRO(readlink)                               \
  MACRO(__getdirentries64)                      \
  MACRO(getdirentriesattr)                      \
  MACRO(getrusage)                              \
  MACRO(__getrlimit)                            \
  MACRO(__setrlimit)                            \
  MACRO(sigprocmask)                            \
  MACRO(sigaltstack)                            \
  MACRO(sigaction)                              \
  MACRO(__pthread_sigmask)                      \
  MACRO(__fsgetpath)                            \
  MACRO(__disable_threadsignal)                 \
  MACRO(__sysctl)                               \
  MACRO(__mac_syscall)                          \
  MACRO(getaudit_addr)                          \
  MACRO(umask)                                  \
  MACRO(__select)                               \
  MACRO(__process_policy)                       \
  MACRO(__kdebug_trace)                         \
  MACRO(guarded_kqueue_np)                      \
  MACRO(csops)                                  \
  MACRO(__getlogin)                             \
  MACRO(__workq_kernreturn)                     \
  MACRO(start_wqthread)                         \
  /* pthreads interface functions */            \
  MACRO(pthread_cond_wait)                      \
  MACRO(pthread_cond_timedwait)                 \
  MACRO(pthread_cond_timedwait_relative_np)     \
  MACRO(pthread_create)                         \
  MACRO(pthread_join)                           \
  MACRO(pthread_mutex_init)                     \
  MACRO(pthread_mutex_destroy)                  \
  MACRO(pthread_mutex_lock)                     \
  MACRO(pthread_mutex_trylock)                  \
  MACRO(pthread_mutex_unlock)                   \
  /* C Library functions */                     \
  MACRO(dlclose)                                \
  MACRO(dlopen)                                 \
  MACRO(dlsym)                                  \
  MACRO(fclose)                                 \
  MACRO(fopen)                                  \
  MACRO(fread)                                  \
  MACRO(fseek)                                  \
  MACRO(ftell)                                  \
  MACRO(fwrite)                                 \
  MACRO(getenv)                                 \
  MACRO(localtime_r)                            \
  MACRO(gmtime_r)                               \
  MACRO(localtime)                              \
  MACRO(gmtime)                                 \
  MACRO(mktime)                                 \
  MACRO(setlocale)                              \
  MACRO(strftime)                               \
  MACRO(arc4random)                             \
  MACRO(mach_absolute_time)                     \
  MACRO(mach_msg)                               \
  MACRO(mach_timebase_info)                     \
  MACRO(mach_vm_allocate)                       \
  MACRO(mach_vm_deallocate)                     \
  MACRO(mach_vm_protect)                        \
  MACRO(sandbox_free_error)                     \
  MACRO(sandbox_init)                           \
  MACRO(sandbox_init_with_parameters)           \
  MACRO(vm_copy)                                \
  MACRO(vm_purgable_control)                    \
  /* NSPR functions */                          \
  MACRO(PL_NewHashTable)                        \
  MACRO(PL_HashTableDestroy)                    \
  /* Objective C functions */                   \
  MACRO(class_getClassMethod)                   \
  MACRO(class_getInstanceMethod)                \
  MACRO(method_exchangeImplementations)         \
  MACRO(objc_autoreleasePoolPop)                \
  MACRO(objc_autoreleasePoolPush)               \
  MACRO(objc_msgSend)                           \
  /* Cocoa functions */                         \
  MACRO(AcquireFirstMatchingEventInQueue)       \
  MACRO(CFArrayAppendValue)                     \
  MACRO(CFArrayCreate)                          \
  MACRO(CFArrayGetCount)                        \
  MACRO(CFArrayGetValueAtIndex)                 \
  MACRO(CFArrayRemoveValueAtIndex)              \
  MACRO(CFAttributedStringCreate)               \
  MACRO(CFBundleCopyExecutableURL)              \
  MACRO(CFBundleCopyInfoDictionaryForURL)       \
  MACRO(CFBundleCreate)                         \
  MACRO(CFBundleGetBundleWithIdentifier)        \
  MACRO(CFBundleGetDataPointerForName)          \
  MACRO(CFBundleGetFunctionPointerForName)      \
  MACRO(CFBundleGetIdentifier)                  \
  MACRO(CFBundleGetInfoDictionary)              \
  MACRO(CFBundleGetMainBundle)                  \
  MACRO(CFBundleGetValueForInfoDictionaryKey)   \
  MACRO(CFDataGetBytePtr)                       \
  MACRO(CFDataGetLength)                        \
  MACRO(CFDateFormatterCreate)                  \
  MACRO(CFDateFormatterGetFormat)               \
  MACRO(CFDictionaryAddValue)                   \
  MACRO(CFDictionaryCreate)                     \
  MACRO(CFDictionaryCreateMutable)              \
  MACRO(CFDictionaryCreateMutableCopy)          \
  MACRO(CFDictionaryGetValue)                   \
  MACRO(CFDictionaryGetValueIfPresent)          \
  MACRO(CFDictionaryReplaceValue)               \
  MACRO(CFEqual)                                \
  MACRO(CFGetTypeID)                            \
  MACRO(CFLocaleCopyCurrent)                    \
  MACRO(CFLocaleCopyPreferredLanguages)         \
  MACRO(CFLocaleCreate)                         \
  MACRO(CFLocaleGetIdentifier)                  \
  MACRO(CFNotificationCenterAddObserver)        \
  MACRO(CFNotificationCenterGetLocalCenter)     \
  MACRO(CFNotificationCenterRemoveObserver)     \
  MACRO(CFNumberCreate)                         \
  MACRO(CFNumberGetValue)                       \
  MACRO(CFNumberIsFloatType)                    \
  MACRO(CFPreferencesAppValueIsForced)          \
  MACRO(CFPreferencesCopyAppValue)              \
  MACRO(CFPreferencesCopyValue)                 \
  MACRO(CFPropertyListCreateFromXMLData)        \
  MACRO(CFPropertyListCreateWithStream)         \
  MACRO(CFReadStreamClose)                      \
  MACRO(CFReadStreamCreateWithFile)             \
  MACRO(CFReadStreamOpen)                       \
  MACRO(CFRelease)                              \
  MACRO(CFRetain)                               \
  MACRO(CFRunLoopAddSource)                     \
  MACRO(CFRunLoopGetCurrent)                    \
  MACRO(CFRunLoopRemoveSource)                  \
  MACRO(CFRunLoopSourceCreate)                  \
  MACRO(CFRunLoopSourceSignal)                  \
  MACRO(CFRunLoopWakeUp)                        \
  MACRO(CFStringAppendCharacters)               \
  MACRO(CFStringCompare)                        \
  MACRO(CFStringCreateArrayBySeparatingStrings) \
  MACRO(CFStringCreateMutable)                  \
  MACRO(CFStringCreateWithBytes)                \
  MACRO(CFStringCreateWithBytesNoCopy)          \
  MACRO(CFStringCreateWithCharactersNoCopy)     \
  MACRO(CFStringCreateWithCString)              \
  MACRO(CFStringCreateWithFormat)               \
  MACRO(CFStringGetBytes)                       \
  MACRO(CFStringGetCharacters)                  \
  MACRO(CFStringGetCString)                     \
  MACRO(CFStringGetCStringPtr)                  \
  MACRO(CFStringGetIntValue)                    \
  MACRO(CFStringGetLength)                      \
  MACRO(CFStringGetMaximumSizeForEncoding)      \
  MACRO(CFStringHasPrefix)                      \
  MACRO(CFStringTokenizerAdvanceToNextToken)    \
  MACRO(CFStringTokenizerCreate)                \
  MACRO(CFStringTokenizerGetCurrentTokenRange)  \
  MACRO(CFURLCreateFromFileSystemRepresentation) \
  MACRO(CFURLCreateFromFSRef)                   \
  MACRO(CFURLCreateWithFileSystemPath)          \
  MACRO(CFURLCreateWithString)                  \
  MACRO(CFURLGetFileSystemRepresentation)       \
  MACRO(CFURLGetFSRef)                          \
  MACRO(CFUUIDCreate)                           \
  MACRO(CFUUIDCreateString)                     \
  MACRO(CFUUIDGetUUIDBytes)                     \
  MACRO(CGAffineTransformConcat)                \
  MACRO(CGBitmapContextCreateImage)             \
  MACRO(CGBitmapContextCreateWithData)          \
  MACRO(CGBitmapContextGetBytesPerRow)          \
  MACRO(CGBitmapContextGetHeight)               \
  MACRO(CGBitmapContextGetWidth)                \
  MACRO(CGColorRelease)                         \
  MACRO(CGColorSpaceCopyICCProfile)             \
  MACRO(CGFontCreateCopyWithVariations)         \
  MACRO(CGColorSpaceCreateDeviceRGB)            \
  MACRO(CGColorSpaceCreatePattern)              \
  MACRO(CGColorSpaceRelease)                    \
  MACRO(CGContextBeginTransparencyLayerWithRect) \
  MACRO(CGContextClipToRects)                   \
  MACRO(CGContextConcatCTM)                     \
  MACRO(CGContextDrawImage)                     \
  MACRO(CGContextEndTransparencyLayer)          \
  MACRO(CGContextFillRect)                      \
  MACRO(CGContextGetClipBoundingBox)            \
  MACRO(CGContextGetCTM)                        \
  MACRO(CGContextGetType)                       \
  MACRO(CGContextGetUserSpaceToDeviceSpaceTransform) \
  MACRO(CGContextRestoreGState)                 \
  MACRO(CGContextSaveGState)                    \
  MACRO(CGContextSetAllowsFontSubpixelPositioning) \
  MACRO(CGContextSetAllowsFontSubpixelQuantization) \
  MACRO(CGContextSetAlpha)                      \
  MACRO(CGContextSetBaseCTM)                    \
  MACRO(CGContextSetCTM)                        \
  MACRO(CGContextSetGrayFillColor)              \
  MACRO(CGContextSetRGBFillColor)               \
  MACRO(CGContextSetShouldAntialias)            \
  MACRO(CGContextSetShouldSmoothFonts)          \
  MACRO(CGContextSetShouldSubpixelPositionFonts) \
  MACRO(CGContextSetShouldSubpixelQuantizeFonts) \
  MACRO(CGContextSetTextDrawingMode)            \
  MACRO(CGContextSetTextMatrix)                 \
  MACRO(CGContextScaleCTM)                      \
  MACRO(CGContextTranslateCTM)                  \
  MACRO(CGDataProviderCreateWithData)           \
  MACRO(CGDisplayCopyColorSpace)                \
  MACRO(CGDisplayIOServicePort)                 \
  MACRO(CGEventSourceCounterForEventType)       \
  MACRO(CGFontCopyTableForTag)                  \
  MACRO(CGFontCopyTableTags)                    \
  MACRO(CGFontCopyVariations)                   \
  MACRO(CGFontCreateWithDataProvider)           \
  MACRO(CGFontCreateWithFontName)               \
  MACRO(CGFontCreateWithPlatformFont)           \
  MACRO(CGFontGetAscent)                        \
  MACRO(CGFontGetCapHeight)                     \
  MACRO(CGFontGetDescent)                       \
  MACRO(CGFontGetFontBBox)                      \
  MACRO(CGFontGetGlyphAdvances)                 \
  MACRO(CGFontGetGlyphBBoxes)                   \
  MACRO(CGFontGetGlyphPath)                     \
  MACRO(CGFontGetLeading)                       \
  MACRO(CGFontGetUnitsPerEm)                    \
  MACRO(CGFontGetXHeight)                       \
  MACRO(CGImageGetHeight)                       \
  MACRO(CGImageGetWidth)                        \
  MACRO(CGImageRelease)                         \
  MACRO(CGMainDisplayID)                        \
  MACRO(CGPathAddPath)                          \
  MACRO(CGPathApply)                            \
  MACRO(CGPathContainsPoint)                    \
  MACRO(CGPathCreateMutable)                    \
  MACRO(CGPathGetBoundingBox)                   \
  MACRO(CGPathGetCurrentPoint)                  \
  MACRO(CGPathIsEmpty)                          \
  MACRO(CGSSetDebugOptions)                     \
  MACRO(CTFontCopyFamilyName)                   \
  MACRO(CTFontCopyFontDescriptor)               \
  MACRO(CTFontCopyGraphicsFont)                 \
  MACRO(CTFontCopyTable)                        \
  MACRO(CTFontCopyVariationAxes)                \
  MACRO(CTFontCreateForString)                  \
  MACRO(CTFontCreatePathForGlyph)               \
  MACRO(CTFontCreateWithFontDescriptor)         \
  MACRO(CTFontCreateWithGraphicsFont)           \
  MACRO(CTFontCreateWithName)                   \
  MACRO(CTFontDescriptorCopyAttribute)          \
  MACRO(CTFontDescriptorCreateCopyWithAttributes) \
  MACRO(CTFontDescriptorCreateMatchingFontDescriptors) \
  MACRO(CTFontDescriptorCreateWithAttributes)   \
  MACRO(CTFontDrawGlyphs)                       \
  MACRO(CTFontGetAdvancesForGlyphs)             \
  MACRO(CTFontGetAscent)                        \
  MACRO(CTFontGetBoundingBox)                   \
  MACRO(CTFontGetBoundingRectsForGlyphs)        \
  MACRO(CTFontGetCapHeight)                     \
  MACRO(CTFontGetDescent)                       \
  MACRO(CTFontGetGlyphCount)                    \
  MACRO(CTFontGetGlyphsForCharacters)           \
  MACRO(CTFontGetLeading)                       \
  MACRO(CTFontGetSize)                          \
  MACRO(CTFontGetSymbolicTraits)                \
  MACRO(CTFontGetUnderlinePosition)             \
  MACRO(CTFontGetUnderlineThickness)            \
  MACRO(CTFontGetUnitsPerEm)                    \
  MACRO(CTFontGetXHeight)                       \
  MACRO(CTFontManagerCopyAvailableFontFamilyNames) \
  MACRO(CTFontManagerRegisterFontsForURLs)      \
  MACRO(CTFontManagerSetAutoActivationSetting)  \
  MACRO(CTLineCreateWithAttributedString)       \
  MACRO(CTLineGetGlyphRuns)                     \
  MACRO(CTRunGetAttributes)                     \
  MACRO(CTRunGetGlyphCount)                     \
  MACRO(CTRunGetGlyphsPtr)                      \
  MACRO(CTRunGetPositionsPtr)                   \
  MACRO(CTRunGetStringIndicesPtr)               \
  MACRO(CTRunGetStringRange)                    \
  MACRO(CTRunGetTypographicBounds)              \
  MACRO(CUIDraw)                                \
  MACRO(FSCompareFSRefs)                        \
  MACRO(FSGetVolumeInfo)                        \
  MACRO(FSFindFolder)                           \
  MACRO(Gestalt)                                \
  MACRO(GetEventClass)                          \
  MACRO(GetCurrentEventQueue)                   \
  MACRO(GetCurrentProcess)                      \
  MACRO(GetEventAttributes)                     \
  MACRO(GetEventDispatcherTarget)               \
  MACRO(GetEventKind)                           \
  MACRO(HIThemeDrawButton)                      \
  MACRO(HIThemeDrawFrame)                       \
  MACRO(HIThemeDrawGroupBox)                    \
  MACRO(HIThemeDrawGrowBox)                     \
  MACRO(HIThemeDrawMenuBackground)              \
  MACRO(HIThemeDrawMenuItem)                    \
  MACRO(HIThemeDrawMenuSeparator)               \
  MACRO(HIThemeDrawSeparator)                   \
  MACRO(HIThemeDrawTabPane)                     \
  MACRO(HIThemeDrawTrack)                       \
  MACRO(HIThemeGetGrowBoxBounds)                \
  MACRO(HIThemeSetFill)                         \
  MACRO(IORegistryEntrySearchCFProperty)        \
  MACRO(LSCopyAllHandlersForURLScheme)          \
  MACRO(LSCopyApplicationForMIMEType)           \
  MACRO(LSCopyItemAttribute)                    \
  MACRO(LSCopyKindStringForMIMEType)            \
  MACRO(LSGetApplicationForInfo)                \
  MACRO(LSGetApplicationForURL)                 \
  MACRO(NSClassFromString)                      \
  MACRO(NSRectFill)                             \
  MACRO(NSSearchPathForDirectoriesInDomains)    \
  MACRO(NSSetFocusRingStyle)                    \
  MACRO(NSTemporaryDirectory)                   \
  MACRO(OSSpinLockLock)                         \
  MACRO(ReleaseEvent)                           \
  MACRO(RemoveEventFromQueue)                   \
  MACRO(RetainEvent)                            \
  MACRO(SendEventToEventTarget)                 \
  MACRO(SLDisplayCopyColorSpace)                \
  MACRO(SLDisplayIOServicePort)                 \
  MACRO(SLEventSourceCounterForEventType)       \
  MACRO(SLMainDisplayID)

#define MAKE_CALL_EVENT(aName)  CallEvent_ ##aName ,

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
// system call redirections
///////////////////////////////////////////////////////////////////////////////

static ssize_t
RR_recv(int aSockFd, void* aBuf, size_t aLength, int aFlags)
{
  RecordReplayFunction(recv, ssize_t, aSockFd, aBuf, aLength, aFlags);
  events.CheckInput(aLength);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  MOZ_RELEASE_ASSERT((size_t) rval <= aLength);
  events.RecordOrReplayBytes(aBuf, rval);
  return rval;
}

static ssize_t
RR_recvmsg(int aSockFd, struct msghdr* aMsg, int aFlags)
{
  size_t* initialLengths = nullptr;
  if (!AreThreadEventsPassedThrough()) {
    initialLengths = new size_t[2 + aMsg->msg_iovlen];
    initialLengths[0] = aMsg->msg_controllen;
    initialLengths[1] = aMsg->msg_iovlen;
    for (int i = 0; i < aMsg->msg_iovlen; i++) {
      initialLengths[i + 2] = aMsg->msg_iov[i].iov_len;
    }
  }

  RecordReplayFunction(recvmsg, ssize_t, aSockFd, aMsg, aFlags);

  for (size_t i = 0; i < 2 + initialLengths[1]; i++) {
    events.CheckInput(initialLengths[i]);
  }
  delete[] initialLengths;

  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }

  events.RecordOrReplayValue(&aMsg->msg_flags);
  events.RecordOrReplayValue(&aMsg->msg_controllen);
  events.RecordOrReplayBytes(aMsg->msg_control, aMsg->msg_controllen);

  size_t nbytes = (size_t) rval;
  events.RecordOrReplayValue(&aMsg->msg_iovlen);
  for (int i = 0; i < aMsg->msg_iovlen && nbytes; i++) {
    struct iovec* iov = &aMsg->msg_iov[i];
    events.RecordOrReplayValue(&iov->iov_len);

    size_t iovbytes = nbytes <= iov->iov_len ? nbytes : iov->iov_len;
    events.RecordOrReplayBytes(iov->iov_base, iovbytes);
    nbytes -= iovbytes;
  }
  MOZ_RELEASE_ASSERT(nbytes == 0);
  return rval;
}

static ssize_t
RR_sendmsg(int aFd, struct msghdr* aMsg, int aFlags)
{
  RecordReplayFunction(sendmsg, ssize_t, aFd, aMsg, aFlags);
  events.CheckInput(aMsg->msg_iovlen);
  for (size_t i = 0; i < (size_t) aMsg->msg_iovlen; i++) {
    events.CheckInput(aMsg->msg_iov[i].iov_len);
  }
  RecordOrReplayHadErrorNegative(rrf);
  return rval;
}

static ssize_t
RR_kevent(int aKq, const struct kevent* aChanges, int aChangeCount,
          struct kevent* aEvents, int aEventCount,
          const struct timespec* aTimeout)
{
  RecordReplayFunction(kevent, ssize_t,
                       aKq, aChanges, aChangeCount, aEvents, aEventCount, aTimeout);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }

  // Copy the kevents verbatim. Note that kevent has an opaque pointer field
  // |udata| whose value could potentially vary between recording and replay.
  // Fortunately, all mozilla callers of this function only use scalar values
  // in |udata| so we don't need to be smarter here.
  events.CheckInput(aEventCount);
  events.RecordOrReplayBytes(aEvents, aEventCount * sizeof(struct kevent));
  return rval;
}

static ssize_t
RR_kevent64(int aKq, const struct kevent64_s* aChanges, int aChangeCount,
            struct kevent64_s* aEvents, int aEventCount, unsigned int aFlags,
            const struct timespec* aTimeout)
{
  RecordReplayFunction(kevent64, ssize_t,
                       aKq, aChanges, aChangeCount, aEvents, aEventCount, aFlags, aTimeout);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }

  // Copy the kevents verbatim, as for kevent().
  events.CheckInput(aEventCount);
  events.RecordOrReplayBytes(aEvents, aEventCount * sizeof(struct kevent64_s));
  return rval;
}

static ssize_t
RR_madvise(void* aAddress, size_t aSize, int aFlags)
{
  // Don't do anything with madvise for now, it is called by the JS GC.
  return OriginalCall(madvise, int, aAddress, aSize, aFlags);
}

static ssize_t
RR_mprotect(void* aAddress, size_t aSize, int aFlags)
{
  if (!HasSavedCheckpoint()) {
    return OriginalCall(mprotect, int, aAddress, aSize, aFlags);
  }
  return 0;
}

static void*
RR_mmap(void* aAddress, size_t aSize, int aProt, int aFlags, int aFd, off_t aOffset)
{
  MOZ_RELEASE_ASSERT(aAddress == PageBase(aAddress));

  // Make sure that fixed mappings do not interfere with snapshot state.
  if (aFlags & MAP_FIXED) {
    CheckFixedMemory(aAddress, RoundupSizeToPageBoundary(aSize));
  }

  void* memory = nullptr;
  if ((aFlags & MAP_ANON) || (IsReplaying() && !AreThreadEventsPassedThrough())) {
    // Get an anonymous mapping for the result.
    if (aFlags & MAP_FIXED) {
      // For fixed allocations, make sure this memory region is mapped and zero.
      if (!HasSavedCheckpoint()) {
        // Make sure this memory region is writable.
        OriginalCall(mprotect, int, aAddress, aSize, PROT_READ | PROT_WRITE | PROT_EXEC);
      }
      memset(aAddress, 0, aSize);
      memory = aAddress;
    } else {
      memory = AllocateMemoryTryAddress(aAddress, RoundupSizeToPageBoundary(aSize),
                                        MemoryKind::Tracked);
    }
  } else {
    // We have to call mmap itself, which can change memory protection flags
    // for memory that is already allocated. If we haven't saved a checkpoint
    // then this is no problem, but after saving a checkpoint we have to make
    // sure that protection flags are what we expect them to be.
    int newProt = HasSavedCheckpoint() ? (PROT_READ | PROT_EXEC) : aProt;
    memory = OriginalCall(mmap, void*, aAddress, aSize, newProt, aFlags, aFd, aOffset);

    if (aFlags & MAP_FIXED) {
      MOZ_RELEASE_ASSERT(memory == aAddress);
      RestoreWritableFixedMemory(memory, RoundupSizeToPageBoundary(aSize));
    } else if (memory && memory != (void*)-1) {
      RegisterAllocatedMemory(memory, RoundupSizeToPageBoundary(aSize), MemoryKind::Tracked);
    }
  }

  if (!(aFlags & MAP_ANON) && !AreThreadEventsPassedThrough()) {
    // Include the data just mapped in the recording.
    MOZ_RELEASE_ASSERT(memory && memory != (void*)-1);
    RecordReplayAssert("mmap");
    MOZ_RELEASE_ASSERT(aSize == RecordReplayValue(aSize));
    RecordReplayBytes(memory, aSize);
  }

  return memory;
}

static ssize_t
RR_munmap(void* aAddress, size_t aSize)
{
  DeallocateMemory(aAddress, aSize, MemoryKind::Tracked);
  return 0;
}

static ssize_t
RR_read(int aFd, void* aBuf, size_t aCount)
{
  RecordReplayFunction(read, ssize_t, aFd, aBuf, aCount);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(aCount);
  MOZ_RELEASE_ASSERT((size_t)rval <= aCount);
  events.RecordOrReplayBytes(aBuf, rval);
  return rval;
}

static ssize_t
RR___read_nocancel(int aFd, void* aBuf, size_t aCount)
{
  RecordReplayFunction(__read_nocancel, ssize_t, aFd, aBuf, aCount);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(aCount);
  MOZ_RELEASE_ASSERT((size_t)rval <= aCount);
  events.RecordOrReplayBytes(aBuf, rval);
  return rval;
}

static ssize_t
RR_pread(int aFd, void* aBuf, size_t aCount, size_t aOffset)
{
  RecordReplayFunction(pread, ssize_t, aFd, aBuf, aCount, aOffset);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(aCount);
  MOZ_RELEASE_ASSERT(!aBuf || (size_t) rval <= aCount);
  events.RecordOrReplayBytes(aBuf, rval);
  return rval;
}

static ssize_t
RR_write(int aFd, void* aBuf, size_t aCount)
{
  RecordReplayFunction(write, ssize_t, aFd, aBuf, aCount);
  RecordOrReplayHadErrorNegative(rrf);
  return rval;
}

RRFunctionNegError3(__write_nocancel)
RRFunctionNegError3(open)
RRFunctionNegError3(__open_nocancel)
RRFunctionNegError3(shm_open)
RRFunctionNegError3(socket)
RRFunctionNegError0(kqueue)
RRFunctionNegError1(close)
RRFunctionNegError1(__close_nocancel)
RRFunctionNegError2(mkdir)
RRFunctionNegError1(dup)
RRFunctionNegError2(access)
RRFunctionNegError3(lseek)

static ssize_t
RR_pipe(int* aFds)
{
  RecordReplayFunction(pipe, ssize_t, aFds);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.RecordOrReplayBytes(aFds, 2 * sizeof(int));
  return rval;
}

static ssize_t
RR_socketpair(int aDomain, int aType, int aProtocol, int* aFds)
{
  RecordReplayFunction(socketpair, ssize_t, aDomain, aType, aProtocol, aFds);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.RecordOrReplayBytes(aFds, 2 * sizeof(int));
  return rval;
}

static ssize_t
RR_fileport_makeport(int aFd, size_t* aPortName)
{
  RecordReplayFunction(fileport_makeport, ssize_t, aFd, aPortName);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.RecordOrReplayValue(aPortName);
  return rval;
}

static ssize_t
RR_getsockopt(int aSockFd, int aLevel, int aOptName, void* aOptVal, int* aOptLen)
{
  int initoptlen = *aOptLen;
  RecordReplayFunction(getsockopt, ssize_t, aSockFd, aLevel, aOptName, aOptVal, aOptLen);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput((size_t) initoptlen);
  events.RecordOrReplayValue(aOptLen);
  MOZ_RELEASE_ASSERT(*aOptLen <= initoptlen);
  events.RecordOrReplayBytes(aOptVal, *aOptLen);
  return rval;
}

static ssize_t
RR_gettimeofday(struct timeval* aTimeVal, struct timezone* aTimeZone)
{
  // If we have diverged from the recording, just get the actual current time
  // rather than causing the current debugger operation to fail. This function
  // is frequently called via e.g. JS natives which the debugger will execute.
  if (HasDivergedFromRecording()) {
    AutoEnsurePassThroughThreadEvents pt;
    return OriginalCall(gettimeofday, ssize_t, aTimeVal, aTimeZone);
  }

  RecordReplayFunction(gettimeofday, ssize_t, aTimeVal, aTimeZone);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput((aTimeVal ? 1 : 0) | (aTimeZone ? 2 : 0));
  if (aTimeVal) {
    events.RecordOrReplayBytes(aTimeVal, sizeof(struct timeval));
  }
  if (aTimeZone) {
    events.RecordOrReplayBytes(aTimeZone, sizeof(struct timezone));
  }
  return rval;
}

RRFunction0(getuid)
RRFunction0(geteuid)
RRFunction0(getgid)
RRFunction0(getegid)
RRFunction0(issetugid)
RRFunction0(__gettid)
RRFunction0(getpid)

static ssize_t
RR_fcntl(int aFd, int aCmd, size_t aArg)
{
  switch (aCmd) {
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

  RecordReplayFunction(fcntl, ssize_t, aFd, aCmd, aArg);
  RecordOrReplayHadErrorNegative(rrf);
  return rval;
}

static ssize_t
RR_getattrlist(const char* aPath, struct attrlist* aAttrList,
               void* aAttrBuf, size_t aAttrBufSize, unsigned long aOptions)
{
  RecordReplayFunction(getattrlist, ssize_t, aPath, aAttrList, aAttrBuf, aAttrBufSize, aOptions);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(aAttrBufSize);
  events.RecordOrReplayBytes(aAttrBuf, aAttrBufSize);
  return rval;
}

#define RecordReplayStatFunction(aName, aBufType)       \
  static ssize_t                                        \
  RR_ ##aName(size_t a0, aBufType* aBuf)                \
  {                                                     \
    RecordReplayFunction(aName, ssize_t, a0, aBuf);     \
    if (RecordOrReplayHadErrorNegative(rrf)) {          \
      return rval;                                      \
    }                                                   \
    events.RecordOrReplayBytes(aBuf, sizeof(aBufType)); \
    return rval;                                        \
  }

RecordReplayStatFunction(fstat$INODE64, struct stat)
RecordReplayStatFunction(lstat$INODE64, struct stat)
RecordReplayStatFunction(stat$INODE64, struct stat)
RecordReplayStatFunction(statfs$INODE64, struct statfs)
RecordReplayStatFunction(fstatfs$INODE64, struct statfs)

static ssize_t
RR_readlink(const char* aPath, char* aBuf, size_t aBufSize)
{
  RecordReplayFunction(readlink, ssize_t, aPath, aBuf, aBufSize);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(aBufSize);
  events.RecordOrReplayBytes(aBuf, aBufSize);
  return rval;
}

static ssize_t
RR___getdirentries64(int aFd, void* aBuf, size_t aBufSize, size_t* aPosition)
{
  RecordReplayFunction(__getdirentries64, ssize_t, aFd, aBuf, aBufSize, aPosition);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(aBufSize);
  events.RecordOrReplayBytes(aBuf, aBufSize);
  events.RecordOrReplayValue(aPosition);
  return rval;
}

static ssize_t
RR_getdirentriesattr(int aFd, struct attrlist* aAttrList,
                     void* aBuffer, size_t aBuffersize,
                     size_t* aCount, size_t* aBase, size_t* aNewState, size_t aOptions)
{
  RecordReplayFunction(getdirentriesattr, ssize_t,
                       aFd, aAttrList, aBuffer, aBuffersize, aCount, aBase, aNewState, aOptions);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(aBuffersize);
  events.RecordOrReplayBytes(aAttrList, sizeof(struct attrlist));
  events.RecordOrReplayBytes(aBuffer, aBuffersize);
  events.RecordOrReplayValue(aCount);
  events.RecordOrReplayValue(aBase);
  events.RecordOrReplayValue(aNewState);
  return rval;
}

static ssize_t
RR_getrusage(int aWho, struct rusage* aUsage)
{
  RecordReplayFunction(getrusage, ssize_t, aWho, aUsage);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.RecordOrReplayBytes(aUsage, sizeof(*aUsage));
  return rval;
}

static ssize_t
RR___getrlimit(int aWho, struct rlimit* aLimit)
{
  RecordReplayFunction(__getrlimit, ssize_t, aWho, aLimit);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.RecordOrReplayBytes(aLimit, sizeof(*aLimit));
  return rval;
}

RRFunctionNegError2(__setrlimit)

static ssize_t
RR_sigprocmask(int aHow, const sigset_t* aSet, sigset_t* aOldSet)
{
  RecordReplayFunction(sigprocmask, ssize_t, aHow, aSet, aOldSet);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(!!aOldSet);
  if (aOldSet) {
    events.RecordOrReplayBytes(aOldSet, sizeof(*aOldSet));
  }
  return rval;
}

static ssize_t
RR_sigaltstack(const stack_t* aSs, stack_t* aOldSs)
{
  RecordReplayFunction(sigaltstack, ssize_t, aSs, aOldSs);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(!!aOldSs);
  if (aOldSs) {
    events.RecordOrReplayBytes(aOldSs, sizeof(*aOldSs));
  }
  return rval;
}

static ssize_t
RR_sigaction(int aSignum, const struct sigaction* aAct, struct sigaction* aOldAct)
{
  RecordReplayFunction(sigaction, ssize_t, aSignum, aAct, aOldAct);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(!!aOldAct);
  if (aOldAct) {
    events.RecordOrReplayBytes(aOldAct, sizeof(struct sigaction));
  }
  return rval;
}

static ssize_t
RR___pthread_sigmask(int aHow, const sigset_t* aSet, sigset_t* aOldSet)
{
  RecordReplayFunction(__pthread_sigmask, ssize_t, aHow, aSet, aOldSet);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(!!aOldSet);
  if (aOldSet) {
    events.RecordOrReplayBytes(aOldSet, sizeof(sigset_t));
  }
  return rval;
}

static ssize_t
RR___fsgetpath(void* aBuf, size_t aBufSize, void* aFsId, uint64_t aObjId)
{
  RecordReplayFunction(__fsgetpath, ssize_t, aBuf, aBufSize, aFsId, aObjId);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(aBufSize);
  events.RecordOrReplayBytes(aBuf, aBufSize);
  return rval;
}

static ssize_t
RR___disable_threadsignal(int a0)
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
  return OriginalCall(__disable_threadsignal, ssize_t, a0);
}

static ssize_t
RR___sysctl(int* aName, size_t aNamelen, void* aOld, size_t* aOldlenp,
            void* aNewData, size_t aNewLen)
{
  size_t initlen = aOldlenp ? *aOldlenp : 0;
  RecordReplayFunction(__sysctl, ssize_t, aName, aNamelen, aOld, aOldlenp, aNewData, aNewLen);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput((aOld ? 1 : 0) | (aOldlenp ? 2 : 0));
  events.CheckInput(initlen);
  if (aOldlenp) {
    events.RecordOrReplayValue(aOldlenp);
  }
  if (aOld) {
    MOZ_RELEASE_ASSERT(aOldlenp);
    events.RecordOrReplayBytes(aOld, *aOldlenp);
  }
  return rval;
}

RRFunctionNegError3(__mac_syscall)

static ssize_t
RR_getaudit_addr(auditinfo_addr_t* aAuditInfo, size_t aLength)
{
  RecordReplayFunction(getaudit_addr, ssize_t, aAuditInfo, aLength);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.RecordOrReplayBytes(aAuditInfo, sizeof(*aAuditInfo));
  return rval;
}

RRFunction1(umask)

static ssize_t
RR___select(int aFdCount, fd_set* aReadFds, fd_set* aWriteFds,
            fd_set* aExceptFds, timeval* aTimeout)
{
  RecordReplayFunction(__select, ssize_t, aFdCount, aReadFds, aWriteFds, aExceptFds, aTimeout);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.RecordOrReplayBytes(aReadFds, sizeof(fd_set));
  events.RecordOrReplayBytes(aWriteFds, sizeof(fd_set));
  events.RecordOrReplayBytes(aExceptFds, sizeof(fd_set));
  events.CheckInput(!!aTimeout);
  if (aTimeout) {
    events.RecordOrReplayBytes(aTimeout, sizeof(timeval));
  }
  return rval;
}

RRFunctionNegError3(__process_policy);
RRFunctionNegError6(__kdebug_trace);

static ssize_t
RR_guarded_kqueue_np(size_t* aGuard, int aFlags)
{
  RecordReplayFunction(guarded_kqueue_np, ssize_t, aGuard, aFlags);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.RecordOrReplayValue(aGuard);
  return rval;
}

static ssize_t
RR_csops(pid_t aPid, uint32_t aOps, void* aBuf, size_t aBufSize)
{
  RecordReplayFunction(csops, ssize_t, aPid, aOps, aBuf, aBufSize);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(aBufSize);
  events.RecordOrReplayBytes(aBuf, aBufSize);
  return rval;
}

static ssize_t
RR___getlogin(char* aName, size_t aLength)
{
  RecordReplayFunction(__getlogin, ssize_t, aName, aLength);
  if (RecordOrReplayHadErrorNegative(rrf)) {
    return rval;
  }
  events.CheckInput(aLength);
  events.RecordOrReplayBytes(aName, aLength);
  return rval;
}

static ssize_t
RR___workq_kernreturn(size_t a0, size_t a1, size_t a2, size_t a3)
{
  // Busy-wait until initialization is complete.
  while (!gInitialized) {
    ThreadYield();
  }

  // Make sure we know this thread exists.
  Thread::Current();

  return OriginalCall(__workq_kernreturn, ssize_t, a0, a1, a2, a3);
}

static void
RR_start_wqthread(size_t a0, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5, size_t a6, size_t a7)
{
  // When replaying we don't want system threads to run, but by the time we
  // initialize the record/replay system GCD has already started running.
  // Use this redirection to watch for new threads being spawned by GCD, and
  // suspend them immediately.
  if (IsReplaying()) {
    Thread::WaitForeverNoIdle();
  }

  OriginalCall(start_wqthread, void, a0, a1, a2, a3, a4, a5, a6, a7);
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
WaitForCvar(pthread_mutex_t* aMutex, bool aRecordReturnValue,
            const std::function<ssize_t()>& aCallback)
{
  Lock* lock = Lock::Find(aMutex);
  if (!lock) {
    AutoEnsurePassThroughThreadEvents pt;
    return aCallback();
  }
  RecordReplayAssert("WaitForCvar %d", (int) lock->Id());
  ssize_t rv = 0;
  if (IsRecording()) {
    {
      AutoPassThroughThreadEvents pt;
      rv = aCallback();
    }
  } else {
    DirectUnlockMutex(aMutex);
  }
  lock->Enter([=]() { DirectLockMutex(aMutex); });
  if (aRecordReturnValue) {
    return RecordReplayValue(rv);
  }
  MOZ_RELEASE_ASSERT(rv == 0);
  return 0;
}

static ssize_t
RR_pthread_cond_wait(pthread_cond_t* aCond, pthread_mutex_t* aMutex)
{
  return WaitForCvar(aMutex, false,
                     [=]() { return OriginalCall(pthread_cond_wait, ssize_t, aCond, aMutex); });
}

static ssize_t
RR_pthread_cond_timedwait(pthread_cond_t* aCond, pthread_mutex_t* aMutex,
                          timespec* aTimeout)
{
  return WaitForCvar(aMutex, true,
                     [=]() { return OriginalCall(pthread_cond_timedwait, ssize_t,
                                                 aCond, aMutex, aTimeout); });
}

static ssize_t
RR_pthread_cond_timedwait_relative_np(pthread_cond_t* aCond, pthread_mutex_t* aMutex,
                                      timespec* aTimeout)
{
  return WaitForCvar(aMutex, true,
                     [=]() { return OriginalCall(pthread_cond_timedwait_relative_np, ssize_t,
                                                 aCond, aMutex, aTimeout); });
}

static ssize_t
RR_pthread_create(pthread_t* aToken, const pthread_attr_t* aAttr,
                  void* (*aStart)(void*), void* aStartArg)
{
  if (AreThreadEventsPassedThrough()) {
    return OriginalCall(pthread_create, ssize_t, aToken, aAttr, aStart, aStartArg);
  }

  int detachState;
  int rv = pthread_attr_getdetachstate(aAttr, &detachState);
  MOZ_RELEASE_ASSERT(rv == 0);

  *aToken = Thread::StartThread((Thread::Callback) aStart, aStartArg,
                                detachState == PTHREAD_CREATE_JOINABLE);
  return 0;
}

static ssize_t
RR_pthread_join(pthread_t aToken, void** aPtr)
{
  if (AreThreadEventsPassedThrough()) {
    return OriginalCall(pthread_join, int, aToken, aPtr);
  }

  Thread* thread = Thread::GetByNativeId(aToken);
  thread->Join();

  *aPtr = nullptr;
  return 0;
}

static ssize_t
RR_pthread_mutex_init(pthread_mutex_t* aMutex, const pthread_mutexattr_t* aAttr)
{
  Lock::New(aMutex);
  return OriginalCall(pthread_mutex_init, ssize_t, aMutex, aAttr);
}

static ssize_t
RR_pthread_mutex_destroy(pthread_mutex_t* aMutex)
{
  Lock::Destroy(aMutex);
  return OriginalCall(pthread_mutex_destroy, ssize_t, aMutex);
}

static ssize_t
RR_pthread_mutex_lock(pthread_mutex_t* aMutex)
{
  Lock* lock = Lock::Find(aMutex);
  if (!lock) {
    AutoEnsurePassThroughThreadEventsUseStackPointer pt;
    return OriginalCall(pthread_mutex_lock, ssize_t, aMutex);
  }
  if (IsRecording()) {
    DirectLockMutex(aMutex);
  }
  lock->Enter([=]() { DirectLockMutex(aMutex); });
  return 0;
}

static ssize_t
RR_pthread_mutex_trylock(pthread_mutex_t* aMutex)
{
  Lock* lock = Lock::Find(aMutex);
  if (!lock) {
    AutoEnsurePassThroughThreadEvents pt;
    return OriginalCall(pthread_mutex_trylock, ssize_t, aMutex);
  }
  ssize_t rv = 0;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    rv = OriginalCall(pthread_mutex_trylock, ssize_t, aMutex);
  }
  rv = RecordReplayValue(rv);
  MOZ_RELEASE_ASSERT(rv == 0 || rv == EBUSY);
  if (rv == 0) {
    lock->Enter([=]() { DirectLockMutex(aMutex); });
  }
  return rv;
}

static ssize_t
RR_pthread_mutex_unlock(pthread_mutex_t* aMutex)
{
  AutoEnsurePassThroughThreadEventsUseStackPointer pt;
  return OriginalCall(pthread_mutex_unlock, ssize_t, aMutex);
}

///////////////////////////////////////////////////////////////////////////////
// stdlib redirections
///////////////////////////////////////////////////////////////////////////////

static ssize_t
RR_dlclose(void* aHandle)
{
  return 0;
}

static void*
RR_dlopen(const char* aFilename, int aFlags)
{
  AutoEnsurePassThroughThreadEvents pt;
  return OriginalCall(dlopen, void*, aFilename, aFlags);
}

static void*
RR_dlsym(void* aHandle, const char* aSymbol)
{
  AutoEnsurePassThroughThreadEvents pt;
  return OriginalCall(dlsym, void*, aHandle, aSymbol);
}

RRFunctionNegError1(fclose)
RRFunctionZeroError2(fopen)

static size_t
RR_fread(void *aBuf, size_t aElemSize, size_t aCapacity, FILE *aFile)
{
  RecordReplayFunction(fread, size_t, aBuf, aElemSize, aCapacity, aFile);
  events.RecordOrReplayValue(&rval);
  events.CheckInput(aElemSize);
  events.CheckInput(aCapacity);
  MOZ_RELEASE_ASSERT((size_t)rval <= aCapacity);
  events.RecordOrReplayBytes(aBuf, rval * aElemSize);
  return rval;
}

RRFunctionNegError3(fseek)
RRFunctionNegError1(ftell)
RRFunction4(fwrite)

static void
RecordReplayCString(Stream& aEvents, char** aString)
{
  size_t len = (IsRecording() && *aString) ? strlen(*aString) + 1 : 0;
  aEvents.RecordOrReplayValue(&len);
  if (IsReplaying()) {
    *aString = len ? NewLeakyArray<char>(len) : nullptr;
  }
  if (len) {
    aEvents.RecordOrReplayBytes(*aString, len);
  }
}

static char*
RR_getenv(char* aName)
{
  RecordReplayFunction(getenv, char*, aName);
  RecordReplayCString(events, &rval);
  return rval;
}

#define RecordReplayTimeFunction(aName)                                 \
  static struct tm*                                                     \
  RR_ ##aName(const time_t* aTime, struct tm* aResult)                  \
  {                                                                     \
    MOZ_RELEASE_ASSERT(aResult);                                        \
    RecordReplayFunction(aName, struct tm*, aTime, aResult);            \
    if (RecordOrReplayHadErrorZero(rrf)) {                              \
      return nullptr;                                                   \
    }                                                                   \
    events.RecordOrReplayBytes(aResult, sizeof(struct tm));             \
    if (IsRecording()) {                                                \
      MOZ_RELEASE_ASSERT(rval == aResult);                              \
    }                                                                   \
    return aResult;                                                     \
  }

RecordReplayTimeFunction(localtime_r)
RecordReplayTimeFunction(gmtime_r)

static struct tm gGlobalTM;

// localtime and gmtime behave the same as localtime_r and gmtime_r, except
// they are not reentrant.
#define RecordReplayNonReentrantTimeFunction(aName)                     \
  static struct tm*                                                     \
  RR_ ##aName(const time_t* aTime)                                      \
  {                                                                     \
    return RR_ ##aName ##_r(aTime, &gGlobalTM);                         \
  }

RecordReplayNonReentrantTimeFunction(localtime)
RecordReplayNonReentrantTimeFunction(gmtime)

static time_t
RR_mktime(struct tm* aTime)
{
  RecordReplayFunction(mktime, time_t, aTime);
  events.RecordOrReplayBytes(aTime, sizeof(struct tm));
  events.RecordOrReplayValue(&rval);
  return rval;
}

static char*
RR_setlocale(int aCategory, const char* aLocale)
{
  RecordReplayFunction(setlocale, char*, aCategory, aLocale);
  RecordReplayCString(events, &rval);
  return rval;
}

static size_t
RR_strftime(char* aStr, size_t aSize, const char* aFormat, const struct tm* aTime)
{
  RecordReplayFunction(strftime, size_t, aStr, aSize, aFormat, aTime);
  events.CheckInput(aSize);
  events.RecordOrReplayValue(&rval);
  MOZ_RELEASE_ASSERT(rval < aSize);
  events.RecordOrReplayBytes(aStr, rval + 1);
  return rval;
}

static size_t
RR_arc4random()
{
  RecordReplayFunction(arc4random, size_t);
  events.RecordOrReplayValue(&rval);
  return rval;
}

static uint64_t
RR_mach_absolute_time()
{
  RecordReplayFunction(mach_absolute_time, uint64_t);
  events.RecordOrReplayValue(&rval);
  return rval;
}

static mach_msg_return_t
RR_mach_msg(mach_msg_header_t* aMsg, mach_msg_option_t aOption, mach_msg_size_t aSendSize,
            mach_msg_size_t aReceiveLimit, mach_port_t aReceiveName, mach_msg_timeout_t aTimeout,
            mach_port_t aNotify)
{
  RecordReplayFunction(mach_msg, mach_msg_return_t,
                       aMsg, aOption, aSendSize, aReceiveLimit, aReceiveName, aTimeout, aNotify);
  AutoOrderedAtomicAccess(); // mach_msg may be used for inter-thread synchronization.
  events.RecordOrReplayValue(&rval);
  events.CheckInput(aReceiveLimit);
  if (aReceiveLimit) {
    events.RecordOrReplayBytes(aMsg, aReceiveLimit);
  }
  return rval;
}

static kern_return_t
RR_mach_timebase_info(mach_timebase_info_data_t* aInfo)
{
  RecordReplayFunction(mach_timebase_info, kern_return_t, aInfo);
  events.RecordOrReplayValue(&rval);
  events.RecordOrReplayBytes(aInfo, sizeof(*aInfo));
  return rval;
}

static void
RR_sandbox_free_error(char* aBuf)
{
  if (IsRecording()) {
    OriginalCall(sandbox_free_error, void, aBuf);
  } else {
    delete[] aBuf;
  }
}

static ssize_t
RR_sandbox_init(const char* aProfile, uint64_t aFlags, char** aErrorBuf)
{
  // By passing through events when initializing the sandbox, we ensure both
  // that we actually initialize the process sandbox while replaying as well as
  // while recording, and that any activity in here does not interfere with the
  // replay.
  AutoEnsurePassThroughThreadEvents pt;
  return OriginalCall(sandbox_init, ssize_t,
                      aProfile, aFlags, aErrorBuf);
}

static ssize_t
RR_sandbox_init_with_parameters(const char* aProfile, uint64_t aFlags,
                                const char *const aParameters[], char** aErrorBuf)
{
  // As for sandbox_init, call this function even while replaying.
  AutoEnsurePassThroughThreadEvents pt;
  return OriginalCall(sandbox_init_with_parameters, ssize_t,
                      aProfile, aFlags, aParameters, aErrorBuf);
}

static kern_return_t
RR_mach_vm_allocate(vm_map_t aTarget, mach_vm_address_t* aAddress,
                    mach_vm_size_t aSize, int aFlags)
{
  *aAddress = (mach_vm_address_t) AllocateMemory(aSize, MemoryKind::Tracked);
  return KERN_SUCCESS;
}

static kern_return_t
RR_mach_vm_deallocate(vm_map_t aTarget, mach_vm_address_t aAddress, mach_vm_size_t aSize)
{
  DeallocateMemory((void*) aAddress, aSize, MemoryKind::Tracked);
  return KERN_SUCCESS;
}

static kern_return_t
RR_mach_vm_protect(vm_map_t aTarget, mach_vm_address_t aAddress, mach_vm_size_t aSize,
                   boolean_t aSetMaximum, vm_prot_t aNewProtection)
{
  if (!HasSavedCheckpoint()) {
    return OriginalCall(mach_vm_protect, kern_return_t,
                        aTarget, aAddress, aSize, aSetMaximum, aNewProtection);
  }
  return KERN_SUCCESS;
}

static kern_return_t
RR_vm_purgable_control(vm_map_t aTarget, mach_vm_address_t aAddress,
                       vm_purgable_t aControl, int* aState)
{
  // Never allow purging of volatile memory, to simplify memory snapshots.
  *aState = VM_PURGABLE_NONVOLATILE;
  return KERN_SUCCESS;
}

static kern_return_t
RR_vm_copy(vm_map_t aTarget, vm_address_t aSourceAddress,
           vm_size_t aSize, vm_address_t aDestAddress)
{
  // Asking the kernel to copy memory doesn't work right if the destination is
  // non-writable, so do the copy manually.
  memcpy((void*) aDestAddress, (void*) aSourceAddress, aSize);
  return KERN_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// NSPR redirections
///////////////////////////////////////////////////////////////////////////////

// Even though NSPR is compiled as part of firefox, it is easier to just
// redirect this stuff than get record/replay related changes into the source.

static PLHashTable*
RR_PL_NewHashTable(PRUint32 aNumBuckets, PLHashFunction aKeyHash,
                   PLHashComparator aKeyCompare, PLHashComparator aValueCompare,
                   const PLHashAllocOps *aAllocOps, void *aAllocPriv)
{
  GeneratePLHashTableCallbacks(&aKeyHash, &aKeyCompare, &aValueCompare, &aAllocOps, &aAllocPriv);
  return OriginalCall(PL_NewHashTable, PLHashTable*,
                      aNumBuckets, aKeyHash, aKeyCompare, aValueCompare, aAllocOps, aAllocPriv);
}

static void
RR_PL_HashTableDestroy(PLHashTable* aTable)
{
  void* priv = aTable->allocPriv;
  OriginalCall(PL_HashTableDestroy, void, aTable);
  DestroyPLHashTableCallbacks(priv);
}

///////////////////////////////////////////////////////////////////////////////
// Objective C redirections
///////////////////////////////////////////////////////////////////////////////

RRFunction2(class_getClassMethod)
RRFunction2(class_getInstanceMethod)
RRFunctionVoid2(method_exchangeImplementations)
RRFunctionVoid1(objc_autoreleasePoolPop)
RRFunction0(objc_autoreleasePoolPush)

// Capture the arguments that can be passed to an Objective C message. We only
// need to capture enough argument data here for messages sent directly from
// Gecko code (i.e. where events are not passed through). Messages sent while
// events are passed through are performed with the same stack and register
// state as when they were initially invoked.
struct MessageArguments
{
  size_t obj;       // 0
  const char* msg;  // 8
  size_t arg0;      // 16
  size_t arg1;      // 24
  size_t arg2;      // 32
  size_t arg3;      // 40
  double floatarg0; // 48
  size_t scratch;   // 56
  size_t stack[64]; // 64
};                  // 576

extern "C" {

extern size_t
RecordReplayInvokeObjCMessage(MessageArguments* aArguments, void* aFnPtr);

__asm(
"_RecordReplayInvokeObjCMessage:"

  // Save function pointer in rax.
  "movq %rsi, %rax;"

  // Save arguments on the stack. This also aligns the stack.
  "push %rdi;"

  // Count how many stack arguments we need to copy.
  "movq $64, %rsi;"

  // Enter the loop below. The compiler might not place this block of code
  // adjacent to the loop, so perform the jump explicitly.
  "jmp _RecordReplayInvokeObjCMessage_Loop;"

  // Copy each stack argument to the stack.
"_RecordReplayInvokeObjCMessage_Loop:"
  "subq $1, %rsi;"
  "movq 64(%rdi, %rsi, 8), %rdx;"
  "push %rdx;"
  "testq %rsi, %rsi;"
  "jne _RecordReplayInvokeObjCMessage_Loop;"

  // Copy each register argument into the appropriate register.
  "movq 8(%rdi), %rsi;"
  "movq 16(%rdi), %rdx;"
  "movq 24(%rdi), %rcx;"
  "movq 32(%rdi), %r8;"
  "movq 40(%rdi), %r9;"
  "movsd 48(%rdi), %xmm0;"
  "movq 0(%rdi), %rdi;"

  // Call the saved function pointer.
  "callq *%rax;"

  // Pop the copied stack arguments.
  "addq $512, %rsp;"

  // Save any floating point rval to the arguments.
  "pop %rdi;"
  "movsd %xmm0, 48(%rdi);"

  "ret;"
);

} // extern "C"

static bool
TestObjCObjectClass(size_t aObj, const char* aName)
{
  // When recording we can test to see what the class of an object is, but we
  // have to record the result of the test because the object "pointers" we
  // have when replaying are not valid.
  bool found = false;
  if (IsRecording()) {
    Class cls = object_getClass((id)aObj);
    while (cls) {
      if (!strcmp(class_getName(cls), aName)) {
        found = true;
        break;
      }
      cls = class_getSuperclass(cls);
    }
  }
  return RecordReplayValue(found);
}

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

extern "C" {

size_t
RecordReplayInterceptObjCMessage(MessageArguments* aArguments)
{
  if (AreThreadEventsPassedThrough()) {
    aArguments->scratch = (size_t) OriginalFunction(CallEvent_objc_msgSend);
    return 1;
  }
  EnsureNotDivergedFromRecording();

  RecordReplayAssert("objc_msgSend: %s", aArguments->msg);

  size_t rval = 0;
  double floatRval = 0;
  bool handled = false;

  // Watch for some top level NSApplication messages that can cause Gecko
  // events to be processed.
  if ((!strcmp(aArguments->msg, "run") ||
       !strcmp(aArguments->msg, "nextEventMatchingMask:untilDate:inMode:dequeue:")) &&
      TestObjCObjectClass(aArguments->obj, "NSApplication"))
  {
    PassThroughThreadEventsAllowCallbacks([&]() {
        rval = RecordReplayInvokeObjCMessage(aArguments, OriginalFunction(CallEvent_objc_msgSend));
      });
    handled = true;
  }

  // Other messages are performed as normal.
  if (!handled && IsRecording()) {
    AutoPassThroughThreadEvents pt;
    rval = RecordReplayInvokeObjCMessage(aArguments, OriginalFunction(CallEvent_objc_msgSend));
    floatRval = aArguments->floatarg0;
  }

  rval = RecordReplayValue(rval);
  RecordReplayBytes(&floatRval, sizeof(double));

  // Do some extra recording on messages that return additional state.

  if (!strcmp(aArguments->msg, "countByEnumeratingWithState:objects:count:") &&
      TestObjCObjectClass(aArguments->obj, "NSArray"))
  {
    NSFastEnumerationState* state = (NSFastEnumerationState*)aArguments->arg0;
    if (IsReplaying()) {
      state->itemsPtr = NewLeakyArray<id>(rval);
      state->mutationsPtr = &gNeverChange;
    }
    RecordReplayBytes(state->itemsPtr, rval * sizeof(id));
  }

  if (!strcmp(aArguments->msg, "getCharacters:") &&
      TestObjCObjectClass(aArguments->obj, "NSString"))
  {
    size_t len = 0;
    if (IsRecording()) {
      AutoPassThroughThreadEvents pt;
      len = CFStringGetLength((CFStringRef) aArguments->obj);
    }
    len = RecordReplayValue(len);
    RecordReplayBytes((void*)aArguments->arg0, len * sizeof(wchar_t));
  }

  if ((!strcmp(aArguments->msg, "UTF8String") ||
       !strcmp(aArguments->msg, "cStringUsingEncoding:")) &&
      TestObjCObjectClass(aArguments->obj, "NSString"))
  {
    size_t len = RecordReplayValue(IsRecording() ? strlen((const char*) rval) : 0);
    if (IsReplaying()) {
      rval = (size_t) NewLeakyArray<uint8_t>(len + 1);
    }
    RecordReplayBytes((void*) rval, len + 1);
  }

  // Remember the return value for the caller.
  aArguments->scratch = rval;
  aArguments->floatarg0 = floatRval;
  return 0;
}

extern size_t
RR_objc_msgSend(id, SEL, ...);

__asm(
"_RR_objc_msgSend:"

  // Make space for a MessageArguments struct on the stack.
  "subq $576, %rsp;"

  // Fill in the structure's contents.
  "movq %rdi, 0(%rsp);"
  "movq %rsi, 8(%rsp);"
  "movq %rdx, 16(%rsp);"
  "movq %rcx, 24(%rsp);"
  "movq %r8, 32(%rsp);"
  "movq %r9, 40(%rsp);"
  "movsd %xmm0, 48(%rsp);"

  // Count how many stack arguments we need to save.
  "movq $64, %rsi;"

  // Enter the loop below. The compiler might not place this block of code
  // adjacent to the loop, so perform the jump explicitly.
  "jmp _RR_objc_msgSend_Loop;"

  // Save stack arguments into the structure.
"_RR_objc_msgSend_Loop:"
  "subq $1, %rsi;"
  "movq 584(%rsp, %rsi, 8), %rdx;" // Ignore the return ip on the stack.
  "movq %rdx, 64(%rsp, %rsi, 8);"
  "testq %rsi, %rsi;"
  "jne _RR_objc_msgSend_Loop;"

  // Place the structure's address into the first argument register.
  "movq %rsp, %rdi;"

  // The stack must be have 16 byte alignment. The MessageArguments structure
  // uses 576 bytes, and the frame pointer pushed by the callee uses 8 bytes,
  // so push 8 bytes of empty space.
  "subq $8, %rsp;"
  "call _RecordReplayInterceptObjCMessage;"
  "addq $8, %rsp;"

  "testq %rax, %rax;"
  "je RR_objc_msgSend_done;"

  // Events are passed through. Invoke the message with the original stack and
  // register state.
  "movq 0(%rsp), %rdi;"
  "movq 8(%rsp), %rsi;"
  "movq 16(%rsp), %rdx;"
  "movq 24(%rsp), %rcx;"
  "movq 32(%rsp), %r8;"
  "movq 40(%rsp), %r9;"
  "movsd 48(%rsp), %xmm0;"
  "movq 56(%rsp), %rax;"
  "addq $576, %rsp;"
  "jmpq *%rax;"

  // The message has been recorded/replayed.
"RR_objc_msgSend_done:"
  // Restore scalar and floating point return values.
  "movsd 48(%rsp), %xmm0;"
  "movq 56(%rsp), %rax;"

  // Pop the structure from the stack.
  "addq $576, %rsp;"

  // Return to caller.
  "ret;"
);

} // extern "C"

///////////////////////////////////////////////////////////////////////////////
// Cocoa redirections
///////////////////////////////////////////////////////////////////////////////

RRFunction4(AcquireFirstMatchingEventInQueue)

static void
RR_CFArrayAppendValue(CFArrayRef aArray, CFTypeRef aValue)
{
  RecordReplayFunctionVoid(CFArrayAppendValue, aArray, aValue);
}

RRFunction4(CFArrayCreate)
RRFunction1(CFArrayGetCount)
RRFunction2(CFArrayGetValueAtIndex)
RRFunctionVoid2(CFArrayRemoveValueAtIndex)
RRFunction3(CFAttributedStringCreate)
RRFunction1(CFBundleCopyExecutableURL)
RRFunction1(CFBundleCopyInfoDictionaryForURL)
RRFunction2(CFBundleCreate)
RRFunction1(CFBundleGetBundleWithIdentifier)

static void*
RR_CFBundleGetDataPointerForName(CFBundleRef aBundle, CFStringRef aSymbolName)
{
  if (AreThreadEventsPassedThrough()) {
    return OriginalCall(CFBundleGetDataPointerForName, void*, aBundle, aSymbolName);
  }
  return nullptr;
}

static void*
RR_CFBundleGetFunctionPointerForName(CFBundleRef aBundle, CFStringRef aFunctionName)
{
  if (AreThreadEventsPassedThrough()) {
    return OriginalCall(CFBundleGetFunctionPointerForName, void*, aBundle, aFunctionName);
  }
  return nullptr;
}

RRFunction1(CFBundleGetIdentifier)
RRFunction2(CFBundleGetValueForInfoDictionaryKey)
RRFunction1(CFBundleGetInfoDictionary)
RRFunction0(CFBundleGetMainBundle)

static const UInt8*
RR_CFDataGetBytePtr(CFDataRef aData)
{
  RecordReplayFunction(CFDataGetBytePtr, const UInt8*, aData);
  size_t len = 0;
  if (IsRecording()) {
    len = OriginalCall(CFDataGetLength, size_t, aData);
  }
  events.RecordOrReplayValue(&len);
  if (IsReplaying()) {
    rval = NewLeakyArray<UInt8>(len);
  }
  events.RecordOrReplayBytes((void*) rval, len);
  return rval;
}

RRFunction1(CFDataGetLength)
RRFunction4(CFDateFormatterCreate)
RRFunction1(CFDateFormatterGetFormat)
RRFunctionVoid3(CFDictionaryAddValue)
RRFunction6(CFDictionaryCreate)
RRFunction4(CFDictionaryCreateMutable)
RRFunction3(CFDictionaryCreateMutableCopy)
RRFunction2(CFDictionaryGetValue)

static Boolean
RR_CFDictionaryGetValueIfPresent(CFDictionaryRef aDict, const void *aKey, const void **aValue)
{
  RecordReplayFunction(CFDictionaryGetValueIfPresent, Boolean, aDict, aKey, aValue);
  events.RecordOrReplayValue(aValue);
  events.RecordOrReplayValue(&rval);
  return rval;
}

RRFunctionVoid3(CFDictionaryReplaceValue)
RRFunction2(CFEqual)
RRFunction1(CFGetTypeID)
RRFunction0(CFLocaleCopyCurrent)
RRFunction0(CFLocaleCopyPreferredLanguages)
RRFunction2(CFLocaleCreate)
RRFunction1(CFLocaleGetIdentifier)

static void DummyCFNotificationCallback(CFNotificationCenterRef aCenter, void* aObserver,
                                        CFStringRef aName, const void* aObject,
                                        CFDictionaryRef aUserInfo)
{
  // FIXME
  //MOZ_CRASH();
}

static void
RR_CFNotificationCenterAddObserver(CFNotificationCenterRef aCenter, const void* aObserver,
                                   CFNotificationCallback aCallback, CFStringRef aName,
                                   const void* aObject,
                                   CFNotificationSuspensionBehavior aSuspensionBehavior)
{
  if (!AreThreadEventsPassedThrough()) {
    aCallback = DummyCFNotificationCallback;
  }

  RecordReplayFunctionVoid(CFNotificationCenterAddObserver,
                           aCenter, aObserver, aCallback, aName, aObject, aSuspensionBehavior);
}

RRFunction0(CFNotificationCenterGetLocalCenter)
RRFunctionVoid4(CFNotificationCenterRemoveObserver)
RRFunction3(CFNumberCreate)

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

static Boolean
RR_CFNumberGetValue(CFNumberRef aNumber, CFNumberType aType, void* aValuePtr)
{
  RecordReplayFunction(CFNumberGetValue, Boolean, aNumber, aType, aValuePtr);
  events.CheckInput(aType);
  events.RecordOrReplayValue(&rval);
  events.RecordOrReplayBytes(aValuePtr, CFNumberTypeBytes(aType));
  return rval;
}

RRFunction1(CFNumberIsFloatType)
RRFunction2(CFPreferencesAppValueIsForced)
RRFunction2(CFPreferencesCopyAppValue)
RRFunction4(CFPreferencesCopyValue)
RRFunction4(CFPropertyListCreateFromXMLData)
RRFunction6(CFPropertyListCreateWithStream)
RRFunctionVoid1(CFReadStreamClose)
RRFunction2(CFReadStreamCreateWithFile)
RRFunction1(CFReadStreamOpen)
RRFunction1(CFRelease)
RRFunction1(CFRetain)
RRFunctionVoid3(CFRunLoopAddSource)
RRFunction0(CFRunLoopGetCurrent)
RRFunctionVoid3(CFRunLoopRemoveSource)

static CFRunLoopSourceRef
RR_CFRunLoopSourceCreate(CFAllocatorRef aAllocator, CFIndex aOrder, CFRunLoopSourceContext* aCx)
{
  if (AreThreadEventsPassedThrough()) {
    return OriginalCall(CFRunLoopSourceCreate, CFRunLoopSourceRef, aAllocator, aOrder, aCx);
  }

  RegisterCallbackData(BitwiseCast<void*>(aCx->perform));
  RegisterCallbackData(aCx->info);

  if (IsRecording()) {
    CallbackWrapperData* wrapperData = new CallbackWrapperData(aCx->perform, aCx->info);
    aCx->perform = CFRunLoopPerformCallBackWrapper;
    aCx->info = wrapperData;
  }

  RecordReplayFunction(CFRunLoopSourceCreate, CFRunLoopSourceRef, aAllocator, aOrder, aCx);
  events.RecordOrReplayValue((size_t*)&rval);
  return rval;
}

RRFunctionVoid1(CFRunLoopSourceSignal)
RRFunctionVoid1(CFRunLoopWakeUp)
RRFunctionVoid3(CFStringAppendCharacters)
RRFunction3(CFStringCompare)
RRFunction3(CFStringCreateArrayBySeparatingStrings)
RRFunction2(CFStringCreateMutable)
RRFunction5(CFStringCreateWithBytes)
RRFunction6(CFStringCreateWithBytesNoCopy)
RRFunction4(CFStringCreateWithCharactersNoCopy)
RRFunction3(CFStringCreateWithCString)
RRFunction10(CFStringCreateWithFormat) // Hope this is enough arguments...

static CFIndex
RR_CFStringGetBytes(CFStringRef aString, CFRange aRange, CFStringEncoding aEncoding,
                    UInt8 aLossByte, Boolean aIsExternalRepresentation,
                    UInt8* aBuffer, CFIndex aMaxBufLen, CFIndex* aUsedBufLen)
{
  RecordReplayFunction(CFStringGetBytes, CFIndex,
                       aString, aRange, aEncoding, aLossByte, aIsExternalRepresentation,
                       aBuffer, aMaxBufLen, aUsedBufLen);
  events.CheckInput(aMaxBufLen);
  events.CheckInput((aUsedBufLen ? 1 : 0) | (aBuffer ? 2 : 0));
  events.RecordOrReplayValue(&rval);
  if (aUsedBufLen) {
    events.RecordOrReplayValue(aUsedBufLen);
  }
  if (aBuffer) {
    events.RecordOrReplayBytes(aBuffer, aUsedBufLen ? *aUsedBufLen : aMaxBufLen);
  }
  return rval;
}

static void
RR_CFStringGetCharacters(CFStringRef aString, CFRange aRange, UniChar* aBuffer)
{
  RecordReplayFunctionVoid(CFStringGetCharacters, aString, aRange, aBuffer);
  events.CheckInput(aRange.length);
  events.RecordOrReplayBytes(aBuffer, aRange.length * sizeof(UniChar));
}

RRFunction1(CFStringGetIntValue)

static Boolean
RR_CFStringGetCString(CFStringRef aString, char* aBuffer, CFIndex aBufferSize,
                      CFStringEncoding aEncoding)
{
  RecordReplayFunction(CFStringGetCString, Boolean,
                       aString, aBuffer, aBufferSize, aEncoding);
  events.CheckInput(aBufferSize);
  events.RecordOrReplayValue(&rval);
  events.RecordOrReplayBytes(aBuffer, aBufferSize);
  return rval;
}

static const char*
RR_CFStringGetCStringPtr(CFStringRef aString, CFStringEncoding aEncoding)
{
  if (AreThreadEventsPassedThrough()) {
    return OriginalCall(CFStringGetCStringPtr, const char*, aString, aEncoding);
  }
  return nullptr;
}

RRFunction1(CFStringGetLength)
RRFunction2(CFStringGetMaximumSizeForEncoding)
RRFunction2(CFStringHasPrefix)
RRFunction1(CFStringTokenizerAdvanceToNextToken)
RRFunction5(CFStringTokenizerCreate)
RRFunctionTypes1(CFStringTokenizerGetCurrentTokenRange, CFRange, CFStringTokenizerRef)
RRFunction4(CFURLCreateFromFileSystemRepresentation)
RRFunction2(CFURLCreateFromFSRef)
RRFunction3(CFURLCreateWithString)
RRFunction4(CFURLCreateWithFileSystemPath)

static Boolean
RR_CFURLGetFileSystemRepresentation(CFURLRef aUrl, Boolean aResolveAgainstBase,
                                    UInt8* aBuffer, CFIndex aMaxBufLen)
{
  RecordReplayFunction(CFURLGetFileSystemRepresentation, Boolean,
                       aUrl, aResolveAgainstBase, aBuffer, aMaxBufLen);
  events.CheckInput(aMaxBufLen);
  events.RecordOrReplayValue(&rval);
  events.RecordOrReplayBytes(aBuffer, aMaxBufLen);
  return rval;
}

static Boolean
RR_CFURLGetFSRef(CFURLRef aUrl, struct FSRef* aFsRef)
{
  RecordReplayFunction(CFURLGetFSRef, Boolean, aUrl, aFsRef);
  events.RecordOrReplayValue(&rval);
  events.RecordOrReplayBytes(aFsRef, sizeof(FSRef));
  return rval;
}

RRFunction1(CFUUIDCreate)
RRFunction2(CFUUIDCreateString)
RRFunctionTypes1(CFUUIDGetUUIDBytes, CFUUIDBytes, CFUUIDRef)
RRFunctionTypes2(CGAffineTransformConcat, CGAffineTransform, CGAffineTransform, CGAffineTransform)
RRFunction1(CGBitmapContextCreateImage)

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

static CGContextRef
RR_CGBitmapContextCreateWithData(void *aData, size_t aWidth, size_t aHeight,
                                 size_t aBitsPerComponent, size_t aBytesPerRow,
                                 CGColorSpaceRef aSpace, uint32_t aBitmapInfo,
                                 CGBitmapContextReleaseDataCallback aReleaseCallback,
                                 void *aReleaseInfo)
{
  RecordReplayFunction(CGBitmapContextCreateWithData, CGContextRef,
                       aData, aWidth, aHeight, aBitsPerComponent, aBytesPerRow, aSpace,
                       aBitmapInfo, aReleaseCallback, aReleaseInfo);
  events.RecordOrReplayValue(&rval);

  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  gContextData.emplaceBack(rval, aData, aHeight * aBytesPerRow);
  return rval;
}

static void
FlushContext(CGContextRef aContext)
{
  for (int i = gContextData.length() - 1; i >= 0; i--) {
    if (aContext == gContextData[i].mContext) {
      RecordReplayBytes(gContextData[i].mData, gContextData[i].mDataSize);
      return;
    }
  }
  MOZ_CRASH();
}

RRFunction1(CGBitmapContextGetBytesPerRow)
RRFunction1(CGBitmapContextGetHeight)
RRFunction1(CGBitmapContextGetWidth)
RRFunction1(CGColorRelease)
RRFunction0(CGColorSpaceCreateDeviceRGB)
RRFunction1(CGColorSpaceCreatePattern)
RRFunction1(CGColorSpaceCopyICCProfile)
RRFunction1(CGColorSpaceRelease)
RRFunctionTypesVoid3(CGContextBeginTransparencyLayerWithRect,
                     CGContextRef, CGRect, CFDictionaryRef)
RRFunction3(CGContextClipToRects)
RRFunctionTypesVoid2(CGContextConcatCTM, CGContextRef, CGAffineTransform)

static void
RR_CGContextDrawImage(CGContextRef aCx, CGRect aRect, CGImageRef aImage)
{
  RecordReplayFunctionVoid(CGContextDrawImage, aCx, aRect, aImage);
  FlushContext(aCx);
}

RRFunctionVoid1(CGContextEndTransparencyLayer)

static void
RR_CGContextFillRect(CGContextRef aCx, CGRect aRect)
{
  RecordReplayFunctionVoid(CGContextFillRect, aCx, aRect);
  FlushContext(aCx);
}

RRFunctionTypes1(CGContextGetClipBoundingBox, CGRect, CGContextRef)
RRFunctionTypes1(CGContextGetCTM, CGAffineTransform, CGContextRef)
RRFunction1(CGContextGetType)
RRFunctionTypes1(CGContextGetUserSpaceToDeviceSpaceTransform, CGAffineTransform, CGContextRef)

static void
RR_CGContextRestoreGState(CGContextRef aCx)
{
  if (!IsReplaying()) {
    Maybe<AutoPassThroughThreadEvents> pt;
    if (!AreThreadEventsPassedThrough()) {
      pt.emplace();
    }
    OriginalCall(CGContextRestoreGState, void, aCx);
  }
}

RRFunctionVoid1(CGContextSaveGState)
RRFunctionVoid2(CGContextSetAllowsFontSubpixelPositioning)
RRFunctionVoid2(CGContextSetAllowsFontSubpixelQuantization)
RRFunctionTypesVoid2(CGContextSetAlpha, CGContextRef, CGFloat)
RRFunctionTypesVoid2(CGContextSetBaseCTM, CGContextRef, CGAffineTransform)
RRFunctionTypesVoid2(CGContextSetCTM, CGContextRef, CGAffineTransform)
RRFunctionTypesVoid3(CGContextSetGrayFillColor, CGContextRef, CGFloat, CGFloat)
RRFunctionTypesVoid5(CGContextSetRGBFillColor, CGContextRef, CGFloat, CGFloat, CGFloat, CGFloat)
RRFunctionVoid2(CGContextSetShouldAntialias)
RRFunctionVoid2(CGContextSetShouldSmoothFonts)
RRFunctionVoid2(CGContextSetShouldSubpixelPositionFonts)
RRFunctionVoid2(CGContextSetShouldSubpixelQuantizeFonts)
RRFunctionVoid2(CGContextSetTextDrawingMode)
RRFunctionTypesVoid2(CGContextSetTextMatrix, CGContextRef, CGAffineTransform)
RRFunctionVoid3(CGContextScaleCTM)
RRFunctionVoid3(CGContextTranslateCTM)

static CGDataProviderRef
RR_CGDataProviderCreateWithData(void* aInfo, const void* aData,
                                size_t aSize, CGDataProviderReleaseDataCallback aReleaseData)
{
  RecordReplayFunction(CGDataProviderCreateWithData, CGDataProviderRef,
                       aInfo, aData, aSize, aReleaseData);
  events.RecordOrReplayValue((size_t*)&rval);
  if (IsReplaying()) {
    // Immediately release the data, since there is no data provider to do it for us.
    aReleaseData(aInfo, aData, aSize);
  }
  return rval;
}

RRFunction1(CGDisplayCopyColorSpace)
RRFunction1(CGDisplayIOServicePort)
RRFunction2(CGEventSourceCounterForEventType)
RRFunction2(CGFontCopyTableForTag)
RRFunction1(CGFontCopyTableTags)
RRFunction1(CGFontCopyVariations)
RRFunction2(CGFontCreateCopyWithVariations)
RRFunction1(CGFontCreateWithDataProvider)
RRFunction1(CGFontCreateWithFontName)
RRFunction1(CGFontCreateWithPlatformFont)
RRFunction1(CGFontGetAscent)
RRFunction1(CGFontGetCapHeight)
RRFunction1(CGFontGetDescent)
RRFunctionTypes1(CGFontGetFontBBox, CGRect, CGFontRef)

static bool
RR_CGFontGetGlyphAdvances(CGFontRef aFont, const CGGlyph* aGlyphs, size_t aCount, int* aAdvances)
{
  RecordReplayFunction(CGFontGetGlyphAdvances, bool, aFont, aGlyphs, aCount, aAdvances);
  events.CheckInput(aCount);
  events.RecordOrReplayValue(&rval);
  events.RecordOrReplayBytes(aAdvances, aCount * sizeof(int));
  return rval;
}

static bool
RR_CGFontGetGlyphBBoxes(CGFontRef aFont, const CGGlyph* aGlyphs, size_t aCount, CGRect* aBoxes)
{
  RecordReplayFunction(CGFontGetGlyphBBoxes, bool, aFont, aGlyphs, aCount, aBoxes);
  events.CheckInput(aCount);
  events.RecordOrReplayValue(&rval);
  events.RecordOrReplayBytes(aBoxes, aCount * sizeof(CGRect));
  return rval;
}

RRFunction4(CGFontGetGlyphPath)
RRFunction1(CGFontGetLeading)
RRFunction1(CGFontGetUnitsPerEm)
RRFunction1(CGFontGetXHeight)
RRFunction1(CGImageGetHeight)
RRFunction1(CGImageGetWidth)
RRFunction1(CGImageRelease)
RRFunction0(CGMainDisplayID)
RRFunctionVoid3(CGPathAddPath)

static void
RR_CGPathApply(CGPathRef aPath, void* aData, CGPathApplierFunction aFunction)
{
  if (AreThreadEventsPassedThrough()) {
    OriginalCall(CGPathApply, void, aPath, aData, aFunction);
  } else {
    RegisterCallbackData(BitwiseCast<void*>(aFunction));
    RegisterCallbackData(aData);
    PassThroughThreadEventsAllowCallbacks([&]() {
        CallbackWrapperData wrapperData(aFunction, aData);
        OriginalCall(CGPathApply, void, aPath, &wrapperData, CGPathApplierFunctionWrapper);
      });
    RemoveCallbackData(aData);
  }
}

RRFunction4(CGPathContainsPoint)
RRFunction0(CGPathCreateMutable)
RRFunctionTypes1(CGPathGetBoundingBox, CGRect, CGPathRef)

static CGRect
RR_CTFontGetBoundingRectsForGlyphs(CTFontRef aFont, CTFontOrientation aOrientation,
                                   const CGGlyph* aGlyphs, CGRect* aRects, CFIndex aCount)
{
  RecordReplayFunction(CTFontGetBoundingRectsForGlyphs, CGRect, aFont, aOrientation,
                       aGlyphs, aRects, aCount);
  events.RecordOrReplayBytes(&rval, sizeof(CGRect));
  events.CheckInput(!!aRects);
  if (aRects) {
    events.CheckInput(aCount);
    events.RecordOrReplayBytes(aRects, aCount * sizeof(CGRect));
  }
  return rval;
}

RRFunctionTypes1(CGPathGetCurrentPoint, CGPoint, CGPathRef)
RRFunction1(CGPathIsEmpty)
RRFunction1(CGSSetDebugOptions)
RRFunction1(CTFontCopyFamilyName)
RRFunction1(CTFontCopyFontDescriptor)
RRFunction2(CTFontCopyGraphicsFont)
RRFunction3(CTFontCopyTable)
RRFunction1(CTFontCopyVariationAxes)
RRFunctionTypes3(CTFontCreateForString, CTFontRef,
                 CTFontRef, CFStringRef, CFRange)
RRFunction3(CTFontCreatePathForGlyph)
RRFunctionTypes3(CTFontCreateWithFontDescriptor, CTFontRef,
                 CTFontDescriptorRef, CGFloat, CGAffineTransform*)
RRFunctionTypes4(CTFontCreateWithGraphicsFont, CTFontRef,
                 CGFontRef, CGFloat, CGAffineTransform*, CTFontDescriptorRef)
RRFunctionTypes3(CTFontCreateWithName, CTFontRef,
                 CFStringRef, CGFloat, CGAffineTransform*)

static void
RR_CTFontDrawGlyphs(CTFontRef aFont, CGGlyph* aGlyphs, CGPoint* aPositions,
                    size_t aCount, CGContextRef aContext)
{
  RecordReplayFunctionVoid(CTFontDrawGlyphs, aFont, aGlyphs, aPositions, aCount, aContext);
  FlushContext(aContext);
}

RRFunction2(CTFontDescriptorCopyAttribute)
RRFunction2(CTFontDescriptorCreateCopyWithAttributes)
RRFunction2(CTFontDescriptorCreateMatchingFontDescriptors)
RRFunction1(CTFontDescriptorCreateWithAttributes)

static double
RR_CTFontGetAdvancesForGlyphs(CTFontRef aFont, CTFontOrientation aOrientation,
                              const CGGlyph* aGlyphs, CGSize* aAdvances, CFIndex aCount)
{
  RecordReplayFunction(CTFontGetAdvancesForGlyphs, double, aFont, aOrientation,
                       aGlyphs, aAdvances, aCount);
  events.RecordOrReplayBytes(&rval, sizeof(double));
  events.CheckInput(!!aAdvances);
  if (aAdvances) {
    events.CheckInput(aCount);
    events.RecordOrReplayBytes(aAdvances, aCount * sizeof(CGSize));
  }
  return rval;
}

RRFunctionTypes1(CTFontGetAscent, CGFloat, CTFontRef)
RRFunctionTypes1(CTFontGetBoundingBox, CGRect, CTFontRef)
RRFunctionTypes1(CTFontGetCapHeight, CGFloat, CTFontRef)
RRFunctionTypes1(CTFontGetDescent, CGFloat, CTFontRef)
RRFunction1(CTFontGetGlyphCount)

static bool
RR_CTFontGetGlyphsForCharacters(CTFontRef aFont, const UniChar* aCharacters,
                                CGGlyph* aGlyphs, CFIndex aCount)
{
  RecordReplayFunction(CTFontGetGlyphsForCharacters, bool, aFont, aCharacters, aGlyphs, aCount);
  events.RecordOrReplayValue(&rval);
  events.CheckInput(aCount);
  events.RecordOrReplayBytes(aGlyphs, aCount * sizeof(CGGlyph));
  return rval;
}

RRFunctionTypes1(CTFontGetLeading, CGFloat, CTFontRef)
RRFunctionTypes1(CTFontGetSize, CGFloat, CTFontRef)
RRFunction1(CTFontGetSymbolicTraits)
RRFunctionTypes1(CTFontGetUnderlinePosition, CGFloat, CTFontRef)
RRFunctionTypes1(CTFontGetUnderlineThickness, CGFloat, CTFontRef)
RRFunction1(CTFontGetUnitsPerEm)
RRFunctionTypes1(CTFontGetXHeight, CGFloat, CTFontRef)
RRFunction0(CTFontManagerCopyAvailableFontFamilyNames)
RRFunction3(CTFontManagerRegisterFontsForURLs)
RRFunctionVoid2(CTFontManagerSetAutoActivationSetting)
RRFunction1(CTLineCreateWithAttributedString)
RRFunction1(CTLineGetGlyphRuns)
RRFunction1(CTRunGetAttributes)
RRFunction1(CTRunGetGlyphCount)

// Note: We only redirect CTRunGetGlyphsPtr, not CTRunGetGlyphs. The latter may
// be implemented with a loop that jumps back into the code we overwrite with a
// jump, a pattern which ProcessRedirect.cpp does not handle. For the moment,
// Gecko code only calls CTRunGetGlyphs if CTRunGetGlyphsPtr fails, so make
// sure that CTRunGetGlyphsPtr always succeeds when it is being recorded.
static const CGGlyph*
RR_CTRunGetGlyphsPtr(CTRunRef aRun)
{
  RecordReplayFunction(CTRunGetGlyphsPtr, CGGlyph*, aRun);
  size_t numGlyphs;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    numGlyphs = CTRunGetGlyphCount(aRun);
    if (!rval) {
      rval = NewLeakyArray<CGGlyph>(numGlyphs);
      CTRunGetGlyphs(aRun, CFRangeMake(0, 0), rval);
    }
  }
  events.RecordOrReplayValue(&numGlyphs);
  if (IsReplaying()) {
    rval = NewLeakyArray<CGGlyph>(numGlyphs);
  }
  events.RecordOrReplayBytes(rval, numGlyphs * sizeof(CGGlyph));
  return rval;
}

// Note: The same concerns apply here as for CTRunGetGlyphsPtr.
static const CGPoint*
RR_CTRunGetPositionsPtr(CTRunRef aRun)
{
  RecordReplayFunction(CTRunGetPositionsPtr, CGPoint*, aRun);
  size_t numGlyphs;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    numGlyphs = CTRunGetGlyphCount(aRun);
    if (!rval) {
      rval = NewLeakyArray<CGPoint>(numGlyphs);
      CTRunGetPositions(aRun, CFRangeMake(0, 0), rval);
    }
  }
  events.RecordOrReplayValue(&numGlyphs);
  if (IsReplaying()) {
    rval = NewLeakyArray<CGPoint>(numGlyphs);
  }
  events.RecordOrReplayBytes(rval, numGlyphs * sizeof(CGPoint));
  return rval;
}

// Note: The same concerns apply here as for CTRunGetGlyphsPtr.
static const CFIndex*
RR_CTRunGetStringIndicesPtr(CTRunRef aRun)
{
  RecordReplayFunction(CTRunGetStringIndicesPtr, CFIndex*, aRun);
  size_t numGlyphs;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    numGlyphs = CTRunGetGlyphCount(aRun);
    if (!rval) {
      rval = NewLeakyArray<CFIndex>(numGlyphs);
      CTRunGetStringIndices(aRun, CFRangeMake(0, 0), rval);
    }
  }
  events.RecordOrReplayValue(&numGlyphs);
  if (IsReplaying()) {
    rval = NewLeakyArray<CFIndex>(numGlyphs);
  }
  events.RecordOrReplayBytes(rval, numGlyphs * sizeof(CFIndex));
  return rval;
}

RRFunctionTypes1(CTRunGetStringRange, CFRange, CTRunRef)

static double
RR_CTRunGetTypographicBounds(CTRunRef aRun, CFRange aRange,
                             CGFloat* aAscent, CGFloat* aDescent, CGFloat* aLeading)
{
  RecordReplayFunction(CTRunGetTypographicBounds, double,
                       aRun, aRange, aAscent, aDescent, aLeading);
  events.CheckInput((aAscent ? 1 : 0) | (aDescent ? 2 : 0) | (aLeading ? 4 : 0));
  events.RecordOrReplayBytes(&rval, sizeof(double));
  if (aAscent) {
    events.RecordOrReplayBytes(aAscent, sizeof(CGFloat));
  }
  if (aDescent) {
    events.RecordOrReplayBytes(aDescent, sizeof(CGFloat));
  }
  if (aLeading) {
    events.RecordOrReplayBytes(aLeading, sizeof(CGFloat));
  }
  return rval;
}

RRFunctionTypesVoid5(CUIDraw, void*, CGRect, CGContextRef, CFDictionaryRef, CFDictionaryRef*)
RRFunction2(FSCompareFSRefs)

static OSErr
RR_FSGetVolumeInfo(size_t a0, size_t a1, size_t a2, size_t a3, size_t a4,
                   HFSUniStr255* aVolumeName, FSRef* aFsRef)
{
  RecordReplayFunction(FSGetVolumeInfo, OSErr, a0, a1, a2, a3, a4, aVolumeName, aFsRef);
  events.RecordOrReplayValue(&rval);
  events.RecordOrReplayBytes(aVolumeName, sizeof(HFSUniStr255));
  events.RecordOrReplayBytes(aFsRef, sizeof(FSRef));
  return rval;
}

static OSErr
RR_FSFindFolder(size_t a0, size_t a1, size_t a2, FSRef* aFsRef)
{
  RecordReplayFunction(FSFindFolder, OSErr, a0, a1, a2, aFsRef);
  events.RecordOrReplayValue(&rval);
  events.RecordOrReplayBytes(aFsRef, sizeof(FSRef));
  return rval;
}

static OSErr
RR_Gestalt(OSType aSelector, SInt32* aResponse)
{
  RecordReplayFunction(Gestalt, OSErr, aSelector, aResponse);
  events.RecordOrReplayValue(&rval);
  events.RecordOrReplayValue(aResponse);
  return rval;
}

RRFunction1(GetEventClass)
RRFunction0(GetCurrentEventQueue)

static OSErr
RR_GetCurrentProcess(ProcessSerialNumber* aPSN)
{
  RecordReplayFunction(GetCurrentProcess, OSErr, aPSN);
  events.RecordOrReplayValue(&rval);
  events.RecordOrReplayBytes(aPSN, sizeof(*aPSN));
  return rval;
}

RRFunction1(GetEventAttributes)
RRFunction0(GetEventDispatcherTarget)
RRFunction1(GetEventKind)
RRFunction5(HIThemeDrawButton)
RRFunction4(HIThemeDrawFrame)
RRFunction4(HIThemeDrawGroupBox)
RRFunction4(HIThemeDrawGrowBox)
RRFunction4(HIThemeDrawMenuBackground)
RRFunction6(HIThemeDrawMenuItem)
RRFunction5(HIThemeDrawMenuSeparator)
RRFunction4(HIThemeDrawSeparator)
RRFunction4(HIThemeDrawTabPane)
RRFunction4(HIThemeDrawTrack)

static OSStatus
RR_HIThemeGetGrowBoxBounds(const HIPoint* aOrigin, const HIThemeGrowBoxDrawInfo* aDrawInfo,
                           HIRect* aBounds)
{
  RecordReplayFunction(HIThemeGetGrowBoxBounds, OSStatus, aOrigin, aDrawInfo, aBounds);
  events.RecordOrReplayValue(&rval);
  events.RecordOrReplayBytes(aBounds, sizeof(*aBounds));
  return rval;
}

RRFunction4(HIThemeSetFill)
RRFunction5(IORegistryEntrySearchCFProperty)
RRFunction1(LSCopyAllHandlersForURLScheme)

static OSStatus
RR_LSCopyApplicationForMIMEType(CFStringRef aInMIMEType, LSRolesMask aInRoleMask,
                                CFURLRef* aOutAppURL)
{
  RecordReplayFunction(LSCopyApplicationForMIMEType, OSStatus,
                       aInMIMEType, aInRoleMask, aOutAppURL);
  events.CheckInput(!!aOutAppURL);
  events.RecordOrReplayValue(&rval);
  if (aOutAppURL) {
    events.RecordOrReplayValue((size_t*)aOutAppURL);
  }
  return rval;
}

static OSStatus
RR_LSCopyItemAttribute(const FSRef* aInItem, LSRolesMask aInRoles,
                       CFStringRef aInAttributeName, CFTypeRef* aOutValue)
{
  RecordReplayFunction(LSCopyItemAttribute, OSStatus,
                       aInItem, aInRoles, aInAttributeName, aOutValue);
  events.CheckInput(!!aOutValue);
  events.RecordOrReplayValue(&rval);
  if (aOutValue) {
    events.RecordOrReplayValue((size_t*)aOutValue);
  }
  return rval;
}

static OSStatus
RR_LSCopyKindStringForMIMEType(CFStringRef aInMIMEType, CFStringRef* aOutKindString)
{
  RecordReplayFunction(LSCopyKindStringForMIMEType, OSStatus, aInMIMEType, aOutKindString);
  events.CheckInput(!!aOutKindString);
  events.RecordOrReplayValue(&rval);
  if (aOutKindString) {
    events.RecordOrReplayValue((size_t*)aOutKindString);
  }
  return rval;
}

static void
RecordReplayFSRefAndURLRef(Stream& aStream, OSStatus* aOutStatus,
                           FSRef* aOutAppRef, CFURLRef* aOutAppURL)
{
  aStream.CheckInput((aOutAppRef ? 1 : 0) | (aOutAppURL ? 2 : 0));
  aStream.RecordOrReplayValue(aOutStatus);
  if (aOutAppRef) {
    aStream.RecordOrReplayBytes(aOutAppRef, sizeof(FSRef));
  }
  if (aOutAppURL) {
    aStream.RecordOrReplayValue((size_t*)aOutAppURL);
  }
}

static OSStatus
RR_LSGetApplicationForInfo(OSType aInType, OSType aInCreator,
                           CFStringRef aInExtension, LSRolesMask aInRoleMask,
                           FSRef* aOutAppRef, CFURLRef* aOutAppURL)
{
  RecordReplayFunction(LSGetApplicationForInfo, OSStatus,
                       aInType, aInCreator, aInExtension, aInRoleMask, aOutAppRef, aOutAppURL);
  RecordReplayFSRefAndURLRef(events, &rval, aOutAppRef, aOutAppURL);
  return rval;
}

static OSStatus
RR_LSGetApplicationForURL(CFURLRef aInURL, LSRolesMask aInRoleMask,
                          FSRef* aOutAppRef, CFURLRef* aOutAppURL)
{
  RecordReplayFunction(LSGetApplicationForURL, OSStatus,
                       aInURL, aInRoleMask, aOutAppRef, aOutAppURL);
  RecordReplayFSRefAndURLRef(events, &rval, aOutAppRef, aOutAppURL);
  return rval;
}

RRFunction1(NSClassFromString)
RRFunctionTypesVoid1(NSRectFill, CGRect /* Actually NSRect, but they appear to always be the same */)
RRFunction3(NSSearchPathForDirectoriesInDomains)
RRFunction1(NSSetFocusRingStyle)
RRFunction0(NSTemporaryDirectory)
RRFunction1(ReleaseEvent)

static void
RR_OSSpinLockLock(OSSpinLock *lock)
{
  // These spin locks never need to be recorded, but they are used by malloc
  // and can end up calling other recorded functions like mach_absolute_time,
  // so make sure events are passed through here. Note that we don't have to
  // redirect OSSpinLockUnlock, as it doesn't have these issues.
  AutoEnsurePassThroughThreadEventsUseStackPointer pt;
  OriginalCall(OSSpinLockLock, void, lock);
}

RRFunction2(RemoveEventFromQueue)
RRFunction1(RetainEvent)
RRFunction2(SendEventToEventTarget)

// These are not public APIs, but other redirected functions may be aliases for
// these which are dynamically installed on the first call in a way that our
// redirection mechanism doesn't completely account for.
RRFunction2(SLEventSourceCounterForEventType)
RRFunction1(SLDisplayCopyColorSpace)
RRFunction1(SLDisplayIOServicePort)
RRFunction0(SLMainDisplayID)

///////////////////////////////////////////////////////////////////////////////
// Redirection generation
///////////////////////////////////////////////////////////////////////////////

#define MAKE_REDIRECTION_ENTRY(aName)    \
  { #aName, nullptr, (uint8_t*) RR_ ##aName },

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

// From chromium/src/base/eintr_wrapper.h
#define HANDLE_EINTR(x) ({ \
  typeof(x) __eintr_result__; \
  do { \
    __eintr_result__ = x; \
  } while (__eintr_result__ == -1 && errno == EINTR); \
  __eintr_result__;\
})

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
