/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "mkutils.h" 
#include "mkgeturl.h"
#include "net.h"
#include "secnav.h"
#include "ldap.h"

#ifdef MOZ_SECURITY
#include "certldap.h"
#include "mkcertld.h"
#endif

PRIVATE int32
net_CertLdapLoad(ActiveEntry *ce)
{
    int err = 0;
#ifdef MOZ_SECURITY
    CertLdapConnData *connData;
    
    connData = SECNAV_CertLdapLoad(ce->URL_s);
    ce->con_data = connData;
    if ( connData == NULL ) {
	err = -1;
    }
    
    if ( err ) {
	ce->status = err;
    } else {
#endif

	PR_ASSERT(0);

#ifdef XP_UNIX
	NET_SetConnectSelect(ce->window_id, ce->socket);
	NET_TotalNumberOfOpenConnections++;
#else
	NET_SetCallNetlibAllTheTime(ce->window_id, "mkcertld");
#endif
#ifdef MOZ_SECURITY
    }
#endif    
    return(err);
}

PRIVATE int32
net_ProcessCertLdap(ActiveEntry *ce)
{
    int err = 0;
#ifdef MOZ_SECURITY
    CertLdapConnData *connData;
    
    connData = (CertLdapConnData *)ce->con_data;

#ifdef XP_UNIX
    NET_ClearConnectSelect(ce->window_id, connData->fd);
    NET_SetReadSelect(ce->window_id, connData->fd);
#endif

    err = SECNAV_CertLdapProcess(connData);
#endif    
    if ( err ) {
	if ( err == 1 ) {
	    /* done */
	    ce->status = 0;
	    err = -1;
	} else {
	    ce->status = err;
	}
#ifdef XP_UNIX
#ifdef MOZ_SECURITY
	NET_ClearReadSelect(ce->window_id, connData->fd);
#endif
	NET_TotalNumberOfOpenConnections--;
#else
        NET_ClearCallNetlibAllTheTime(ce->window_id, "mkcertld");
#endif

    }

    return(err);
}

PRIVATE int32
net_InterruptCertLdap(ActiveEntry *ce)
{
    int err;
#ifdef MOZ_SECURITY
    CertLdapConnData *connData;
    
    connData = (CertLdapConnData *)ce->con_data;

    err = SECNAV_CertLdapInterrupt(connData);
#endif
    ce->status = MK_INTERRUPTED;

#ifdef XP_UNIX
#ifdef MOZ_SECURITY
    NET_ClearReadSelect(ce->window_id, connData->fd);
#endif
    NET_TotalNumberOfOpenConnections--;
#else
    NET_ClearCallNetlibAllTheTime(ce->window_id, "mkcertld");
#endif

    return(err);
}

PRIVATE void
net_CleanupCertLdap(void)
{
}

MODULE_PRIVATE void
NET_InitCertLdapProtocol(void)
{
        static NET_ProtoImpl certldap_proto_impl;

        certldap_proto_impl.init = net_CertLdapLoad;
        certldap_proto_impl.process = net_ProcessCertLdap;
        certldap_proto_impl.interrupt = net_InterruptCertLdap;
        certldap_proto_impl.resume = NULL;
        certldap_proto_impl.cleanup = net_CleanupCertLdap;

        NET_RegisterProtocolImplementation(&certldap_proto_impl, INTERNAL_CERTLDAP_TYPE_URL);
}

