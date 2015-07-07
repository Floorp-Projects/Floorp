/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 cin et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <inttypes.h>
#include <stdlib.h>
#include "TestCommon.h"
#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsIIncrementalDownload.h"
#include "nsIRequestObserver.h"
#include "nsIProgressEventSink.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "prprf.h"
#include "prenv.h"
#include "mozilla/Attributes.h"

//-----------------------------------------------------------------------------

class FetchObserver final : public nsIRequestObserver
                          , public nsIProgressEventSink
{
  ~FetchObserver() {}
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIPROGRESSEVENTSINK
};

NS_IMPL_ISUPPORTS(FetchObserver, nsIRequestObserver,
                  nsIProgressEventSink)

NS_IMETHODIMP
FetchObserver::OnStartRequest(nsIRequest *request, nsISupports *context)
{
  printf("FetchObserver::OnStartRequest\n");
  return NS_OK;
}

NS_IMETHODIMP
FetchObserver::OnProgress(nsIRequest *request, nsISupports *context,
                          int64_t progress, int64_t progressMax)
{
  printf("FetchObserver::OnProgress [%" PRId64 "/%" PRId64 "]\n",
         progress, progressMax);
  return NS_OK;
}

NS_IMETHODIMP
FetchObserver::OnStatus(nsIRequest *request, nsISupports *context,
                        nsresult status, const char16_t *statusText)
{
  return NS_OK;
}

NS_IMETHODIMP
FetchObserver::OnStopRequest(nsIRequest *request, nsISupports *context,
                             nsresult status)
{
  printf("FetchObserver::OnStopRequest [status=%x]\n",
         static_cast<uint32_t>(status));

  QuitPumpingEvents();
  return NS_OK;
}

//-----------------------------------------------------------------------------

static nsresult
DoIncrementalFetch(const char *uriSpec, const char *resultPath, int32_t chunkSize,
                   int32_t interval)
{
  nsCOMPtr<nsIFile> resultFile;
  nsresult rv = NS_NewNativeLocalFile(nsDependentCString(resultPath),
                                      false, getter_AddRefs(resultFile));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), uriSpec);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIRequestObserver> observer = new FetchObserver();
  if (!observer)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsCOMPtr<nsIIncrementalDownload> download =
      do_CreateInstance(NS_INCREMENTALDOWNLOAD_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  rv = download->Init(uri, resultFile, chunkSize, interval);
  if (NS_FAILED(rv))
    return rv;

  rv = download->Start(observer, nullptr);
  if (NS_FAILED(rv))
    return rv;

  PumpEvents();
  return NS_OK;
}

int
main(int argc, char **argv)
{
  if (PR_GetEnv("MOZ_BREAK_ON_MAIN"))
    NS_BREAK();

  if (argc < 5) {
    fprintf(stderr, "USAGE: TestIncrementalDownload <url> <file> <chunksize> <interval-in-seconds>\n");
    return -1;
  }

  nsresult rv = NS_InitXPCOM2(nullptr, nullptr, nullptr);
  if (NS_FAILED(rv))
    return -1;

  int32_t chunkSize = atoi(argv[3]);
  int32_t interval = atoi(argv[4]);

  rv = DoIncrementalFetch(argv[1], argv[2], chunkSize, interval);
  if (NS_FAILED(rv))
    fprintf(stderr, "ERROR: DoIncrementalFetch failed [%x]\n",
            static_cast<uint32_t>(rv));

  NS_ShutdownXPCOM(nullptr);
  return 0;
}
