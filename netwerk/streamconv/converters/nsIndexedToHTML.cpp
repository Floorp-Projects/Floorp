/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIndexedToHTML.h"
#include "nsNetUtil.h"
#include "nsIStringStream.h"
#include "nsEscape.h"

NS_IMPL_THREADSAFE_ISUPPORTS3(nsIndexedToHTML,
                              nsIStreamConverter,
                              nsIRequestObserver,
                              nsIStreamListener);


NS_IMETHODIMP
nsIndexedToHTML::Convert(nsIInputStream *aFromStream,
                         const PRUnichar *aFromType, const PRUnichar *aToType,
                         nsISupports *aCtxt, nsIInputStream * *_retval) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIndexedToHTML::AsyncConvertData(const PRUnichar *aFromType,
                                  const PRUnichar *aToType,
                                  nsIStreamListener *aListener,
                                  nsISupports *aCtxt) {
    NS_ASSERTION(aListener, "null pointer");
    mListener = aListener;
    return NS_OK;
}

NS_IMETHODIMP
nsIndexedToHTML::OnStartRequest(nsIRequest* request, nsISupports *aContext) 
{
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    uri->GetPath(getter_Copies(mCurrentPath));

    nsString buffer;
//    buffer.AssignWithConversion("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">");    
    buffer.AssignWithConversion("");
    
    buffer.AppendWithConversion("<html>\n<head><title> Index of "); //FIX i18n.
    buffer.AppendWithConversion(mCurrentPath);
    buffer.StripChar('/', buffer.Length() - 1);
    buffer.AppendWithConversion("</title></head>\n<body><pre>\n");

    buffer.AppendWithConversion("<H1> Index of "); //FIX i18n.
    buffer.AppendWithConversion(mCurrentPath);
    buffer.AppendWithConversion("</H1>\n");
    buffer.AppendWithConversion("<hr><table border=0>\n");

//    buffer.AppendWithConversion("<tr><th>Name</th><th>Size</th><th>Last modified</th><th>Description</th></tr>\n"); //FIX i18n.

    const char * path = mCurrentPath.get();
    if (path && *path && path[1] != '\0') 
    {
        buffer.AppendWithConversion("<tr>\n <td><a HREF=\"");
        buffer.AppendWithConversion(mCurrentPath);
        buffer.StripChar('/', buffer.Length() - 1);
        buffer.AppendWithConversion("/../\"> ..</a></td>\n");
    }

    // Push buffer to the listener now, so the initial HTML will not
    // be parsed in OnDataAvailable().

    nsresult rv = mListener->OnStartRequest(request, aContext);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStream> inputData;
    nsCOMPtr<nsISupports>    inputDataSup;
    rv = NS_NewStringInputStream(getter_AddRefs(inputDataSup), buffer);
    if (NS_FAILED(rv)) return rv;

    inputData = do_QueryInterface(inputDataSup);
    rv = mListener->OnDataAvailable(request, aContext,
                                    inputData, 0, buffer.Length());
    if (NS_FAILED(rv)) return rv;
    buffer.AssignWithConversion("");
    return rv;
}

NS_IMETHODIMP
nsIndexedToHTML::OnStopRequest(nsIRequest* request, nsISupports *aContext,
                               nsresult aStatus) {
    nsresult rv = NS_OK;
    nsString buffer;
    buffer.AssignWithConversion("</table><hr></pre></body></html>\n");    
    
    nsCOMPtr<nsIInputStream> inputData;
    nsCOMPtr<nsISupports>    inputDataSup;

    rv = NS_NewStringInputStream(getter_AddRefs(inputDataSup), buffer);
    if (NS_FAILED(rv)) return rv;

    inputData = do_QueryInterface(inputDataSup);

    rv = mListener->OnDataAvailable(request, aContext,
                                    inputData, 0, buffer.Length());
    if (NS_FAILED(rv)) return rv;

    return mListener->OnStopRequest(request, aContext, aStatus);
}

nsresult 
nsIndexedToHTML::Handle201(char* buffer, nsString &pushBuffer)
{
    // buffer should be in the format:
    // filename content-length last-modified file-type

    if (buffer == nsnull)
        return NS_ERROR_NULL_POINTER;

    char* bufferOffset = nsnull;

    if (*buffer == '\"') {
        buffer++;
        bufferOffset = PL_strchr((const char*)buffer, '\"');
    } else {
        bufferOffset = PL_strchr((const char*)buffer, ' ');
    }

    if (bufferOffset == nsnull)
        return NS_ERROR_FAILURE;   

    *bufferOffset = '\0';
    nsCString filename(buffer);
    ++bufferOffset;
   
    pushBuffer.AppendWithConversion("<tr>\n <td>");
    pushBuffer.AppendWithConversion("<a HREF=\"");
    
    const char * path = mCurrentPath.get();
    
    if (path && *path && path[1] != '\0') { 
        pushBuffer.AppendWithConversion(mCurrentPath);
        pushBuffer.StripChar('/', pushBuffer.Length() - 1);
    }

    pushBuffer.AppendWithConversion("/");
    pushBuffer.AppendWithConversion(filename);
    pushBuffer.AppendWithConversion("\"> ");

    nsUnescape(NS_CONST_CAST(char*, filename.get()));
    pushBuffer.AppendWithConversion(filename);
    pushBuffer.AppendWithConversion("</a>");
    pushBuffer.AppendWithConversion("</td>\n");

    while (*bufferOffset)
    {
        char* bufferStart =  bufferOffset;

        if (*bufferStart == '\"') {
            bufferStart++;
            bufferOffset = PL_strchr((const char*)bufferStart, '\"');
        } else {
            bufferOffset = PL_strchr((const char*)bufferStart, ' ');
        }
        
        if (bufferOffset == nsnull)
            return NS_ERROR_FAILURE;   
        
        *bufferOffset = '\0';
        ++bufferOffset;
        nsUnescape(bufferStart);
        nsCString cstring(bufferStart);
        cstring.ToLowerCase();

        pushBuffer.AppendWithConversion(" <td>");
        pushBuffer.AppendWithConversion(cstring);
        pushBuffer.AppendWithConversion("</td>\n");
    }

    pushBuffer.AppendWithConversion("</tr>");
    pushBuffer.AppendWithConversion(CRLF);

//    nsCString x; x.AssignWithConversion(pushBuffer);
//    printf("/n/n%s/n/n", (const char*)x);

    return NS_OK;
}

#define NSINDEXTOHTML_BUFFER_SIZE 4096

NS_IMETHODIMP
nsIndexedToHTML::OnDataAvailable(nsIRequest* request, nsISupports *aContext,
                                 nsIInputStream *aInStream,
                                 PRUint32 aOffset, PRUint32 aCount) 
{
    nsresult rv = NS_OK;
    nsString pushBuffer;

    char buffer[NSINDEXTOHTML_BUFFER_SIZE+1];
    char* startOffset = nsnull;
    char* endOffset = nsnull;

    while (aCount)
    {
        PRUint32 read = 0;
        rv = aInStream->Read(buffer, NSINDEXTOHTML_BUFFER_SIZE, &read);
        if (NS_FAILED(rv)) return rv;

        buffer[read] = '\0';
        aCount -= read;
        startOffset = (char*)&buffer;

        while (startOffset)
        {
            // TODO: we should handle the 200 line.  For now,
            // we assume that it contains:
            //
            // 200: filename content-length last-modified file-type\n

            // Look for a line begining with "201: " 
            startOffset = PL_strstr((const char*)startOffset, "201: ");
            if (startOffset == nsnull)
                break;

            endOffset = PL_strchr((const char*)startOffset, '\n');
            if (endOffset == nsnull)
                break;   
            
            *endOffset = '\0';
            
            rv = Handle201(startOffset + 5, pushBuffer);
            if (NS_FAILED(rv)) return rv;
            
            startOffset = ++endOffset;
        }
    }

    if (!pushBuffer.IsEmpty()) {
        nsCOMPtr<nsIInputStream> inputData;
        nsCOMPtr<nsISupports>    inputDataSup;
        rv = NS_NewStringInputStream(getter_AddRefs(inputDataSup), pushBuffer);

        if (NS_FAILED(rv))
            return rv;

        inputData = do_QueryInterface(inputDataSup);
        
        rv = mListener->OnDataAvailable(request, 
                                        aContext,
                                        inputData, 
                                        0, 
                                        pushBuffer.Length());
    }

    return rv; 
} 


nsIndexedToHTML::nsIndexedToHTML() {
    NS_INIT_REFCNT();
}

nsIndexedToHTML::~nsIndexedToHTML() {
}
