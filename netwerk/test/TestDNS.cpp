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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corp.
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

#include "TestCommon.h"
#include <stdio.h>
#include <stdlib.h>
#include "nsIServiceManager.h"
#include "nsIDNSService.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsNetCID.h"
#include "prinrval.h"
#include "prthread.h"
#include "prnetdb.h"
#include "nsCRT.h"

class myDNSListener : public nsIDNSListener
{
public:
    NS_DECL_ISUPPORTS

    myDNSListener(const char *host, PRInt32 index)
        : mHost(host)
        , mIndex(index) {}
    virtual ~myDNSListener() {}

    NS_IMETHOD OnLookupComplete(nsIDNSRequest *request,
                                nsIDNSRecord  *rec,
                                nsresult       status)
    {
        printf("%d: OnLookupComplete called [host=%s status=%x rec=%p]\n",
            mIndex, mHost.get(), status, (void*)rec);

        if (NS_SUCCEEDED(status)) {
            nsCAutoString buf;

            rec->GetCanonicalName(buf);
            printf("%d: canonname=%s\n", mIndex, buf.get());

            PRBool hasMore;
            while (NS_SUCCEEDED(rec->HasMore(&hasMore)) && hasMore) {
                rec->GetNextAddrAsString(buf);
                printf("%d: => %s\n", mIndex, buf.get());
            }
        }

        return NS_OK;
    }

private:
    nsCString mHost;
    PRInt32   mIndex;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(myDNSListener, nsIDNSListener)

int main(int argc, char **argv)
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    int sleepLen = 10; // default: 10 seconds

    if (argc == 1) {
        printf("usage: TestDNS [-N] hostname1 [hostname2 ...]\n");
        return -1;
    }

    nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
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
            nsCAutoString hostBuf;
            if (nsCRT::IsAscii(argv[i]))
                hostBuf.Assign(argv[i]);
            else
                hostBuf = NS_ConvertUCS2toUTF8(NS_ConvertASCIItoUCS2(argv[i]));

            nsCOMPtr<nsIDNSListener> listener = new myDNSListener(argv[i], i);

            nsCOMPtr<nsIDNSRequest> req;
            nsresult rv = dns->AsyncResolve(hostBuf,
                                            nsIDNSService::RESOLVE_CANONICAL_NAME,
                                            listener, nsnull, getter_AddRefs(req));
            if (NS_FAILED(rv))
                printf("### AsyncResolve failed [rv=%x]\n", rv);
        }

        printf("main thread sleeping for %d seconds...\n", sleepLen);
        PR_Sleep(PR_SecondsToInterval(sleepLen));
    }

    printf("shutting down main thread...\n");
    dns->Shutdown();
    return 0;
}
