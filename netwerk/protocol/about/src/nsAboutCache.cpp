/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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

#include "nsICachedNetData.h"
#include "nsINetDataCacheRecord.h"


NS_IMPL_ISUPPORTS(nsAboutCache, NS_GET_IID(nsIAboutModule));

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
        (void)p.Mid(param, pos+1, -1);
        if (param.Equals("new"))
            statType = nsTraceRefcnt::NEW_STATS;
        else if (param.Equals("clear"))
            clear = PR_TRUE;
        else if (param.Equals("leaks"))
            leaks = PR_TRUE;
    }
*/
    // Get the cache manager service
    NS_WITH_SERVICE(nsINetDataCacheManager, cacheManager,
                    NS_NETWORK_CACHE_MANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStorageStream> storageStream;
    nsCOMPtr<nsIOutputStream> outputStream;
    nsCString buffer;

    // Init: (block size, maximum length)
    rv = NS_NewStorageStream(256, (PRUint32)-1, getter_AddRefs(storageStream));
    if (NS_FAILED(rv)) return rv;

    rv = storageStream->GetOutputStream(0, getter_AddRefs(outputStream));
    if (NS_FAILED(rv)) return rv;

    mBuffer.Assign("<HTML><TITLE>Information about the Netscape Cache Manager"
                   "</TITLE>\n");
    outputStream->Write(mBuffer, mBuffer.Length(), &bytesWritten);


    nsCOMPtr<nsISimpleEnumerator> moduleIterator;
    nsCOMPtr<nsISimpleEnumerator> entryIterator;

    rv = cacheManager->NewCacheModuleIterator(getter_AddRefs(moduleIterator));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISupports> item;
    nsCOMPtr<nsINetDataCache> cache;
    nsCOMPtr<nsINetDataCacheRecord> cacheRecord;

    do {
      PRInt32 cacheEntryCount = 0;
      PRInt32 cacheSize = 0;
      PRBool bMoreModules = PR_FALSE;

      rv = moduleIterator->HasMoreElements(&bMoreModules);
      if (!bMoreModules) break;

      rv = moduleIterator->GetNext(getter_AddRefs(item));
      if (NS_FAILED(rv)) break;

      cache = do_QueryInterface(item);
      rv = cache->NewCacheEntryIterator(getter_AddRefs(entryIterator));
      if (NS_FAILED(rv)) continue;

      DumpCacheInfo(outputStream, cache);

      DumpCacheEntries(outputStream, cacheManager, cache);
/*
      do {
        char *key;
        PRUint32 length;
        PRBool bMoreEntries = PR_FALSE;

        rv = entryIterator->HasMoreElements(&bMoreEntries);
        if (!bMoreEntries) break;

        rv = entryIterator->GetNext(getter_AddRefs(item));
        if (NS_FAILED(rv)) break;

        cacheRecord = do_QueryInterface(item);

        cacheEntryCount += 1;
        cacheRecord->GetStoredContentLength(&length);
        cacheSize += length;

        buffer.Truncate(0);
        buffer.AppendInt(cacheEntryCount);
        buffer.Append(".\t");
        buffer.AppendInt(length);
        buffer.Append(" bytes\t");
        outputStream->Write(buffer, buffer.Length(), &bytesWritten);

        rv = cacheRecord->GetKey(&length, &key);
        if (NS_FAILED(rv)) break;

        outputStream->Write(key, strlen(key), &bytesWritten);
        outputStream->Write("\n", 1, &bytesWritten);

        nsMemory::Free(key);

      } while(1);

      buffer.Truncate(0);
      buffer.Append("\nTotal Bytes in Cache: ");
      buffer.AppendInt(cacheSize);
      buffer.Append("\n\n");
      outputStream->Write(buffer, buffer.Length(), &bytesWritten);
*/
    } while(1);

    mBuffer.Assign("</HTML>\n");
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


void nsAboutCache::DumpCacheInfo(nsIOutputStream *aStream, nsINetDataCache *aCache)
{
  PRUint32 bytesWritten, value;
  nsXPIDLString str;

  // Write out the Cache Name
  aCache->GetDescription(getter_Copies(str));

  mBuffer.Assign("<h2>");
  mBuffer.AppendWithConversion(str);
  mBuffer.Append("</h2>\n");

  // Write out cache info

  mBuffer.Append("<TABLE>\n<TR><TD><b>Current entries:</TD>\n");
  value = 0;
  aCache->GetNumEntries(&value);
  mBuffer.Append("<TD>");
  mBuffer.AppendInt(value);
  mBuffer.Append("</TD>\n</TR>\n");

  mBuffer.Append("<TABLE>\n<TR><TD><b>Maximum entries:</TD>\n");
  value = 0;
  aCache->GetMaxEntries(&value);
  mBuffer.Append("<TD>");
  mBuffer.AppendInt(value);
  mBuffer.Append("</TD>\n</TR>\n");

  mBuffer.Append("<TABLE>\n<TR><TD><b>Storage in use:</TD>\n");
  mBuffer.Append("<TD>");
  value = 0;
  aCache->GetStorageInUse(&value);
  mBuffer.AppendInt(value);
  mBuffer.Append("kb</TD>\n</TR>\n");

  mBuffer.Append("</TABLE>\n<HR>\n");

  aStream->Write(mBuffer, mBuffer.Length(), &bytesWritten);
}


void nsAboutCache::DumpCacheEntryInfo(nsIOutputStream *aStream,
                                      nsINetDataCacheManager *aCacheMgr,
                                      char * aKey, char *aSecondaryKey,
                                      PRUint32 aSecondaryKeyLen)
{
  nsresult rv;
  PRUint32 bytesWritten;
  nsXPIDLCString str;
  nsCOMPtr<nsICachedNetData> entry;

  rv = aCacheMgr->GetCachedNetData(aKey, aSecondaryKey, aSecondaryKeyLen, 0,
                                   getter_AddRefs(entry));
  if (NS_FAILED(rv)) return;

  // URI
  mBuffer.Assign("<tt>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
                     "&nbsp;&nbsp;&nbsp;&nbsp;URL: </tt>");
  entry->GetUriSpec(getter_Copies(str));
  mBuffer.Append("<A HREF=\"");
  mBuffer.Append(str);
  mBuffer.Append("\">");
  mBuffer.Append(str);
  mBuffer.Append("</A><BR>\n");

  // Content length
  PRUint32 length = 0;
  entry->GetStoredContentLength(&length);

  mBuffer.Append("<tt>Content Length: </tt>");
  mBuffer.AppendInt(length);
  mBuffer.Append("<BR>\n");

  // Number of accesses
  PRUint16 numAccesses = 0;
  entry->GetNumberAccesses(&numAccesses);

  mBuffer.Append("<tt>&nbsp;# of accesses: </tt>");
  mBuffer.AppendInt(numAccesses);
  mBuffer.Append("<BR>\n");

  // Last modified time
  char buf[255];
  PRExplodedTime et;
  PRTime t;

  mBuffer.Append("<tt>&nbsp;Last Modified: </tt>");

  entry->GetLastModifiedTime(&t);
  if (LL_NE(t, LL_ZERO)) {
    PR_ExplodeTime(t, PR_LocalTimeParameters, &et);
    PR_FormatTime(buf, sizeof(buf), "%c", &et);
    mBuffer.Append(buf);
  } else {
    mBuffer.Append("No modified date sent");
  }
  mBuffer.Append("<BR>");

  // Expires time
  mBuffer.Append("<tt>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
                 "Expires: </tt>");

  entry->GetExpirationTime(&t);
  if (LL_NE(t, LL_ZERO)) {
    PR_ExplodeTime(t, PR_LocalTimeParameters, &et);
    PR_FormatTime(buf, sizeof(buf), "%c", &et);
    mBuffer.Append(buf);
  } else {
    mBuffer.Append("No expiration date sent");
  }
  mBuffer.Append("<BR>");

  // Flags
  PRBool flag = PR_FALSE, foundFlag = PR_FALSE;
  mBuffer.Append("<tt>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
                 "Flags: </tt><b>");

  flag = PR_FALSE;
  entry->GetPartialFlag(&flag);
  if (flag) {
    mBuffer.Append("PARTIAL ");
    foundFlag = PR_TRUE;
  }
  flag = PR_FALSE;
  entry->GetUpdateInProgress(&flag);
  if (flag) {
    mBuffer.Append("UPDATE_IN_PROGRESS ");
    foundFlag = PR_TRUE;
  }
  flag = PR_FALSE;
  entry->GetInUse(&flag);
  if (flag) {
    mBuffer.Append("IN_USE");
    foundFlag = PR_TRUE;
  }

  if (!foundFlag) {
    mBuffer.Append("</b>none<BR>\n");
  } else {
    mBuffer.Append("</b><BR>\n");
  }

  // Entry is done...
  mBuffer.Append("\n\n<P>\n");

  aStream->Write(mBuffer, mBuffer.Length(), &bytesWritten);
}

nsresult nsAboutCache::DumpCacheEntries(nsIOutputStream *aStream,
                                        nsINetDataCacheManager *aCacheMgr,
                                        nsINetDataCache *aCache)
{
  nsresult rv;
  nsCOMPtr<nsISimpleEnumerator> entryIterator;
  nsCOMPtr<nsISupports> item;
  nsCOMPtr<nsINetDataCacheRecord> cacheRecord;

  rv = aCache->NewCacheEntryIterator(getter_AddRefs(entryIterator));
  if (NS_FAILED(rv)) return rv;

  do {
    char *key, *secondaryKey;
    PRUint32 keyLength;
    PRBool bMoreEntries = PR_FALSE;

    rv = entryIterator->HasMoreElements(&bMoreEntries);
    if (!bMoreEntries) break;

    rv = entryIterator->GetNext(getter_AddRefs(item));
    if (NS_FAILED(rv)) break;

    cacheRecord = do_QueryInterface(item);

    rv = cacheRecord->GetKey(&keyLength, &key);
    if (NS_FAILED(rv)) break;

    // The URI spec is stored as the second of the two components that make up
    // the nsINetDataCacheRecord key and is separated from the first component
    // by a NUL character.
    for(secondaryKey = key; *secondaryKey; secondaryKey++)
        keyLength--;

    // Account for NUL character
    if (keyLength) {
      keyLength--;
      secondaryKey++;
    } else {
      secondaryKey = nsnull;
    }

    DumpCacheEntryInfo(aStream, aCacheMgr, key, secondaryKey, keyLength);

    nsMemory::Free(key);
  } while(1);

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
