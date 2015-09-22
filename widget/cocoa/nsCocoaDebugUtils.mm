/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCocoaDebugUtils.h"

#include <pthread.h>
#include <libproc.h>
#include <stdarg.h>
#include <time.h>
#include <execinfo.h>
#include <asl.h>

static char gProcPath[PROC_PIDPATHINFO_MAXSIZE] = {0};
static char gBundleID[MAXPATHLEN] = {0};

static void MaybeGetPathAndID()
{
  if (!gProcPath[0]) {
    proc_pidpath(getpid(), gProcPath, sizeof(gProcPath));
  }
  if (!gBundleID[0]) {
    // Apple's CFLog() uses "com.apple.console" (in its call to asl_open()) if
    // it can't find the bundle id.
    CFStringRef bundleID = NULL;
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle) {
      bundleID = CFBundleGetIdentifier(mainBundle);
    }
    if (!bundleID) {
      strcpy(gBundleID, "com.apple.console");
    } else {
      CFStringGetCString(bundleID, gBundleID, sizeof(gBundleID),
                         kCFStringEncodingUTF8);
    }
  }
}

static void GetThreadName(char* aName, size_t aSize)
{
  pthread_getname_np(pthread_self(), aName, aSize);
}

void
nsCocoaDebugUtils::DebugLog(const char* aFormat, ...)
{
  va_list args;
  va_start(args, aFormat);
  CFStringRef formatCFSTR =
    CFStringCreateWithCString(kCFAllocatorDefault, aFormat,
                              kCFStringEncodingUTF8);
  DebugLogV(true, formatCFSTR, args);
  CFRelease(formatCFSTR);
  va_end(args);
}

void
nsCocoaDebugUtils::DebugLogInt(bool aDecorate, const char* aFormat, ...)
{
  va_list args;
  va_start(args, aFormat);
  CFStringRef formatCFSTR =
    CFStringCreateWithCString(kCFAllocatorDefault, aFormat,
                              kCFStringEncodingUTF8);
  DebugLogV(aDecorate, formatCFSTR, args);
  CFRelease(formatCFSTR);
  va_end(args);
}

void
nsCocoaDebugUtils::DebugLogV(bool aDecorate, CFStringRef aFormat,
                             va_list aArgs)
{
  MaybeGetPathAndID();

  CFStringRef message =
    CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL,
                                         aFormat, aArgs);

  int msgLength =
    CFStringGetMaximumSizeForEncoding(CFStringGetLength(message),
                                      kCFStringEncodingUTF8);
  char* msgUTF8 = (char*) calloc(msgLength, 1);
  CFStringGetCString(message, msgUTF8, msgLength, kCFStringEncodingUTF8);
  CFRelease(message);

  int finishedLength = msgLength + PROC_PIDPATHINFO_MAXSIZE;
  char* finished = (char*) calloc(finishedLength, 1);
  const time_t currentTime = time(NULL);
  char timestamp[30] = {0};
  ctime_r(&currentTime, timestamp);
  if (aDecorate) {
    char threadName[MAXPATHLEN] = {0};
    GetThreadName(threadName, sizeof(threadName));
    snprintf(finished, finishedLength, "(%s) %s[%u] %s[%p] %s\n",
            timestamp, gProcPath, getpid(), threadName, pthread_self(), msgUTF8);
  } else {
    snprintf(finished, finishedLength, "%s\n", msgUTF8);
  }
  free(msgUTF8);

  fputs(finished, stdout);

  // Use the Apple System Log facility, as NSLog and CFLog do.
  aslclient asl = asl_open(NULL, gBundleID, ASL_OPT_NO_DELAY);
  aslmsg msg = asl_new(ASL_TYPE_MSG);
  asl_set(msg, ASL_KEY_LEVEL, "4"); // kCFLogLevelWarning, used by NSLog()
  asl_set(msg, ASL_KEY_MSG, finished);
  asl_send(asl, msg);
  asl_free(msg);
  asl_close(asl);

  free(finished);
}

CSTypeRef
nsCocoaDebugUtils::sInitializer = {0};

CSSymbolicatorRef
nsCocoaDebugUtils::sSymbolicator = {0};

#define STACK_MAX 256

void
nsCocoaDebugUtils::PrintStackTrace()
{
  void** addresses = (void**) calloc(STACK_MAX, sizeof(void*));
  if (!addresses) {
    return;
  }

  CSSymbolicatorRef symbolicator = GetSymbolicatorRef();
  if (CSIsNull(symbolicator)) {
    free(addresses);
    return;
  }

  uint32_t count = backtrace(addresses, STACK_MAX);
  for (uint32_t i = 0; i < count; ++i) {
    PrintAddress(addresses[i]);
  }

  ReleaseSymbolicator();
  free(addresses);
}

void
nsCocoaDebugUtils::PrintAddress(void* aAddress)
{
  const char* ownerName = "unknown";
  const char* addressString = "unknown + 0";

  char* allocatedOwnerName = nullptr;
  char* allocatedAddressString = nullptr;

  CSSymbolOwnerRef owner = {0};
  CSSymbolicatorRef symbolicator = GetSymbolicatorRef();

  if (!CSIsNull(symbolicator)) {
    owner =
      CSSymbolicatorGetSymbolOwnerWithAddressAtTime(symbolicator,
                                                    (unsigned long long) aAddress,
                                                    kCSNow);
  }
  if (!CSIsNull(owner)) {
    ownerName = allocatedOwnerName = GetOwnerNameInt(aAddress, owner);
    addressString = allocatedAddressString = GetAddressStringInt(aAddress, owner);
  }
  DebugLogInt(false, "    (%s) %s", ownerName, addressString);

  free(allocatedOwnerName);
  free(allocatedAddressString);

  ReleaseSymbolicator();
}

char*
nsCocoaDebugUtils::GetOwnerName(void* aAddress)
{
  return GetOwnerNameInt(aAddress);
}

char*
nsCocoaDebugUtils::GetOwnerNameInt(void* aAddress, CSTypeRef aOwner)
{
  char* retval = (char*) calloc(MAXPATHLEN, 1);

  const char* ownerName = "unknown";

  CSSymbolicatorRef symbolicator = GetSymbolicatorRef();
  CSTypeRef owner = aOwner;

  if (CSIsNull(owner) && !CSIsNull(symbolicator)) {
    owner =
      CSSymbolicatorGetSymbolOwnerWithAddressAtTime(symbolicator,
                                                    (unsigned long long) aAddress,
                                                    kCSNow);
  }

  if (!CSIsNull(owner)) {
    ownerName = CSSymbolOwnerGetName(owner);
  }

  snprintf(retval, MAXPATHLEN, "%s", ownerName);
  ReleaseSymbolicator();

  return retval;
}

char*
nsCocoaDebugUtils::GetAddressString(void* aAddress)
{
  return GetAddressStringInt(aAddress);
}

char*
nsCocoaDebugUtils::GetAddressStringInt(void* aAddress, CSTypeRef aOwner)
{
  char* retval = (char*) calloc(MAXPATHLEN, 1);

  const char* addressName = "unknown";
  unsigned long long addressOffset = 0;

  CSSymbolicatorRef symbolicator = GetSymbolicatorRef();
  CSTypeRef owner = aOwner;

  if (CSIsNull(owner) && !CSIsNull(symbolicator)) {
    owner =
      CSSymbolicatorGetSymbolOwnerWithAddressAtTime(symbolicator,
                                                    (unsigned long long) aAddress,
                                                    kCSNow);
  }

  if (!CSIsNull(owner)) {
    CSSymbolRef symbol =
      CSSymbolOwnerGetSymbolWithAddress(owner,
                                        (unsigned long long) aAddress);
    if (!CSIsNull(symbol)) {
      addressName = CSSymbolGetName(symbol);
      CSRange range = CSSymbolGetRange(symbol);
      addressOffset = (unsigned long long) aAddress - range.location;
    } else {
      addressOffset = (unsigned long long)
        aAddress - CSSymbolOwnerGetBaseAddress(owner);
    }
  }

  snprintf(retval, MAXPATHLEN, "%s + 0x%llx",
           addressName, addressOffset);
  ReleaseSymbolicator();

  return retval;
}

CSSymbolicatorRef
nsCocoaDebugUtils::GetSymbolicatorRef()
{
  if (CSIsNull(sSymbolicator)) {
    // 0x40e0000 is the value returned by
    // uint32_t CSSymbolicatorGetFlagsForNListOnlyData(void).  We don't use
    // this method directly because it doesn't exist on OS X 10.6.  Unless
    // we limit ourselves to NList data, it will take too long to get a
    // stack trace where Dwarf debugging info is available (about 15 seconds
    // with Firefox).  This means we won't be able to get a CSSourceInfoRef,
    // or line number information.  Oh well.
    sSymbolicator =
      CSSymbolicatorCreateWithPidFlagsAndNotification(getpid(),
                                                      0x40e0000, 0);
  }
  // Retaining just after creation prevents crashes when calling symbolicator
  // code (for example from PrintStackTrace()) as Firefox is quitting.  Not
  // sure why.  Doing this may mean that we leak sSymbolicator on quitting
  // (if we ever created it).  No particular harm in that, though.
  return CSRetain(sSymbolicator);
}

void
nsCocoaDebugUtils::ReleaseSymbolicator()
{
  if (!CSIsNull(sSymbolicator)) {
    CSRelease(sSymbolicator);
  }
}
