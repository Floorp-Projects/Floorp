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
 * The Original Code is i-DNS.net International, Inc. code.
 *
 * The Initial Developer of the Original Code is
 * i-DNS.net International.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): darin@netscape.com,
 *                 gagan@netscape.com,
 *                 william.tan@i-dns.net
 *
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

#include <stdio.h>
#include "nsIIDNService.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsString.h"

int main(int argc, char **argv) {
    // Test case from RFC 3492 (7.1 - Simplified Chinese)
    const char plain[] =
         "\xE4\xBB\x96\xE4\xBB\xAC\xE4\xB8\xBA\xE4\xBB\x80\xE4\xB9\x88\xE4\xB8\x8D\xE8\xAF\xB4\xE4\xB8\xAD\xE6\x96\x87";
    const char encoded[] = "xn--ihqwcrb4cv8a8dqg056pqjye";

    nsCOMPtr<nsIIDNService> converter = do_GetService(NS_IDNSERVICE_CONTRACTID);
    NS_ASSERTION(converter, "idnSDK not installed!");
    if (converter) {
        nsCAutoString buf;
        nsresult rv = converter->ConvertUTF8toACE(NS_LITERAL_CSTRING(plain), buf);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error ConvertUTF8toACE");
        NS_ASSERTION(buf.Equals(NS_LITERAL_CSTRING(encoded), nsCaseInsensitiveCStringComparator()), 
                     "encode result incorrect");
        printf("encoded = %s\n", buf.get());

        buf.Truncate();
        rv = converter->ConvertACEtoUTF8(NS_LITERAL_CSTRING(encoded), buf);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error ConvertACEtoUTF8");
        NS_ASSERTION(buf.Equals(NS_LITERAL_CSTRING(plain), nsCaseInsensitiveCStringComparator()), 
                     "decode result incorrect");
        printf("decoded = ");
        NS_ConvertUTF8toUCS2 u(buf);
        for (int i = 0; u[i]; i++) {
          printf("U+%.4X ", u[i]);
        }
        printf("\n");

        PRBool isAce;
        rv = converter->IsACE(NS_LITERAL_CSTRING("www.xn--ihqwcrb4cv8a8dqg056pqjye.com"), &isAce);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error IsACE");
        NS_ASSERTION(isAce, "IsACE incorrect result");
    }
    return 0;
}
