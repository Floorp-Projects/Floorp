/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* API for getting a stack trace of the C/C++ stack on the current thread */

#ifndef nsStackWalk_h_
#define nsStackWalk_h_

/* WARNING: This file is intended to be included from C or C++ files. */

#include "nscore.h"
#include <mozilla/StandardInteger.h>

#ifdef __cplusplus
extern "C" {
#endif

// aSP will be the best approximation possible of what the stack pointer will be
// pointing to when the execution returns to executing that at that PC.
// If no approximation can be made it will be NULL.
typedef void
(* NS_WalkStackCallback)(void *aPC, void *aSP, void *aClosure);

/**
 * Call aCallback for the C/C++ stack frames on the current thread, from
 * the caller of NS_StackWalk to main (or above).
 *
 * @param aCallback    Callback function, called once per frame.
 * @param aSkipFrames  Number of initial frames to skip.  0 means that
 *                     the first callback will be for the caller of
 *                     NS_StackWalk.
 * @param aClosure     Caller-supplied data passed through to aCallback.
 * @param aThread      The thread for which the stack is to be retrieved.
 *                     Passing null causes us to walk the stack of the
 *                     current thread. On Windows, this is a thread HANDLE.
 *                     It is currently not supported on any other platform.
 * @param aPlatformData Platform specific data that can help in walking the
 *                      stack, this should be NULL unless you really know
 *                      what you're doing! This needs to be a pointer to a
 *                      CONTEXT on Windows and should not be passed on other
 *                      platforms.
 *
 * Returns NS_ERROR_NOT_IMPLEMENTED on platforms where it is
 * unimplemented.
 * Returns NS_ERROR_UNEXPECTED when the stack indicates that the thread
 * is in a very dangerous situation (e.g., holding sem_pool_lock in 
 * Mac OS X pthreads code). Callers should then bail out immediately.
 *
 * May skip some stack frames due to compiler optimizations or code
 * generation.
 */
XPCOM_API(nsresult)
NS_StackWalk(NS_WalkStackCallback aCallback, uint32_t aSkipFrames,
             void *aClosure, uintptr_t aThread, void *aPlatformData);

typedef struct {
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
} nsCodeAddressDetails;

/**
 * For a given pointer to code, fill in the pieces of information used
 * when printing a stack trace.
 *
 * @param aPC         The code address.
 * @param aDetails    A structure to be filled in with the result.
 */
XPCOM_API(nsresult)
NS_DescribeCodeAddress(void *aPC, nsCodeAddressDetails *aDetails);

/**
 * Format the information about a code address in a format suitable for
 * stack traces on the current platform.  When available, this string
 * should contain the function name, source file, and line number.  When
 * these are not available, library and offset should be reported, if
 * possible.
 *
 * @param aPC         The code address.
 * @param aDetails    The value filled in by NS_DescribeCodeAddress(aPC).
 * @param aBuffer     A string to be filled in with the description.
 *                    The string will always be null-terminated.
 * @param aBufferSize The size, in bytes, of aBuffer, including
 *                    room for the terminating null.  If the information
 *                    to be printed would be larger than aBuffer, it
 *                    will be truncated so that aBuffer[aBufferSize-1]
 *                    is the terminating null.
 */
XPCOM_API(nsresult)
NS_FormatCodeAddressDetails(void *aPC, const nsCodeAddressDetails *aDetails,
                            char *aBuffer, uint32_t aBufferSize);

#ifdef __cplusplus
}
#endif

#endif /* !defined(nsStackWalk_h_) */
