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
 * pkix_pl_ldapresponse.h
 *
 * LdapResponse Object Definitions
 *
 */

#ifndef _PKIX_PL_LDAPRESPONSE_H
#define _PKIX_PL_LDAPRESPONSE_H

#include "pkix_pl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PKIX_PL_LdapResponseStruct{
        LDAPMessage decoded;
        PKIX_UInt32 partialLength;
        PKIX_UInt32 totalLength;
        SECItem derEncoded;
};

/* see source file for function documentation */

PKIX_Error *
pkix_pl_LdapResponse_Create(
        LDAPMessageType responseType,
        PKIX_UInt32 totalLength,
        PKIX_UInt32 bytesAvailable,
        void *partialData,
        PKIX_UInt32 *pBytesConsumed,
        PKIX_PL_LdapResponse **pResponse,
        void *plContext);

PKIX_Error *
pkix_pl_LdapResponse_Append(
        PKIX_PL_LdapResponse *response,
        PKIX_UInt32 partialLength,
        void *partialData,
        PKIX_UInt32 *bytesConsumed,
        void *plContext);

PKIX_Error *
pkix_pl_LdapResponse_IsComplete(
        PKIX_PL_LdapResponse *response,
        PKIX_Boolean *pIsComplete,
        void *plContext);

PKIX_Error *
pkix_pl_LdapResponse_Decode(
        PRArenaPool *arena,
        PKIX_PL_LdapResponse *response,
        SECStatus *pStatus,
        void *plContext);

PKIX_Error *
pkix_pl_LdapResponse_GetMessage(
        PKIX_PL_LdapResponse *response,
        LDAPMessage **pMessage,
        void *plContext);

PKIX_Error *
pkix_pl_LdapResponse_GetMessageType(
        PKIX_PL_LdapResponse *response,
        LDAPMessageType *pMessageType,
        void *plContext);

PKIX_Error *
pkix_pl_LdapResponse_GetCapacity(
        PKIX_PL_LdapResponse *response,
        PKIX_UInt32 *pCapacity,
        void *plContext);

PKIX_Error *
pkix_pl_LdapResponse_GetResultCode(
        PKIX_PL_LdapResponse *response,
        LDAPResultCode *pResultCode,
        void *plContext);

PKIX_Error *
pkix_pl_LdapResponse_GetAttributes(
        PKIX_PL_LdapResponse *response,
        LDAPSearchResponseAttr ***pAttributes,
        void *plContext);

PKIX_Error *pkix_pl_LdapResponse_RegisterSelf(void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_PL_LDAPRESPONSE_H */
