/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "stdio.h"
#include <windows.h>

#include "nscore.h"
#include "nsXPComCIID.h"
#include "nsISocketTransportService.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsITransport.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"

#ifdef XP_PC
#define XPCOM_DLL  "xpcom32.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define XPCOM_DLL  "libxpcom.so"
#endif
#endif

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
  NS_IMETHOD OnStartBinding(nsISupports* context);

  NS_IMETHOD OnDataAvailable(nsISupports* context,
                             nsIInputStream *aIStream, 
                             PRUint32 aLength);

  NS_IMETHOD OnStopBinding(nsISupports* context,
                           nsresult aStatus,
                           nsIString* aMsg);
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
InputTestConsumer::OnStartBinding(nsISupports* context)
{
  printf("+++ OnStartBinding +++\n");
  return NS_OK;
}


NS_IMETHODIMP
InputTestConsumer::OnDataAvailable(nsISupports* context,
                                   nsIInputStream *aIStream, 
                                   PRUint32 aLength)
{
  char buf[1025];
  while (aLength > 0) {
    PRUint32 amt;
    nsresult rv = aIStream->Read(buf, 1024, &amt);
    buf[amt] = '\0';
    printf(buf);
    aLength -= amt;
  }

  return NS_OK;
}


NS_IMETHODIMP
InputTestConsumer::OnStopBinding(nsISupports* context,
                         nsresult aStatus,
                         nsIString* aMsg)
{
  gKeepRunning = 0;
  printf("+++ OnStopBinding +++\n");
  return NS_OK;
}





int
main(int argc, char* argv[])
{
  nsresult rv;

  if (argc < 1) {
      printf("usage: %s <host>\n", argv[0]);
      return -1;
  }

  int port;
  char* hostName = argv[1];
//nsString portString(argv[2]);

//port = portString.ToInteger(&rv);
  port = 13;

  // XXX why do I have to do this?!
  nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
  rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                        "components");
  if (NS_FAILED(rv)) return rv;

  // Create the Event Queue for this thread...
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  PLEventQueue* eventQ;
  rv = eventQService->CreateThreadEventQueue();
  if (NS_FAILED(rv)) return rv;

  eventQService->GetThreadEventQueue(PR_CurrentThread(), &eventQ);
  
  NS_WITH_SERVICE(nsISocketTransportService, sts, kSocketTransportServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsITransport* transport;

  rv = sts->CreateTransport(hostName, port, &transport);
  if (NS_SUCCEEDED(rv)) {
    transport->AsyncRead(nsnull, eventQ, new InputTestConsumer);

    NS_RELEASE(transport);
  }

  // Enter the message pump to allow the URL load to proceed.
  while ( gKeepRunning ) {
#ifdef XP_PC
    MSG msg;

    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
#endif
  }

  sts->Shutdown();
  NS_RELEASE(sts);
  NS_RELEASE(eventQService);

  return 0;
}

