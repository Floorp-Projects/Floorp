/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 cin et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include "TestCommon.h"
#include "nsNetUtil.h"
#include "nsIIncrementalDownload.h"
#include "nsIRequestObserver.h"
#include "nsIProgressEventSink.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "prprf.h"
#include "prenv.h"

//-----------------------------------------------------------------------------

class FetchObserver : public nsIRequestObserver
                    , public nsIProgressEventSink
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIPROGRESSEVENTSINK
};

NS_IMPL_ISUPPORTS2(FetchObserver, nsIRequestObserver,
                   nsIProgressEventSink)

NS_IMETHODIMP
FetchObserver::OnStartRequest(nsIRequest *request, nsISupports *context)
{
  printf("FetchObserver::OnStartRequest\n");
  return NS_OK;
}

NS_IMETHODIMP
FetchObserver::OnProgress(nsIRequest *request, nsISupports *context,
                          PRUint64 progress, PRUint64 progressMax)
{
  printf("FetchObserver::OnProgress [%lu/%lu]\n",
         (unsigned long)progress, (unsigned long)progressMax);
  return NS_OK;
}

NS_IMETHODIMP
FetchObserver::OnStatus(nsIRequest *request, nsISupports *context,
                        nsresult status, const PRUnichar *statusText)
{
  return NS_OK;
}

NS_IMETHODIMP
FetchObserver::OnStopRequest(nsIRequest *request, nsISupports *context,
                             nsresult status)
{
  printf("FetchObserver::OnStopRequest [status=%x]\n", status);

  QuitPumpingEvents();
  return NS_OK;
}

//-----------------------------------------------------------------------------

static nsresult
DoIncrementalFetch(const char *uriSpec, const char *resultPath, PRInt32 chunkSize,
                   PRInt32 interval)
{
  nsCOMPtr<nsILocalFile> resultFile;
  nsresult rv = NS_NewNativeLocalFile(nsDependentCString(resultPath),
                                      PR_FALSE, getter_AddRefs(resultFile));
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

  rv = download->Start(observer, nsnull);
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

  nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
  if (NS_FAILED(rv))
    return -1;

  PRInt32 chunkSize = atoi(argv[3]);
  PRInt32 interval = atoi(argv[4]);

  rv = DoIncrementalFetch(argv[1], argv[2], chunkSize, interval);
  if (NS_FAILED(rv))
    fprintf(stderr, "ERROR: DoIncrementalFetch failed [%x]\n", rv);

  NS_ShutdownXPCOM(nsnull);
  return 0;
}
