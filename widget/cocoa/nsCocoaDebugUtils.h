/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCocoaDebugUtils_h_
#define nsCocoaDebugUtils_h_

#include <CoreServices/CoreServices.h>

// Definitions and declarations of stuff used by us from the CoreSymbolication
// framework.  This is an undocumented, private framework available on OS X
// 10.6 and up.  It's used by Apple utilities like dtrace, atos, ReportCrash
// and crashreporterd.

typedef struct _CSTypeRef {
  unsigned long type;
  void* contents;
} CSTypeRef;

typedef CSTypeRef CSSymbolicatorRef;
typedef CSTypeRef CSSymbolOwnerRef;
typedef CSTypeRef CSSymbolRef;
typedef CSTypeRef CSSourceInfoRef;

typedef struct _CSRange {
  unsigned long long location;
  unsigned long long length;
} CSRange;

typedef unsigned long long CSArchitecture;

#define kCSNow LONG_MAX

extern "C" {

CSSymbolicatorRef
CSSymbolicatorCreateWithPid(pid_t pid);

CSSymbolicatorRef
CSSymbolicatorCreateWithPidFlagsAndNotification(pid_t pid,
                                                uint32_t flags,
                                                uint32_t notification);

CSArchitecture
CSSymbolicatorGetArchitecture(CSSymbolicatorRef symbolicator);

CSSymbolOwnerRef
CSSymbolicatorGetSymbolOwnerWithAddressAtTime(CSSymbolicatorRef symbolicator,
                                              unsigned long long address,
                                              long time);

const char*
CSSymbolOwnerGetName(CSSymbolOwnerRef owner);

unsigned long long
CSSymbolOwnerGetBaseAddress(CSSymbolOwnerRef owner);

CSSymbolRef
CSSymbolOwnerGetSymbolWithAddress(CSSymbolOwnerRef owner,
                                  unsigned long long address);

CSSourceInfoRef
CSSymbolOwnerGetSourceInfoWithAddress(CSSymbolOwnerRef owner,
                                      unsigned long long address);

const char*
CSSymbolGetName(CSSymbolRef symbol);

CSRange
CSSymbolGetRange(CSSymbolRef symbol);

const char*
CSSourceInfoGetFilename(CSSourceInfoRef info);

uint32_t
CSSourceInfoGetLineNumber(CSSourceInfoRef info);

CSTypeRef
CSRetain(CSTypeRef);

void
CSRelease(CSTypeRef);

bool
CSIsNull(CSTypeRef);

void
CSShow(CSTypeRef);

const char*
CSArchitectureGetFamilyName(CSArchitecture);

} // extern "C"

class nsCocoaDebugUtils
{
public:
  // Like NSLog() but records more information (for example the full path to
  // the executable and the "thread name").  Like NSLog(), writes to both
  // stdout and the system log.
  static void DebugLog(const char* aFormat, ...);

  // Logs a stack trace of the current point of execution, to both stdout and
  // the system log.
  static void PrintStackTrace();

  // Returns the name of the module that "owns" aAddress.  This must be
  // free()ed by the caller.
  static char* GetOwnerName(void* aAddress);

  // Returns a symbolicated representation of aAddress.  This must be
  // free()ed by the caller.
  static char* GetAddressString(void* aAddress);

private:
  static void DebugLogInt(bool aDecorate, const char* aFormat, ...);
  static void DebugLogV(bool aDecorate, CFStringRef aFormat, va_list aArgs);

  static void PrintAddress(void* aAddress);

  // The values returned by GetOwnerNameInt() and GetAddressStringInt() must
  // be free()ed by the caller.
  static char* GetOwnerNameInt(void* aAddress,
                               CSTypeRef aOwner = sInitializer);
  static char* GetAddressStringInt(void* aAddress,
                                   CSTypeRef aOwner = sInitializer);

  static CSSymbolicatorRef GetSymbolicatorRef();
  static void ReleaseSymbolicator();

  static CSTypeRef sInitializer;
  static CSSymbolicatorRef sSymbolicator;
};

#endif // nsCocoaDebugUtils_h_
