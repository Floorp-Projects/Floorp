/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* Implementation of xptiZipLoader. */

#include "xptiprivate.h"

#ifdef XPTI_HAS_ZIP_SUPPORT

static const char gCacheProgID[] = "component://netscape/libjar/zip-reader-cache";

static const PRUint32 gCacheSize = 1;

nsCOMPtr<nsIZipReaderCache> xptiZipLoader::gCache = nsnull;

// static 
nsIZipReader*
xptiZipLoader::GetZipReader(nsILocalFile* file)
{
    NS_ASSERTION(file, "bad file");
    
    if(!gCache)
    {
        gCache = do_CreateInstance(gCacheProgID);
        if(!gCache || NS_FAILED(gCache->Init(gCacheSize)))
            return nsnull;
    }

    nsIZipReader* reader = nsnull;

    if(NS_FAILED(gCache->GetZip(file, &reader)))
        return nsnull;

    return reader;
}

// static 
void
xptiZipLoader::Shutdown()
{
    gCache = nsnull;
}

// static 
PRBool 
xptiZipLoader::EnumerateZipEntries(nsILocalFile* file,
                                   xptiEntrySink* sink,
                                   xptiWorkingSet* aWorkingSet)
{
    NS_ASSERTION(file, "loser!");
    NS_ASSERTION(sink, "loser!");
    NS_ASSERTION(aWorkingSet, "loser!");

    nsCOMPtr<nsIZipReader> zip = dont_AddRef(GetZipReader(file));
    if(!zip)
        return PR_FALSE;
    
    nsCOMPtr<nsISimpleEnumerator> entries;
    if(NS_FAILED(zip->FindEntries("*.xpt", getter_AddRefs(entries))) ||
       !entries)
    {
        // XXX We TRUST that this means there are no .xpt files.    
        return PR_TRUE;
    }

    do
    {
        PRBool result;
        int index = 0;
        PRBool hasMore;
        
        if(NS_FAILED(entries->HasMoreElements(&hasMore)))
            return PR_FALSE;
        if(!hasMore)
            break;

        nsCOMPtr<nsISupports> sup;
        if(NS_FAILED(entries->GetNext(getter_AddRefs(sup))) ||!sup)
            return PR_FALSE;
        
        nsCOMPtr<nsIZipEntry> entry = do_QueryInterface(sup);
        if(!entry)
            return PR_FALSE;

        // we have a zip entry!

        char* itemName = nsnull;
        
        if(NS_FAILED(entry->GetName(&itemName)) || !itemName)
            return PR_FALSE;

        XPTHeader* header = 
            ReadXPTFileFromOpenZip(zip, entry, itemName, aWorkingSet);
        
        if(header)
            result = sink->FoundEntry(itemName, index++, header, aWorkingSet);
        nsMemory::Free(itemName);

        if(!header)
            return PR_FALSE;
        
        if(result != PR_TRUE)
            return result;        
    } while(1);

    return PR_TRUE;
}

// static
XPTHeader* 
xptiZipLoader::ReadXPTFileFromZip(nsILocalFile* file,
                                  const char* entryName,
                                  xptiWorkingSet* aWorkingSet)
{
    nsCOMPtr<nsIZipReader> zip = dont_AddRef(GetZipReader(file));
    if(!zip)
        return nsnull;

    nsCOMPtr<nsIZipEntry> entry;
    if(NS_FAILED(zip->GetEntry(entryName, getter_AddRefs(entry))) || !entry)
    {
        return nsnull;
    }

    return ReadXPTFileFromOpenZip(zip, entry, entryName, aWorkingSet);
}

// static
XPTHeader* 
xptiZipLoader::ReadXPTFileFromOpenZip(nsIZipReader* zip,
                                      nsIZipEntry* entry,
                                      const char* entryName,
                                      xptiWorkingSet* aWorkingSet)
{
    NS_ASSERTION(zip, "loser!");
    NS_ASSERTION(entry, "loser!");
    NS_ASSERTION(entryName, "loser!");

    XPTHeader *header = nsnull;
    char *whole = nsnull;
    XPTState *state = nsnull;
    XPTCursor cursor;
    PRUint32 flen;
    PRUint32 totalRead = 0;

    if(NS_FAILED(entry->GetRealSize(&flen)) || !flen)
    {
        return nsnull;
    }

    nsCOMPtr<nsIInputStream> stream;
    if(NS_FAILED(zip->GetInputStream(entryName, getter_AddRefs(stream))) || 
       !stream)
    {
        return nsnull;
    }

    whole = new char[flen];
    if (!whole)
    {
        return nsnull;
    }

    // all exits from on here should be via 'goto out' 

    while(flen - totalRead)
    {
        PRUint32 avail;
        PRUint32 read;

        if(NS_FAILED(stream->Available(&avail)))
        {
            goto out;
        }

        if(avail > flen)
        {
            goto out;
        }

        if(NS_FAILED(stream->Read(whole+totalRead, avail, &read)))
        {
            goto out;
        }

        totalRead += read;
    }
    
    // Go ahead and close the stream now.
    stream = nsnull;

    if(!(state = XPT_NewXDRState(XPT_DECODE, whole, flen)))
    {
        goto out;
    }
    
    if(!XPT_MakeCursor(state, XPT_HEADER, 0, &cursor))
    {
        goto out;
    }
    
    if (!XPT_DoHeader(aWorkingSet->GetStructArena(), &cursor, &header))
    {
        header = nsnull;
        goto out;
    }

 out:
    if(state)
        XPT_DestroyXDRState(state);
    if(whole)
        delete [] whole;
    return header;
}

#endif /* XPTI_HAS_ZIP_SUPPORT */
