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

#include "nsNSSIOLayer.h"
#include "nsNSSCallbacks.h"

#include "prlog.h"
#include "nsISecurityManagerComponent.h"
#include "nsIServiceManager.h"
#include "nsIWebProgressListener.h"
#include "nsIChannel.h"
#include "nsIBadCertListener.h"
#include "nsNSSCertificate.h"

#include "nsXPIDLString.h"

#include "nsNSSHelper.h"

#include "ssl.h"
#include "secerr.h"
#include "sslerr.h"
#include "certdb.h"

//#define DEBUG_SSL_VERBOSE //Enable this define to get minimal 
                            //reports when doing SSL read/write
                            
//#define DUMP_BUFFER  //Enable this define along with
                       //DEBUG_SSL_VERBOSE to dump SSL
                       //read/write buffer to a log.
                       //Uses PR_LOG except on Mac where
                       //we always write out to our own
                       //file.

static nsISecurityManagerComponent* gNSSService = nsnull;
static PRBool firstTime = PR_TRUE;
static PRDescIdentity nsSSLIOLayerIdentity;
static PRIOMethods nsSSLIOLayerMethods;

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

nsNSSSocketInfo::nsNSSSocketInfo()
  : mFd(nsnull),
    mSecurityState(nsIWebProgressListener::STATE_IS_INSECURE),
    mForceHandshake(PR_FALSE),
    mUseTLS(PR_FALSE)
{ 
  NS_INIT_ISUPPORTS();
}

nsNSSSocketInfo::~nsNSSSocketInfo()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsNSSSocketInfo,
                              nsITransportSecurityInfo,
                              nsISSLSocketControl,
                              nsIInterfaceRequestor)

NS_IMETHODIMP
nsNSSSocketInfo::GetHostName(char * *aHostName)
{
  if (mHostName.IsEmpty())
    *aHostName = nsnull;
  else
    *aHostName = mHostName.ToNewCString();
  return NS_OK;
}


nsresult 
nsNSSSocketInfo::SetHostName(const char *aHostName)
{
  mHostName.AssignWithConversion(aHostName);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetHostPort(PRInt32 *aPort)
{
  *aPort = mHostPort;
  return NS_OK;
}

nsresult 
nsNSSSocketInfo::SetHostPort(PRInt32 aPort)
{
  mHostPort = aPort;
  return NS_OK;
}


NS_IMETHODIMP
nsNSSSocketInfo::GetProxyName(char** aName)
{
  if (mProxyName.IsEmpty())
    *aName = nsnull;
  else
    *aName = mProxyName.ToNewCString();
  return NS_OK;
}


nsresult 
nsNSSSocketInfo::SetProxyName(const char* aName)
{
  mProxyName.AssignWithConversion(aName);
  return NS_OK;
}


NS_IMETHODIMP 
nsNSSSocketInfo::GetProxyPort(PRInt32* aPort)
{
  *aPort = mProxyPort;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetProxyPort(PRInt32 aPort)
{
  mProxyPort = aPort;
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
    *aText = mShortDesc.ToNewUnicode();
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
  if (!mCallbacks) return NS_ERROR_FAILURE;

  // Proxy of the channel callbacks should probably go here, rather
  // than in the password callback code

  return mCallbacks->GetInterface(uuid, result);
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
nsNSSSocketInfo::GetUseTLS(PRBool* aResult)
{
  *aResult = mUseTLS;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetUseTLS(PRBool useTLS)
{
  mUseTLS = useTLS;
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

  PR_Write(mFd, nsnull, 0);
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
  
  PRBool nonblock = sockopt.value.non_blocking;
  sockopt.option = PR_SockOpt_Nonblocking;
  sockopt.value.non_blocking = PR_FALSE;
  PR_SetSocketOption(fd, &sockopt);
  
  nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*) fd->secret;
  
  status = fd->lower->methods->connect(fd->lower, addr, 
                                       PR_INTERVAL_NO_TIMEOUT);
  if (status != PR_SUCCESS) {
    PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("[%p] Lower layer connect error: %d\n",
                                      (void*)fd, PR_GetError()));
    goto loser;
  }
  
  PRBool forceHandshake, useTLS;
  infoObject->GetForceHandshake(&forceHandshake);
  infoObject->GetUseTLS(&useTLS);
  
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] Connect: forceHandshake = %d, useTLS = %d\n",
                                    (void*)fd, forceHandshake, useTLS));

  if (!useTLS && forceHandshake) {
    PRInt32 res = SSL_ForceHandshake(fd);
    
    if (res == -1) {
      PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("[%p] ForceHandshake failure -- error %d\n",
                                        (void*)fd, PR_GetError()));
      status = PR_FAILURE;
    }
  }

 loser:
  sockopt.option = PR_SockOpt_Nonblocking;
  sockopt.value.non_blocking = nonblock;
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
  nsNSSSocketInfo *infoObject = (nsNSSSocketInfo*) popped->secret;
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

#ifdef DEBUG_SSL_VERBOSE
static PRInt32 PR_CALLBACK
nsSSLIOLayerRead(PRFileDesc* fd, void* buf, PRInt32 amount)
{
  if (!fd || !fd->lower)
    return PR_FAILURE;

  PRInt32 bytesRead = fd->lower->methods->read(fd->lower, buf, amount);
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] read %d bytes\n", (void*)fd, bytesRead));
  DEBUG_DUMP_BUFFER((unsigned char*)buf, bytesRead);
  return bytesRead;
}

static PRInt32 PR_CALLBACK
nsSSLIOLayerWrite(PRFileDesc* fd, const void* buf, PRInt32 amount)
{
  if (!fd || !fd->lower)
    return PR_FAILURE;
    
  DEBUG_DUMP_BUFFER((unsigned char*)buf, amount);
  PRInt32 bytesWritten = fd->lower->methods->write(fd->lower, buf, amount);
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] wrote %d bytes\n", (void*)fd, bytesWritten));
  return bytesWritten;
}

#endif // DEBUG_SSL_VERBOSE

nsresult InitNSSMethods()
{
  nsSSLIOLayerIdentity = PR_GetUniqueIdentity("NSS layer");
  nsSSLIOLayerMethods  = *PR_GetDefaultIOMethods();
  
  nsSSLIOLayerMethods.connect = nsSSLIOLayerConnect;
  nsSSLIOLayerMethods.close = nsSSLIOLayerClose;
  nsSSLIOLayerMethods.available = nsSSLIOLayerAvailable;

#ifdef DEBUG_SSL_VERBOSE
  nsSSLIOLayerMethods.read = nsSSLIOLayerRead;
  nsSSLIOLayerMethods.write = nsSSLIOLayerWrite;
#endif
  
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
                      PRBool useTLS)
{
  if (firstTime) {
    nsresult rv = InitNSSMethods();
    if (NS_FAILED(rv)) return rv;
    firstTime = PR_FALSE;
  }

  PRFileDesc* sock = PR_OpenTCPSocket(PR_AF_INET6);
  if (!sock) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = nsSSLIOLayerAddToSocket(host, port, proxyHost, proxyPort,
                                        sock, info, useTLS);
  if (NS_FAILED(rv)) {
    PR_Close(sock);
    return rv;
  }

  *fd = sock;
  return NS_OK;
}

static PRBool
nsCertErrorNeedsDialog(int error)
{
  return ((error == SEC_ERROR_UNKNOWN_ISSUER) ||
          (error == SEC_ERROR_UNTRUSTED_ISSUER) ||
          (error == SSL_ERROR_BAD_CERT_DOMAIN) ||
          (error == SSL_ERROR_POST_WARNING) ||
          (error == SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE) ||
          (error == SEC_ERROR_CA_CERT_INVALID) ||
          (error == SEC_ERROR_EXPIRED_CERTIFICATE));
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
    rv = badCertHandler->UnknownIssuer(csi, callBackCert, &addType, &retVal);
    break;
  case SSL_ERROR_BAD_CERT_DOMAIN:
    {
      char *url = SSL_RevealURL(sslSocket);
      NS_ASSERTION(url, "could not find valid URL in ssl socket");
      nsAutoString autoURL = NS_ConvertASCIItoUCS2(url);
      PRUnichar *autoUnichar = autoURL.ToNewUnicode();
      NS_ASSERTION(autoUnichar, "Could not allocate new PRUnichar");
      rv = badCertHandler->MismatchDomain(csi, autoUnichar,
                                          callBackCert, &retVal);
      Recycle(autoUnichar);
      if (NS_SUCCEEDED(rv) && retVal) {
        rv = CERT_AddOKDomainName(peerCert, url);
      }
      PR_Free(url);
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
  default:
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

static SECStatus
nsNSSBadCertHandler(void *arg, PRFileDesc *sslSocket)
{
  SECStatus rv = SECFailure;
  int error;
  nsNSSSocketInfo* infoObject = (nsNSSSocketInfo *)arg;
  CERTCertificate *peerCert;
  nsNSSCertificate *nssCert;


  peerCert = SSL_PeerCertificate(sslSocket);
  nssCert = new nsNSSCertificate(peerCert);
  if (!nssCert) {
    return SECFailure;
  } 
  NS_ADDREF(nssCert);
  while (rv != SECSuccess) {
    error = PR_GetError();
    if (!nsCertErrorNeedsDialog(error)) {
      // Some weird error we don't really know how to handle.
      break;
    }
    if (!nsContinueDespiteCertError(infoObject, sslSocket, 
                                    error, nssCert)) {
      break;
    }
    rv = verifyCertAgain(peerCert, sslSocket, infoObject);
  }
  NS_RELEASE(nssCert);
  CERT_DestroyCertificate(peerCert); 
  return rv;
}

nsresult
nsSSLIOLayerAddToSocket(const char* host,
                        PRInt32 port,
                        const char* proxyHost,
                        PRInt32 proxyPort,
                        PRFileDesc* fd,
                        nsISupports** info,
                        PRBool useTLS)
{
  PRFileDesc* layer = nsnull;
  nsresult rv;
  PRInt32 ret;

  if (firstTime) {
    rv = InitNSSMethods();
    if (NS_FAILED(rv)) return rv;
    firstTime = PR_FALSE;
  }

  nsNSSSocketInfo* infoObject = new nsNSSSocketInfo();
  if (!infoObject) return NS_ERROR_FAILURE;
  
  NS_ADDREF(infoObject);
  
  infoObject->SetHostName(host);
  infoObject->SetHostPort(port);
  infoObject->SetProxyName(proxyHost);
  infoObject->SetProxyPort(proxyPort);
  infoObject->SetUseTLS(useTLS);

  PRFileDesc* sslSock = SSL_ImportFD(nsnull, fd);
  if (!sslSock) {
    NS_ASSERTION(PR_FALSE, "NSS: Error importing socket");
    goto loser;
  }

  infoObject->SetFileDescPtr(sslSock);
  SSL_SetPKCS11PinArg(sslSock, (nsIInterfaceRequestor*)infoObject);
  SSL_HandshakeCallback(sslSock, HandshakeCallback, infoObject);
  SSL_GetClientAuthDataHook(sslSock, (SSLGetClientAuthData)NSS_GetClientAuthData,
                            nsnull);

  ret = SSL_SetURL(sslSock, host);
  if (ret == -1) {
    NS_ASSERTION(PR_FALSE, "NSS: Error setting server name");
    goto loser;
  }
  
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
  if ((useTLS || proxyHost) &&
      SECSuccess != SSL_OptionSet(sslSock, SSL_SECURITY, PR_FALSE))
    goto loser;

  if (SECSuccess != SSL_OptionSet(sslSock, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE)) {
    goto loser;
  }
  if (SECSuccess != SSL_OptionSet(sslSock, SSL_ENABLE_FDX, PR_TRUE)) {
    goto loser;
  }
  if (SECSuccess != SSL_BadCertHook(sslSock, (SSLBadCertHandler) nsNSSBadCertHandler,
                                    infoObject)) {
    goto loser;
  }
  return NS_OK;
 loser:
  NS_IF_RELEASE(infoObject);
  PR_FREEIF(layer);
  return NS_ERROR_FAILURE;
}
  
