/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christian Biesinger <cbiesinger@web.de>
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
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"

#include <stdio.h>

/*
 * Test synchronous Open.
 */

#define RETURN_IF_FAILED(rv, what) \
    PR_BEGIN_MACRO \
    if (NS_FAILED(rv)) { \
        printf(what ": failed - %08x\n", rv); \
        return -1; \
    } \
    PR_END_MACRO

int
main(int argc, char **argv)
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv;
    char buf[256];

    if (argc != 3) {
        printf("Usage: TestOpen url filename\nLoads a URL using ::Open, writing it to a file\n");
        return -1;
    }

    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIInputStream> stream;

    rv = NS_NewURI(getter_AddRefs(uri), argv[1]);
    RETURN_IF_FAILED(rv, "NS_NewURI");

    rv = NS_OpenURI(getter_AddRefs(stream), uri);
    RETURN_IF_FAILED(rv, "NS_OpenURI");

    FILE* outfile = fopen(argv[2], "wb");
    if (!outfile) {
      printf("error opening %s\n", argv[2]);
      return 1;
    }

    PRUint32 read;
    while (NS_SUCCEEDED(stream->Read(buf, sizeof(buf), &read)) && read) {
      fwrite(buf, 1, read, outfile);
    }
    printf("Done\n");

    fclose(outfile);

    NS_ShutdownXPCOM(nsnull);
    return 0;
}
