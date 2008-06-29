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
 * pkix_pl_ldapcertstore.h
 *
 * LDAPCertstore Object Type Definition
 *
 */

#ifndef _PKIX_PL_LDAPCERTSTORE_H
#define _PKIX_PL_LDAPCERTSTORE_H

#include "pkix_pl_ldapt.h"
#include "pkix_pl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * At the time of this version, there are unresolved questions about the LDAP
 * protocol. Although RFC1777 describes a BIND and UNBIND message, it is not
 * clear whether they are appropriate to this application. We have tested only
 * using servers that do not expect authentication, and that reject BIND
 * messages. It is not clear what values might be appropriate for the bindname
 * and authentication fields, which are currently implemented as char strings
 * supplied by the caller. (If this changes, the API and possibly the templates
 * will have to change.) Therefore the CertStore_Create API contains a BindAPI
 * structure, a union, which will have to be revised and extended when this
 * area of the protocol is better understood.
 *
 * It is further assumed that a given LdapCertStore will connect only to a
 * single server, and that the creation of the socket will initiate the
 * CONNECT. Therefore the LdapCertStore handles only the case of continuing
 * the connection, if nonblocking I/O is being used.
 */

typedef enum {
        LDAP_CONNECT_PENDING,
        LDAP_CONNECTED,
        LDAP_BIND_PENDING,
        LDAP_BIND_RESPONSE,
        LDAP_BIND_RESPONSE_PENDING,
        LDAP_BOUND,
        LDAP_SEND_PENDING,
        LDAP_RECV,
        LDAP_RECV_PENDING,
        LDAP_RECV_INITIAL,
        LDAP_RECV_NONINITIAL,
        LDAP_ABANDON_PENDING
} LDAPConnectStatus;

#define LDAP_CACHEBUCKETS       128
#define RCVBUFSIZE              512

struct PKIX_PL_LdapCertStoreContext {
        PKIX_PL_LdapClient *client;
};

/* see source file for function documentation */

PKIX_Error *pkix_pl_LdapCertStoreContext_RegisterSelf(void *plContext);

PKIX_Error *
pkix_pl_LdapCertStore_BuildCertList(
        PKIX_List *responseList,
        PKIX_List **pCerts,
        void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_PL_LDAPCERTSTORE_H */
