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
 * pkix_comcertselparams.h
 *
 * ComCertSelParams Object Type Definition
 *
 */

#ifndef _PKIX_COMCERTSELPARAMS_H
#define _PKIX_COMCERTSELPARAMS_H

#include "pkix_tools.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * pathToNamesConstraint is Name Constraints generated based on the
 * pathToNames. We save a cached copy to save regeneration for each
 * check. SubjAltNames also has its cache, since SubjAltNames are
 * verified by checker, its cache copy is stored in checkerstate.
 */
struct PKIX_ComCertSelParamsStruct {
        PKIX_Int32 version;
        PKIX_Int32 minPathLength;
        PKIX_Boolean matchAllSubjAltNames;
        PKIX_PL_X500Name *subject;
        PKIX_List *policies; /* List of PKIX_PL_OID */
        PKIX_PL_Cert *cert;
        PKIX_PL_CertNameConstraints *nameConstraints;
        PKIX_List *pathToNames; /* List of PKIX_PL_GeneralNames */
        PKIX_List *subjAltNames; /* List of PKIX_PL_GeneralNames */
        PKIX_List *extKeyUsage; /* List of PKIX_PL_OID */
        PKIX_UInt32 keyUsage;
        PKIX_PL_Date *date;
        PKIX_PL_Date *certValid;
        PKIX_PL_X500Name *issuer;
        PKIX_PL_BigInt *serialNumber;
        PKIX_PL_ByteArray *authKeyId;
        PKIX_PL_ByteArray *subjKeyId;
        PKIX_PL_PublicKey *subjPubKey;
        PKIX_PL_OID *subjPKAlgId;
};

/* see source file for function documentation */

PKIX_Error *pkix_ComCertSelParams_RegisterSelf(void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_COMCERTSELPARAMS_H */
