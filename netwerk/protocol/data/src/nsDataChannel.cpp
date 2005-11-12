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

#include "nsDataChannel.h"
#include "nsNetUtil.h"
#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsReadableUtils.h"
#include "nsNetSegmentUtils.h"
#include "nsEscape.h"
#include "plbase64.h"
#include "plstr.h"
#include "prmem.h"

nsresult
nsDataChannel::OpenContentStream(PRBool async, nsIInputStream **result)
{
    NS_ENSURE_TRUE(URI(), NS_ERROR_NOT_INITIALIZED);

    nsresult rv;
    PRBool lBase64 = PR_FALSE;

    nsCAutoString spec;
    rv = URI()->GetAsciiSpec(spec);
    if (NS_FAILED(rv)) return rv;

    // move past "data:"
    char *buffer = (char *) strstr(spec.BeginWriting(), "data:");
    if (!buffer) {
        // malformed uri
        return NS_ERROR_MALFORMED_URI;
    }
    buffer += 5;

    // First, find the start of the data
    char *comma = strchr(buffer, ',');
    if (!comma)
        return NS_ERROR_MALFORMED_URI;

    *comma = '\0';

    // determine if the data is base64 encoded.
    char *base64 = strstr(buffer, ";base64");
    if (base64) {
        lBase64 = PR_TRUE;
        *base64 = '\0';
    }

    nsCString contentType, contentCharset;

    if (comma == buffer) {
        // nothing but data
        contentType.AssignLiteral("text/plain");
        contentCharset.AssignLiteral("US-ASCII");
    } else {
        // everything else is content type
        char *semiColon = (char *) strchr(buffer, ';');
        if (semiColon)
            *semiColon = '\0';
        
        if (semiColon == buffer || base64 == buffer) {
            // there is no content type, but there are other parameters
            contentType.AssignLiteral("text/plain");
        } else {
            contentType = buffer;
            ToLowerCase(contentType);
        }

        if (semiColon) {
            char *charset = PL_strcasestr(semiColon + 1, "charset=");
            if (charset)
                contentCharset = charset + sizeof("charset=") - 1;

            *semiColon = ';';
        }
    }
    contentType.StripWhitespace();
    contentCharset.StripWhitespace();

    char *dataBuffer = nsnull;
    PRBool cleanup = PR_FALSE;
    if (!lBase64 && ((strncmp(contentType.get(),"text/",5) == 0) ||
                     contentType.Find("xml") != kNotFound)) {
        // it's text, don't compress spaces
        dataBuffer = comma+1;
    } else {
        // it's ascii encoded binary, don't let any spaces in
        nsCAutoString dataBuf(comma+1);
        dataBuf.StripWhitespace();
        dataBuffer = ToNewCString(dataBuf);
        if (!dataBuffer)
            return NS_ERROR_OUT_OF_MEMORY;
        cleanup = PR_TRUE;
    }
    
    nsCOMPtr<nsIInputStream> bufInStream;
    nsCOMPtr<nsIOutputStream> bufOutStream;
    
    // create an unbounded pipe.
    rv = NS_NewPipe(getter_AddRefs(bufInStream),
                    getter_AddRefs(bufOutStream),
                    NET_DEFAULT_SEGMENT_SIZE, PR_UINT32_MAX,
                    async, PR_TRUE);
    if (NS_FAILED(rv))
        goto cleanup;

    PRUint32 dataLen, contentLen;
    dataLen = nsUnescapeCount(dataBuffer);
    if (lBase64) {
        *base64 = ';';
        PRInt32 resultLen = 0;
        if (dataBuffer[dataLen-1] == '=') {
            if (dataBuffer[dataLen-2] == '=')
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
        char * decodedData = PL_Base64Decode(dataBuffer, dataLen, nsnull);
        if (!decodedData) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto cleanup;
        }

        rv = bufOutStream->Write(decodedData, resultLen, &contentLen);

        PR_Free(decodedData);
    } else {
        rv = bufOutStream->Write(dataBuffer, dataLen, &contentLen);
    }
    if (NS_FAILED(rv))
        goto cleanup;

    *comma = ',';

    SetContentType(contentType);
    SetContentCharset(contentCharset);
    SetContentLength64(contentLen);

    NS_ADDREF(*result = bufInStream);

cleanup:
    if (cleanup)
        nsMemory::Free(dataBuffer);
    return rv;
}
