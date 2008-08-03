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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 * Portions created by Red Hat, Inc, are Copyright (C) 2005
 *
 * Contributor(s):
 *   Bob Relyea (rrelyea@redhat.com)
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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: staticobj.c,v $ $Revision: 1.1 $ $Date: 2005/11/04 02:05:04 $""; @(#) $RCSfile: staticobj.c,v $ $Revision: 1.1 $ $Date: 2005/11/04 02:05:04 $";
#endif /* DEBUG */

#ifndef CKCAPI_H
#include "ckcapi.h"
#endif /* CKCAPI_H */

static const CK_TRUST ckt_netscape_valid = CKT_NETSCAPE_VALID;
static const CK_OBJECT_CLASS cko_certificate = CKO_CERTIFICATE;
static const CK_TRUST ckt_netscape_trusted_delegator = CKT_NETSCAPE_TRUSTED_DELEGATOR;
static const CK_OBJECT_CLASS cko_netscape_trust = CKO_NETSCAPE_TRUST;
static const CK_BBOOL ck_true = CK_TRUE;
static const CK_OBJECT_CLASS cko_data = CKO_DATA;
static const CK_CERTIFICATE_TYPE ckc_x_509 = CKC_X_509;
static const CK_BBOOL ck_false = CK_FALSE;
static const CK_OBJECT_CLASS cko_netscape_builtin_root_list = CKO_NETSCAPE_BUILTIN_ROOT_LIST;

/* example of a static object */
static const CK_ATTRIBUTE_TYPE nss_ckcapi_types_1 [] = {
 CKA_CLASS,  CKA_TOKEN,  CKA_PRIVATE,  CKA_MODIFIABLE,  CKA_LABEL
};

static const NSSItem nss_ckcapi_items_1 [] = {
  { (void *)&cko_data, (PRUint32)sizeof(CK_OBJECT_CLASS) },
  { (void *)&ck_true, (PRUint32)sizeof(CK_BBOOL) },
  { (void *)&ck_false, (PRUint32)sizeof(CK_BBOOL) },
  { (void *)&ck_false, (PRUint32)sizeof(CK_BBOOL) },
  { (void *)"Mozilla CAPI Access", (PRUint32)20 }
};

PR_IMPLEMENT_DATA(ckcapiInternalObject) nss_ckcapi_data[] = {
  { ckcapiRaw, { 5, nss_ckcapi_types_1, nss_ckcapi_items_1} , {NULL} },
};

PR_IMPLEMENT_DATA(const PRUint32) nss_ckcapi_nObjects = 1;
