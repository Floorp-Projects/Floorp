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
 * The Original Code is the PKIX-C library.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are
 * Copyright 2004-2007 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems, Inc.
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
/*
 * This file defines functions associated with the PKIX_RevocationChecker
 * type.
 *
 */

#ifndef _PKIX_REVCHECKER_H
#define _PKIX_REVCHECKER_H

#include "pkixt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* General
 *
 * Please refer to the libpkix Programmer's Guide for detailed information
 * about how to use the libpkix library. Certain key warnings and notices from
 * that document are repeated here for emphasis.
 *
 * All identifiers in this file (and all public identifiers defined in
 * libpkix) begin with "PKIX_". Private identifiers only intended for use
 * within the library begin with "pkix_".
 *
 * A function returns NULL upon success, and a PKIX_Error pointer upon failure.
 *
 * Unless otherwise noted, for all accessor (gettor) functions that return a
 * PKIX_PL_Object pointer, callers should assume that this pointer refers to a
 * shared object. Therefore, the caller should treat this shared object as
 * read-only and should not modify this shared object. When done using the
 * shared object, the caller should release the reference to the object by
 * using the PKIX_PL_Object_DecRef function.
 *
 * While a function is executing, if its arguments (or anything referred to by
 * its arguments) are modified, free'd, or destroyed, the function's behavior
 * is undefined.
 *
 */

/* PKIX_RevocationChecker
 *
 * PKIX_RevocationCheckers provide a standard way for the caller to insert
 * their own custom revocation checks to verify the revocation status of
 * certificates. This may be useful in many scenarios, including when the
 * caller wishes to use their own revocation checking mechanism instead of (or
 * in addition to) the default revocation checking mechanism provided by
 * libpkix, which uses CRLs. The RevCallback allows custom revocation checking
 * to take place. Additionally, the RevocationChecker can be initialized with
 * a revCheckerContext, which is where the caller can specify configuration
 * data such as the IP address of a revocation server. Note that this
 * revCheckerContext must be a PKIX_PL_Object, allowing it to be
 * reference-counted and allowing it to provide the standard PKIX_PL_Object
 * functions (Equals, Hashcode, ToString, Compare, Duplicate).
 *
 * Once the caller has created the RevocationChecker object(s), the caller
 * then specifies the RevocationChecker object(s) in a ProcessingParams object
 * and passes that object to PKIX_ValidateChain or PKIX_BuildChain, which uses
 * the objects to call the user's callback functions as needed during the
 * validation or building process.
 *
 * Note that if multiple revocation checkers are added, the order is
 * important, in that each revocation checker will be called sequentially
 * until the revocation status can be determined or all the revocation checkers
 * have been called. Also note that the default CRL revocation checker will
 * always be called last after all the custom revocation checkers have been
 * called. This default CRL revocation checking can be disabled by calling
 * PKIX_ProcessingParams_SetRevocationEnabled with a Boolean parameter of
 * PKIX_FALSE. This will ONLY disable the CRL revocation checker, not the
 * custom RevocationCheckers specified by the caller.
 *
 * For example, assume the caller specifies an OCSP RevocationChecker in the
 * ProcessingParams object. Let's look at two scenarios:
 *
 * 1) SetRevocationEnabled(PKIX_FALSE)
 *
 *      The OCSP RevocationChecker will be used. If it is unable to determine
 *      whether the certificate has been revoked (perhaps the network is down),
 *      the revocation check fails safe and the certificate is rejected
 *      (assumed to be revoked).
 *
 * 2) SetRevocationEnabled(PKIX_TRUE)
 *      [This doesn't need to be called, since this is the default behavior]
 *
 *      The OCSP RevocationChecker will be used first. If it is unable to
 *      determine whether the certificate has been revoked (perhaps the network
 *      is down), the default CRL revocation checker is used next. If it is
 *      also unable to determine whether the certificate has been revoked, the
 *      revocation check fails safe. Note that this is a useful scenario where
 *      the CRL check is only done if the OCSP check is unable to take place.
 */

/*
 * FUNCTION: PKIX_RevocationChecker_RevCallback
 * DESCRIPTION:
 *
 *  This callback function determines the revocation status of the specified
 *  Cert pointed to by "cert" and stores it at "pResultCode". If a checker
 *  initiates non-blocking I/O, it stores a platform-dependent non-blocking
 *  I/O context at "pNBIOContext". A subsequent call with that same value on
 *  input allows the operation to continue. On completion, with no non-blocking
 *  I/O pending, NULL is stored at "pNBIOContext".
 *
 * PARAMETERS:
 *  "revCheckerContext"
 *      Address of RevocationCheckerContext for the RevocationChecker whose
 *      RevCallback logic is to be used. Must be non-NULL.
 *  "cert"
 *      Address of Cert whose revocation status is to be determined.
 *      Must be non-NULL.
 *  "procParams"
 *      Address of ProcessingParams used to initialize the checker.
 *      Must be non-NULL.
 *  "pNBIOContext"
 *      Address at which platform-dependent non-blocking I/O context is stored.
 *      Must be non-NULL.
 *  "pResultCode"
 *      Address where revocation status will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe
 *
 *  Multiple threads must be able to safely call this function without
 *  worrying about conflicts, even if they're operating on the same objects.
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a RevocationChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
typedef PKIX_Error *
(*PKIX_RevocationChecker_RevCallback)(
        PKIX_PL_Object *revCheckerContext,
        PKIX_PL_Cert *cert,
        PKIX_ProcessingParams *procParams,
        void **pNBIOContext,
        PKIX_UInt32 *pResultCode,
        void *plContext);

/*
 * FUNCTION: PKIX_RevocationChecker_Create
 * DESCRIPTION:
 *
 *  Creates a new RevocationChecker using the Object pointed to by
 *  "revCheckerContext" (if any) and stores it at "pRevChecker". The new
 *  RevocationChecker uses the RevCallback pointed to by "callback". Once
 *  created, a RevocationChecker is immutable.
 * PARAMETERS:
 *  "callback"
 *      The RevCallback function to be used. Must be non-NULL.
 *  "revCheckerContext"
 *      Address of Object representing the RevocationChecker's context.
 *  "pRevChecker"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see THREAD SAFETY DEFINITIONS at top of file)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a RevocationChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
PKIX_RevocationChecker_Create(
        PKIX_RevocationChecker_RevCallback callback,
        PKIX_PL_Object *revCheckerContext,
        PKIX_RevocationChecker **pRevChecker,
        void *plContext);

/*
 * FUNCTION: PKIX_RevocationChecker_GetRevCallback
 * DESCRIPTION:
 *
 *  Retrieves a pointer to "revChecker's" Rev callback function and puts it in
 *  "pCallback".
 *
 * PARAMETERS:
 *  "revChecker"
 *      The RevocationChecker whose Rev callback is desired. Must be non-NULL.
 *  "pCallback"
 *      Address where Rev callback function pointer will be stored.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see THREAD SAFETY DEFINITIONS at top of file)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a RevocationChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
PKIX_RevocationChecker_GetRevCallback(
        PKIX_RevocationChecker *revChecker,
        PKIX_RevocationChecker_RevCallback *pCallback,
        void *plContext);

/*
 * FUNCTION: PKIX_RevocationChecker_GetRevCheckerContext
 * DESCRIPTION:
 *
 *  Retrieves a pointer to a PKIX_PL_Object representing the context (if any)
 *  of the RevocationChecker pointed to by "revChecker" and stores it at
 *  "pRevCheckerContext".
 *
 * PARAMETERS:
 *  "revChecker"
 *      Address of RevocationChecker whose context is to be stored.
 *      Must be non-NULL.
 *  "pRevCheckerContext"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see THREAD SAFETY DEFINITIONS at top of file)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a RevocationChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
PKIX_RevocationChecker_GetRevCheckerContext(
        PKIX_RevocationChecker *revChecker,
        PKIX_PL_Object **pRevCheckerContext,
        void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_REVCHECKER_H */
