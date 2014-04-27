/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include <stdio.h>
#include <stdlib.h>
#include "nsIServiceManager.h"
#include "nsPIDNSService.h"
#include "nsIDNSListener.h"
#include "nsIDNSRecord.h"
#include "nsICancelable.h"
#include "nsCOMPtr.h"
#include "nsStringAPI.h"
#include "nsNetCID.h"
#include "prinrval.h"
#include "prthread.h"
#include "prnetdb.h"
#include "nsXPCOM.h"
#include "nsServiceManagerUtils.h"

class myDNSListener : public nsIDNSListener
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    myDNSListener(const char *host, int32_t index)
        : mHost(host)
        , mIndex(index) {}
    virtual ~myDNSListener() {}

    NS_IMETHOD OnLookupComplete(nsICancelable *request,
                                nsIDNSRecord  *rec,
                                nsresult       status)
    {
        printf("%d: OnLookupComplete called [host=%s status=%x rec=%p]\n",
            mIndex, mHost.get(), static_cast<uint32_t>(status), (void*)rec);

        if (NS_SUCCEEDED(status)) {
            nsAutoCString buf;

            rec->GetCanonicalName(buf);
            printf("%d: canonname=%s\n", mIndex, buf.get());

            bool hasMore;
            while (NS_SUCCEEDED(rec->HasMore(&hasMore)) && hasMore) {
                rec->GetNextAddrAsString(buf);
                printf("%d: => %s\n", mIndex, buf.get());
            }
        }

        return NS_OK;
    }

private:
    nsCString mHost;
    int32_t   mIndex;
};

NS_IMPL_ISUPPORTS(myDNSListener, nsIDNSListener)

static bool IsAscii(const char *s)
{
  for (; *s; ++s) {
    if (*s & 0x80)
      return false;
  }

  return true;
}

int main(int argc, char **argv)
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    int sleepLen = 10; // default: 10 seconds

    if (argc == 1) {
        printf("usage: TestDNS [-N] hostname1 [hostname2 ...]\n");
        return -1;
    }

    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);

        nsCOMPtr<nsPIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
        if (!dns)
            return -1;

        if (argv[1][0] == '-') {
            sleepLen = atoi(argv[1]+1);
            argv++;
            argc--;
        }

        for (int j=0; j<2; ++j) {
            for (int i=1; i<argc; ++i) {
                // assume non-ASCII input is given in the native charset 
                nsAutoCString hostBuf;
                if (IsAscii(argv[i]))
                    hostBuf.Assign(argv[i]);
                else
                    hostBuf = NS_ConvertUTF16toUTF8(NS_ConvertASCIItoUTF16(argv[i]));

                nsCOMPtr<nsIDNSListener> listener = new myDNSListener(argv[i], i);

                nsCOMPtr<nsICancelable> req;
                nsresult rv = dns->AsyncResolve(hostBuf,
                                                nsIDNSService::RESOLVE_CANONICAL_NAME,
                                                listener, nullptr, getter_AddRefs(req));
                if (NS_FAILED(rv))
                    printf("### AsyncResolve failed [rv=%x]\n",
                           static_cast<uint32_t>(rv));
            }

            printf("main thread sleeping for %d seconds...\n", sleepLen);
            PR_Sleep(PR_SecondsToInterval(sleepLen));
        }

        printf("shutting down main thread...\n");
        dns->Shutdown();
    }

    NS_ShutdownXPCOM(nullptr);
    return 0;
}
