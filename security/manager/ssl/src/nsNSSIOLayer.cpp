/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@netscape.com>
 *  Javier Delgadillo <javi@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "nsNSSComponent.h"
#include "nsNSSIOLayer.h"
#include "nsNSSCallbacks.h"

#include "prlog.h"
#include "prnetdb.h"
#include "nsIPrompt.h"
#include "nsIPref.h"
#include "nsISecurityManagerComponent.h"
#include "nsIServiceManager.h"
#include "nsIWebProgressListener.h"
#include "nsIChannel.h"
#include "nsIBadCertListener.h"
#include "nsNSSCertificate.h"
#include "nsINSSDialogs.h"
#include "nsIProxyObjectManager.h"
#include "nsProxiedService.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"

#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"

#include "ssl.h"
#include "secerr.h"
#include "sslerr.h"
#include "secder.h"
#include "secasn1.h"
#include "genname.h"
#include "certdb.h"
#include "cert.h"


//#define DEBUG_SSL_VERBOSE //Enable this define to get minimal 
                            //reports when doing SSL read/write
                            
//#define DUMP_BUFFER  //Enable this define along with
                       //DEBUG_SSL_VERBOSE to dump SSL
                       //read/write buffer to a log.
                       //Uses PR_LOG except on Mac where
                       //we always write out to our own
                       //file.

#define HASH_STRING_KEY(buf,size,host,port) PR_snprintf((buf),(size),"%s:%d",(host),(port))


/* SSM_UserCertChoice: enum for cert choice info */
typedef enum {ASK, AUTO} SSM_UserCertChoice;


/* strings for marking invalid user cert nicknames */
#define NICKNAME_EXPIRED_STRING " (expired)"
#define NICKNAME_NOT_YET_VALID_STRING " (not yet valid)"

static SECStatus PR_CALLBACK
nsNSS_SSLGetClientAuthData(void *arg, PRFileDesc *socket,
						   CERTDistNames *caNames,
						   CERTCertificate **pRetCert,
						   SECKEYPrivateKey **pRetKey);
static SECStatus PR_CALLBACK
nsNSS_SSLGetClientAuthData(void *arg, PRFileDesc *socket,
						   CERTDistNames *caNames,
						   CERTCertificate **pRetCert,
						   SECKEYPrivateKey **pRetKey);
static nsISecurityManagerComponent* gNSSService = nsnull;
static PRBool firstTime = PR_TRUE;
static PRDescIdentity nsSSLIOLayerIdentity;
static PRIOMethods nsSSLIOLayerMethods;
static nsHashtable *gTLSIntolerantSites = nsnull;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

#if defined(DEBUG_SSL_VERBOSE) && defined (XP_MAC)

#ifdef PR_LOG
#undef PR_LOG
#endif

static PRFileDesc *gMyLogFile = nsnull;
#define MAC_LOG_FILE "MAC PIPNSS Log File"

void MyLogFunction(const char *fmt, ...)
{
  
  va_list ap;
  va_start(ap,fmt);
  if (gMyLogFile == nsnull)
    gMyLogFile = PR_Open(MAC_LOG_FILE, PR_WRONLY | PR_CREATE_FILE | PR_APPEND,
                         0600);
  if (!gMyLogFile)
      return;
  PR_vfprintf(gMyLogFile, fmt, ap);
  va_end(ap);
}

#define PR_LOG(module,level,args) MyLogFunction args
#endif

static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);

static PRFileDesc*
nsSSLIOLayerImportFD(PRFileDesc *fd,
                     nsNSSSocketInfo *infoObject,
                     const char *host);
static nsresult
nsSSLIOLayerSetOptions(PRFileDesc *fd, PRBool forTLSStepUp, 
                       const char *proxyHost, const char *host, PRInt32 port,
                       nsNSSSocketInfo *infoObject);


nsNSSSocketInfo::nsNSSSocketInfo()
  : mFd(nsnull),
    mSecurityState(nsIWebProgressListener::STATE_IS_INSECURE),
    mForceHandshake(PR_FALSE),
    mForTLSStepUp(PR_FALSE),
    mFirstWrite(PR_TRUE),
    mTLSIntolerant(PR_FALSE),
    mPort(0)
{ 
  NS_INIT_ISUPPORTS();
}

nsNSSSocketInfo::~nsNSSSocketInfo()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS4(nsNSSSocketInfo,
                              nsITransportSecurityInfo,
                              nsISSLSocketControl,
                              nsIInterfaceRequestor,
                              nsISSLStatusProvider)

nsresult
nsNSSSocketInfo::GetFirstWrite(PRBool *aFirstWrite)
{
  *aFirstWrite = mFirstWrite;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetFirstWrite(PRBool aFirstWrite)
{
  mFirstWrite = aFirstWrite;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetHostName(const char* host)
{
  mHostName.Adopt(host ? nsCRT::strdup(host) : 0);
  return NS_OK;
}

nsresult
nsNSSSocketInfo::GetHostName(char **host)
{
  *host = (mHostName) ? nsCRT::strdup(mHostName) : nsnull;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetPort(PRInt32 aPort)
{
  mPort = aPort;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::GetPort(PRInt32 *aPort)
{
  *aPort = mPort;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::GetTLSIntolerant(PRBool *aTLSIntolerant)
{
  *aTLSIntolerant = mTLSIntolerant;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetTLSIntolerant(PRBool aTLSIntolerant)
{
  mTLSIntolerant = aTLSIntolerant;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetNotificationCallbacks(nsIInterfaceRequestor** aCallbacks)
{
  *aCallbacks = mCallbacks;
  NS_IF_ADDREF(*aCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks)
{
  mCallbacks = aCallbacks;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetSecurityState(PRInt32* state)
{
  *state = mSecurityState;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetSecurityState(PRInt32 aState)
{
  mSecurityState = aState;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetShortSecurityDescription(PRUnichar** aText) {
  if (mShortDesc.IsEmpty())
    *aText = nsnull;
  else
    *aText = ToNewUnicode(mShortDesc);
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetShortSecurityDescription(const PRUnichar* aText) {
  mShortDesc.Assign(aText);
  return NS_OK;
}

/* void getInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP nsNSSSocketInfo::GetInterface(const nsIID & uuid, void * *result)
{
  nsresult rv;
  if (!mCallbacks) {
    nsCOMPtr<nsIInterfaceRequestor> ir = new PipUIContext();
    if (!ir)
      return NS_ERROR_OUT_OF_MEMORY;

    rv = ir->GetInterface(uuid, result);
  } else {
    // Proxy of the channel callbacks should probably go here, rather
    // than in the password callback code

    rv = mCallbacks->GetInterface(uuid, result);
  }
  return rv;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetForceHandshake(PRBool* forceHandshake)
{
  *forceHandshake = mForceHandshake;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::SetForceHandshake(PRBool forceHandshake)
{
  mForceHandshake = forceHandshake;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::GetForTLSStepUp(PRBool* aResult)
{
  *aResult = mForTLSStepUp;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetForTLSStepUp(PRBool forTLSStepUp)
{
  mForTLSStepUp = forTLSStepUp;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::ProxyStepUp()
{
  return TLSStepUp();
}

NS_IMETHODIMP
nsNSSSocketInfo::TLSStepUp()
{
  if (SECSuccess != SSL_OptionSet(mFd, SSL_SECURITY, PR_TRUE))
    return NS_ERROR_FAILURE;

  if (SECSuccess != SSL_ResetHandshake(mFd, PR_FALSE))
    return NS_ERROR_FAILURE;

  // This is a work around for NSS bug 56924 which is scheduled
  // for a fix in NSS 3.3, but we're currently on version 3.2.1,
  // so we need this work around.
  PR_Write(mFd, nsnull, 0);

  mFirstWrite = PR_TRUE;

  return NS_OK;
}

nsresult nsNSSSocketInfo::GetFileDescPtr(PRFileDesc** aFilePtr)
{
  *aFilePtr = mFd;
  return NS_OK;
}

nsresult nsNSSSocketInfo::SetFileDescPtr(PRFileDesc* aFilePtr)
{
  mFd = aFilePtr;
  return NS_OK;
}

nsresult nsNSSSocketInfo::GetSSLStatus(nsISSLStatus** _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");

  *_result = mSSLStatus;
  NS_IF_ADDREF(*_result);

  return NS_OK;
}

nsresult nsNSSSocketInfo::SetSSLStatus(nsISSLStatus *aSSLStatus)
{
  mSSLStatus = aSSLStatus;

  return NS_OK;
}

nsresult
nsSSLIOLayerFreeTLSIntolerantSites()
{
  if (gTLSIntolerantSites) {
    delete gTLSIntolerantSites;
    gTLSIntolerantSites = nsnull;
  }
  return NS_OK;
}

nsresult
displayAlert(nsXPIDLString formattedString, nsNSSSocketInfo *infoObject)
{
	
	// The interface requestor object may not be safe, so
    // proxy the call to get the nsIPrompt.

     nsCOMPtr<nsIProxyObjectManager> proxyman(do_GetService(NS_XPCOMPROXY_CONTRACTID));
     if (!proxyman) 
       return NS_ERROR_FAILURE;
 
     nsCOMPtr<nsIInterfaceRequestor> proxiedCallbacks;
     proxyman->GetProxyForObject(NS_UI_THREAD_EVENTQ,
                                 NS_GET_IID(nsIInterfaceRequestor),
                                 NS_STATIC_CAST(nsIInterfaceRequestor*,infoObject),
                                 PROXY_SYNC,
                                 getter_AddRefs(proxiedCallbacks));

     nsCOMPtr<nsIPrompt> prompt (do_GetInterface(proxiedCallbacks));
  
     if (!prompt)
       return NS_ERROR_NO_INTERFACE;

     nsCOMPtr<nsIPrompt> proxyPrompt;
     // Finally, get a proxy for the nsIPrompt
     proxyman->GetProxyForObject(NS_UI_THREAD_EVENTQ,
                                 NS_GET_IID(nsIPrompt),
                                 prompt,
                                 PROXY_SYNC,
                                 getter_AddRefs(proxyPrompt));
     proxyPrompt->Alert(nsnull, formattedString.get());
     return NS_OK;
     
}



static nsresult
nsHandleSSLError(nsNSSSocketInfo *socketInfo, PRInt32 err)
{
  nsresult rv;
  NS_DEFINE_CID(nssComponentCID, NS_NSSCOMPONENT_CID);
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(nssComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  nsXPIDLCString hostName;
  socketInfo->GetHostName(getter_Copies(hostName));
  NS_ConvertASCIItoUCS2 hostNameU(hostName);
  const PRUnichar *params[2];
  nsXPIDLString formattedString;

  switch (err) {
  case SSL_ERROR_SSL_DISABLED:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("SSLDisabled").get(),
                                                params, 1,
                                                getter_Copies(formattedString));
    break;
  case SSL_ERROR_SSL2_DISABLED:
    params[0] = hostNameU.get();
    nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("SSL2Disabled").get(),
                                                params, 1,
                                                getter_Copies(formattedString));
    break;
  case SSL_ERROR_EXPORT_ONLY_SERVER:
  case SSL_ERROR_US_ONLY_SERVER:
  case SSL_ERROR_NO_CYPHER_OVERLAP:
    {
      NS_DEFINE_CID(StringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
      nsCOMPtr<nsIStringBundleService> service = 
                                  do_GetService(StringBundleServiceCID, &rv);
      nsCOMPtr<nsIStringBundle> brandBundle;
      service->CreateBundle("chrome://global/locale/brand.properties",
                            getter_AddRefs(brandBundle));
      nsXPIDLString brandShortName;
      brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                     getter_Copies(brandShortName));
      params[0] = brandShortName.get();
      params[1] = hostNameU.get();
      nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("SSLNoMatchingCiphers").get(),
                                                  params, 2,
                                                  getter_Copies(formattedString));
                                                  
    }
    break;
  default:
    {
      char buf[80];
      PR_snprintf(buf, 80, "%ld", err);
      NS_ConvertASCIItoUCS2 errorCode(buf);
      params[0] = hostNameU.get();
      params[1] = errorCode.get(); 
      nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("SSLGenericError").get(),
                                                  params, 2, 
                                                  getter_Copies(formattedString));
      
    }                                           
    break;
  }

   rv = displayAlert(formattedString, socketInfo);
   return rv;
}


static PRStatus PR_CALLBACK
nsSSLIOLayerConnect(PRFileDesc* fd, const PRNetAddr* addr,
                    PRIntervalTime timeout)
{
  if (!fd || !fd->lower)
    return PR_FAILURE;
  
  PRStatus status = PR_SUCCESS;
  
  // Due to limitations in NSPR 4.0, we must execute this entire connect
  // as a blocking operation.
  
  PRSocketOptionData sockopt;
  sockopt.option = PR_SockOpt_Nonblocking;
  PR_GetSocketOption(fd, &sockopt);
  PRBool oldBlockVal = sockopt.value.non_blocking;
  
  nsNSSSocketInfo *infoObject = (nsNSSSocketInfo*)fd->secret;
  
  sockopt.option = PR_SockOpt_Nonblocking;
  sockopt.value.non_blocking = PR_FALSE;
  PR_SetSocketOption(fd, &sockopt);
  
  status = fd->lower->methods->connect(fd->lower, addr, 
                                       PR_INTERVAL_NO_TIMEOUT);
  if (status != PR_SUCCESS) {
    PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("[%p] Lower layer connect error: %d\n",
                                      (void*)fd, PR_GetError()));
    goto loser;
  }
  
  PRBool forceHandshake, forTLSStepUp;
  infoObject->GetForceHandshake(&forceHandshake);
  infoObject->GetForTLSStepUp(&forTLSStepUp);
  
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] Connect: forceHandshake = %d, forTLSStepUp = %d\n",
                                    (void*)fd, forceHandshake, forTLSStepUp));

  if (!forTLSStepUp && forceHandshake) {
    // Kick-start the handshake in order to work around bug 66706
    PRInt32 res = PR_Write(fd,0,0);
    
    if (res == -1) {
      PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("[%p] ForceHandshake failure -- error %d\n",
                                        (void*)fd, PR_GetError()));
      status = PR_FAILURE;
    }
  }

 loser:
  // We put the Nonblocking bit back to the value it was when 
  // we entered this function.
  NS_ASSERTION(sockopt.option == PR_SockOpt_Nonblocking,
               "sockopt.option was re-set to an unexpected value");
  sockopt.value.non_blocking = oldBlockVal;
  PR_SetSocketOption(fd, &sockopt);

  return status;
}

static PRInt32 PR_CALLBACK
nsSSLIOLayerAvailable(PRFileDesc *fd)
{
  if (!fd || !fd->lower)
    return PR_FAILURE;

  PRInt32 bytesAvailable = SSL_DataPending(fd->lower);
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] available %d bytes\n", (void*)fd, bytesAvailable));
  return bytesAvailable;
}

static PRStatus PR_CALLBACK
nsSSLIOLayerClose(PRFileDesc *fd)
{
  if (!fd)
    return PR_FAILURE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] Shutting down socket\n", (void*)fd));

  PRFileDesc* popped = PR_PopIOLayer(fd, PR_TOP_IO_LAYER);
  PRStatus status = fd->methods->close(fd);
  if (status != PR_SUCCESS) return status;
  
  popped->identity = PR_INVALID_IO_LAYER;
  nsNSSSocketInfo *infoObject = (nsNSSSocketInfo *)popped->secret;
  NS_RELEASE(infoObject);
  
  return status;
}

#if defined(DEBUG_SSL_VERBOSE) && defined(DUMP_BUFFER)
/* Dumps a (potentially binary) buffer using SSM_DEBUG. 
   (We could have used the version in ssltrace.c, but that's
   specifically tailored to SSLTRACE. Sigh. */
#define DUMPBUF_LINESIZE 24
static void
nsDumpBuffer(unsigned char *buf, PRIntn len)
{
  char hexbuf[DUMPBUF_LINESIZE*3+1];
  char chrbuf[DUMPBUF_LINESIZE+1];
  static const char *hex = "0123456789abcdef";
  PRIntn i = 0, l = 0;
  char ch, *c, *h;
  if (len == 0)
    return;
  hexbuf[DUMPBUF_LINESIZE*3] = '\0';
  chrbuf[DUMPBUF_LINESIZE] = '\0';
  (void) memset(hexbuf, 0x20, DUMPBUF_LINESIZE*3);
  (void) memset(chrbuf, 0x20, DUMPBUF_LINESIZE);
  h = hexbuf;
  c = chrbuf;

  while (i < len)
  {
    ch = buf[i];

    if (l == DUMPBUF_LINESIZE)
    {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("%s%s\n", hexbuf, chrbuf));
      (void) memset(hexbuf, 0x20, DUMPBUF_LINESIZE*3);
      (void) memset(chrbuf, 0x20, DUMPBUF_LINESIZE);
      h = hexbuf;
      c = chrbuf;
      l = 0;
    }

    /* Convert a character to hex. */
    *h++ = hex[(ch >> 4) & 0xf];
    *h++ = hex[ch & 0xf];
    h++;
        
    /* Put the character (if it's printable) into the character buffer. */
    if ((ch >= 0x20) && (ch <= 0x7e))
      *c++ = ch;
    else
      *c++ = '.';
    i++; l++;
  }
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("%s%s\n", hexbuf, chrbuf));
}

#define DEBUG_DUMP_BUFFER(buf,len) nsDumpBuffer(buf,len)
#else
#define DEBUG_DUMP_BUFFER(buf,len)
#endif

static PRInt32 PR_CALLBACK
nsSSLIOLayerRead(PRFileDesc* fd, void* buf, PRInt32 amount)
{
  if (!fd || !fd->lower)
    return PR_FAILURE;

  nsNSSSocketInfo *socketInfo = nsnull;
  socketInfo = (nsNSSSocketInfo*)fd->secret;
  NS_ASSERTION(socketInfo,"nsNSSSocketInfo was null for an fd");
  PRBool tlsIntolerant;
  socketInfo->GetTLSIntolerant(&tlsIntolerant);
  if (tlsIntolerant) {
    // By returning 0 here, necko will retry the connection
    // again. 
    return 0;
  }

  PRInt32 bytesRead = fd->lower->methods->read(fd->lower, buf, amount);
#ifdef DEBUG_SSL_VERBOSE
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] read %d bytes\n", (void*)fd, bytesRead));
  DEBUG_DUMP_BUFFER((unsigned char*)buf, bytesRead);
#endif

  if (bytesRead == -1) {
    PRInt32 err = PR_GetError();
    if (IS_SSL_ERROR(err)) {
      nsHandleSSLError(socketInfo, err);
    }
  }

  return bytesRead;
}

static PRInt32 PR_CALLBACK
nsSSLIOLayerWrite(PRFileDesc* fd, const void* buf, PRInt32 amount)
{
  if (!fd || !fd->lower)
    return PR_FAILURE;
    
#ifdef DEBUG_SSL_VERBOSE
  DEBUG_DUMP_BUFFER((unsigned char*)buf, amount);
#endif
  nsNSSSocketInfo *socketInfo = nsnull;
  PRBool firstWrite;
  socketInfo = (nsNSSSocketInfo*)fd->secret;
  NS_ASSERTION(socketInfo,"nsNSSSocketInfo was null for an fd");
  socketInfo->GetFirstWrite(&firstWrite);
  PRBool oldBlockVal;
  PRBool oldBlockReset = PR_FALSE;
  PRSocketOptionData sockopt;

  if (firstWrite) {
    // We only want the first write to be blocking so that we
    // can trap the case of the TLS intolerant server.
    sockopt.option = PR_SockOpt_Nonblocking;
    PR_GetSocketOption(fd, &sockopt);
    oldBlockVal = sockopt.value.non_blocking;
    sockopt.value.non_blocking = PR_FALSE;
    PR_SetSocketOption(fd, &sockopt);
  }

  PRInt32 bytesWritten = fd->lower->methods->write(fd->lower, buf, amount);

#ifdef DEBUG_SSL_VERBOSE
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] wrote %d bytes\n", (void*)fd, bytesWritten));
#endif

  // This is where we work around all of those SSL servers that don't 
  // conform to the SSL spec and shutdown a connection when we request
  // SSL v3.1 (aka TLS).  The spec says the client says what version
  // of the protocol we're willing to perform, in our case SSL v3.1
  // In its response, the server says which version it wants to perform.
  // Many servers out there only know how to do v3.0.  Next, we're supposed
  // to send back the version of the protocol we requested (ie v3.1).  At
  // this point many servers's implementations are broken and they shut
  // down the connection when they don't see the version they sent back.
  // This is supposed to prevent a man in the middle from forcing one
  // side to dumb down to a lower level of the protocol.  Unfortunately,
  // there are enough broken servers out there that such a gross work-around
  // is necessary.  :(

  
  if (bytesWritten == -1) {
    // Let's see if there was an error set by the SSL libraries that we
    // should tell the user about.
    PRInt32 err = PR_GetError();
    if (firstWrite) {
      PRBool tlsOn;
      SSL_OptionGet(fd->lower, SSL_ENABLE_TLS, &tlsOn);
      if (tlsOn) {
        // Make necko re-try this connection by sending back an EOF
        // on the first read. (ie premature EOF)
        bytesWritten = 0;
        socketInfo->SetTLSIntolerant(PR_TRUE);
        // Now let's add this site to the list of TLS intolerant
        // sites.
        char buf[1024];
        PRInt32 port;
        nsXPIDLCString host;
        socketInfo->GetPort(&port);
        socketInfo->GetHostName(getter_Copies(host));
        HASH_STRING_KEY(buf,1024,host.get(),port);
        nsCStringKey key (buf);
        // We don't really wanna associate a value.  If it's
        // in the table, that means it's TLS intolerant and
        // we don't really need to know anything else.
        gTLSIntolerantSites->Put(&key, nsnull);    
      } else if (IS_SSL_ERROR(err)) {
         // This is the case where  the first write failed with 
         // TLS turned off.
         nsHandleSSLError(socketInfo, err);
       }
     } else if (IS_SSL_ERROR(err)) {
       // This is the case where a subsequent write has failed, 
       // ie not the first write.
       nsHandleSSLError(socketInfo, err);

    }
  }
  // TLS intolerant servers only cause the first write to fail, so let's 
  // set the fristWrite attribute to false so that we don't try the logic
  // above again in a subsequent write.
  if (firstWrite) {
    socketInfo->SetFirstWrite(PR_FALSE);
    if (!oldBlockReset) {
      sockopt.option = PR_SockOpt_Nonblocking;
      sockopt.value.non_blocking = oldBlockVal;
      PR_SetSocketOption(fd, &sockopt);
    }
  }
  return bytesWritten;
}

nsresult InitNSSMethods()
{
  nsSSLIOLayerIdentity = PR_GetUniqueIdentity("NSS layer");
  nsSSLIOLayerMethods  = *PR_GetDefaultIOMethods();
  
  nsSSLIOLayerMethods.connect = nsSSLIOLayerConnect;
  nsSSLIOLayerMethods.close = nsSSLIOLayerClose;
  nsSSLIOLayerMethods.available = nsSSLIOLayerAvailable;
  nsSSLIOLayerMethods.write = nsSSLIOLayerWrite;
  nsSSLIOLayerMethods.read = nsSSLIOLayerRead;
  
  nsresult rv;
  /* This performs NSS initialization for us */
  rv = nsServiceManager::GetService(PSM_COMPONENT_CONTRACTID,
                                    NS_GET_IID(nsISecurityManagerComponent),
                                    (nsISupports**)&gNSSService);
  return rv;
}

nsresult
nsSSLIOLayerNewSocket(const char *host,
                      PRInt32 port,
                      const char *proxyHost,
                      PRInt32 proxyPort,
                      PRFileDesc **fd,
                      nsISupports** info,
                      PRBool forTLSStepUp)
{
  // XXX - this code is duplicated in nsSSLIOLayerAddToSocket
  if (firstTime) {
    nsresult rv = InitNSSMethods();
    if (NS_FAILED(rv)) return rv;
    gTLSIntolerantSites =  new nsHashtable(16, PR_TRUE);
    if (!gTLSIntolerantSites) return NS_ERROR_OUT_OF_MEMORY;
    firstTime = PR_FALSE;
  }

  PRFileDesc* sock = PR_OpenTCPSocket(PR_AF_INET6);
  if (!sock) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = nsSSLIOLayerAddToSocket(host, port, proxyHost, proxyPort,
                                        sock, info, forTLSStepUp);
  if (NS_FAILED(rv)) {
    PR_Close(sock);
    return rv;
  }

  *fd = sock;
  return NS_OK;
}



static char* defaultServerNickname(CERTCertificate* cert)
{
  char* nickname = nsnull;
  int count;
  PRBool conflict;
  char* servername = nsnull;
  
  servername = CERT_GetCommonName(&cert->subject);
  if (servername == NULL) {
    return nsnull;
  }
   
  count = 1;
  while (1) {
    if (count == 1) {
      nickname = PR_smprintf("%s", servername);
    }
    else {
      nickname = PR_smprintf("%s #%d", servername, count);
    }
    if (nickname == NULL) {
      break;
    }

    conflict = SEC_CertNicknameConflict(nickname, &cert->derSubject,
                                        cert->dbhandle);
    if (conflict == PR_SUCCESS) {
      break;
    }
    PR_Free(nickname);
    count++;
  }
  PR_FREEIF(servername);
  return nickname;
}



static nsresult
addCertToDB(CERTCertificate *peerCert, PRInt16 addType)
{
  CERTCertTrust trust;
  SECStatus rv;
  nsresult retVal = NS_ERROR_FAILURE;
  char *nickname;
  
  switch (addType) {
    case nsIBadCertListener::ADD_TRUSTED_PERMANENTLY:
      nickname = defaultServerNickname(peerCert);
      if (nsnull == nickname)
        break;
      memset((void*)&trust, 0, sizeof(trust));
      rv = CERT_DecodeTrustString(&trust, "P"); 
      if (rv != SECSuccess) {
        return NS_ERROR_FAILURE;
      }
      rv = CERT_AddTempCertToPerm(peerCert, nickname, &trust);
      if (rv == SECSuccess)
        retVal = NS_OK;
      PR_Free(nickname);
      break;
    case nsIBadCertListener::ADD_TRUSTED_FOR_SESSION:
      // XXX We need an API from NSS to do this so 
      //     that we don't have to access the fields 
      //     in the cert directly.
      peerCert->keepSession = PR_TRUE;
      CERTCertTrust *trustPtr;
      if (!peerCert->trust) {
        trustPtr = (CERTCertTrust*)PORT_ArenaZAlloc(peerCert->arena,
                                                    sizeof(CERTCertTrust));
        if (!trustPtr)
          break;

        peerCert->trust = trustPtr;
      } else {
        trustPtr = peerCert->trust;
      }
      rv = CERT_DecodeTrustString(trustPtr, "P");
      if (rv != SECSuccess)
        break;

      retVal = NS_OK;      
      break;
    default:
      PR_ASSERT(!"Invalid value for addType passed to addCertDB");
      break;
  }
  return retVal;
}

static PRBool
nsContinueDespiteCertError(nsNSSSocketInfo  *infoObject,
                           PRFileDesc       *sslSocket,
                           int               error,
                           nsNSSCertificate *nssCert)
{
  PRBool retVal = PR_FALSE;
  nsIBadCertListener *badCertHandler;
  PRInt16 addType = nsIBadCertListener::UNINIT_ADD_FLAG;
  nsresult rv;

  if (!nssCert)
    return PR_FALSE;
  rv = getNSSDialogs((void**)&badCertHandler, 
                     NS_GET_IID(nsIBadCertListener));
  if (NS_FAILED(rv)) 
    return PR_FALSE;
  nsITransportSecurityInfo *csi = NS_STATIC_CAST(nsITransportSecurityInfo*,
                                                 infoObject);
  nsIX509Cert *callBackCert = NS_STATIC_CAST(nsIX509Cert*, nssCert);
  CERTCertificate *peerCert = nssCert->GetCert();
  NS_ASSERTION(peerCert, "Got nsnull cert back from nsNSSCertificate");
  switch (error) {
  case SEC_ERROR_UNKNOWN_ISSUER:
  case SEC_ERROR_CA_CERT_INVALID:
  case SEC_ERROR_UNTRUSTED_ISSUER:
  /* This is a temporay fix for bug# - We are showing a unknown ca dialog,
     when actually the ca cert has expired/not yet valid. We need to change
     this in future - need to define a proper ui for this situation
  */
  case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:

    rv = badCertHandler->UnknownIssuer(csi, callBackCert, &addType, &retVal);
    break;
  case SSL_ERROR_BAD_CERT_DOMAIN:
    {
      nsXPIDLCString url; url.Adopt(SSL_RevealURL(sslSocket));
      NS_ASSERTION(url.get(), "could not find valid URL in ssl socket");
      rv = badCertHandler->MismatchDomain(csi, NS_ConvertASCIItoUCS2(url).get(),
                                          callBackCert, &retVal);
      if (NS_SUCCEEDED(rv) && retVal) {
        rv = CERT_AddOKDomainName(peerCert, url);
      }
    }
    break;
  case SEC_ERROR_EXPIRED_CERTIFICATE:
    rv = badCertHandler->CertExpired(csi, callBackCert, & retVal);
    if (rv == SECSuccess && retVal) {
      // XXX We need an NSS API for this equivalent functionality.
      //     Having to reach inside the cert is evil.
      peerCert->timeOK = PR_TRUE;
    }
    break;
  case SEC_ERROR_CRL_EXPIRED:
    {
      nsXPIDLCString url; url.Adopt(SSL_RevealURL(sslSocket));
      NS_ASSERTION(url, "could not find valid URL in ssl socket");
      rv = badCertHandler->CrlNextupdate(csi, NS_ConvertASCIItoUCS2(url).get(), callBackCert);
      if (NS_SUCCEEDED(rv) && retVal) {
        rv = CERT_AddOKDomainName(peerCert, url.get());
      }
      retVal = PR_FALSE;
    }
    break;
  default:
    displayUnknownCertErrorAlert(infoObject, error);
    rv = NS_ERROR_FAILURE;
    break;
  }
  if (retVal && addType != nsIBadCertListener::UNINIT_ADD_FLAG) {
    addCertToDB(peerCert, addType);
  }
  NS_RELEASE(badCertHandler);
  CERT_DestroyCertificate(peerCert);
  return NS_FAILED(rv) ? PR_FALSE : retVal;
}



nsresult
displayUnknownCertErrorAlert(nsNSSSocketInfo *infoObject, int error)
{
    nsresult rv;
    NS_DEFINE_CID(nssComponentCID, NS_NSSCOMPONENT_CID);
    nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(nssComponentCID, &rv));
    if (NS_FAILED(rv))
      return rv;
    nsXPIDLString formattedString;
    nsXPIDLCString hostName;
    infoObject->GetHostName(getter_Copies(hostName));
    NS_ConvertASCIItoUCS2 hostNameU(hostName);
    const PRUnichar *params[2];
    char buf[80];
    PR_snprintf(buf, 80, "%ld", error);
    NS_ConvertASCIItoUCS2 errorCode(buf);
    params[0] = hostNameU.get();
    params[1] = errorCode.get();
	
    //This is not a very precise String - should add some entry like 'unknown cert
    //error code etc. ...' in the .property file. Using 'Generic ssl error for now.
    nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("SSLGenericError").get(),
                                                  params, 2, 
                                                  getter_Copies(formattedString));
    
    rv = displayAlert(formattedString, infoObject);

    //Possibly, in most of the cases, we would never not look into this return 
    //value in the caller
    return rv;
}



static SECStatus
verifyCertAgain(CERTCertificate *cert, 
                PRFileDesc      *sslSocket,
                nsNSSSocketInfo *infoObject)
{
  SECStatus rv;

  // If we get here, the user has accepted the cert so
  // far, so we don't check the signature again.
  rv = CERT_VerifyCertNow(CERT_GetDefaultCertDB(), cert,
                          PR_FALSE, certUsageSSLServer,
                          (void*)infoObject);

  if (rv != SECSuccess) {
    return rv;
  }
  
  // Check the name field against the desired hostname.
  char *hostname = SSL_RevealURL(sslSocket); 
  if (hostname && hostname[0]) {
    rv = CERT_VerifyCertName(cert, hostname);
  } else {
    rv = SECFailure;
  }

  if (rv != SECSuccess) {
    PR_SetError(SSL_ERROR_BAD_CERT_DOMAIN, 0);
  }
  PR_FREEIF(hostname);
  return rv;
}
/*
 * Function: SECStatus nsConvertCANamesToStrings()
 * Purpose: creates CA names strings from (CERTDistNames* caNames)
 *
 * Arguments and return values
 * - arena: arena to allocate strings on
 * - caNameStrings: filled with CA names strings on return
 * - caNames: CERTDistNames to extract strings from
 * - return: SECSuccess if successful; error code otherwise
 *
 * Note: copied in its entirety from Nova code
 */
SECStatus nsConvertCANamesToStrings(PRArenaPool* arena, char** caNameStrings,
                                      CERTDistNames* caNames)
{
    SECItem* dername;
    SECStatus rv;
    int headerlen;
    uint32 contentlen;
    SECItem newitem;
    int n;
    char* namestring;

    for (n = 0; n < caNames->nnames; n++) {
        newitem.data = NULL;
        dername = &caNames->names[n];

        rv = DER_Lengths(dername, &headerlen, &contentlen);

        if (rv != SECSuccess) {
            goto loser;
        }

        if (headerlen + contentlen != dername->len) {
            /* This must be from an enterprise 2.x server, which sent
             * incorrectly formatted der without the outer wrapper of
             * type and length.  Fix it up by adding the top level
             * header.
             */
            if (dername->len <= 127) {
                newitem.data = (unsigned char *) PR_Malloc(dername->len + 2);
                if (newitem.data == NULL) {
                    goto loser;
                }
                newitem.data[0] = (unsigned char)0x30;
                newitem.data[1] = (unsigned char)dername->len;
                (void)memcpy(&newitem.data[2], dername->data, dername->len);
            }
            else if (dername->len <= 255) {
                newitem.data = (unsigned char *) PR_Malloc(dername->len + 3);
                if (newitem.data == NULL) {
                    goto loser;
                }
                newitem.data[0] = (unsigned char)0x30;
                newitem.data[1] = (unsigned char)0x81;
                newitem.data[2] = (unsigned char)dername->len;
                (void)memcpy(&newitem.data[3], dername->data, dername->len);
            }
            else {
                /* greater than 256, better be less than 64k */
                newitem.data = (unsigned char *) PR_Malloc(dername->len + 4);
                if (newitem.data == NULL) {
                    goto loser;
                }
                newitem.data[0] = (unsigned char)0x30;
                newitem.data[1] = (unsigned char)0x82;
                newitem.data[2] = (unsigned char)((dername->len >> 8) & 0xff);
                newitem.data[3] = (unsigned char)(dername->len & 0xff);
                memcpy(&newitem.data[4], dername->data, dername->len);
            }
            dername = &newitem;
        }

        namestring = CERT_DerNameToAscii(dername);
        if (namestring == NULL) {
            /* XXX - keep going until we fail to convert the name */
            caNameStrings[n] = "";
        }
        else {
            caNameStrings[n] = PORT_ArenaStrdup(arena, namestring);
            PR_Free(namestring);
            if (caNameStrings[n] == NULL) {
                goto loser;
            }
        }

        if (newitem.data != NULL) {
            PR_Free(newitem.data);
        }
    }

    return SECSuccess;
loser:
    if (newitem.data != NULL) {
        PR_Free(newitem.data);
    }
    return SECFailure;
}

/*
 * structs and ASN1 templates for the limited scope-of-use extension
 *
 * CertificateScopeEntry ::= SEQUENCE {
 *     name GeneralName, -- pattern, as for NameConstraints
 *     portNumber INTEGER OPTIONAL }
 *
 * CertificateScopeOfUse ::= SEQUENCE OF CertificateScopeEntry
 */
/*
 * CERTCertificateScopeEntry: struct for scope entry that can be consumed by
 *                            the code
 * certCertificateScopeOfUse: struct that represents the decoded extension data
 */
typedef struct {
    SECItem derConstraint;
    SECItem derPort;
    CERTGeneralName* constraint; /* decoded constraint */
    PRIntn port; /* decoded port number */
} CERTCertificateScopeEntry;

typedef struct {
    CERTCertificateScopeEntry** entries;
} certCertificateScopeOfUse;

/* corresponding ASN1 templates */
static const SEC_ASN1Template cert_CertificateScopeEntryTemplate[] = {
    { SEC_ASN1_SEQUENCE, 
      0, NULL, sizeof(CERTCertificateScopeEntry) },
    { SEC_ASN1_ANY,
      offsetof(CERTCertificateScopeEntry, derConstraint) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_INTEGER,
      offsetof(CERTCertificateScopeEntry, derPort) },
    { 0 }
};

static const SEC_ASN1Template cert_CertificateScopeOfUseTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, cert_CertificateScopeEntryTemplate }
};

/* 
 * decodes the extension data and create CERTCertificateScopeEntry that can
 * be consumed by the code
 */
static
SECStatus cert_DecodeScopeOfUseEntries(PRArenaPool* arena, SECItem* extData,
                                       CERTCertificateScopeEntry*** entries,
                                       int* numEntries)
{
    certCertificateScopeOfUse* scope = NULL;
    SECStatus rv = SECSuccess;
    int i;

    *entries = NULL; /* in case of failure */
    *numEntries = 0; /* ditto */

    scope = (certCertificateScopeOfUse*)
        PORT_ArenaZAlloc(arena, sizeof(certCertificateScopeOfUse));
    if (scope == NULL) {
        goto loser;
    }

    rv = SEC_ASN1DecodeItem(arena, (void*)scope, 
                            cert_CertificateScopeOfUseTemplate, extData);
    if (rv != SECSuccess) {
        goto loser;
    }

    *entries = scope->entries;
    PR_ASSERT(*entries != NULL);

    /* first, let's count 'em. */
    for (i = 0; (*entries)[i] != NULL; i++) ;
    *numEntries = i;

    /* convert certCertificateScopeEntry sequence into what we can readily
     * use
     */
    for (i = 0; i < *numEntries; i++) {
        (*entries)[i]->constraint = 
            cert_DecodeGeneralName(arena, &((*entries)[i]->derConstraint), 
                                   NULL);
        if ((*entries)[i]->derPort.data != NULL) {
            (*entries)[i]->port = 
                (int)DER_GetInteger(&((*entries)[i]->derPort));
        }
        else {
            (*entries)[i]->port = 0;
        }
    }

    goto done;
loser:
    if (rv == SECSuccess) {
        rv = SECFailure;
    }
done:
    return rv;
}

static SECStatus cert_DecodeCertIPAddress(SECItem* genname, 
                                          PRUint32* constraint, PRUint32* mask)
{
    /* in case of failure */
    *constraint = 0;
    *mask = 0;

    PR_ASSERT(genname->data != NULL);
    if (genname->data == NULL) {
        return SECFailure;
    }
    if (genname->len != 8) {
        /* the length must be 4 byte IP address with 4 byte subnet mask */
        return SECFailure;
    }

    /* get them in the right order */
    *constraint = PR_ntohl((PRUint32)(*genname->data));
    *mask = PR_ntohl((PRUint32)(*(genname->data + 4)));

    return SECSuccess;
}

static char* _str_to_lower(char* string)
{
#ifdef XP_WIN
    return _strlwr(string);
#else
    int i;
    for (i = 0; string[i] != '\0'; i++) {
        string[i] = tolower(string[i]);
    }
    return string;
#endif
}

/*
 * Sees if the client certificate has a restriction in presenting the cert
 * to the host: returns PR_TRUE if there is no restriction or if the hostname
 * (and the port) satisfies the restriction, or PR_FALSE if the hostname (and
 * the port) does not satisfy the restriction
 */
static PRBool CERT_MatchesScopeOfUse(CERTCertificate* cert, char* hostname,
                                     char* hostIP, PRIntn port)
{
    PRBool rv = PR_TRUE; /* whether the cert can be presented */
    SECStatus srv;
    SECItem extData;
    PRArenaPool* arena = NULL;
    CERTCertificateScopeEntry** entries = NULL;
    /* arrays of decoded scope entries */
    int numEntries = 0;
    int i;
    char* hostLower = NULL;
    PRUint32 hostIPAddr = 0;

    PR_ASSERT((cert != NULL) && (hostname != NULL) && (hostIP != NULL));

    /* find cert extension */
    srv = CERT_FindCertExtension(cert, SEC_OID_NS_CERT_EXT_SCOPE_OF_USE,
                                 &extData);
    if (srv != SECSuccess) {
        /* most of the time, this means the extension was not found: also,
         * since this is not a critical extension (as of now) we may simply
         * return PR_TRUE
         */
        goto done;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        goto done;
    }

    /* decode the scope of use entries into pairs of GeneralNames and
     * an optional port numbers
     */
    srv = cert_DecodeScopeOfUseEntries(arena, &extData, &entries, &numEntries);
    if (srv != SECSuccess) {
        /* XXX What should we do when we failed to decode the extension?  This
         *     may mean either the extension was malformed or some (unlikely)
         *     fatal error on our part: my argument is that if the extension 
         *     was malformed the extension "disqualifies" as a valid 
         *     constraint and we may present the cert
         */
        goto done;
    }

    /* loop over these structures */
    for (i = 0; i < numEntries; i++) {
        /* determine whether the GeneralName is a DNS pattern, an IP address 
         * constraint, or else
         */
        CERTGeneralName* genname = entries[i]->constraint;

        /* if constraint is NULL, don't bother looking */
        if (genname == NULL) {
            /* this is not a failure: just continue */
            continue;
        }

        switch (genname->type) {
        case certDNSName: {
            /* we have a DNS name constraint; we should use only the host name
             * information
             */
            char* pattern = NULL;
            char* substring = NULL;

            /* null-terminate the string */
            genname->name.other.data[genname->name.other.len] = '\0';
            pattern = _str_to_lower((char*)genname->name.other.data);

            if (hostLower == NULL) {
                /* so that it's done only if necessary and only once */
                hostLower = _str_to_lower(PL_strdup(hostname));
            }

            /* the hostname satisfies the constraint */
            if (((substring = strstr(hostLower, pattern)) != NULL) &&
                /* the hostname contains the pattern */
                (strlen(substring) == strlen(pattern)) &&
                /* the hostname ends with the pattern */
                ((substring == hostLower) || (*(substring-1) == '.'))) {
                /* the hostname either is identical to the pattern or
                 * belongs to a subdomain
                 */
                rv = PR_TRUE;
            }
            else {
                rv = PR_FALSE;
            }
            /* clean up strings if necessary */
            break;
        }
        case certIPAddress: {
            PRUint32 constraint;
            PRUint32 mask;
            PRNetAddr addr;
            
            if (hostIPAddr == 0) {
                /* so that it's done only if necessary and only once */
                PR_StringToNetAddr(hostIP, &addr);
                hostIPAddr = addr.inet.ip;
            }

            if (cert_DecodeCertIPAddress(&(genname->name.other), &constraint, 
                                         &mask) != SECSuccess) {
                continue;
            }
            if ((hostIPAddr & mask) == (constraint & mask)) {
                rv = PR_TRUE;
            }
            else {
                rv = PR_FALSE;
            }
            break;
        }
        default:
            /* ill-formed entry: abort */
            continue; /* go to the next entry */
        }

        if (!rv) {
            /* we do not need to check the port: go to the next entry */
            continue;
        }

        /* finally, check the optional port number */
        if ((entries[i]->port != 0) && (port != entries[i]->port)) {
            /* port number does not match */
            rv = PR_FALSE;
            continue;
        }

        /* we have a match */
        PR_ASSERT(rv);
        break;
    }
done:
    /* clean up entries */
    if (arena != NULL) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    if (hostLower != NULL) {
        PR_Free(hostLower);
    }
    return rv;
}

/*
 * Function: SSMStatus SSM_SetUserCertChoice()

 * Purpose: sets certChoice by reading the preference
 *
 * Arguments and return values
 * - conn: SSMSSLDataConnection
 * - returns: SSM_SUCCESS if successful; SSM_FAILURE otherwise
 *
 * Note: If done properly, this function will read the identifier strings
 *		 for ASK and AUTO modes, read the selected strings from the
 *		 preference, compare the strings, and determine in which mode it is
 *		 in.
 *       We currently use ASK mode for UI apps and AUTO mode for UI-less
 *       apps without really asking for preferences.
 */
nsresult nsGetUserCertChoice(SSM_UserCertChoice* certChoice)
{
	char *mode=NULL;
	nsresult ret;

	NS_ENSURE_ARG_POINTER(certChoice);

	nsCOMPtr<nsIPref> prefService = do_GetService(NS_PREF_CONTRACTID);

	ret = prefService->CopyCharPref("security.default_personal_cert", &mode);
	if (NS_FAILED(ret)) {
		goto loser;
	}

    if (PL_strcmp(mode, "Select Automatically") == 0) {
		*certChoice = AUTO;
	}
    else if (PL_strcmp(mode, "Ask Every Time") == 0) {
        *certChoice = ASK;
    }
    else {
		ret = NS_ERROR_FAILURE;
	}

loser:
	if (mode) {
		nsMemory::Free(mode);
	}
	return ret;
}

/*
 * Function: SECStatus SSM_SSLGetClientAuthData()
 * Purpose: this callback function is used to pull client certificate
 *			information upon server request
 *
 * Arguments and return values
 * - arg: SSL data connection
 * - socket: SSL socket we're dealing with
 * - caNames: list of CA names
 * - pRetCert: returns a pointer to a pointer to a valid certificate if
 *			   successful; otherwise NULL
 * - pRetKey: returns a pointer to a pointer to the corresponding key if
 *			  successful; otherwise NULL
 * - returns: SECSuccess if successful; error code otherwise
 */
SECStatus nsNSS_SSLGetClientAuthData(void* arg, PRFileDesc* socket,
								   CERTDistNames* caNames,
								   CERTCertificate** pRetCert,
								   SECKEYPrivateKey** pRetKey)
{
  void* wincx = NULL;
  SECStatus ret = SECFailure;
  nsresult rv;
  nsNSSSocketInfo* info = NULL;
  PRArenaPool* arena = NULL;
  char** caNameStrings;
  CERTCertificate* cert = NULL;
  CERTCertificate* serverCert = NULL;
  SECKEYPrivateKey* privKey = NULL;
  CERTCertList* certList = NULL;
  CERTCertListNode* node;
  CERTCertNicknames* nicknames = NULL;
  char* extracted = NULL;
  PRIntn keyError = 0; /* used for private key retrieval error */
  SSM_UserCertChoice certChoice;
  PRUint32 NumberOfCerts = 0;
	
  /* do some argument checking */
  if (socket == NULL || caNames == NULL || pRetCert == NULL ||
      pRetKey == NULL) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return SECFailure;
  }

  /* get PKCS11 pin argument */
  wincx = SSL_RevealPinArg(socket);
  if (wincx == NULL) {
    return SECFailure;
  }

  /* get the socket info */
  info = (nsNSSSocketInfo*)socket->higher->secret;

  /* create caNameStrings */
  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (arena == NULL) {
    goto loser;
  }

  caNameStrings = (char**)PORT_ArenaAlloc(arena, 
                                          sizeof(char*)*(caNames->nnames));
  if (caNameStrings == NULL) {
    goto loser;
  }


  ret = nsConvertCANamesToStrings(arena, caNameStrings, caNames);
  if (ret != SECSuccess) {
    goto loser;
  }

  /* get the preference */
  if (NS_FAILED(nsGetUserCertChoice(&certChoice))) {
    goto loser;
  }

  /* find valid user cert and key pair */	
  if (certChoice == AUTO) {
    /* automatically find the right cert */

    /* find all user certs that are valid and for SSL */
    certList = CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(), 
                                         certUsageSSLClient, PR_TRUE,
                                         PR_TRUE, wincx);
    if (certList == NULL) {
      goto noCert;
    }

    /* filter the list to those issued by CAs supported by the server */
    ret = CERT_FilterCertListByCANames(certList, caNames->nnames,
                                       caNameStrings, certUsageSSLClient);
    if (ret != SECSuccess) {
      goto noCert;
    }

    /* make sure the list is not empty */
    node = CERT_LIST_HEAD(certList);
    if (CERT_LIST_END(node, certList)) {
      goto noCert;
    }

    /* loop through the list until we find a cert with a key */
    while (!CERT_LIST_END(node, certList)) {
      /* if the certificate has restriction and we do not satisfy it
       * we do not use it
       */
#if 0		/* XXX This must be re-enabled */
      if (!CERT_MatchesScopeOfUse(node->cert, info->GetHostName,
                                  info->GetHostIP, info->GetHostPort)) {
          node = CERT_LIST_NEXT(node);
          continue;
      }
#endif

      privKey = PK11_FindKeyByAnyCert(node->cert, wincx);
      if (privKey != NULL) {
          /* this is a good cert to present */
          cert = CERT_DupCertificate(node->cert);
          break;
      }
      keyError = PR_GetError();
      if (keyError == SEC_ERROR_BAD_PASSWORD) {
          /* problem with password: bail */
          goto loser;
      }

      node = CERT_LIST_NEXT(node);
    }

    if (cert == NULL) {
        goto noCert;
    }
  }
  else {
    /* user selects a cert to present */
    int i;
    nsIClientAuthDialogs *dialogs = NULL;
    PRInt32 selectedIndex = -1;
    PRUnichar **certNicknameList = NULL;
    PRUnichar **certDetailsList = NULL;
    PRBool canceled;

    /* find all user certs that are for SSL */
    /* note that we are allowing expired certs in this list */
    certList = CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(), 
                                         certUsageSSLClient, PR_FALSE, 
                                         PR_FALSE, wincx);
    if (certList == NULL) {
      goto noCert;
    }

    if (caNames->nnames != 0) {
      /* filter the list to those issued by CAs supported by the 
       * server 
       */
      ret = CERT_FilterCertListByCANames(certList, caNames->nnames, 
                                        caNameStrings, 
                                        certUsageSSLClient);
      if (ret != SECSuccess) {
        goto loser;
      }
    }

    if (CERT_LIST_END(CERT_LIST_HEAD(certList), certList)) {
      /* list is empty - no matching certs */
      goto noCert;
    }

    /* filter it further for hostname restriction */
    node = CERT_LIST_HEAD(certList);
    while (!CERT_LIST_END(node, certList)) {
      ++NumberOfCerts;
#if 0 /* XXX Fix this */
      if (!CERT_MatchesScopeOfUse(node->cert, conn->hostName,
                                  conn->hostIP, conn->port)) {
        CERTCertListNode* removed = node;
        node = CERT_LIST_NEXT(removed);
        CERT_RemoveCertListNode(removed);
      }
      else {
        node = CERT_LIST_NEXT(node);
      }
#endif
      node = CERT_LIST_NEXT(node);
    }
    if (CERT_LIST_END(CERT_LIST_HEAD(certList), certList)) {
      goto noCert;
    }

    nicknames = CERT_NicknameStringsFromCertList(certList,
                                                 NICKNAME_EXPIRED_STRING,
                                                 NICKNAME_NOT_YET_VALID_STRING);

    if (nicknames == NULL) {
      goto loser;
    }

    NS_ASSERTION(nicknames->numnicknames == NumberOfCerts, "nicknames->numnicknames != NumberOfCerts");

    /* Get the SSL Certificate */
    serverCert = SSL_PeerCertificate(socket);
    if (serverCert == NULL) {
      /* couldn't get the server cert: what do I do? */
      goto loser;
    }

    /* Get CN and O of the subject and O of the issuer */
    NS_ConvertUTF8toUCS2 cn(CERT_GetCommonName(&serverCert->subject));
    NS_ConvertUTF8toUCS2 org(CERT_GetOrgName(&serverCert->subject));
    NS_ConvertUTF8toUCS2 issuer(CERT_GetOrgName(&serverCert->issuer));

    certNicknameList = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * nicknames->numnicknames);
    certDetailsList = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * nicknames->numnicknames);

    nsCOMPtr<nsIProxyObjectManager> proxyman(do_GetService(NS_XPCOMPROXY_CONTRACTID));
    NS_DEFINE_CID(nssComponentCID, NS_NSSCOMPONENT_CID);
    nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(nssComponentCID, &rv));

    if (proxyman && nssComponent)
    for (i = 0, node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList);
         ++i, node = CERT_LIST_NEXT(node)
        )
    {
      nsNSSCertificate *tempCert = new nsNSSCertificate(node->cert);
      NS_ADDREF(tempCert);

      nsCOMPtr<nsIX509Cert> x509 = do_QueryInterface(tempCert);

      nsCOMPtr<nsIX509Cert> x509Proxy;
      proxyman->GetProxyForObject( NS_UI_THREAD_EVENTQ,
                                   nsIX509Cert::GetIID(),
                                   x509,
                                   PROXY_SYNC | PROXY_ALWAYS,
                                   getter_AddRefs(x509Proxy));

      if (x509Proxy) {
        nsAutoString nickWithSerial;
        nsAutoString str;
        nsAutoString info;
        PRUnichar *temp1 = 0;

        nickWithSerial.Append(NS_ConvertUTF8toUCS2(nicknames->nicknames[i]));

        if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertInfoIssuedFor").get(), info))) {
          str.Append(info);
          str.Append(NS_LITERAL_STRING("\n"));
        }

        if (NS_SUCCEEDED(x509Proxy->GetSubjectName(&temp1)) && temp1 && nsCharTraits<PRUnichar>::length(temp1)) {
          str.Append(NS_LITERAL_STRING("  "));
          if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSubject").get(), info))) {
            str.Append(info);
            str.Append(NS_LITERAL_STRING(": "));
          }
          str.Append(temp1);
          nsMemory::Free(temp1);
          str.Append(NS_LITERAL_STRING("\n"));
        }

        if (NS_SUCCEEDED(x509Proxy->GetSerialNumber(&temp1)) && temp1 && nsCharTraits<PRUnichar>::length(temp1)) {
          str.Append(NS_LITERAL_STRING("  "));
          if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSerialNo").get(), info))) {
            str.Append(info);
            str.Append(NS_LITERAL_STRING(": "));
          }
          str.Append(temp1);

          nickWithSerial.Append(NS_LITERAL_STRING(" ["));
          nickWithSerial.Append(temp1);
          nickWithSerial.Append(NS_LITERAL_STRING("]"));

          nsMemory::Free(temp1);
          str.Append(NS_LITERAL_STRING("\n"));
        }


        {
          nsCOMPtr<nsIX509CertValidity> validity;
          nsCOMPtr<nsIX509CertValidity> originalValidity;
          rv = x509Proxy->GetValidity(getter_AddRefs(originalValidity));
          if (NS_SUCCEEDED(rv) && originalValidity) {
            proxyman->GetProxyForObject( NS_UI_THREAD_EVENTQ,
                                         nsIX509CertValidity::GetIID(),
                                         originalValidity,
                                         PROXY_SYNC | PROXY_ALWAYS,
                                         getter_AddRefs(validity));
          }

          if (validity) {
            str.Append(NS_LITERAL_STRING("  "));
            if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertInfoValid").get(), info))) {
              str.Append(info);
            }

            if (NS_SUCCEEDED(validity->GetNotBeforeLocalTime(&temp1)) && temp1 && nsCharTraits<PRUnichar>::length(temp1)) {
              str.Append(NS_LITERAL_STRING(" "));
              if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertInfoFrom").get(), info))) {
                str.Append(info);
              }
              str.Append(NS_LITERAL_STRING(" "));
              str.Append(temp1);
              nsMemory::Free(temp1);
            }

            if (NS_SUCCEEDED(validity->GetNotAfterLocalTime(&temp1)) && temp1 && nsCharTraits<PRUnichar>::length(temp1)) {
              str.Append(NS_LITERAL_STRING(" "));
              if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertInfoTo").get(), info))) {
                str.Append(info);
              }
              str.Append(NS_LITERAL_STRING(" "));
              str.Append(temp1);
              nsMemory::Free(temp1);
            }

            str.Append(NS_LITERAL_STRING("\n"));
          }
        }

        PRUint32 tempInt = 0;
        if (NS_SUCCEEDED(x509Proxy->GetPurposes(&tempInt, &temp1)) && temp1 && nsCharTraits<PRUnichar>::length(temp1)) {
          str.Append(NS_LITERAL_STRING("  "));
          if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertInfoPurposes").get(), info))) {
            str.Append(info);
          }
          str.Append(NS_LITERAL_STRING(": "));
          str.Append(temp1);
          nsMemory::Free(temp1);
          str.Append(NS_LITERAL_STRING("\n"));
        }

        if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertInfoIssuedBy").get(), info))) {
          str.Append(info);
          str.Append(NS_LITERAL_STRING("\n"));
        }

        if (NS_SUCCEEDED(x509Proxy->GetIssuerName(&temp1)) && temp1 && nsCharTraits<PRUnichar>::length(temp1)) {
          str.Append(NS_LITERAL_STRING("  "));
          if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSubject").get(), info))) {
            str.Append(info);
            str.Append(NS_LITERAL_STRING(": "));
          }
          str.Append(temp1);
          nsMemory::Free(temp1);
          str.Append(NS_LITERAL_STRING("\n"));
        }

        /*
          the above produces output the following output:

          Issued to: 
            Subject: $subjectName
            Serial number: $serialNumber
            Valid from: $starting_date to $expriation_date
            Purposes: $purposes
          Issued by:
            Subject: $issuerName
        */

        certNicknameList[i] = ToNewUnicode(nickWithSerial);
        certDetailsList[i] = ToNewUnicode(str);
      }

      NS_RELEASE(tempCert);
    }

    /* Throw up the client auth dialog and get back the index of the selected cert */
    rv = getNSSDialogs((void**)&dialogs, NS_GET_IID(nsIClientAuthDialogs));

    if (NS_FAILED(rv)) goto loser;

    rv = dialogs->ChooseCertificate(info, cn.get(), org.get(), issuer.get(), 
      (const PRUnichar**)certNicknameList, (const PRUnichar**)certDetailsList,
      nicknames->numnicknames, &selectedIndex, &canceled);

    for (i = 0; i < nicknames->numnicknames; ++i) {
      nsMemory::Free(certNicknameList[i]);
      nsMemory::Free(certDetailsList[i]);
    }
    nsMemory::Free(certNicknameList);
    nsMemory::Free(certDetailsList);

    NS_RELEASE(dialogs);
    if (NS_FAILED(rv)) goto loser;

    if (canceled) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }

    for (i = 0, node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList);
         ++i, node = CERT_LIST_NEXT(node)) {

      if (i == selectedIndex) {
        cert = CERT_DupCertificate(node->cert);
        break;
      }
    }

    if (cert == NULL) {
      goto loser;
    }

    /* go get the private key */
    privKey = PK11_FindKeyByAnyCert(cert, wincx);
    if (privKey == NULL) {
      keyError = PR_GetError();
      if (keyError == SEC_ERROR_BAD_PASSWORD) {
          /* problem with password: bail */
          goto loser;
      }
      else {
          goto noCert;
      }
    }
  }
  goto done;

noCert:
loser:
  if (ret == SECSuccess) {
    ret = SECFailure;
  }
  if (cert != NULL) {
    CERT_DestroyCertificate(cert);
    cert = NULL;
  }
done:
  if (extracted != NULL) {
    PR_Free(extracted);
  }
  if (nicknames != NULL) {
    CERT_FreeNicknames(nicknames);
  }
  if (certList != NULL) {
    CERT_DestroyCertList(certList);
  }
  if (arena != NULL) {
    PORT_FreeArena(arena, PR_FALSE);
  }

  *pRetCert = cert;
  *pRetKey = privKey;

  return ret;
}

static SECStatus
nsNSSBadCertHandler(void *arg, PRFileDesc *sslSocket)
{
  SECStatus rv = SECFailure;
  int error;
  nsNSSSocketInfo* infoObject = (nsNSSSocketInfo *)arg;
  CERTCertificate *peerCert;
  nsNSSCertificate *nssCert;

  error = PR_GetError();
  peerCert = SSL_PeerCertificate(sslSocket);
  nssCert = new nsNSSCertificate(peerCert);
  if (!nssCert) {
    return SECFailure;
  } 
  NS_ADDREF(nssCert);
  while (rv != SECSuccess) {
     //Func nsContinueDespiteCertError does the same set of checks as func.
     //nsCertErrorNeedsDialog. So, removing call to nsCertErrorNeedsDialog
     if (!nsContinueDespiteCertError(infoObject, sslSocket, 
                                    error, nssCert)) {
      break;
    }
    rv = verifyCertAgain(peerCert, sslSocket, infoObject);
	error = PR_GetError();
  }
  NS_RELEASE(nssCert);
  CERT_DestroyCertificate(peerCert); 
  return rv;
}

static PRFileDesc*
nsSSLIOLayerImportFD(PRFileDesc *fd,
                     nsNSSSocketInfo *infoObject,
                     const char *host)
{
  PRFileDesc* sslSock = SSL_ImportFD(nsnull, fd);
  if (!sslSock) {
    NS_ASSERTION(PR_FALSE, "NSS: Error importing socket");
    return nsnull;
  }
  SSL_SetPKCS11PinArg(sslSock, (nsIInterfaceRequestor*)infoObject);
  SSL_HandshakeCallback(sslSock, HandshakeCallback, infoObject);
  SSL_GetClientAuthDataHook(sslSock, 
                            (SSLGetClientAuthData)nsNSS_SSLGetClientAuthData,
                            infoObject);
  SSL_AuthCertificateHook(sslSock, AuthCertificateCallback, 0);

  PRInt32 ret = SSL_SetURL(sslSock, host);
  if (ret == -1) {
    NS_ASSERTION(PR_FALSE, "NSS: Error setting server name");
    goto loser;
  }
  return sslSock;
loser:
  if (sslSock) {
    PR_Close(sslSock);
  }
  return nsnull;
}

static nsresult
nsSSLIOLayerSetOptions(PRFileDesc *fd, PRBool forTLSStepUp, 
                       const char *proxyHost, const char *host, PRInt32 port,
                       nsNSSSocketInfo *infoObject)
{
  if ((forTLSStepUp || proxyHost) &&
      SECSuccess != SSL_OptionSet(fd, SSL_SECURITY, PR_FALSE))
    return NS_ERROR_FAILURE;

  if (forTLSStepUp) {
    if (SECSuccess != SSL_OptionSet(fd, SSL_ENABLE_SSL2, PR_FALSE)) {
      return NS_ERROR_FAILURE;
    }
    if (SECSuccess != SSL_OptionSet(fd, SSL_V2_COMPATIBLE_HELLO, PR_FALSE)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Let's see if we're trying to connect to a site we know is
  // TLS intolerant.
  char buf[1024];
  HASH_STRING_KEY(buf,1024,host,port);
  nsCStringKey key (buf);
  if (gTLSIntolerantSites->Exists(&key) && 
      SECSuccess != SSL_OptionSet(fd, SSL_ENABLE_TLS, PR_FALSE)) {
    return NS_ERROR_FAILURE;
  }

  if (SECSuccess != SSL_OptionSet(fd, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE)) {
    return NS_ERROR_FAILURE;
  }
  if (SECSuccess != SSL_BadCertHook(fd, (SSLBadCertHandler) nsNSSBadCertHandler,
                                    infoObject)) {
    return NS_ERROR_FAILURE;
  }

  // Set the Peer ID so that SSL proxy connections work properly.
  char *peerId = PR_smprintf("%s:%d", host, port);
  if (SECSuccess != SSL_SetSockPeerID(fd, peerId)) {
    PR_smprintf_free(peerId);
    return NS_ERROR_FAILURE;
  }

  PR_smprintf_free(peerId);
  return NS_OK;
}

nsresult
nsSSLIOLayerAddToSocket(const char* host,
                        PRInt32 port,
                        const char* proxyHost,
                        PRInt32 proxyPort,
                        PRFileDesc* fd,
                        nsISupports** info,
                        PRBool forTLSStepUp)
{
  PRFileDesc* layer = nsnull;
  nsresult rv;

  // XXX - this code is duplicated in nsSSLIONewSocket
  if (firstTime) {
    rv = InitNSSMethods();
    if (NS_FAILED(rv)) return rv;
    gTLSIntolerantSites =  new nsHashtable(16, PR_TRUE);
    if (!gTLSIntolerantSites) return NS_ERROR_OUT_OF_MEMORY;
    firstTime = PR_FALSE;
  }

  nsNSSSocketInfo* infoObject = new nsNSSSocketInfo();
  if (!infoObject) return NS_ERROR_FAILURE;
  
  NS_ADDREF(infoObject);
  infoObject->SetForTLSStepUp(forTLSStepUp);
  infoObject->SetHostName(host);
  infoObject->SetPort(port);

  PRFileDesc *sslSock = nsSSLIOLayerImportFD(fd, infoObject, host);
  if (!sslSock) {
    NS_ASSERTION(PR_FALSE, "NSS: Error importing socket");
    goto loser;
  }


  infoObject->SetFileDescPtr(sslSock);

  /* Now, layer ourselves on top of the SSL socket... */
  layer = PR_CreateIOLayerStub(nsSSLIOLayerIdentity,
                               &nsSSLIOLayerMethods);
  if (!layer)
    goto loser;
  
  layer->secret = (PRFilePrivate*) infoObject;
  rv = PR_PushIOLayer(sslSock, PR_GetLayersIdentity(sslSock), layer);
  
  if (NS_FAILED(rv)) {
    goto loser;
  }

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] Socket set up\n", (void*)sslSock));
  infoObject->QueryInterface(NS_GET_IID(nsISupports), (void**) (info));

  /* This is rather confusing, but now, "layer" points to the SSL socket,
     and that's what we should use for manipulating it. */

  rv = nsSSLIOLayerSetOptions(layer, forTLSStepUp, proxyHost, host, port,
                              infoObject);

  if (NS_FAILED(rv))
    goto loser;

  // We are going use a clear connection first //
  if (forTLSStepUp || proxyHost) {
    infoObject->SetFirstWrite(PR_FALSE);
  }

  return NS_OK;
 loser:
  NS_IF_RELEASE(infoObject);
  PR_FREEIF(layer);
  return NS_ERROR_FAILURE;
}
