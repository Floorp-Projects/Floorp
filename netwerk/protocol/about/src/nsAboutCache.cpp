/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Alexey Chernyak <alexeyc@bigfoot.com> (XHTML 1.1 conversion)
 *   Henrik Gemal <gemal@gemal.dk>
 *   Darin Fisher <darin@netscape.com>
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

#include "nsAboutCache.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIInputStream.h"
#include "nsIStorageStream.h"
#include "nsISimpleEnumerator.h"
#include "nsXPIDLString.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "prtime.h"

#include "nsICacheService.h"

static PRTime SecondsToPRTime(PRUint32 t_sec)
{
    PRTime t_usec, usec_per_sec;
    LL_I2L(t_usec, t_sec);
    LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
    LL_MUL(t_usec, t_usec, usec_per_sec);
    return t_usec;
}
static void PrintTimeString(char *buf, PRUint32 bufsize, PRUint32 t_sec)
{
    PRExplodedTime et;
    PRTime t_usec = SecondsToPRTime(t_sec);
    PR_ExplodeTime(t_usec, PR_LocalTimeParameters, &et);
    PR_FormatTime(buf, bufsize, "%c", &et);
}


NS_IMPL_ISUPPORTS2(nsAboutCache, nsIAboutModule, nsICacheVisitor)

NS_IMETHODIMP
nsAboutCache::NewChannel(nsIURI *aURI, nsIChannel **result)
{
    nsresult rv;
    PRUint32 bytesWritten;

    *result = nsnull;
/*
    nsXPIDLCString path;
    rv = aURI->GetPath(getter_Copies(path));
    if (NS_FAILED(rv)) return rv;

    PRBool clear = PR_FALSE;
    PRBool leaks = PR_FALSE;

    nsCAutoString p(path);
    PRInt32 pos = p.Find("?");
    if (pos > 0) {
        nsCAutoString param;
        (void)p.Right(param, p.Length() - (pos+1));
        if (param.Equals("new"))
            statType = nsTraceRefcnt::NEW_STATS;
        else if (param.Equals("clear"))
            clear = PR_TRUE;
        else if (param.Equals("leaks"))
            leaks = PR_TRUE;
    }
*/
    // Get the cache manager service
    nsCOMPtr<nsICacheService> cacheService = 
             do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStorageStream> storageStream;
    nsCOMPtr<nsIOutputStream> outputStream;

    // Init: (block size, maximum length)
    rv = NS_NewStorageStream(256, (PRUint32)-1, getter_AddRefs(storageStream));
    if (NS_FAILED(rv)) return rv;

    rv = storageStream->GetOutputStream(0, getter_AddRefs(outputStream));
    if (NS_FAILED(rv)) return rv;

    mBuffer.Assign("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\"\n"
                   "    \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"
                   "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
                   "<head>\n<title>Information about the Cache Service</title>\n</head>\n"
                   "<body>\n<div>\n");

    outputStream->Write(mBuffer, mBuffer.Length(), &bytesWritten);

    rv = ParseURI(aURI, mDeviceID);
    if (NS_FAILED(rv)) return rv;

    mStream = outputStream;
    rv = cacheService->VisitEntries(this);
    if (NS_FAILED(rv)) return rv;

    mBuffer.Truncate(0);
    if (!mDeviceID.IsEmpty()) {
        mBuffer.Append("</pre>\n");
    }
    mBuffer.Append("</div>\n</body>\n</html>\n");
    outputStream->Write(mBuffer, mBuffer.Length(), &bytesWritten);
        
    nsCOMPtr<nsIInputStream> inStr;
    PRUint32 size;

    rv = storageStream->GetLength(&size);
    if (NS_FAILED(rv)) return rv;

    rv = storageStream->NewInputStream(0, getter_AddRefs(inStr));
    if (NS_FAILED(rv)) return rv;

    nsIChannel* channel;
    rv = NS_NewInputStreamChannel(&channel, aURI, inStr, "text/html", size);
    if (NS_FAILED(rv)) return rv;

    *result = channel;
    return rv;
}


NS_IMETHODIMP
nsAboutCache::VisitDevice(const char *deviceID,
                          nsICacheDeviceInfo *deviceInfo,
                          PRBool *visitEntries)
{
    PRUint32 bytesWritten, value;
    nsXPIDLCString str;

    *visitEntries = PR_FALSE;

    if (mDeviceID.IsEmpty() || mDeviceID.Equals(deviceID)) {

        // Write out the Cache Name
        deviceInfo->GetDescription(getter_Copies(str));

        mBuffer.Assign("<h2>");
        mBuffer.Append(str);
        mBuffer.Append("</h2>\n<br />\n");

        // Write out cache info

        mBuffer.Append("<table>\n");

        mBuffer.Append("\n<tr>\n<td><b>Number of entries: </b></td>\n");
        value = 0;
        deviceInfo->GetEntryCount(&value);
        mBuffer.Append("<td><tt>");
        mBuffer.AppendInt(value);
        mBuffer.Append("</tt></td>\n</tr>\n");

        mBuffer.Append("\n<tr>\n<td><b>Maximum storage size: </b></td>\n");
        value = 0;
        deviceInfo->GetMaximumSize(&value);
        mBuffer.Append("<td><tt>");
        mBuffer.AppendInt(value);
        mBuffer.Append(" Bytes</tt></td>\n</tr>\n");

        mBuffer.Append("\n<tr>\n<td><b>Storage in use: </b></td>\n");
        mBuffer.Append("<td><tt>");
        value = 0;
        deviceInfo->GetTotalSize(&value);
        mBuffer.AppendInt(value);
        mBuffer.Append(" Bytes</tt></td>\n</tr>\n");

        mBuffer.Append("</table>\n");
        
        deviceInfo->GetUsageReport(getter_Copies(str));
        mBuffer.Append(str);
        mBuffer.Append("\n<br />");

        if (mDeviceID.IsEmpty()) {
            mBuffer.Append("\n<a href=\"about:cache?device=");
            mBuffer.Append(deviceID);
            mBuffer.Append("\">List Cache Entries</a>\n");
            mBuffer.Append("<hr />\n");
        } else {
            *visitEntries = PR_TRUE;
            mBuffer.Append("<hr />\n<pre>\n");
        }
        
        mStream->Write(mBuffer, mBuffer.Length(), &bytesWritten);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsAboutCache::VisitEntry(const char *deviceID,
                         nsICacheEntryInfo *entryInfo,
                         PRBool *visitNext)
{
    nsresult        rv;
    PRUint32        bytesWritten;
    nsXPIDLCString  key;
    nsXPIDLCString  clientID;
    PRBool          streamBased;
    
    rv = entryInfo->GetKey(getter_Copies(key));
    if (NS_FAILED(rv))  return rv;

    rv = entryInfo->GetClientID(getter_Copies(clientID));
    if (NS_FAILED(rv))  return rv;

    rv = entryInfo->IsStreamBased(&streamBased);
    if (NS_FAILED(rv)) return rv;

    // Generate a about:cache-entry URL for this entry...
    nsCAutoString url;
    url += NS_LITERAL_CSTRING("about:cache-entry?client=");
    url += clientID;
    url += NS_LITERAL_CSTRING("&amp;sb=");
    url += streamBased ? "1" : "0";
    url += NS_LITERAL_CSTRING("&amp;key=");
    url += key; // key

    // Entry start...

    // URI
    mBuffer.Assign("<b>           Key: </b><a href=\"");
    mBuffer.Append(url);
    mBuffer.Append("\">");
    mBuffer.Append(key);
    mBuffer.Append("</a>");

    // Content length
    PRUint32 length = 0;
    entryInfo->GetDataSize(&length);

    mBuffer.Append("\n<b>     Data size: </b>");
    mBuffer.AppendInt(length);
    mBuffer.Append(" Bytes");

    // Number of accesses
    PRInt32 fetchCount = 0;
    entryInfo->GetFetchCount(&fetchCount);

    mBuffer.Append("\n<b>   Fetch count: </b>");
    mBuffer.AppendInt(fetchCount);

    // vars for reporting time
    char buf[255];
    PRUint32 t;

    // Last modified time
    mBuffer.Append("\n<b> Last Modified: </b>");
    entryInfo->GetLastModified(&t);
    if (t) {
        PrintTimeString(buf, sizeof(buf), t);
        mBuffer.Append(buf);
    } else
        mBuffer.Append("No last modified time");

    // Expires time
    mBuffer.Append("\n<b>       Expires: </b>");
    entryInfo->GetExpirationTime(&t);
    if (t) {
        PrintTimeString(buf, sizeof(buf), t);
        mBuffer.Append(buf);
    } else {
        mBuffer.Append("No expiration time");
    }

    // Entry is done...
    mBuffer.Append("\n\n");

    mStream->Write(mBuffer, mBuffer.Length(), &bytesWritten);

    *visitNext = PR_TRUE;
    return NS_OK;
}


nsresult
nsAboutCache::ParseURI(nsIURI * uri, nsCString &deviceID)
{
    //
    // about:cache[?device=string]
    //
    nsresult rv;

    deviceID.Truncate();

    nsXPIDLCString path;
    rv = uri->GetPath(getter_Copies(path));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString p(path);

    nsReadingIterator<char> start, valueStart, end;
    p.BeginReading(start);
    p.EndReading(end);

    valueStart = end;
    if (!FindInReadable(NS_LITERAL_CSTRING("?device="), start, valueStart))
        return NS_OK;

    deviceID.Assign(Substring(valueStart, end));
    return NS_OK;
}


NS_METHOD
nsAboutCache::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsAboutCache* about = new nsAboutCache();
    if (about == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(about);
    nsresult rv = about->QueryInterface(aIID, aResult);
    NS_RELEASE(about);
    return rv;
}



////////////////////////////////////////////////////////////////////////////////
