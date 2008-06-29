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
 * pkix_pl_x500name.h
 *
 * X500Name Object Type Definitions
 *
 */

#ifndef _PKIX_PL_X500NAME_H
#define _PKIX_PL_X500NAME_H

#include "pkix_pl_common.h"

#ifdef __cplusplus
extern "C" {
#endif


struct PKIX_PL_X500NameStruct{
        PRArenaPool *arena; /* X500Name arena. Shared arena with nssDN
                             * and derName */
        CERTName nssDN;
        SECItem derName;    /* adding DER encoded CERTName to the structure
                             * to avoid unnecessary name encoding when pass
                             * der name to cert finder */
};

/* see source file for function documentation */

PKIX_Error *pkix_pl_X500Name_RegisterSelf(void *plContext);

PKIX_Error *pkix_pl_X500Name_GetDERName(
        PKIX_PL_X500Name *xname,
        PRArenaPool *arena,
        SECItem **pSECName,
        void *plContext);

#ifdef BUILD_LIBPKIX_TESTS
PKIX_Error * pkix_pl_X500Name_CreateFromUtf8(
        char *stringRep,
        PKIX_PL_X500Name **pName,
        void *plContext);
#endif /* BUILD_LIBPKIX_TESTS */

PKIX_Error *pkix_pl_X500Name_GetCommonName(
        PKIX_PL_X500Name *xname,
        unsigned char **pCommonName,
        void *plContext);

PKIX_Error *
pkix_pl_X500Name_GetCountryName(
        PKIX_PL_X500Name *xname,
        unsigned char **pCountryName,
        void *plContext);

PKIX_Error *
pkix_pl_X500Name_GetOrgName(
        PKIX_PL_X500Name *xname,
        unsigned char **pOrgName,
        void *plContext);

PKIX_Error *
pkix_pl_X500Name_GetCERTName(
        PKIX_PL_X500Name *xname,
        CERTName **pCERTName,
        void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_PL_X500NAME_H */
