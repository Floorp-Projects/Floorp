/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#ifndef nsProtocolProxyService_h__
#define nsProtocolProxyService_h__

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsIPrefBranch.h"
#include "nsIProtocolProxyService2.h"
#include "nsIProtocolProxyFilter.h"
#include "nsIProxyAutoConfig.h"
#include "nsISystemProxySettings.h"
#include "nsIProxyInfo.h"
#include "nsIObserver.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsPACMan.h"
#include "prtime.h"
#include "prmem.h"
#include "prio.h"
#include "mozilla/Attributes.h"

typedef nsDataHashtable<nsCStringHashKey, PRUint32> nsFailedProxyTable;

class nsProxyInfo;
struct nsProtocolInfo;

class nsProtocolProxyService MOZ_FINAL : public nsIProtocolProxyService2
                                       , public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLPROXYSERVICE2
    NS_DECL_NSIPROTOCOLPROXYSERVICE
    NS_DECL_NSIOBSERVER

    nsProtocolProxyService() NS_HIDDEN;

    NS_HIDDEN_(nsresult) Init();

protected:
    friend class nsAsyncResolveRequest;

    ~nsProtocolProxyService() NS_HIDDEN;

    /**
     * This method is called whenever a preference may have changed or
     * to initialize all preferences.
     *
     * @param prefs
     *        This must be a pointer to the root pref branch.
     * @param name
     *        This can be the name of a fully-qualified preference, or it can
     *        be null, in which case all preferences will be initialized.
     */
    NS_HIDDEN_(void) PrefsChanged(nsIPrefBranch *prefs, const char *name);

    /**
     * This method is called to create a nsProxyInfo instance from the given
     * PAC-style proxy string.  It parses up to the end of the string, or to
     * the next ';' character.
     * 
     * @param proxy
     *        The PAC-style proxy string to parse.  This must not be null.
     * @param aResolveFlags
     *        The flags passed to Resolve or AsyncResolve that are stored in 
     *        proxyInfo.
     * @param result
     *        Upon return this points to a newly allocated nsProxyInfo or null
     *        if the proxy string was invalid.
     *
     * @return A pointer beyond the parsed proxy string (never null).
     */
    NS_HIDDEN_(const char *) ExtractProxyInfo(const char *proxy,
                                              PRUint32 aResolveFlags,
                                              nsProxyInfo **result);

    /**
     * Load the specified PAC file.
     * 
     * @param pacURI
     *        The URI spec of the PAC file to load.
     */
    NS_HIDDEN_(nsresult) ConfigureFromPAC(const nsCString &pacURI, bool forceReload);

    /**
     * This method builds a list of nsProxyInfo objects from the given PAC-
     * style string.
     *
     * @param pacString
     *        The PAC-style proxy string to parse.  This may be empty.
     * @param aResolveFlags
     *        The flags passed to Resolve or AsyncResolve that are stored in 
     *        proxyInfo.
     * @param result
     *        The resulting list of proxy info objects.
     */
    NS_HIDDEN_(void) ProcessPACString(const nsCString &pacString,
                                      PRUint32 aResolveFlags,
                                      nsIProxyInfo **result);

    /**
     * This method generates a string valued identifier for the given
     * nsProxyInfo object.
     *
     * @param pi
     *        The nsProxyInfo object from which to generate the key.
     * @param result
     *        Upon return, this parameter holds the generated key.
     */
    NS_HIDDEN_(void) GetProxyKey(nsProxyInfo *pi, nsCString &result);

    /**
     * @return Seconds since start of session.
     */
    NS_HIDDEN_(PRUint32) SecondsSinceSessionStart();

    /**
     * This method removes the specified proxy from the disabled list.
     *
     * @param pi
     *        The nsProxyInfo object identifying the proxy to enable.
     */
    NS_HIDDEN_(void) EnableProxy(nsProxyInfo *pi);

    /**
     * This method adds the specified proxy to the disabled list.
     *
     * @param pi
     *        The nsProxyInfo object identifying the proxy to disable.
     */
    NS_HIDDEN_(void) DisableProxy(nsProxyInfo *pi);

    /**
     * This method tests to see if the given proxy is disabled.
     *
     * @param pi
     *        The nsProxyInfo object identifying the proxy to test.
     *
     * @return True if the specified proxy is disabled.
     */
    NS_HIDDEN_(bool) IsProxyDisabled(nsProxyInfo *pi);

    /**
     * This method queries the protocol handler for the given scheme to check
     * for the protocol flags and default port.
     *
     * @param uri
     *        The URI to query.
     * @param info
     *        Holds information about the protocol upon return.  Pass address
     *        of structure when you call this method.  This parameter must not
     *        be null.
     */
    NS_HIDDEN_(nsresult) GetProtocolInfo(nsIURI *uri, nsProtocolInfo *result);

    /**
     * This method is an internal version nsIProtocolProxyService::newProxyInfo
     * that expects a string literal for the type.
     *
     * @param type
     *        The proxy type.
     * @param host
     *        The proxy host name (UTF-8 ok).
     * @param port
     *        The proxy port number.
     * @param flags
     *        The proxy flags (nsIProxyInfo::flags).
     * @param timeout
     *        The failover timeout for this proxy.
     * @param next
     *        The next proxy to try if this one fails.
     * @param aResolveFlags
     *        The flags passed to resolve (from nsIProtocolProxyService).
     * @param result
     *        The resulting nsIProxyInfo object.
     */
    NS_HIDDEN_(nsresult) NewProxyInfo_Internal(const char *type,
                                               const nsACString &host,
                                               PRInt32 port,
                                               PRUint32 flags,
                                               PRUint32 timeout,
                                               nsIProxyInfo *next,
                                               PRUint32 aResolveFlags,
                                               nsIProxyInfo **result);

    /**
     * This method is an internal version of Resolve that does not query PAC.
     * It performs all of the built-in processing, and reports back to the
     * caller with either the proxy info result or a flag to instruct the
     * caller to use PAC instead.
     *
     * @param uri
     *        The URI to test.
     * @param info
     *        Information about the URI's protocol.
     * @param flags
     *        The flags passed to either the resolve or the asyncResolve method.
     * @param usePAC
     *        If this flag is set upon return, then PAC should be queried to
     *        resolve the proxy info.
     * @param result
     *        The resulting proxy info or null.
     */
    NS_HIDDEN_(nsresult) Resolve_Internal(nsIURI *uri,
                                          const nsProtocolInfo &info,
                                          PRUint32 flags,
                                          bool *usePAC, 
                                          nsIProxyInfo **result);

    /**
     * This method applies the registered filters to the given proxy info
     * list, and returns a possibly modified list.
     *
     * @param uri
     *        The URI corresponding to this proxy info list.
     * @param info
     *        Information about the URI's protocol.
     * @param proxyInfo
     *        The proxy info list to be modified.  This is an inout param.
     */
    NS_HIDDEN_(void) ApplyFilters(nsIURI *uri, const nsProtocolInfo &info,
                                  nsIProxyInfo **proxyInfo);

    /**
     * This method is a simple wrapper around ApplyFilters that takes the
     * proxy info list inout param as a nsCOMPtr.
     */
    inline void ApplyFilters(nsIURI *uri, const nsProtocolInfo &info,
                             nsCOMPtr<nsIProxyInfo> &proxyInfo)
    {
      nsIProxyInfo *pi = nullptr;
      proxyInfo.swap(pi);
      ApplyFilters(uri, info, &pi);
      proxyInfo.swap(pi);
    }

    /**
     * This method prunes out disabled and disallowed proxies from a given
     * proxy info list.
     *
     * @param info
     *        Information about the URI's protocol.
     * @param proxyInfo
     *        The proxy info list to be modified.  This is an inout param.
     */
    NS_HIDDEN_(void) PruneProxyInfo(const nsProtocolInfo &info,
                                    nsIProxyInfo **proxyInfo);

    /**
     * This method populates mHostFiltersArray from the given string.
     *
     * @param hostFilters
     *        A "no-proxy-for" exclusion list.
     */
    NS_HIDDEN_(void) LoadHostFilters(const char *hostFilters);

    /**
     * This method checks the given URI against mHostFiltersArray.
     *
     * @param uri
     *        The URI to test.
     * @param defaultPort
     *        The default port for the given URI.
     *
     * @return True if the URI can use the specified proxy.
     */
    NS_HIDDEN_(bool) CanUseProxy(nsIURI *uri, PRInt32 defaultPort);

public:
    // The Sun Forte compiler and others implement older versions of the
    // C++ standard's rules on access and nested classes.  These structs
    // need to be public in order to deal with those compilers.

    struct HostInfoIP {
        PRUint16   family;
        PRUint16   mask_len;
        PRIPv6Addr addr; // possibly IPv4-mapped address
    };

    struct HostInfoName {
        char    *host;
        PRUint32 host_len;
    };

protected:

    // simplified array of filters defined by this struct
    struct HostInfo {
        bool    is_ipaddr;
        PRInt32 port;
        union {
            HostInfoIP   ip;
            HostInfoName name;
        };

        HostInfo()
            : is_ipaddr(false)
            { /* other members intentionally uninitialized */ }
       ~HostInfo() {
            if (!is_ipaddr && name.host)
                nsMemory::Free(name.host);
        }
    };

    // This structure is allocated for each registered nsIProtocolProxyFilter.
    struct FilterLink {
      struct FilterLink                *next;
      PRUint32                          position;
      nsCOMPtr<nsIProtocolProxyFilter>  filter;

      FilterLink(PRUint32 p, nsIProtocolProxyFilter *f)
        : next(nullptr), position(p), filter(f) {}

      // Chain deletion to simplify cleaning up the filter links
      ~FilterLink() { if (next) delete next; }
    };

    // Indicates if local hosts (plain hostnames, no dots) should use the proxy
    bool mFilterLocalHosts;

    // Holds an array of HostInfo objects
    nsTArray<nsAutoPtr<HostInfo> > mHostFiltersArray;

    // Points to the start of a sorted by position, singly linked list
    // of FilterLink objects.
    FilterLink                  *mFilters;

    PRUint32                     mProxyConfig;

    nsCString                    mHTTPProxyHost;
    PRInt32                      mHTTPProxyPort;

    nsCString                    mFTPProxyHost;
    PRInt32                      mFTPProxyPort;

    nsCString                    mHTTPSProxyHost;
    PRInt32                      mHTTPSProxyPort;
    
    nsCString                    mSOCKSProxyHost;
    PRInt32                      mSOCKSProxyPort;
    PRInt32                      mSOCKSProxyVersion;
    bool                         mSOCKSProxyRemoteDNS;

    nsRefPtr<nsPACMan>           mPACMan;  // non-null if we are using PAC
    nsCOMPtr<nsISystemProxySettings> mSystemProxySettings;

    PRTime                       mSessionStart;
    nsFailedProxyTable           mFailedProxies;
    PRInt32                      mFailedProxyTimeout;
};

#endif // !nsProtocolProxyService_h__
