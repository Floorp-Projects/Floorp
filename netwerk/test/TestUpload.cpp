/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include <algorithm>
#ifdef WIN32 
#include <windows.h>
#endif

#include "nsIComponentRegistrar.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"

#include "nsIUploadChannel.h"

#include "prlog.h"
//
// set NSPR_LOG_MODULES=Test:5
//
static PRLogModuleInfo *gTestLog = nullptr;
#define LOG(args) PR_LOG(gTestLog, PR_LOG_DEBUG, args)

//-----------------------------------------------------------------------------
// InputTestConsumer
//-----------------------------------------------------------------------------

class InputTestConsumer : public nsIStreamListener
{
  virtual ~InputTestConsumer();

public:

  InputTestConsumer();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
};

InputTestConsumer::InputTestConsumer()
{
}

InputTestConsumer::~InputTestConsumer()
{
}

NS_IMPL_ISUPPORTS(InputTestConsumer,
                  nsIStreamListener,
                  nsIRequestObserver)

NS_IMETHODIMP
InputTestConsumer::OnStartRequest(nsIRequest *request, nsISupports* context)
{
  LOG(("InputTestConsumer::OnStartRequest\n"));
  return NS_OK;
}

NS_IMETHODIMP
InputTestConsumer::OnDataAvailable(nsIRequest *request, 
                                   nsISupports* context,
                                   nsIInputStream *aIStream, 
                                   uint64_t aSourceOffset,
                                   uint32_t aLength)
{
  char buf[1025];
  uint32_t amt, size;
  nsresult rv;

  while (aLength) {
    size = std::min<uint32_t>(aLength, sizeof(buf));
    rv = aIStream->Read(buf, size, &amt);
    if (NS_FAILED(rv)) {
      NS_ASSERTION((NS_BASE_STREAM_WOULD_BLOCK != rv), 
                   "The stream should never block.");
      return rv;
    }
    aLength -= amt;
  }
  return NS_OK;
}


NS_IMETHODIMP
InputTestConsumer::OnStopRequest(nsIRequest *request, nsISupports* context,
                                 nsresult aStatus)
{
    LOG(("InputTestConsumer::OnStopRequest [status=%x]\n", aStatus));
    QuitPumpingEvents();
    return NS_OK;
}


int
main(int argc, char* argv[])
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv;

    if (argc < 2) {
        printf("usage: %s <url> <file-to-upload>\n", argv[0]);
        return -1;
    }
    char* uriSpec  = argv[1];
    char* fileName = argv[2];

    gTestLog = PR_NewLogModule("Test");

    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);

        // first thing to do is create ourselves a stream that
        // is to be uploaded.
        nsCOMPtr<nsIInputStream> uploadStream;
        rv = NS_NewPostDataStream(getter_AddRefs(uploadStream),
                                  true,
                                  nsDependentCString(fileName)); // XXX UTF-8
        if (NS_FAILED(rv)) return -1;

        // create our url.
        nsCOMPtr<nsIURI> uri;
        rv = NS_NewURI(getter_AddRefs(uri), uriSpec);
        if (NS_FAILED(rv)) return -1;

        nsCOMPtr<nsIScriptSecurityManager> secman =
          do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return -1;
        nsCOMPtr<nsIPrincipal> systemPrincipal;
        rv = secman->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
        if (NS_FAILED(rv)) return -1;

        nsCOMPtr<nsIChannel> channel;
        rv = NS_NewChannel(getter_AddRefs(channel),
                           uri,
                           systemPrincipal,
                           nsILoadInfo::SEC_NORMAL,
                           nsIContentPolicy::TYPE_OTHER);
        if (NS_FAILED(rv)) return -1;
	
        // QI and set the upload stream
        nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(channel));
        uploadChannel->SetUploadStream(uploadStream, EmptyCString(), -1);

        // create a dummy listener
        InputTestConsumer* listener;

        listener = new InputTestConsumer;
        if (!listener) {
            NS_ERROR("Failed to create a new stream listener!");
            return -1;
        }
        NS_ADDREF(listener);

        channel->AsyncOpen(listener, nullptr);

        PumpEvents();
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nullptr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");

    return 0;
}

