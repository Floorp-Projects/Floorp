/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsIOService.h"
#include "nsIProtocolHandler.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIFileTransportService.h"
#include "nsIURI.h"
#include "nsIStreamListener.h"
#include "prprf.h"
#include "nsLoadGroup.h"
#include "nsInputStreamChannel.h"
#include "nsXPIDLString.h" 
#include "nsIErrorService.h" 
#include "netCore.h"
#include "nsIObserverService.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIPref.h"
#include "nsICategoryManager.h"
#include "nsIURLParser.h"
#include "nsISupportsPrimitives.h"
#include "nsIProxiedProtocolHandler.h"
#include "nsIProxyInfo.h"
#include "nsITimelineService.h"
#include "nsEscape.h"

static NS_DEFINE_CID(kFileTransportService, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);
static NS_DEFINE_CID(kErrorServiceCID, NS_ERRORSERVICE_CID);
static NS_DEFINE_CID(kProtocolProxyServiceCID, NS_PROTOCOLPROXYSERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kStdURLParserCID, NS_STANDARDURLPARSER_CID);

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
  1080, // SOCKS
  2049, // nfs
  4045, // lockd
  6000, // x11        
  0,    // This MUST be zero so that we can populating the array
};

////////////////////////////////////////////////////////////////////////////////

nsIOService::nsIOService()
    : mOffline(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsresult
nsIOService::Init()
{
    nsresult rv = NS_OK;
    
    // Hold onto the eventQueue service.  We do not want any eventqueues to go away
    // when we shutdown until we process all remaining transports

    if (NS_SUCCEEDED(rv))
        mEventQueueService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
  
    
    // We need to get references to these services so that we can shut them
    // down later. If we wait until the nsIOService is being shut down,
    // GetService will fail at that point.
    rv = nsServiceManager::GetService(kSocketTransportServiceCID,
                                      NS_GET_IID(nsISocketTransportService),
                                      getter_AddRefs(mSocketTransportService));

    if (NS_FAILED(rv)) return rv;

    rv = nsServiceManager::GetService(kFileTransportService,
                                      NS_GET_IID(nsIFileTransportService),
                                      getter_AddRefs(mFileTransportService));
    if (NS_FAILED(rv)) return rv;

    rv = nsServiceManager::GetService(kDNSServiceCID,
                                      NS_GET_IID(nsIDNSService),
                                      getter_AddRefs(mDNSService));
    if (NS_FAILED(rv)) return rv;

    rv = nsServiceManager::GetService(kProtocolProxyServiceCID,
                                      NS_GET_IID(nsIProtocolProxyService),
                                      getter_AddRefs(mProxyService));
    if (NS_FAILED(rv)) return rv;

    // XXX hack until xpidl supports error info directly (http://bugzilla.mozilla.org/show_bug.cgi?id=13423)
    nsCOMPtr<nsIErrorService> errorService = do_GetService(kErrorServiceCID, &rv);
    if (NS_FAILED(rv)) 
        return rv;
    
    rv = errorService->RegisterErrorStringBundle(NS_ERROR_MODULE_NETWORK, NECKO_MSGS_URL);
    if (NS_FAILED(rv)) return rv;
    rv = errorService->RegisterErrorStringBundleKey(NS_NET_STATUS_READ_FROM, "ReadFrom");
    if (NS_FAILED(rv)) return rv;
    rv = errorService->RegisterErrorStringBundleKey(NS_NET_STATUS_WROTE_TO, "WroteTo");
    if (NS_FAILED(rv)) return rv;
    rv = errorService->RegisterErrorStringBundleKey(NS_NET_STATUS_RESOLVING_HOST, "ResolvingHost");
    if (NS_FAILED(rv)) return rv;
    rv = errorService->RegisterErrorStringBundleKey(NS_NET_STATUS_CONNECTED_TO, "ConnectedTo");
    if (NS_FAILED(rv)) return rv;
    rv = errorService->RegisterErrorStringBundleKey(NS_NET_STATUS_SENDING_TO, "SendingTo");
    if (NS_FAILED(rv)) return rv;
    rv = errorService->RegisterErrorStringBundleKey(NS_NET_STATUS_RECEIVING_FROM, "ReceivingFrom");
    if (NS_FAILED(rv)) return rv;
    rv = errorService->RegisterErrorStringBundleKey(NS_NET_STATUS_CONNECTING_TO, "ConnectingTo");
    if (NS_FAILED(rv)) return rv;
    
    // setup our bad port list stuff
    for(int i=0; gBadPortList[i]; i++)
    {
        mRestrictedPortList.AppendElement((void*)gBadPortList[i]);
    }

    // Lets make it really easy to block extra ports:
    nsCOMPtr<nsIPref> prefService(do_GetService(kPrefServiceCID, &rv));
    if (NS_FAILED(rv) && !prefService) {
        NS_ASSERTION(0, "Prefs not found!");
        return NS_ERROR_FAILURE;
    }
    
    char* portList = nsnull;
    prefService->CopyCharPref("network.security.ports.banned", &portList);
    if (portList) {
        char* tokp;
        char* currentPos = portList;  
        while ( (tokp = nsCRT::strtok(currentPos, ",", &currentPos)) != nsnull )
        {
            nsCAutoString tmp(tokp);
            tmp.StripWhitespace();

            PRInt32 aErrorCode;
            PRInt32 value = tmp.ToInteger(&aErrorCode);
            mRestrictedPortList.AppendElement((void*)value);
        }

        PL_strfree(portList);
    }
    
    portList = nsnull;
    prefService->CopyCharPref("network.security.ports.banned.override", &portList);
    if (portList) {
        char* tokp;
        char* currentPos = portList;  
        while ( (tokp = nsCRT::strtok(currentPos, ",", &currentPos)) != nsnull )
        {
            nsCAutoString tmp(tokp);
            tmp.StripWhitespace();

            PRInt32 aErrorCode;
            PRInt32 value = tmp.ToInteger(&aErrorCode);
            mRestrictedPortList.RemoveElement((void*)value);
        }

        PL_strfree(portList);
    }

    return NS_OK;
}


nsIOService::~nsIOService()
{
    // mURLParsers is a voidarray; we must release ourselves
    for (PRInt32 i = 0; i < mURLParsers.Count(); i++)
    {
        nsISupports *temp;
        temp = NS_STATIC_CAST(nsISupports*, mURLParsers[i]);
        NS_IF_RELEASE(temp);
    }
    (void)SetOffline(PR_TRUE);
    if (mFileTransportService)
        (void)mFileTransportService->Shutdown();
}   

NS_METHOD
nsIOService::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    static  nsISupports *_rValue = nsnull;

    nsresult rv;
    NS_ENSURE_NO_AGGREGATION(aOuter);

    if (_rValue)
    {
        NS_ADDREF (_rValue);
        *aResult = _rValue;
        return NS_OK;
    }

    nsIOService* _ios = new nsIOService();
    if (_ios == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(_ios);
    rv = _ios->Init();
    if (NS_FAILED(rv))
    {
        delete _ios;
        return rv;
    }

    rv = _ios->QueryInterface(aIID, aResult);

    if (NS_FAILED(rv))
    {
        delete _ios;
        return rv;
    }
    
    _rValue = NS_STATIC_CAST (nsISupports*, *aResult);
    NS_RELEASE (_rValue);
    _rValue = nsnull;

    return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsIOService, nsIIOService);

////////////////////////////////////////////////////////////////////////////////

#define MAX_SCHEME_LENGTH       64      // XXX big enough?

#define MAX_NET_CONTRACTID_LENGTH   (MAX_SCHEME_LENGTH + NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX_LENGTH + 1)

NS_IMETHODIMP
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
            mWeakHandler[i] = getter_AddRefs(NS_GetWeakReference(handler));
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsIOService::GetCachedProtocolHandler(const char *scheme, nsIProtocolHandler **result, PRUint32 start, PRUint32 end)
{
    for (unsigned int i=0; i<NS_N(gScheme); i++)
    {
        // handle unterminated strings
        // start is inclusive, end is exclusive, len = end - start - 1
        if (end != 0 ? !nsCRT::strncasecmp(&scheme[start], gScheme[i],
                                           (end-start)-1) :
                       !nsCRT::strcasecmp(scheme, gScheme[i]))
        {
            if (mWeakHandler[i])
            {
                nsCOMPtr<nsIProtocolHandler> temp = do_QueryReferent(mWeakHandler[i]);
                if (temp)
                {
                    *result = temp.get();
                    NS_ADDREF(*result);
                    return NS_OK;
                }
            }
        }
    }
    return NS_ERROR_FAILURE;
}
 
NS_IMETHODIMP
nsIOService::CacheURLParser(const char *scheme, nsIURLParser *parser)
{
    NS_ENSURE_ARG_POINTER(scheme);
    NS_ENSURE_ARG_POINTER(parser);
    for (unsigned int i=0; i<NS_N(gScheme); i++)
    {
        if (!nsCRT::strcasecmp(scheme, gScheme[i]))
        {
            // we're going to store this in an nsVoidArray, which extends,
            // unlike nsSupportsArray, which doesn't.  We must release
            // them on delete!
            // grab this before overwriting it
            nsIURLParser *old_parser = NS_STATIC_CAST(nsIURLParser*,
                                                      mURLParsers[i]);
            NS_ADDREF(parser);
            mURLParsers.ReplaceElementAt(parser, i);
            // release any old entry, if any, AFTER adding new entry in
            // case they were the same (paranoia)
            NS_IF_RELEASE(old_parser);
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsIOService::GetCachedURLParser(const char *scheme, nsIURLParser **result)
{
    for (unsigned int i=0; i<NS_N(gScheme); i++)
    {
        if (!nsCRT::strcasecmp(scheme, gScheme[i]))
        {
            *result = NS_STATIC_CAST(nsIURLParser*, mURLParsers[i]);
            NS_IF_ADDREF(*result);
            return *result ? NS_OK : NS_ERROR_FAILURE;
        }
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsIOService::GetProtocolHandler(const char* scheme, nsIProtocolHandler* *result)
{
    nsresult rv;

    NS_ASSERTION(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX_LENGTH
                 == nsCRT::strlen(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX),
                 "need to fix NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX_LENGTH");

    NS_ENSURE_ARG_POINTER(scheme);
    // XXX we may want to speed this up by introducing our own protocol 
    // scheme -> protocol handler mapping, avoiding the string manipulation
    // and service manager stuff

    rv = GetCachedProtocolHandler(scheme, result);
    if (NS_SUCCEEDED(rv)) return NS_OK;

    char buf[MAX_NET_CONTRACTID_LENGTH];
    nsCAutoString contractID(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX);
    contractID += scheme;
    contractID.ToLowerCase();
    contractID.ToCString(buf, MAX_NET_CONTRACTID_LENGTH);

    rv = nsServiceManager::GetService(buf, NS_GET_IID(nsIProtocolHandler), (nsISupports **)result);
    if (NS_FAILED(rv)) 
    {
      // okay we don't have a protocol handler to handle this url type, so use the default protocol handler.
      // this will cause urls to get dispatched out to the OS ('cause we can't do anything with them) when 
      // we try to read from a channel created by the default protocol handler.

      rv = nsServiceManager::GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX"default", NS_GET_IID(nsIProtocolHandler), (nsISupports **)result);
      if (NS_FAILED(rv)) return NS_ERROR_UNKNOWN_PROTOCOL;
    }

    CacheProtocolHandler(scheme, *result);

    return NS_OK;
}

NS_IMETHODIMP
nsIOService::ExtractScheme(const char* inURI, PRUint32 *startPos, 
                           PRUint32 *endPos, char* *scheme)
{
    return ExtractURLScheme(inURI, startPos, endPos, scheme);
}

/* nsIURLParser getParserForScheme (in string scheme); */
NS_IMETHODIMP 
nsIOService::GetParserForScheme(const char *scheme, nsIURLParser **_retval)
{
    nsresult rv;
    
    rv = GetCachedURLParser(scheme, _retval);
    if (NS_SUCCEEDED(rv) && *_retval) return NS_OK;


    if (!scheme) {
        if (!mDefaultURLParser) {
            rv = nsServiceManager::GetService(kStdURLParserCID, 
                                              NS_GET_IID(nsIURLParser), 
                                              getter_AddRefs(mDefaultURLParser));
            if (NS_FAILED(rv)) return rv;
        }
        
        *_retval = mDefaultURLParser;
        NS_ADDREF(*_retval);
        return NS_OK;
    }

    nsCOMPtr<nsICategoryManager> catmgr(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISimpleEnumerator> parserList;
    rv = catmgr->EnumerateCategory(NS_IURLPARSER_KEY, getter_AddRefs(parserList));
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsISupportsString> entry;

    // Walk the list of parsers...
    while (1) {
        rv = parserList->GetNext(getter_AddRefs(entry));
        if (NS_FAILED(rv)) break;

        // get the entry string
        nsXPIDLCString entryString;
        rv = entry->GetData(getter_Copies(entryString));
        if (NS_FAILED(rv)) break;

        if (nsCRT::strcmp(entryString, scheme) == 0) {
            nsXPIDLCString contractID;
            rv = catmgr->GetCategoryEntry(NS_IURLPARSER_KEY,(const char *)entryString, getter_Copies(contractID));
            if (NS_FAILED(rv)) break;

            rv = nsServiceManager::GetService(contractID, NS_GET_IID(nsIURLParser), (nsISupports **)_retval);
            if (NS_FAILED(rv))
                return rv;
            if (*_retval)
                CacheURLParser(scheme, *_retval);
            return *_retval ? NS_OK : NS_ERROR_FAILURE;
        }
    }

    // no registered url parser. Just use the default...
    if (!mDefaultURLParser) {
        rv = nsServiceManager::GetService(kStdURLParserCID, 
                                          NS_GET_IID(nsIURLParser), 
                                          getter_AddRefs(mDefaultURLParser));
        if (NS_FAILED(rv)) return rv;
    }

    *_retval = mDefaultURLParser;
    NS_ADDREF(*_retval);
    CacheURLParser(scheme, *_retval);
    return NS_OK;
}

static void CalculateStartEndPos(const char*string, const char* substring, PRUint32 *startPos, PRUint32 *endPos)
{
    // we will only get the first appearance of a substring...
    char* inst = PL_strstr(string, substring);    

    if (startPos)
        *startPos = (PRUint32)(inst - string);
    if (endPos)
        *endPos   = NS_PTR_TO_INT32(inst) + PL_strlen(substring);
}

// Crap.  How do I ensure that startPos and endPos are correct.
NS_IMETHODIMP 
nsIOService::ExtractUrlPart(const char *urlString, PRInt16 flag, PRUint32 *startPos, PRUint32 *endPos, char **urlPart)
{
    nsresult rv;
    nsXPIDLCString scheme;

    ExtractScheme(urlString, startPos, endPos, getter_Copies(scheme));

    if (flag == url_Scheme) {
        CalculateStartEndPos(urlString, scheme, startPos, endPos);

        if (urlPart)
            *urlPart = nsCRT::strdup(scheme.get());
        return NS_OK;
    }

    nsCOMPtr<nsIURLParser> parser;
    rv = GetParserForScheme(scheme, getter_AddRefs(parser));
    if (NS_FAILED(rv)) return rv;
    
    PRInt32 port;
    nsXPIDLCString dummyScheme, username, password, host, path;

    rv = parser->ParseAtScheme(urlString, 
                               getter_Copies(dummyScheme), 
                               getter_Copies(username),
                               getter_Copies(password),
                               getter_Copies(host), 
                               &port, 
                               getter_Copies(path));
    
    if (NS_FAILED(rv)) return rv;
    
    if (flag == url_Username) {
        CalculateStartEndPos(urlString, username, startPos, endPos);
        if (urlPart)
            *urlPart = nsCRT::strdup(username.get());
        return NS_OK;
    }

    if (flag == url_Password) {
        CalculateStartEndPos(urlString, password, startPos, endPos);
        if (urlPart)
            *urlPart = nsCRT::strdup(password.get());
        return NS_OK;
    }

    if (flag == url_Host) {
        CalculateStartEndPos(urlString, host, startPos, endPos);
        if (urlPart)
            *urlPart = nsCRT::strdup(host.get());
        return NS_OK;
    }

    if (flag == url_Path) {
        CalculateStartEndPos(urlString, path, startPos, endPos);
        if (urlPart)
            *urlPart = nsCRT::strdup(path.get());
        return NS_OK;
    }


    nsXPIDLCString directory, fileBaseName, fileExtension, param, query, ref;

    rv = parser->ParseAtDirectory(path, 
                                  getter_Copies(directory), 
                                  getter_Copies(fileBaseName),
                                  getter_Copies(fileExtension),
                                  getter_Copies(param), 
                                  getter_Copies(query), 
                                  getter_Copies(ref));

    if (NS_FAILED(rv)) return rv;
    
    
    if (flag == url_Directory) {
        CalculateStartEndPos(urlString, directory, startPos, endPos);
        if (urlPart)
            *urlPart = nsCRT::strdup(directory.get());
        return NS_OK;
    }

    if (flag == url_FileBaseName) {
        CalculateStartEndPos(urlString, fileBaseName, startPos, endPos);
        if (urlPart)
            *urlPart = nsCRT::strdup(fileBaseName.get());
        return NS_OK;
    }
    
    if (flag == url_FileExtension) {
        CalculateStartEndPos(urlString, fileExtension, startPos, endPos);
        if (urlPart)
            *urlPart = nsCRT::strdup(fileExtension.get());
        return NS_OK;
    }
    
    if (flag == url_Param) {
        CalculateStartEndPos(urlString, param, startPos, endPos);
        if (urlPart)
            *urlPart = nsCRT::strdup(param.get());
        return NS_OK;
    }
    
    if (flag == url_Query) {
        CalculateStartEndPos(urlString, query, startPos, endPos);
        if (urlPart)
            *urlPart = nsCRT::strdup(query.get());
        return NS_OK;
    }

    if (flag == url_Ref) {
        CalculateStartEndPos(urlString, ref, startPos, endPos);
        if (urlPart)
            *urlPart = nsCRT::strdup(ref.get());
        return NS_OK;
    }
    
    
    return NS_OK;
}

NS_IMETHODIMP 
nsIOService::GetProtocolFlags(const char* scheme, PRUint32 *flags)
{
    nsCOMPtr<nsIProtocolHandler> handler;
    nsresult rv = GetProtocolHandler(scheme, getter_AddRefs(handler));
    if (NS_FAILED(rv)) {
        return rv;
    }
    rv = handler->GetProtocolFlags(flags);
    return rv;
}


nsresult
nsIOService::NewURI(const char* aSpec, nsIURI* aBaseURI, nsIURI* *result)
{
    nsresult rv;
    nsIURI* base;
    NS_ENSURE_ARG_POINTER(aSpec);
    nsXPIDLCString scheme;
    nsCOMPtr<nsIProtocolHandler> handler;
    PRUint32 start,end;

    rv = ExtractScheme(aSpec, &start, &end, nsnull);
    if (NS_SUCCEEDED(rv))
    {
        // then aSpec is absolute
        // ignore aBaseURI in this case
        base = nsnull;
        rv = GetCachedProtocolHandler(aSpec, getter_AddRefs(handler),
                                      start,end);
        if (NS_FAILED(rv))
        {
            // not cached; we'll do an allocation
            rv = ExtractScheme(aSpec, nsnull, nsnull, getter_Copies(scheme));
        }
        // else scheme == nsnull, and we succeeded
    }
    else
    {
        // then aSpec is relative
        if (!aBaseURI)
            return NS_ERROR_MALFORMED_URI;
        rv = aBaseURI->GetScheme(getter_Copies(scheme));
        base = aBaseURI;
    }
    // scheme is non-null if we allocated it.
    // base is null if absolute, set if relative
    // rv is success if all is ok, failure if we need to cleanup and exit
    if (NS_SUCCEEDED(rv) && scheme.get())
    {
        // we allocated scheme; get the handler for it
        rv = GetProtocolHandler(scheme, getter_AddRefs(handler));
    }
    if (NS_FAILED(rv)) return rv;

    return handler->NewURI(aSpec, base, result);
}


NS_IMETHODIMP 
nsIOService::NewFileURI(nsIFile *aSpec, nsIURI **_retval)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aSpec);
    
    nsXPIDLCString urlString;
    rv = aSpec->GetURL(getter_Copies(urlString));
    if (NS_FAILED(rv)) return rv;

    return NewURI(urlString, nsnull, _retval);
}

NS_IMETHODIMP
nsIOService::NewChannelFromURI(nsIURI *aURI, nsIChannel **result)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aURI);
    NS_TIMELINE_MARK_URI("nsIOService::NewChannelFromURI(%s)", aURI);

    nsXPIDLCString scheme;
    rv = aURI->GetScheme(getter_Copies(scheme));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIProxyInfo> pi;
    rv = mProxyService->ExamineForProxy(aURI, getter_AddRefs(pi));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIProtocolHandler> handler;

    if (pi && !nsCRT::strcmp(pi->Type(),"http")) {
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
nsIOService::NewChannel(const char *aSpec, nsIURI *aBaseURI, nsIChannel **result)
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    rv = NewURI(aSpec, aBaseURI, getter_AddRefs(uri));
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
    nsCOMPtr<nsIObserverService>
        observerService(do_GetService(NS_OBSERVERSERVICE_CONTRACTID));
    
    nsresult rv1 = NS_OK;
    nsresult rv2 = NS_OK;
    if (offline) {
        // don't care if notification fails
        if (observerService)
            (void)observerService->Notify(this,
                                          NS_LITERAL_STRING("network:offline-status-changed").get(),
                                          NS_LITERAL_STRING("offline").get());
    	mOffline = PR_TRUE;		// indicate we're trying to shutdown
        // be sure to try and shutdown both (even if the first fails)
        if (mDNSService)
            rv1 = mDNSService->Shutdown();  // shutdown dns service first, because it has callbacks for socket transport
        if (mSocketTransportService)
            rv2 = mSocketTransportService->Shutdown();
        if (NS_FAILED(rv1)) return rv1;
        if (NS_FAILED(rv2)) return rv2;

    }
    else if (!offline && mOffline) {
        // go online
        if (mDNSService)
            rv1 = mDNSService->Init();
        if (NS_FAILED(rv2)) return rv1;

        if (mSocketTransportService)
            rv2 = mSocketTransportService->Init();		//XXX should we shutdown the dns service?
        if (NS_FAILED(rv2)) return rv1;        
        mOffline = PR_FALSE;    // indicate success only AFTER we've
                                // brought up the services
        // don't care if notification fails
        if (observerService)
            (void)observerService->Notify(this,
                                          NS_LITERAL_STRING("network:offline-status-changed").get(),
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
// URL parsing utilities

NS_IMETHODIMP
nsIOService::ExtractPort(const char *str, PRInt32 *result)
{
    PRInt32 returnValue = -1;
    *result = (0 < PR_sscanf(str, "%d", &returnValue)) ? returnValue : -1;
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::ResolveRelativePath(const char *relativePath, const char* basePath,
                                 char **result)
{
    nsCAutoString name;
    nsCAutoString path(basePath);
	PRBool needsDelim = PR_FALSE;

	if ( !path.IsEmpty() ) {
		PRUnichar last = path.Last();
		needsDelim = !(last == '/' || last == '\\' );
	}

    PRBool end = PR_FALSE;
    char c;
    while (!end) {
        c = *relativePath++;
        switch (c) {
          case '\0':
          case '#':
          case ';':
          case '?':
            end = PR_TRUE;
            // fall through...
          case '/':
          case '\\':
            // delimiter found
            if (name.Equals("..")) {
                // pop path
                // If we already have the delim at end, then
                //  skip over that when searching for next one to the left
                PRInt32 offset = path.Length() - (needsDelim ? 1 : 2);
                PRInt32 pos = path.RFind("/", PR_FALSE, offset);
                if (pos > 0) {
                    path.Truncate(pos + 1);
                }
                else {
                    return NS_ERROR_MALFORMED_URI;
                }
            }
            else if (name.Equals(".") || name.Equals("")) {
                // do nothing
            }
            else {
                // append name to path
                if (needsDelim)
                    path += "/";
                path += name;
                needsDelim = PR_TRUE;
            }
            name = "";
            break;

          default:
            // append char to name
            name += c;
        }
    }
    // append anything left on relativePath (e.g. #..., ;..., ?...)
    if (c != '\0')
        path += --relativePath;

    *result = path.ToNewCString();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
