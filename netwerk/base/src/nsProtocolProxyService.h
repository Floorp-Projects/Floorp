/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#ifndef nsProtocolProxyService_h__
#define nsProtocolProxyService_h__

#include "plevent.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsIPrefBranch.h"
#include "nsIProtocolProxyService.h"
#include "nsIProxyAutoConfig.h"
#include "nsIProxyInfo.h"
#include "nsIIOService.h"
#include "nsIObserver.h"
#include "prmem.h"
#include "prio.h"

class nsProtocolProxyService : public nsIProtocolProxyService
                             , public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLPROXYSERVICE
    NS_DECL_NSIOBSERVER

    nsProtocolProxyService();
    virtual ~nsProtocolProxyService();

    nsresult Init();

    void PrefsChanged(nsIPrefBranch *, const char* pref);

    class nsProxyInfo : public nsIProxyInfo
    {
    public:
        NS_DECL_ISUPPORTS

        NS_IMETHOD_(const char*) Host() {
            return mHost;
        }

        NS_IMETHOD_(PRInt32) Port() {
            return mPort;
        }

        NS_IMETHOD_(const char*) Type() {
            return mType;
        }

        NS_IMETHOD GetNext(nsIProxyInfo **result) {
            NS_IF_ADDREF(*result = mNext);
            return NS_OK;
        }

        virtual ~nsProxyInfo() {
            if (mHost) nsMemory::Free(mHost);
        }

        nsProxyInfo() : mType(nsnull), mHost(nsnull), mPort(-1) {
        }

        const char            *mType;
        char                  *mHost; // owning reference
        PRInt32                mPort;
        nsCOMPtr<nsIProxyInfo> mNext;
    };

protected:

    const char *ExtractProxyInfo(const char *proxy, PRBool permitHttp, nsProxyInfo **);

    nsresult GetProtocolInfo(const char *scheme, PRUint32 &flags, PRInt32 &defaultPort);
    nsresult NewProxyInfo_Internal(const char *type, char *host, PRInt32 port, nsIProxyInfo **);
    void     LoadFilters(const char *filters);
    PRBool   CanUseProxy(nsIURI *aURI, PRInt32 defaultPort);

    static PRBool PR_CALLBACK CleanupFilterArray(void *aElement, void *aData);
    static void*  PR_CALLBACK HandlePACLoadEvent(PLEvent* aEvent);
    static void   PR_CALLBACK DestroyPACLoadEvent(PLEvent* aEvent);

    struct HostInfoIP {
        PRUint16   family;
        PRUint16   mask_len;
        PRIPv6Addr addr; // possibly IPv4-mapped address
    };

    struct HostInfoName {
        char    *host;
        PRUint32 host_len;
    };

    // simplified array of filters defined by this struct
    struct HostInfo {
        PRBool  is_ipaddr;
        PRInt32 port;
        union {
            HostInfoIP   ip;
            HostInfoName name;
        };

        HostInfo()
            : is_ipaddr(PR_FALSE)
            { /* other members intentionally uninitialized */ }
       ~HostInfo() {
            if (!is_ipaddr && name.host)
                nsMemory::Free(name.host);
        }
    };

    nsVoidArray             mFiltersArray;

    nsCOMPtr<nsIIOService>  mIOService;

    PRUint16                mUseProxy;

    nsCString               mHTTPProxyHost;
    PRInt32                 mHTTPProxyPort;

    nsCString               mFTPProxyHost;
    PRInt32                 mFTPProxyPort;

    nsCString               mGopherProxyHost;
    PRInt32                 mGopherProxyPort;

    nsCString               mHTTPSProxyHost;
    PRInt32                 mHTTPSProxyPort;
    
    nsCString               mSOCKSProxyHost;
    PRInt32                 mSOCKSProxyPort;
    PRInt32                 mSOCKSProxyVersion;

    nsCOMPtr<nsIProxyAutoConfig> mPAC;
    nsCString                    mPACURL;
};

#endif // !nsProtocolProxyService_h__

