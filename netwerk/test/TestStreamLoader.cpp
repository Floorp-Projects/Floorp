#include <stdio.h>
#include "TestCommon.h"
#include "nsNetUtil.h"
#include "nsEventQueueUtils.h"
#include "prlog.h"

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=Test:5
//
static PRLogModuleInfo *gTestLog = nsnull;
#endif
#define LOG(args) PR_LOG(gTestLog, PR_LOG_DEBUG, args)

static PRBool gKeepRunning = PR_TRUE;
static nsIEventQueue* gEventQ = nsnull;

class MyStreamLoaderObserver : public nsIStreamLoaderObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER
};

NS_IMPL_ISUPPORTS1(MyStreamLoaderObserver, nsIStreamLoaderObserver)

NS_IMETHODIMP
MyStreamLoaderObserver::OnStreamComplete(nsIStreamLoader *loader,
                                         nsISupports     *ctxt,
                                         nsresult         status,
                                         PRUint32         resultLen,
                                         const PRUint8   *result)
{
  LOG(("OnStreamComplete [status=%x resultLen=%u]\n", status, resultLen));

  nsCOMPtr<nsIRequest> request;
  loader->GetRequest(getter_AddRefs(request));
  LOG(("  request=%p\n", request.get()));

  gKeepRunning = PR_FALSE;
  return NS_OK;
}

int main(int argc, char **argv)
{
  if (test_common_init(&argc, &argv) != 0)
    return -1;

  if (argc < 2) {
    printf("usage: %s <url>\n", argv[0]);
    return -1;
  }

#if defined(PR_LOGGING)
  gTestLog = PR_NewLogModule("Test");
#endif

  nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
  if (NS_FAILED(rv))
    return -1;

  {
    // Create the Event Queue for this thread...
    rv = NS_GetMainEventQ(&gEventQ);
    if (NS_FAILED(rv))
      return -1;

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), nsDependentCString(argv[1]));
    if (NS_FAILED(rv))
      return -1;

    nsCOMPtr<nsIChannel> chan;
    rv = NS_NewChannel(getter_AddRefs(chan), uri);
    if (NS_FAILED(rv))
      return -1;

    nsCOMPtr<nsIStreamLoaderObserver> observer = new MyStreamLoaderObserver();
    if (!observer)
      return -1;

    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), chan, observer, nsnull);
    if (NS_FAILED(rv))
      return -1;

    // Enter the message pump to allow the URL load to proceed.
    while (gKeepRunning) {
      PLEvent *e;
      gEventQ->WaitForEvent(&e);
      gEventQ->HandleEvent(e);
    }
  } // this scopes the nsCOMPtrs
  // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
  NS_ShutdownXPCOM(nsnull);
  return rv;
}
