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
 * pkix_pl_lifecycle.h
 *
 * Lifecycle Definitions
 *
 */

#ifndef _PKIX_PL_LIFECYCLE_H
#define _PKIX_PL_LIFECYCLE_H

#include "pkix_pl_common.h"
#include "pkix_pl_oid.h"
#include "pkix_pl_aiamgr.h"
#include "pkix_pl_bigint.h"
#include "pkix_pl_bytearray.h"
#include "pkix_pl_hashtable.h"
#include "pkix_pl_mutex.h"
#include "pkix_pl_rwlock.h"
#include "pkix_pl_monitorlock.h"
#include "pkix_pl_string.h"
#include "pkix_pl_cert.h"
#include "pkix_pl_x500name.h"
#include "pkix_pl_generalname.h"
#include "pkix_pl_publickey.h"
#include "pkix_pl_date.h"
#include "pkix_pl_basicconstraints.h"
#include "pkix_pl_certpolicyinfo.h"
#include "pkix_pl_certpolicymap.h"
#include "pkix_pl_certpolicyqualifier.h"
#include "pkix_pl_crlentry.h"
#include "pkix_pl_crl.h"
#include "pkix_pl_colcertstore.h"
#include "pkix_pl_ldapcertstore.h"
#include "pkix_pl_ldapdefaultclient.h"
#include "pkix_pl_ldaprequest.h"
#include "pkix_pl_ldapresponse.h"
#include "pkix_pl_socket.h"
#include "pkix_pl_infoaccess.h"
#include "pkix_store.h"
#include "pkix_error.h"
#include "pkix_logger.h"
#include "pkix_list.h"
#include "pkix_trustanchor.h"
#include "pkix_procparams.h"
#include "pkix_valparams.h"
#include "pkix_valresult.h"
#include "pkix_verifynode.h"
#include "pkix_resourcelimits.h"
#include "pkix_certchainchecker.h"
#include "pkix_revocationchecker.h"
#include "pkix_certselector.h"
#include "pkix_comcertselparams.h"
#include "pkix_crlselector.h"
#include "pkix_comcrlselparams.h"
#include "pkix_targetcertchecker.h"
#include "pkix_basicconstraintschecker.h"
#include "pkix_policynode.h"
#include "pkix_policychecker.h"
#include "pkix_defaultcrlchecker.h"
#include "pkix_signaturechecker.h"
#include "pkix_buildresult.h"
#include "pkix_build.h"
#include "pkix_pl_nameconstraints.h"
#include "pkix_nameconstraintschecker.h"
#include "pkix_ocspchecker.h"
#include "pkix_pl_ocspcertid.h"
#include "pkix_pl_ocsprequest.h"
#include "pkix_pl_ocspresponse.h"
#include "pkix_pl_httpdefaultclient.h"
#include "pkix_pl_httpcertstore.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PKIX_PL_InitializeParamsStruct {
        PKIX_List *loggers;
        PKIX_UInt32 majorVersion;
        PKIX_UInt32 minorVersion;
};

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_PL_LIFECYCLE_H */
