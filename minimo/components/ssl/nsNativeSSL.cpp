#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"

#include "nsCOMPtr.h"
#include "nsNetError.h"
#include "nsNetCID.h"

#include "nspr.h"
#include "private/pprio.h"
#include "nsString.h"
#include "nsCRT.h"

#include "nsISocketProvider.h"
#include "nsITransportSecurityInfo.h"
#include "nsISSLStatusProvider.h"
#include "nsIStringBundle.h"

#include "nsIWebProgressListener.h"

#include "nsSecureBrowserUIImpl.h"

#include <Winsock2.h>
#include <sslsock.h>
#include <Wincrypt.h>
#include <schnlsp.h>

#define MINIMO_PROPERTIES_URL "chrome://minimo/locale/minimo.properties"

static SSL_CRACK_CERTIFICATE_FN  gSslCrackCertificate = 0 ;
static SSL_FREE_CERTIFICATE_FN   gSslFreeCertificate  = 0 ;
static HINSTANCE gSchannel = NULL ;

static PRDescIdentity nsWINCESSLIOLayerIdentity;
static PRIOMethods	  nsWINCESSLIOLayerMethods;


void MessageBoxWSAError(const char * msg)
{
    char buffer[100];
    int error = WSAGetLastError();
    if (error == 0)
      return;

    sprintf(buffer, "%s error is: %d", msg, error);
    MessageBox(0, buffer, buffer, MB_APPLMODAL  | MB_TOPMOST | MB_SETFOREGROUND);
}

class nsWINCESSLSocketProvider : public nsISocketProvider
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISOCKETPROVIDER

    nsWINCESSLSocketProvider() {};
    virtual ~nsWINCESSLSocketProvider() {}
};


class nsWINCESSLSocketInfo : public nsITransportSecurityInfo                           
{
public:
  nsWINCESSLSocketInfo() 
  {
    mSecurityState          = nsIWebProgressListener::STATE_IS_BROKEN; 
    mUserAcceptedSSLProblem = PR_FALSE;
  }

  virtual ~nsWINCESSLSocketInfo() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSITRANSPORTSECURITYINFO

  nsresult Init(const char *destinationHost, const char *proxyHost, PRInt32 proxyPort)
  {
    mDestinationHost = destinationHost;
    mProxyHost       = proxyHost;
    mProxyPort       = proxyPort;

    return NS_OK;
  };

  SOCKET    mSocket;
  nsCString mDestinationHost;
  nsCString mProxyHost;
  PRInt32   mProxyPort;

  PRUint32  mSecurityState;

  PRBool    mUserAcceptedSSLProblem;

};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsWINCESSLSocketInfo, nsITransportSecurityInfo)

NS_IMETHODIMP
nsWINCESSLSocketInfo::GetSecurityState(PRUint32* state)
{
  *state = mSecurityState;
  return NS_OK;
}

NS_IMETHODIMP
nsWINCESSLSocketInfo::GetShortSecurityDescription(PRUnichar** aText) 
{
  *aText = nsnull;
  return NS_OK;
}

static int DisplaySSLProblemDialog(nsWINCESSLSocketInfo* info, PRUnichar* type)
{
  if (info->mUserAcceptedSSLProblem)
    return IDYES;

  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  if (!bundleService)
    return IDNO;
  
  nsCOMPtr<nsIStringBundle> bundle;
  bundleService->CreateBundle(MINIMO_PROPERTIES_URL, getter_AddRefs(bundle));
  
  if (!bundle)
    return IDNO;
  
  nsXPIDLString message;
  nsXPIDLString title;
  bundle->GetStringFromName(type, getter_Copies(message));
  bundle->GetStringFromName(NS_LITERAL_STRING("securityWarningTitle").get(), getter_Copies(title));
  
  int result = MessageBoxW(0, message.get(), title.get(), MB_YESNO | MB_ICONWARNING | MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND );
  
  if (result == IDYES)
    info->mUserAcceptedSSLProblem = PR_TRUE;
  
  return result;
}


static int SSLValidationHook(DWORD dwType, LPVOID pvArg, DWORD dwChainLen, LPBLOB pCertChain, DWORD dwFlags )
{
  nsWINCESSLSocketInfo* info = (nsWINCESSLSocketInfo*)pvArg;
  info->mSecurityState = nsIWebProgressListener::STATE_IS_BROKEN;
  
  if ( SSL_CERT_X509 != dwType )
    return SSL_ERR_BAD_DATA;
  
  if ( SSL_CERT_FLAG_ISSUER_UNKNOWN & dwFlags )
  {
    if (DisplaySSLProblemDialog(info, L"confirmUnknownIssuer") != IDYES)
      return SSL_ERR_CERT_UNKNOWN;
  }
  
  // see:
  // http://groups.google.com/groups?q=SslCrackCertificate&hl=en&lr=&ie=UTF-8&oe=UTF-8&selm=uQf1VcLWBHA.1632%40tkmsftngp05&rnum=3
  
  if (!gSslCrackCertificate)
  {
    gSchannel = LoadLibrary ( "schannel.dll" ) ;
    if ( NULL != gSchannel )
    {
      gSslCrackCertificate = (SSL_CRACK_CERTIFICATE_FN)GetProcAddress ( gSchannel, SSL_CRACK_CERTIFICATE_NAME ) ;
      gSslFreeCertificate  = (SSL_FREE_CERTIFICATE_FN )GetProcAddress ( gSchannel, SSL_FREE_CERTIFICATE_NAME ) ;
    }
    
    if (!gSslCrackCertificate || !gSslFreeCertificate)
    {
      MessageBox(0, "Could not find the right stuff in the default security library", "schannel.dll", MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND);
      return SSL_ERR_BAD_DATA;
    }
  }
  
  X509Certificate * cert = 0;
  if ( !gSslCrackCertificate( pCertChain->pBlobData, pCertChain->cbSize, TRUE, &cert ) )
    return SSL_ERR_BAD_DATA;

  
  // all we have to do is compare the name in the cert to
  // what the hostname was when we created this socket.

  char* subject = strstr(cert->pszSubject, "CN=");
  if (!subject)
    return SSL_ERR_BAD_DATA;

  subject = subject+3; // pass CN=

  // check to see if the common name has any ws
  char* end = strpbrk(subject, "' \t,");
  char save;
  if (end)
  {
    save = end[0];
    end[0] = '\0';
  }
  
  char* destinationHost = (char*) info->mDestinationHost.get();
  
  // if we run across a cert with a *, pass the machine name, and find the host name.
  if (subject[0] == '*')
  {
    destinationHost = strchr(destinationHost, '.');
    if (destinationHost)
      destinationHost++;
    
    subject = subject + 2; // pass the *.
  }
  
  // do the check
  int res = SSL_ERR_CERT_UNKNOWN;

  if (! _stricmp (destinationHost, subject))
  {
    info->mSecurityState = nsIWebProgressListener::STATE_IS_SECURE | nsIWebProgressListener.STATE_SECURE_HIGH;
    res = SSL_ERR_OKAY;
  }
  else
  {
    if (DisplaySSLProblemDialog(info, L"confirmMismatch") == IDYES)
      res = SSL_ERR_OKAY;
  }
  
  if (end)
    end[0] = save;
  
  gSslFreeCertificate ( cert ) ;

  return res;
}


struct sockaddr * 
nspr2winsock(const PRNetAddr *addr, int *len)
{
  struct sockaddr_in *ws = (struct sockaddr_in*) malloc(sizeof(*ws));
  if (!ws)
    return NULL;
  
  memset(ws, 0, sizeof(*ws));
  
  // family
  ws->sin_family = AF_INET;

  // ip
  memcpy(&ws->sin_addr, &addr->inet.ip, sizeof(ws->sin_addr));

  // port
  ws->sin_port = addr->inet.port;

  *len = sizeof(*ws);  
  return (struct sockaddr *)ws;
}

static PRStatus PR_CALLBACK
nsWINCESSLIOLayerConnect(PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime /*timeout*/)
{
    nsWINCESSLSocketInfo * info = (nsWINCESSLSocketInfo*) fd->secret;
    if (info == NULL) 
      return PR_FAILURE;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
    if (!s)
    {
      MessageBox(0, "Failed to create a socket", "connect()", MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND);
      return PR_FAILURE;
    }

    info->mSocket = s;
    
    // set the socket to secure mode
    DWORD dwFlag = SO_SEC_SSL ;
    DWORD error = setsockopt (s, SOL_SOCKET, SO_SECURE, (char *)&dwFlag, sizeof(dwFlag));
    if (error != 0)
    {
      MessageBox(0, "Failed to setsockopt", "connect()", MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND);
      return PR_FAILURE;
    }

    // set the certificate validation callback. 
    SSLVALIDATECERTHOOK sslHook ;
    sslHook.HookFunc = SSLValidationHook;
    
    NS_ADDREF(info);
    sslHook.pvArg = (void *) info; // XXXX Leak! what is the lifespan of this?
    
    if ( WSAIoctl (s, 
                   SO_SSL_SET_VALIDATE_CERT_HOOK,
                   &sslHook, 
                   sizeof sslHook, 
                   NULL, 
                   0, 
                   NULL, 
                   NULL, 
                   NULL) )
    {
      MessageBox(0, "SO_SSL_SET_VALIDATE_CERT_HOOK", "connect()", MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND);
      return PR_FAILURE;
    }

    int len;
    struct sockaddr* ws = nspr2winsock(addr, &len);
    if (!ws)
      return PR_FAILURE;

    int result = connect(s, ws, len);

    if (result == 0)
      return PR_SUCCESS;

    MessageBoxWSAError("connect()");

    return PR_FAILURE;
}

static PRStatus PR_CALLBACK
nsWINCESSLIOLayerClose(PRFileDesc *fd)
{
    nsWINCESSLSocketInfo * info = (nsWINCESSLSocketInfo*) fd->secret;
    PRDescIdentity id = PR_GetLayersIdentity(fd);

    if (info && id == nsWINCESSLIOLayerIdentity)
    {
        NS_RELEASE(info);
        fd->identity = PR_INVALID_IO_LAYER;
    }

    //    closesocket(info->mSocket);
    //    NS_RELEASE(info);
    //    fd->secret = nsnull;
    
    return PR_SUCCESS;
}



static PRInt32 PR_CALLBACK
nsWINCESSLIOLayerAvailable(PRFileDesc *fd)
{
  MessageBox(0, "available.", "available.", MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND);
  return 1;
}

static PRInt32 PR_CALLBACK
nsWINCESSLIOLayerRead(PRFileDesc* fd, void* buf, PRInt32 amount)
{
  nsWINCESSLSocketInfo * info = (nsWINCESSLSocketInfo*) fd->secret;
  if (info == NULL) 
    return PR_OUT_OF_MEMORY_ERROR;

  WSASetLastError (0) ;  
  PRInt32 rv = recv(info->mSocket, (char*)buf, amount, 0);
  MessageBoxWSAError("read");

  return rv;
}

static PRInt32 PR_CALLBACK
nsWINCESSLIOLayerWrite(PRFileDesc* fd, const void* buf, PRInt32 amount)
{
  nsWINCESSLSocketInfo * info = (nsWINCESSLSocketInfo*) fd->secret;
  if (info == NULL) 
    return PR_OUT_OF_MEMORY_ERROR;
    
  WSASetLastError (0) ;  
  PRInt32 rv = send(info->mSocket, (char*)buf, amount, 0);
  MessageBoxWSAError("write");

  return rv;
}


static PRInt16 PR_CALLBACK nsWINCESSLIOLayerPoll(PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
  nsWINCESSLSocketInfo * info = (nsWINCESSLSocketInfo*) fd->secret;
  if (info == NULL) 
    return PR_OUT_OF_MEMORY_ERROR;

  *out_flags = PR_POLL_READ | PR_POLL_WRITE | PR_POLL_EXCEPT;
  return 1;
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsWINCESSLSocketProvider, nsISocketProvider)

NS_IMETHODIMP
nsWINCESSLSocketProvider::NewSocket(PRInt32 family,
                                    const char *host, 
                                    PRInt32 port,
                                    const char *proxyHost,
                                    PRInt32 proxyPort,
                                    PRUint32 flags,
                                    PRFileDesc **result, 
                                    nsISupports **socksInfo)
{
  static PRBool firstTime = PR_TRUE;
  if (firstTime)  {
    
    nsWINCESSLIOLayerIdentity		     = PR_GetUniqueIdentity("WINCESSL layer");
    nsWINCESSLIOLayerMethods		     = *PR_GetDefaultIOMethods();
    
    nsWINCESSLIOLayerMethods.connect   = nsWINCESSLIOLayerConnect;
    nsWINCESSLIOLayerMethods.close     = nsWINCESSLIOLayerClose;
    nsWINCESSLIOLayerMethods.available = nsWINCESSLIOLayerAvailable;
    nsWINCESSLIOLayerMethods.write     = nsWINCESSLIOLayerWrite;
    nsWINCESSLIOLayerMethods.read      = nsWINCESSLIOLayerRead;
    nsWINCESSLIOLayerMethods.poll      = nsWINCESSLIOLayerPoll;

    firstTime = PR_FALSE;
  }
  
  PRFileDesc *fd = PR_OpenTCPSocket(family);
  if (!fd)
    return NS_ERROR_OUT_OF_MEMORY;
  
  PRFileDesc *	layer;
  PRStatus	rv;
  
  layer = PR_CreateIOLayerStub(nsWINCESSLIOLayerIdentity, &nsWINCESSLIOLayerMethods);
  if (! layer)
    return NS_ERROR_UNEXPECTED;
  
  nsWINCESSLSocketInfo * infoObject = new nsWINCESSLSocketInfo();
  if (!infoObject)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(infoObject);

  layer->secret = (PRFilePrivate*) infoObject;
  rv = PR_PushIOLayer(fd, PR_GetLayersIdentity(fd), layer);
  if (NS_FAILED(rv))
    return NS_ERROR_UNEXPECTED;
  
  
  nsresult initrv = infoObject->Init(host, proxyHost, proxyPort);
  if (NS_FAILED(initrv))
  {
    MessageBox(0, "Can not create ssl socket.", "Can not create ssl socket.", MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND);
    return NS_ERROR_FAILURE;
  }

  *result    = fd;
  *socksInfo = (nsISupports*) (nsITransportSecurityInfo*) infoObject;
  return NS_OK;

}

NS_IMETHODIMP
nsWINCESSLSocketProvider::AddToSocket(PRInt32 family,
                                      const char *host,
                                      PRInt32 port,
                                      const char *proxyHost,
                                      PRInt32 proxyPort,
                                      PRUint32 flags,
                                      PRFileDesc *sock, 
                                      nsISupports **socksInfo)
{
  return NS_ERROR_SOCKET_CREATE_FAILED;
}




//------------------------------------------------------------------------------
//  XPCOM REGISTRATION BELOW
//------------------------------------------------------------------------------


#define NS_WINCESSLSOCKETPROVIDER_CID                  \
{ /* 40f0fb5a-9819-4a48-bca9-da8170fd8b58 */           \
    0x40f0fb5a,                                        \
    0x9819,                                            \
    0x4a48,                                            \
    {0xbc, 0xa9, 0xda, 0x81, 0x70, 0xfd, 0x8b, 0x58}   \
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWINCESSLSocketProvider)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecureBrowserUIImpl)

static const nsModuleComponentInfo components[] =
{
    {  "nsWINCESSLSocketProvider",
       NS_WINCESSLSOCKETPROVIDER_CID,
       NS_SSLSOCKETPROVIDER_CONTRACTID,
       nsWINCESSLSocketProviderConstructor
    },
    
    {
      NS_SECURE_BROWSER_UI_CLASSNAME,
      NS_SECURE_BROWSER_UI_CID,
      NS_SECURE_BROWSER_UI_CONTRACTID,
      nsSecureBrowserUIImplConstructor
    },

};

NS_IMPL_NSGETMODULE(nsNativeSSLModule, components)
