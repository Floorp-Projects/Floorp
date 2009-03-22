/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is NS_WalkTheStack.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* API for getting a stack trace of the C/C++ stack on the current thread */

#ifndef nsStackWalk_h_
#define nsStackWalk_h_

/* WARNING: This file is intended to be included from C or C++ files. */

#include "nscore.h"

PR_BEGIN_EXTERN_C

typedef void
(* NS_WalkStackCallback)(void *aPC, void *aClosure);

/**
 * Call aCallback for the C/C++ stack frames on the current thread, from
 * the caller of NS_StackWalk to main (or above).
 *
 * @param aCallback    Callback function, called once per frame.
 * @param aSkipFrames  Number of initial frames to skip.  0 means that
 *                     the first callback will be for the caller of
 *                     NS_StackWalk.
 * @param aClosure     Caller-supplied data passed through to aCallback.
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
NS_StackWalk(NS_WalkStackCallback aCallback, PRUint32 aSkipFrames,
             void *aClosure);

typedef struct {
    /*
     * The name of the shared library or executable containing an
     * address and the address's offset within that library, or empty
     * string and zero if unknown.
     */
    char library[256];
    PRUptrdiff loffset;
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
    PRUptrdiff foffset;
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
                            char *aBuffer, PRUint32 aBufferSize);

PR_END_EXTERN_C

#endif /* !defined(nsStackWalk_h_) */
