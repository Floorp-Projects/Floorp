/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: staticobj.c,v $ $Revision: 1.4 $ $Date: 2012/04/25 14:49:30 $""; @(#) $RCSfile: staticobj.c,v $ $Revision: 1.4 $ $Date: 2012/04/25 14:49:30 $";
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

ckcapiInternalObject nss_ckcapi_data[] = {
  { ckcapiRaw,
    { 5, nss_ckcapi_types_1, nss_ckcapi_items_1} ,
  },

};

const PRUint32 nss_ckcapi_nObjects = 1;
