/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 */
#include "stdio.h"

#ifdef WIN32
#include <windows.h>
#endif
#ifdef XP_OS2
#include <os2.h>
#endif

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsISocketTransportService.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);

static int gKeepRunning = 1;

class InputTestConsumer : public nsIStreamListener
{
public:

  InputTestConsumer();
  virtual ~InputTestConsumer();

  // ISupports interface...
  NS_DECL_ISUPPORTS

  // IStreamListener interface...
  NS_IMETHOD OnStartRequest(nsIChannel* channel, nsISupports* context);

  NS_IMETHOD OnDataAvailable(nsIChannel* channel, nsISupports* context,
                             nsIInputStream *aIStream, 
                             PRUint32 aSourceOffset,
                             PRUint32 aLength);

  NS_IMETHOD OnStopRequest(nsIChannel* channel, nsISupports* context,
                           nsresult aStatus, const PRUnichar* aStatusArg);

};


InputTestConsumer::InputTestConsumer()
{
  NS_INIT_REFCNT();
}

InputTestConsumer::~InputTestConsumer()
{
}


NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
NS_IMPL_ISUPPORTS(InputTestConsumer,kIStreamListenerIID);


NS_IMETHODIMP
InputTestConsumer::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
  printf("+++ OnStartRequest +++\n");
  return NS_OK;
}


NS_IMETHODIMP
InputTestConsumer::OnDataAvailable(nsIChannel* channel, 
                                   nsISupports* context,
                                   nsIInputStream *aIStream, 
                                   PRUint32 aSourceOffset,
                                   PRUint32 aLength)
{
  char buf[1025];
  while (aLength > 0) {
    PRUint32 amt;
    aIStream->Read(buf, 1024, &amt);
    if (amt == 0) break;
    buf[amt] = '\0';
    printf(buf);
    aLength -= amt;
  }

  return NS_OK;
}


NS_IMETHODIMP
InputTestConsumer::OnStopRequest(nsIChannel* channel, nsISupports* context,
                                 nsresult aStatus, const PRUnichar* aStatusArg)
{
  gKeepRunning = 0;
  printf("+++ OnStopRequest status %x +++\n", aStatus);
  return NS_OK;
}


nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
  return rv;
}

int
main(int argc, char* argv[])
{
  nsresult rv;

  if (argc < 2) {
      printf("usage: %s <host>\n", argv[0]);
      return -1;
  }

  int port;
  char* hostName = argv[1];
//nsString portString(argv[2]);

//port = portString.ToInteger(&rv);
  port = 13;

  rv = NS_AutoregisterComponents();
  if (NS_FAILED(rv)) return rv;

  // Create the Event Queue for this thread...
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = eventQService->CreateThreadEventQueue();
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIEventQueue> eventQ;
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eventQ));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE(nsISocketTransportService, sts, kSocketTransportServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsIChannel* transport;

  rv = sts->CreateTransport(hostName, port, nsnull, -1, 0, 0, &transport);
  if (NS_SUCCEEDED(rv)) {
    transport->AsyncRead(nsnull, new InputTestConsumer);

    NS_RELEASE(transport);
  }

  // Enter the message pump to allow the URL load to proceed.
  while ( gKeepRunning ) {

#ifdef WIN32
    MSG msg;

    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
#else
#ifdef XP_MAC
    /* Mac stuff is missing here! */

#else
#ifdef XP_OS2
    QMSG qmsg;

    if (WinGetMsg(0, &qmsg, 0, 0, 0))
      WinDispatchMsg(0, &qmsg);
    else
      gKeepRunning = FALSE;
#else
    PLEvent *gEvent;
    rv = eventQ->GetEvent(&gEvent);
    rv = eventQ->HandleEvent(gEvent);
#endif
#endif
#endif
  }

  sts->Shutdown();

  return 0;
}

