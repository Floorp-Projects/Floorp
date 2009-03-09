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
 * pkix_pl_common.h
 *
 * Common central header file included by all PL source files
 *
 */

#ifndef _PKIX_PL_COMMON_H
#define _PKIX_PL_COMMON_H

/* PKIX HEADERS */
#include "pkix_tools.h"

/* NSS headers */
#include "nss.h"
#include "secport.h"
#include "secasn1.h"
#include "secerr.h"
#include "base64.h"
#include "cert.h"
#include "certdb.h"
#include "genname.h"
#include "xconst.h"
#include "keyhi.h"
#include "ocsp.h"
#include "ocspt.h"
#include "pk11pub.h"
#include "pkcs11.h"
#include "pkcs11t.h"
#include "prio.h"

/* NSPR headers */
#include "nspr.h"

/* private PKIX_PL_NSS system headers */
#include "pkix_pl_object.h"
#include "pkix_pl_string.h"
#include "pkix_pl_ldapt.h"
#include "pkix_pl_aiamgr.h"
#include "pkix_pl_bigint.h"
#include "pkix_pl_oid.h"
#include "pkix_pl_x500name.h"
#include "pkix_pl_generalname.h"
#include "pkix_pl_publickey.h"
#include "pkix_pl_bytearray.h"
#include "pkix_pl_date.h"
#include "pkix_pl_primhash.h"
#include "pkix_pl_basicconstraints.h"
#include "pkix_pl_bytearray.h"
#include "pkix_pl_cert.h"
#include "pkix_pl_certpolicyinfo.h"
#include "pkix_pl_certpolicymap.h"
#include "pkix_pl_certpolicyqualifier.h"
#include "pkix_pl_crl.h"
#include "pkix_pl_crlentry.h"
#include "pkix_pl_nameconstraints.h"
#include "pkix_pl_ocsprequest.h"
#include "pkix_pl_ocspresponse.h"
#include "pkix_pl_pk11certstore.h"
#include "pkix_pl_socket.h"
#include "pkix_pl_ldapcertstore.h"
#include "pkix_pl_ldaprequest.h"
#include "pkix_pl_ldapresponse.h"
#include "pkix_pl_nsscontext.h"
#include "pkix_pl_httpcertstore.h"
#include "pkix_pl_httpdefaultclient.h"
#include "pkix_pl_infoaccess.h"
#include "pkix_sample_modules.h"

#define MAX_DIGITS_32 (PKIX_UInt32) 10

#define PKIX_PL_NSSCALL(type, func, args)  \
        PKIX_ ## type ## _DEBUG_ARG("( Calling %s).\n", #func); \
        (func args)

#define PKIX_PL_NSSCALLRV(type, lvalue, func, args)  \
        PKIX_ ## type ## _DEBUG_ARG("( Calling %s).\n", #func); \
        lvalue = (func args)

/* see source file for function documentation */

PKIX_Error *
pkix_LockObject(
        PKIX_PL_Object *object,
        void *plContext);

PKIX_Error *
pkix_UnlockObject(
        PKIX_PL_Object *object,
        void *plContext);

PKIX_Boolean
pkix_pl_UInt32_Overflows(char *string);

PKIX_Error *
pkix_pl_helperBytes2Ascii(
        PKIX_UInt32 *tokens,
        PKIX_UInt32 numTokens,
        char **pAscii,
        void *plContext);

PKIX_Error *
pkix_pl_ipAddrBytes2Ascii(
        SECItem *secItem,
        char **pAscii,
        void *plContext);

PKIX_Error *
pkix_pl_oidBytes2Ascii(
        SECItem *secItem,
        char **pAscii,
        void *plContext);

/* --String-Encoding-Conversion-Functions------------------------ */

PKIX_Error *
pkix_UTF16_to_EscASCII(
        void *utf16String,
        PKIX_UInt32 utf16Length,
        PKIX_Boolean debug,
        char **pDest,
        PKIX_UInt32 *pLength,
        void *plContext);

PKIX_Error *
pkix_EscASCII_to_UTF16(
        char *escAsciiString,
        PKIX_UInt32 escAsciiLen,
        PKIX_Boolean debug,
        void **pDest,
        PKIX_UInt32 *pLength,
        void *plContext);

PKIX_Error *
pkix_UTF16_to_UTF8(
        void *utf16String,
        PKIX_UInt32 utf16Length,
        PKIX_Boolean null_Term,
        void **pDest,
        PKIX_UInt32 *pLength,
        void *plContext);

PKIX_Error *
pkix_UTF8_to_UTF16(
        void *utf8Source,
        PKIX_UInt32 utf8Length,
        void **pDest,
        PKIX_UInt32 *pLength,
        void *plContext);

#endif /* _PKIX_PL_COMMON_H */
