#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsSOCKSSocketProvider.h"
#include "nsSOCKSIOLayer.h"

//////////////////////////////////////////////////////////////////////////

nsSOCKSSocketProvider::nsSOCKSSocketProvider()
{
   NS_INIT_REFCNT();
}

nsresult
nsSOCKSSocketProvider::Init()
{
   nsresult rv = NS_OK;
   return rv;
}

nsSOCKSSocketProvider::~nsSOCKSSocketProvider()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsSOCKSSocketProvider, nsISocketProvider, nsISOCKSSocketProvider);

NS_METHOD
nsSOCKSSocketProvider::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
   nsresult rv;
   
   nsSOCKSSocketProvider * inst;
   
   if (NULL == aResult) {
      rv = NS_ERROR_NULL_POINTER;
      return rv;
   }
   *aResult = NULL;
   if (NULL != aOuter) {
      rv = NS_ERROR_NO_AGGREGATION;
      return rv;
   }
   
   NS_NEWXPCOM(inst, nsSOCKSSocketProvider);
   if (NULL == inst) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      return rv;
   }
   NS_ADDREF(inst);
   rv = inst->QueryInterface(aIID, aResult);
   NS_RELEASE(inst);

   return rv;
}

NS_IMETHODIMP
nsSOCKSSocketProvider::NewSocket(const char *host, 
				 PRInt32 port,
				 const char *proxyHost,
				 PRInt32 proxyPort,
				 PRFileDesc **_result, 
				 nsISupports **socksInfo)
{
   nsresult rv = nsSOCKSIOLayerNewSocket(host, 
					 port,
					 proxyHost,
					 proxyPort,
					 _result, 
					 socksInfo);

   return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}

NS_IMETHODIMP
nsSOCKSSocketProvider::AddToSocket(const char *host,
				 PRInt32 port,
				 const char *proxyHost,
				 PRInt32 proxyPort,
				 PRFileDesc *socket, 
				 nsISupports **socksInfo)
{
   nsresult rv = nsSOCKSIOLayerAddToSocket(host, 
					   port,
					   proxyHost,
					   proxyPort,
					   socket, 
					   socksInfo);

   return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}
				   
