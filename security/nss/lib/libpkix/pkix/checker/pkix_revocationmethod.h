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
 * pkix_revocationmethod.h
 *
 * RevocationMethod Object
 *
 */

#ifndef _PKIX_REVOCATIONMETHOD_H
#define _PKIX_REVOCATIONMETHOD_H

#include "pkixt.h"
#include "pkix_revocationchecker.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pkix_RevocationMethodStruct pkix_RevocationMethod;

/* Local revocation check function prototype definition.
 * Revocation methods capable of checking revocation though local
 * means(cache) should implement this prototype. */
typedef PKIX_Error *
pkix_LocalRevocationCheckFn(PKIX_PL_Cert *cert, PKIX_PL_Cert *issuer,
                            PKIX_PL_Date *date, 
                            pkix_RevocationMethod *checkerObject,
                            PKIX_ProcessingParams *procParams,
                            PKIX_UInt32 methodFlags,
                            PKIX_Boolean chainVerificationState,
                            PKIX_RevocationStatus *pRevStatus,
                            PKIX_UInt32 *reasonCode,
                            void *plContext);

/* External revocation check function prototype definition.
 * Revocation methods that required external communications(crldp
 * ocsp) shoult implement this prototype. */
typedef PKIX_Error *
pkix_ExternalRevocationCheckFn(PKIX_PL_Cert *cert, PKIX_PL_Cert *issuer,
                               PKIX_PL_Date *date,
                               pkix_RevocationMethod *checkerObject,
                               PKIX_ProcessingParams *procParams,
                               PKIX_UInt32 methodFlags,
                               PKIX_RevocationStatus *pRevStatus,
                               PKIX_UInt32 *reasonCode,
                               void **pNBIOContext, void *plContext);

/* Revocation method structure assosiates revocation types with
 * a set of flags on the method, a priority of the method, and
 * method local/external checker functions. */
struct pkix_RevocationMethodStruct {
    PKIX_RevocationMethodType methodType;
    PKIX_UInt32 flags;
    PKIX_UInt32 priority;
    pkix_LocalRevocationCheckFn (*localRevChecker);
    pkix_ExternalRevocationCheckFn (*externalRevChecker);
};

PKIX_Error *
pkix_RevocationMethod_Duplicate(PKIX_PL_Object *object,
                                PKIX_PL_Object *newObject,
                                void *plContext);

PKIX_Error *
pkix_RevocationMethod_Init(pkix_RevocationMethod *method,
                           PKIX_RevocationMethodType methodType,
                           PKIX_UInt32 flags,
                           PKIX_UInt32 priority,
                           pkix_LocalRevocationCheckFn localRevChecker,
                           pkix_ExternalRevocationCheckFn externalRevChecker,
                           void *plContext);


#ifdef __cplusplus
}
#endif

#endif /* _PKIX_REVOCATIONMETHOD_H */
