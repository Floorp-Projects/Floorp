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
 * pkix_pl_ldapdefaultclient.h
 *
 * LDAPDefaultClient Object Type Definition
 *
 */

#ifndef _PKIX_PL_LDAPDEFAULTCLIENT_H
#define _PKIX_PL_LDAPDEFAULTCLIENT_H

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
 * will have to change.) Therefore the LDAPClient_Create API contains a
 * BindAPI structure, a union, which will have to be revised and extended when
 * this area of the protocol is better understood.
 *
 */

typedef enum {
        CONNECT_PENDING,
        CONNECTED,
        BIND_PENDING,
        BIND_RESPONSE,
        BIND_RESPONSE_PENDING,
        BOUND,
        SEND_PENDING,
        RECV,
        RECV_PENDING,
        RECV_INITIAL,
        RECV_NONINITIAL,
        ABANDON_PENDING
} LdapClientConnectStatus;

struct PKIX_PL_LdapDefaultClientStruct {
        PKIX_PL_LdapClient vtable;
        LdapClientConnectStatus connectStatus;
        PKIX_UInt32 messageID;
        PKIX_PL_HashTable *cachePtr;
        PKIX_PL_Socket *clientSocket;
        PRPollDesc pollDesc;
        void *callbackList; /* cast this to (PKIX_PL_Socket_Callback *) */
        LDAPBindAPI *bindAPI;
        PRArenaPool *arena;
        PRTime lastIO;
        void *sendBuf;
        PKIX_UInt32 bytesToWrite;
        void *rcvBuf;
        PKIX_UInt32 capacity;
        void *currentInPtr;
        PKIX_UInt32 currentBytesAvailable;
        void *bindMsg;
        PKIX_UInt32 bindMsgLen;
        PKIX_List *entriesFound;
        PKIX_PL_LdapRequest *currentRequest;
        PKIX_PL_LdapResponse *currentResponse;
};

/* see source file for function documentation */

PKIX_Error *pkix_pl_LdapDefaultClient_RegisterSelf(void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_PL_LDAPDEFAULTCLIENT_H */
