/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include <stdio.h>
#include "nsIIDNService.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsNetCID.h"
#include "nsStringAPI.h"

int main(int argc, char **argv) {
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    // Test case from RFC 3492 (7.1 - Simplified Chinese)
    const char plain[] =
         "\xE4\xBB\x96\xE4\xBB\xAC\xE4\xB8\xBA\xE4\xBB\x80\xE4\xB9\x88\xE4\xB8\x8D\xE8\xAF\xB4\xE4\xB8\xAD\xE6\x96\x87";
    const char encoded[] = "xn--ihqwcrb4cv8a8dqg056pqjye";

    nsCOMPtr<nsIIDNService> converter = do_GetService(NS_IDNSERVICE_CONTRACTID);
    NS_ASSERTION(converter, "idnSDK not installed!");
    if (converter) {
        nsAutoCString buf;
        nsresult rv = converter->ConvertUTF8toACE(NS_LITERAL_CSTRING(plain), buf);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error ConvertUTF8toACE");
        NS_ASSERTION(buf.Equals(NS_LITERAL_CSTRING(encoded)), 
                     "encode result incorrect");
        printf("encoded = %s\n", buf.get());

        buf.Truncate();
        rv = converter->ConvertACEtoUTF8(NS_LITERAL_CSTRING(encoded), buf);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error ConvertACEtoUTF8");
        NS_ASSERTION(buf.Equals(NS_LITERAL_CSTRING(plain)), 
                     "decode result incorrect");
        printf("decoded = ");
        NS_ConvertUTF8toUTF16 utf(buf);
        const char16_t *u = utf.get();
        for (int i = 0; u[i]; i++) {
          printf("U+%.4X ", u[i]);
        }
        printf("\n");

        bool isAce;
        rv = converter->IsACE(NS_LITERAL_CSTRING("www.xn--ihqwcrb4cv8a8dqg056pqjye.com"), &isAce);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error IsACE");
        NS_ASSERTION(isAce, "IsACE incorrect result");
    }
    return 0;
}
