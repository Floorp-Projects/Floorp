/* vim:set ts=4 sw=4 cindent et: */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIOService.h"
#include "nsIProtocolHandler.h"
#include "nsIFileProtocolHandler.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIURI.h"
#include "nsIStreamListener.h"
#include "prprf.h"
#include "nsLoadGroup.h"
#include "nsInputStreamChannel.h"
#include "nsXPIDLString.h" 
#include "nsReadableUtils.h"
#include "nsIErrorService.h" 
#include "netCore.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefLocalizedString.h"
#include "nsICategoryManager.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIProxiedProtocolHandler.h"
#include "nsIProxyInfo.h"
#include "nsITimelineService.h"
#include "nsEscape.h"
#include "nsNetCID.h"
#include "nsIRecyclingAllocator.h"
#include "nsISocketTransport.h"
#include "nsCRT.h"

#define PORT_PREF_PREFIX     "network.security.ports."
#define PORT_PREF(x)         PORT_PREF_PREFIX x
#define AUTODIAL_PREF        "network.autodial-helper.enabled"

static NS_DEFINE_CID(kStreamTransportServiceCID, NS_STREAMTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);
static NS_DEFINE_CID(kErrorServiceCID, NS_ERRORSERVICE_CID);
static NS_DEFINE_CID(kProtocolProxyServiceCID, NS_PROTOCOLPROXYSERVICE_CID);

// A general port blacklist.  Connections to these ports will not be avoided unless 
// the protocol overrides.
//
// TODO: I am sure that there are more ports to be added.  
//       This cut is based on the classic mozilla codebase

PRInt16 gBadPortList[] = { 
  1,    // tcpmux          
  7,    // echo     
  9,    // discard          
  11,   // systat   
  13,   // daytime          
  15,   // netstat  
  17,   // qotd             
  19,   // chargen  
  20,   // ftp-data         
  21,   // ftp-cntl 
  22,   // ssh              
  23,   // telnet   
  25,   // smtp     
  37,   // time     
  42,   // name     
  43,   // nicname  
  53,   // domain  
  77,   // priv-rjs 
  79,   // finger   
  87,   // ttylink  
  95,   // supdup   
  101,  // hostriame
  102,  // iso-tsap 
  103,  // gppitnp  
  104,  // acr-nema 
  109,  // pop2     
  110,  // pop3     
  111,  // sunrpc   
  113,  // auth     
  115,  // sftp     
  117,  // uucp-path
  119,  // nntp     
  123,  // NTP
  135,  // loc-srv / epmap         
  139,  // netbios
  143,  // imap2  
  179,  // BGP
  389,  // ldap        
  512,  // print / exec          
  513,  // login         
  514,  // shell         
  515,  // printer         
  526,  // tempo         
  530,  // courier        
  531,  // Chat         
  532,  // netnews        
  540,  // uucp       
  556,  // remotefs    
  587,  //
  601,  //       
  2049, // nfs
  4045, // lockd
  6000, // x11        
  0,    // This MUST be zero so that we can populating the array
};

static const char kProfileChangeNetTeardownTopic[] = "profile-change-net-teardown";
static const char kProfileChangeNetRestoreTopic[] = "profile-change-net-restore";

// Necko buffer cache
nsIMemory* nsIOService::gBufferCache = nsnull;

////////////////////////////////////////////////////////////////////////////////

nsIOService::nsIOService()
    : mOffline(PR_FALSE),
      mOfflineForProfileChange(PR_FALSE)
{
    // Get the allocator ready
    if (!gBufferCache)
    {
        nsresult rv = NS_OK;
        nsCOMPtr<nsIRecyclingAllocator> recyclingAllocator =
            do_CreateInstance(NS_RECYCLINGALLOCATOR_CONTRACTID, &rv);
        if (NS_FAILED(rv))
            return;
        rv = recyclingAllocator->Init(NS_NECKO_BUFFER_CACHE_COUNT,
                                      NS_NECKO_15_MINS, "necko");
        if (NS_FAILED(rv))
            return;

        nsCOMPtr<nsIMemory> eyeMemory = do_QueryInterface(recyclingAllocator);
        gBufferCache = eyeMemory.get();
        NS_IF_ADDREF(gBufferCache);
    }
}

nsresult
nsIOService::Init()
{
    nsresult rv;
    
    // Hold onto the eventQueue service.  We do not want any eventqueues to go away
    // when we shutdown until we process all remaining transports

    mEventQueueService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        NS_WARNING("failed to get event queue service");
    
    // We need to get references to these services so that we can shut them
    // down later. If we wait until the nsIOService is being shut down,
    // GetService will fail at that point.

    mSocketTransportService = do_GetService(kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv))
        NS_WARNING("failed to get socket transport service");

    mDNSService = do_GetService(kDNSServiceCID, &rv);
    if (NS_FAILED(rv))
        NS_WARNING("failed to get DNS service");

    mProxyService = do_GetService(kProtocolProxyServiceCID, &rv);
    if (NS_FAILED(rv))
        NS_WARNING("failed to get protocol proxy service");

    // XXX hack until xpidl supports error info directly (bug 13423)
    nsCOMPtr<nsIErrorService> errorService = do_GetService(kErrorServiceCID);
    if (errorService) {
        errorService->RegisterErrorStringBundle(NS_ERROR_MODULE_NETWORK, NECKO_MSGS_URL);
        errorService->RegisterErrorStringBundleKey(nsISocketTransport::STATUS_RESOLVING, "ResolvingHost");
        errorService->RegisterErrorStringBundleKey(nsISocketTransport::STATUS_CONNECTED_TO, "ConnectedTo");
        errorService->RegisterErrorStringBundleKey(nsISocketTransport::STATUS_SENDING_TO, "SendingTo");
        errorService->RegisterErrorStringBundleKey(nsISocketTransport::STATUS_RECEIVING_FROM, "ReceivingFrom");
        errorService->RegisterErrorStringBundleKey(nsISocketTransport::STATUS_CONNECTING_TO, "ConnectingTo");
        errorService->RegisterErrorStringBundleKey(nsISocketTransport::STATUS_WAITING_FOR, "WaitingFor");
    }
    else
        NS_WARNING("failed to get error service");
    
    // setup our bad port list stuff
    for(int i=0; gBadPortList[i]; i++)
        mRestrictedPortList.AppendElement(NS_REINTERPRET_CAST(void *, gBadPortList[i]));

    // Further modifications to the port list come from prefs
    nsCOMPtr<nsIPrefBranchInternal> prefBranch;
    GetPrefBranch(getter_AddRefs(prefBranch));
    if (prefBranch) {
        prefBranch->AddObserver(PORT_PREF_PREFIX, this, PR_TRUE);
        prefBranch->AddObserver(AUTODIAL_PREF, this, PR_TRUE);
        PrefsChanged(prefBranch);
    }
    
    // Register for profile change notifications
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
        observerService->AddObserver(this, kProfileChangeNetTeardownTopic, PR_TRUE);
        observerService->AddObserver(this, kProfileChangeNetRestoreTopic, PR_TRUE);
        observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);
    }
    else
        NS_WARNING("failed to get observer service");
    
    return NS_OK;
}


nsIOService::~nsIOService()
{
}   

NS_IMPL_THREADSAFE_ISUPPORTS3(nsIOService,
                              nsIIOService,
                              nsIObserver,
                              nsISupportsWeakReference)

////////////////////////////////////////////////////////////////////////////////

nsresult
nsIOService::CacheProtocolHandler(const char *scheme, nsIProtocolHandler *handler)
{
    for (unsigned int i=0; i<NS_N(gScheme); i++)
    {
        if (!nsCRT::strcasecmp(scheme, gScheme[i]))
        {
            nsresult rv;
            NS_ASSERTION(!mWeakHandler[i], "Protocol handler already cached");
            // Make sure the handler supports weak references.
            nsCOMPtr<nsISupportsWeakReference> factoryPtr = do_QueryInterface(handler, &rv);
            if (!factoryPtr)
            {
                // Dont cache handlers that dont support weak reference as
                // there is real danger of a circular reference.
#ifdef DEBUG_dp
                printf("DEBUG: %s protcol handler doesn't support weak ref. Not cached.\n", scheme);
#endif /* DEBUG_dp */
                return NS_ERROR_FAILURE;
            }
            mWeakHandler[i] = do_GetWeakReference(handler);
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
}

nsresult
nsIOService::GetCachedProtocolHandler(const char *scheme, nsIProtocolHandler **result, PRUint32 start, PRUint32 end)
{
    PRUint32 len = end - start - 1;
    for (unsigned int i=0; i<NS_N(gScheme); i++)
    {
        if (!mWeakHandler[i])
            continue;

        // handle unterminated strings
        // start is inclusive, end is exclusive, len = end - start - 1
        if (end ? (!nsCRT::strncasecmp(scheme + start, gScheme[i], len)
                   && gScheme[i][len] == '\0')
                : (!nsCRT::strcasecmp(scheme, gScheme[i])))
        {
            return CallQueryReferent(mWeakHandler[i].get(), result);
        }
    }
    return NS_ERROR_FAILURE;
}
 
NS_IMETHODIMP
nsIOService::GetProtocolHandler(const char* scheme, nsIProtocolHandler* *result)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(scheme);
    // XXX we may want to speed this up by introducing our own protocol 
    // scheme -> protocol handler mapping, avoiding the string manipulation
    // and service manager stuff

    rv = GetCachedProtocolHandler(scheme, result);
    if (NS_SUCCEEDED(rv)) return NS_OK;

    PRBool externalProtocol = PR_FALSE;
    PRBool listedProtocol   = PR_TRUE;
    nsCOMPtr<nsIPrefBranchInternal> prefBranch;
    GetPrefBranch(getter_AddRefs(prefBranch));
    if (prefBranch) {
        nsCAutoString externalProtocolPref("network.protocol-handler.external.");
        externalProtocolPref += scheme;
        rv = prefBranch->GetBoolPref(externalProtocolPref.get(), &externalProtocol);
        if (NS_FAILED(rv)) {
            externalProtocol = PR_FALSE;
            listedProtocol   = PR_FALSE;
        }
    }

    if (!externalProtocol) {
        nsCAutoString contractID(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX);
        contractID += scheme;
        ToLowerCase(contractID);

        rv = CallGetService(contractID.get(), result);
    }
    
    if (externalProtocol || NS_FAILED(rv)) {

      // If the pref for this protocol was explicitly set to false, we want to
      // use our special "blocked protocol" handler.  That will ensure we don't
      // open any channels for this protocol.
      if (listedProtocol && !externalProtocol) {
        rv = CallGetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX"default-blocked",
                            result);
        if (NS_FAILED(rv)) return NS_ERROR_UNKNOWN_PROTOCOL;
      }
      else {
    
#ifdef MOZ_X11
        // check to see whether GnomeVFS can handle this URI scheme.  if it can
        // create a nsIURI for the "scheme:", then we assume it has support for
        // the requested protocol.  otherwise, we failover to using the default
        // protocol handler.

        // XXX should this be generalized into something that searches a
        // category?  (see bug 234714)

        rv = CallGetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX"moz-gnomevfs",
                            result);
        if (NS_SUCCEEDED(rv)) {
          nsCAutoString spec(scheme);
          spec.Append(':');
          nsCOMPtr<nsIURI> uri;
          rv = (*result)->NewURI(spec, nsnull, nsnull, getter_AddRefs(uri));
          if (NS_SUCCEEDED(rv))
            return NS_OK;
          NS_RELEASE(*result);
        }
#endif

        // Okay we don't have a protocol handler to handle this url type, so use
        // the default protocol handler.  This will cause urls to get dispatched
        // out to the OS ('cause we can't do anything with them) when we try to
        // read from a channel created by the default protocol handler.

        rv = CallGetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX"default",
                            result);
        if (NS_FAILED(rv)) return NS_ERROR_UNKNOWN_PROTOCOL;
      }
    }

    CacheProtocolHandler(scheme, *result);

    return NS_OK;
}

NS_IMETHODIMP
nsIOService::ExtractScheme(const nsACString &inURI, nsACString &scheme)
{
    return net_ExtractURLScheme(inURI, nsnull, nsnull, &scheme);
}

NS_IMETHODIMP 
nsIOService::GetProtocolFlags(const char* scheme, PRUint32 *flags)
{
    nsCOMPtr<nsIProtocolHandler> handler;
    nsresult rv = GetProtocolHandler(scheme, getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    rv = handler->GetProtocolFlags(flags);
    return rv;
}

nsresult
nsIOService::NewURI(const nsACString &aSpec, const char *aCharset, nsIURI *aBaseURI, nsIURI **result)
{
    nsresult rv;
    nsCAutoString scheme;

    rv = ExtractScheme(aSpec, scheme);
    if (NS_FAILED(rv)) {
        // then aSpec is relative
        if (!aBaseURI)
            return NS_ERROR_MALFORMED_URI;

        rv = aBaseURI->GetScheme(scheme);
        if (NS_FAILED(rv)) return rv;
    }

    // now get the handler for this scheme
    nsCOMPtr<nsIProtocolHandler> handler;
    rv = GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    return handler->NewURI(aSpec, aCharset, aBaseURI, result);
}


NS_IMETHODIMP 
nsIOService::NewFileURI(nsIFile *file, nsIURI **result)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(file);

    nsCOMPtr<nsIProtocolHandler> handler;

    rv = GetProtocolHandler("file", getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFileProtocolHandler> fileHandler( do_QueryInterface(handler, &rv) );
    if (NS_FAILED(rv)) return rv;
    
    return fileHandler->NewFileURI(file, result);
}

NS_IMETHODIMP
nsIOService::NewChannelFromURI(nsIURI *aURI, nsIChannel **result)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aURI);
    NS_TIMELINE_MARK_URI("nsIOService::NewChannelFromURI(%s)", aURI);

    nsCAutoString scheme;
    rv = aURI->GetScheme(scheme);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIProxyInfo> pi;
    if (mProxyService) {
        rv = mProxyService->ExamineForProxy(aURI, getter_AddRefs(pi));
        if (NS_FAILED(rv))
            pi = 0;
    }

    nsCOMPtr<nsIProtocolHandler> handler;

    if (pi && !strcmp(pi->Type(),"http")) {
        // we are going to proxy this channel using an http proxy
        rv = GetProtocolHandler("http", getter_AddRefs(handler));
        if (NS_FAILED(rv)) return rv;
    } else {
        rv = GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIProxiedProtocolHandler> pph = do_QueryInterface(handler);

    if (pph)
        rv = pph->NewProxiedChannel(aURI, pi, result);
    else
        rv = handler->NewChannel(aURI, result);

    return rv;
}

NS_IMETHODIMP
nsIOService::NewChannel(const nsACString &aSpec, const char *aCharset, nsIURI *aBaseURI, nsIChannel **result)
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    rv = NewURI(aSpec, aCharset, aBaseURI, getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    return NewChannelFromURI(uri, result);
}

NS_IMETHODIMP
nsIOService::GetOffline(PRBool *offline)
{
    *offline = mOffline;
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::SetOffline(PRBool offline)
{
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1");
    
    nsresult rv;
    if (offline) {
        NS_NAMED_LITERAL_STRING(offlineString, "offline");
        mOffline = PR_TRUE; // indicate we're trying to shutdown

        // don't care if notification fails
        // this allows users to attempt a little cleanup before dns and socket transport are shut down.
        if (observerService)
            observerService->NotifyObservers(NS_STATIC_CAST(nsIIOService *, this),
                                             "network:offline-about-to-go-offline",
                                             offlineString.get());

        // be sure to try and shutdown both (even if the first fails)...
        // shutdown dns service first, because it has callbacks for socket transport
        if (mDNSService) {
            rv = mDNSService->Shutdown();
            NS_ASSERTION(NS_SUCCEEDED(rv), "DNS service shutdown failed");
        }
        if (mSocketTransportService) {
            rv = mSocketTransportService->Shutdown();
            NS_ASSERTION(NS_SUCCEEDED(rv), "socket transport service shutdown failed");
        }

        // don't care if notification fails
        if (observerService)
            observerService->NotifyObservers(NS_STATIC_CAST(nsIIOService *, this),
                                             "network:offline-status-changed",
                                             offlineString.get());
    }
    else if (!offline && mOffline) {
        // go online
        if (mDNSService) {
            rv = mDNSService->Init();
            NS_ASSERTION(NS_SUCCEEDED(rv), "DNS service init failed");
        }
        if (mSocketTransportService) {
            rv = mSocketTransportService->Init();
            NS_ASSERTION(NS_SUCCEEDED(rv), "socket transport service init failed");
        }
        mOffline = PR_FALSE;    // indicate success only AFTER we've
                                // brought up the services
 
        // don't care if notification fails
        if (observerService)
            observerService->NotifyObservers(NS_STATIC_CAST(nsIIOService *, this),
                                             "network:offline-status-changed",
                                             NS_LITERAL_STRING("online").get());
    }
    return NS_OK;
}


NS_IMETHODIMP
nsIOService::AllowPort(PRInt32 inPort, const char *scheme, PRBool *_retval)
{
    PRInt16 port = inPort;
    if (port == -1) {
        *_retval = PR_TRUE;
        return NS_OK;
    }
        
    // first check to see if the port is in our blacklist:
    PRInt32 badPortListCnt = mRestrictedPortList.Count();
    for (int i=0; i<badPortListCnt; i++)
    {
        if (port == (PRInt32) NS_PTR_TO_INT32(mRestrictedPortList[i]))
        {
            *_retval = PR_FALSE;

            // check to see if the protocol wants to override
            if (!scheme)
                return NS_OK;
            
            nsCOMPtr<nsIProtocolHandler> handler;
            nsresult rv = GetProtocolHandler(scheme, getter_AddRefs(handler));
            if (NS_FAILED(rv)) return rv;

            // let the protocol handler decide
            return handler->AllowPort(port, scheme, _retval);
        }
    }

    *_retval = PR_TRUE;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

void
nsIOService::PrefsChanged(nsIPrefBranch *prefs, const char *pref)
{
    if (!prefs) return;

    // Look for extra ports to block
    if (!pref || strcmp(pref, PORT_PREF("banned")) == 0)
        ParsePortList(prefs, PORT_PREF("banned"), PR_FALSE);

    // ...as well as previous blocks to remove.
    if (!pref || strcmp(pref, PORT_PREF("banned.override")) == 0)
        ParsePortList(prefs, PORT_PREF("banned.override"), PR_TRUE);

    if (!pref || strcmp(pref, AUTODIAL_PREF) == 0) {
        PRBool enableAutodial = PR_FALSE;
        nsresult rv = prefs->GetBoolPref(AUTODIAL_PREF, &enableAutodial);
        // If pref not found, default to disabled.
        if (NS_SUCCEEDED(rv)) {
            if (mSocketTransportService)
                mSocketTransportService->SetAutodialEnabled(enableAutodial);
        }
    }
}

void
nsIOService::ParsePortList(nsIPrefBranch *prefBranch, const char *pref, PRBool remove)
{
    nsXPIDLCString portList;

    // Get a pref string and chop it up into a list of ports.
    prefBranch->GetCharPref(pref, getter_Copies(portList));
    if (portList) {
        nsCStringArray portListArray;
        portListArray.ParseString(portList.get(), ",");
        PRInt32 index;
        for (index=0; index < portListArray.Count(); index++) {
            portListArray[index]->StripWhitespace();
            PRInt32 aErrorCode, portBegin, portEnd;

            if (PR_sscanf(portListArray[index]->get(), "%d-%d", &portBegin, &portEnd) == 2) {
               if ((portBegin < 65536) && (portEnd < 65536)) {
                   PRInt32 curPort;
                   if (remove) {
                        for (curPort=portBegin; curPort <= portEnd; curPort++)
                            mRestrictedPortList.RemoveElement((void*)curPort);
                   } else {
                        for (curPort=portBegin; curPort <= portEnd; curPort++)
                            mRestrictedPortList.AppendElement((void*)curPort);
                   }
               }
            } else {
               PRInt32 port = portListArray[index]->ToInteger(&aErrorCode);
               if (NS_SUCCEEDED(aErrorCode) && port < 65536) {
                   if (remove)
                       mRestrictedPortList.RemoveElement((void*)port);
                   else
                       mRestrictedPortList.AppendElement((void*)port);
               }
            }

        }
    }
}

void
nsIOService::GetPrefBranch(nsIPrefBranchInternal **result)
{
    *result = nsnull;
    CallGetService(NS_PREFSERVICE_CONTRACTID, result);
}

// nsIObserver interface
NS_IMETHODIMP
nsIOService::Observe(nsISupports *subject,
                     const char *topic,
                     const PRUnichar *data)
{
    if (!strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
        nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(subject);
        if (prefBranch)
            PrefsChanged(prefBranch, NS_ConvertUCS2toUTF8(data).get());
    }
    else if (!strcmp(topic, kProfileChangeNetTeardownTopic)) {
        if (!mOffline) {
            SetOffline(PR_TRUE);
            mOfflineForProfileChange = PR_TRUE;
        }
    }
    else if (!strcmp(topic, kProfileChangeNetRestoreTopic)) {
        if (mOfflineForProfileChange) {
            SetOffline(PR_FALSE);
            mOfflineForProfileChange = PR_FALSE;
        }    
    }
    else if (!strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
        SetOffline(PR_TRUE);

        // Break circular reference.
        mProxyService = 0;
    }
    return NS_OK;
}
