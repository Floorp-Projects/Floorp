/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

// data implementation

#include "nsIOService.h"
#include "nsDataChannel.h"
#include "nsDataHandler.h"
#include "nsNetUtil.h"
#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsReadableUtils.h"
#include "nsEscape.h"
#include "plbase64.h"
#include "plstr.h"
#include "prmem.h"

nsresult
nsDataChannel::OpenContentStream(bool async, nsIInputStream **result,
                                 nsIChannel** channel)
{
    NS_ENSURE_TRUE(URI(), NS_ERROR_NOT_INITIALIZED);

    nsresult rv;

    nsCAutoString spec;
    rv = URI()->GetAsciiSpec(spec);
    if (NS_FAILED(rv)) return rv;

    nsCString contentType, contentCharset, dataBuffer, hashRef;
    bool lBase64;
    rv = nsDataHandler::ParseURI(spec, contentType, contentCharset,
                                 lBase64, dataBuffer, hashRef);

    NS_UnescapeURL(dataBuffer);

    if (lBase64) {
        // Don't allow spaces in base64-encoded content. This is only
        // relevant for escaped spaces; other spaces are stripped in
        // NewURI.
        dataBuffer.StripWhitespace();
    }
    
    nsCOMPtr<nsIInputStream> bufInStream;
    nsCOMPtr<nsIOutputStream> bufOutStream;
    
    // create an unbounded pipe.
    rv = NS_NewPipe(getter_AddRefs(bufInStream),
                    getter_AddRefs(bufOutStream),
                    nsIOService::gDefaultSegmentSize,
                    PR_UINT32_MAX,
                    async, PR_TRUE);
    if (NS_FAILED(rv))
        return rv;

    PRUint32 contentLen;
    if (lBase64) {
        const PRUint32 dataLen = dataBuffer.Length();
        PRInt32 resultLen = 0;
        if (dataLen >= 1 && dataBuffer[dataLen-1] == '=') {
            if (dataLen >= 2 && dataBuffer[dataLen-2] == '=')
                resultLen = dataLen-2;
            else
                resultLen = dataLen-1;
        } else {
            resultLen = dataLen;
        }
        resultLen = ((resultLen * 3) / 4);

        // XXX PL_Base64Decode will return a null pointer for decoding
        // errors.  Since those are more likely than out-of-memory,
        // should we return NS_ERROR_MALFORMED_URI instead?
        char * decodedData = PL_Base64Decode(dataBuffer.get(), dataLen, nsnull);
        if (!decodedData) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        rv = bufOutStream->Write(decodedData, resultLen, &contentLen);

        PR_Free(decodedData);
    } else {
        rv = bufOutStream->Write(dataBuffer.get(), dataBuffer.Length(), &contentLen);
    }
    if (NS_FAILED(rv))
        return rv;

    SetContentType(contentType);
    SetContentCharset(contentCharset);
    SetContentLength64(contentLen);

    NS_ADDREF(*result = bufInStream);

    return NS_OK;
}
