/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* API for getting a stack trace of the C/C++ stack on the current thread */

#ifndef mozilla_StackWalk_h
#define mozilla_StackWalk_h

/* WARNING: This file is intended to be included from C or C++ files. */

#include "mozilla/Types.h"
#include <stdint.h>

/**
 * The callback for MozStackWalk.
 *
 * @param aFrameNumber  The frame number (starts at 1, not 0).
 * @param aPC           The program counter value.
 * @param aSP           The best approximation possible of what the stack
 *                      pointer will be pointing to when the execution returns
 *                      to executing that at aPC. If no approximation can
 *                      be made it will be nullptr.
 * @param aClosure      Extra data passed in via MozStackWalk().
 */
typedef void
(*MozWalkStackCallback)(uint32_t aFrameNumber, void* aPC, void* aSP,
                        void* aClosure);

/**
 * Call aCallback for the C/C++ stack frames on the current thread, from
 * the caller of MozStackWalk to main (or above).
 *
 * @param aCallback    Callback function, called once per frame.
 * @param aSkipFrames  Number of initial frames to skip.  0 means that
 *                     the first callback will be for the caller of
 *                     MozStackWalk.
 * @param aMaxFrames   Maximum number of frames to trace.  0 means no limit.
 * @param aClosure     Caller-supplied data passed through to aCallback.
 * @param aThread      The thread for which the stack is to be retrieved.
 *                     Passing null causes us to walk the stack of the
 *                     current thread. On Windows, this is a thread HANDLE.
 *                     It is currently not supported on any other platform.
 * @param aPlatformData Platform specific data that can help in walking the
 *                      stack, this should be nullptr unless you really know
 *                      what you're doing! This needs to be a pointer to a
 *                      CONTEXT on Windows and should not be passed on other
 *                      platforms.
 *
 * May skip some stack frames due to compiler optimizations or code
 * generation.
 *
 * Note: this (and other helper methods) will only be available when
 * MOZ_STACKWALKING is defined, so any new consumers must #if based on that.
 */
MFBT_API bool
MozStackWalk(MozWalkStackCallback aCallback, uint32_t aSkipFrames,
             uint32_t aMaxFrames, void* aClosure, uintptr_t aThread,
             void* aPlatformData);

typedef struct
{
  /*
   * The name of the shared library or executable containing an
   * address and the address's offset within that library, or empty
   * string and zero if unknown.
   */
  char library[256];
  ptrdiff_t loffset;
  /*
   * The name of the file name and line number of the code
   * corresponding to the address, or empty string and zero if
   * unknown.
   */
  char filename[256];
  unsigned long lineno;
  /*
   * The name of the function containing an address and the address's
   * offset within that function, or empty string and zero if unknown.
   */
  char function[256];
  ptrdiff_t foffset;
} MozCodeAddressDetails;

/**
 * For a given pointer to code, fill in the pieces of information used
 * when printing a stack trace.
 *
 * @param aPC         The code address.
 * @param aDetails    A structure to be filled in with the result.
 */
MFBT_API bool
MozDescribeCodeAddress(void* aPC, MozCodeAddressDetails* aDetails);

/**
 * Format the information about a code address in a format suitable for
 * stack traces on the current platform.  When available, this string
 * should contain the function name, source file, and line number.  When
 * these are not available, library and offset should be reported, if
 * possible.
 *
 * Note that this output is parsed by several scripts including the fix*.py and
 * make-tree.pl scripts in tools/rb/. It should only be change with care, and
 * in conjunction with those scripts.
 *
 * @param aBuffer      A string to be filled in with the description.
 *                     The string will always be null-terminated.
 * @param aBufferSize  The size, in bytes, of aBuffer, including
 *                     room for the terminating null.  If the information
 *                     to be printed would be larger than aBuffer, it
 *                     will be truncated so that aBuffer[aBufferSize-1]
 *                     is the terminating null.
 * @param aFrameNumber The frame number.
 * @param aPC          The code address.
 * @param aFunction    The function name. Possibly null or the empty string.
 * @param aLibrary     The library name. Possibly null or the empty string.
 * @param aLOffset     The library offset.
 * @param aFileName    The filename. Possibly null or the empty string.
 * @param aLineNo      The line number. Possibly zero.
 */
MFBT_API void
MozFormatCodeAddress(char* aBuffer, uint32_t aBufferSize, uint32_t aFrameNumber,
                     const void* aPC, const char* aFunction,
                     const char* aLibrary, ptrdiff_t aLOffset,
                     const char* aFileName, uint32_t aLineNo);

/**
 * Format the information about a code address in the same fashion as
 * MozFormatCodeAddress.
 *
 * @param aBuffer      A string to be filled in with the description.
 *                     The string will always be null-terminated.
 * @param aBufferSize  The size, in bytes, of aBuffer, including
 *                     room for the terminating null.  If the information
 *                     to be printed would be larger than aBuffer, it
 *                     will be truncated so that aBuffer[aBufferSize-1]
 *                     is the terminating null.
 * @param aFrameNumber The frame number.
 * @param aPC          The code address.
 * @param aDetails     The value filled in by MozDescribeCodeAddress(aPC).
 */
MFBT_API void
MozFormatCodeAddressDetails(char* aBuffer, uint32_t aBufferSize,
                            uint32_t aFrameNumber, void* aPC,
                            const MozCodeAddressDetails* aDetails);

namespace mozilla {

MFBT_API bool
FramePointerStackWalk(MozWalkStackCallback aCallback, uint32_t aSkipFrames,
                      uint32_t aMaxFrames, void* aClosure, void** aBp,
                      void* aStackEnd);

} // namespace mozilla

/**
 * Initialize the critical sections for this platform so that we can
 * abort stack walks when needed.
 */
MFBT_API void
StackWalkInitCriticalAddress(void);

#endif
