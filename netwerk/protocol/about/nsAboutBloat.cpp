/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTraceRefcntImpl.h"

// if NS_BUILD_REFCNT_LOGGING isn't defined, don't try to build
#ifdef NS_BUILD_REFCNT_LOGGING

#include "nsAboutBloat.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsStringStream.h"
#include "nsXPIDLString.h"
#include "nsIURI.h"
#include "prtime.h"
#include "nsCOMPtr.h"
#include "nsIFileStreams.h"
#include "nsNetUtil.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"

static void GC_gcollect() {}

NS_IMPL_ISUPPORTS1(nsAboutBloat, nsIAboutModule)

NS_IMETHODIMP
nsAboutBloat::NewChannel(nsIURI *aURI, nsIChannel **result)
{
    NS_ENSURE_ARG_POINTER(aURI);
    nsresult rv;
    nsCAutoString path;
    rv = aURI->GetPath(path);
    if (NS_FAILED(rv)) return rv;

    nsTraceRefcntImpl::StatisticsType statType = nsTraceRefcntImpl::ALL_STATS;
    bool clear = false;
    bool leaks = false;

    PRInt32 pos = path.Find("?");
    if (pos > 0) {
        nsCAutoString param;
        (void)path.Right(param, path.Length() - (pos+1));
        if (param.EqualsLiteral("new"))
            statType = nsTraceRefcntImpl::NEW_STATS;
        else if (param.EqualsLiteral("clear"))
            clear = true;
        else if (param.EqualsLiteral("leaks"))
            leaks = true;
    }

    nsCOMPtr<nsIInputStream> inStr;
    if (clear) {
        nsTraceRefcntImpl::ResetStatistics();

        rv = NS_NewCStringInputStream(getter_AddRefs(inStr),
            NS_LITERAL_CSTRING("Bloat statistics cleared."));
        if (NS_FAILED(rv)) return rv;
    }
    else if (leaks) {
        // dump the current set of leaks.
        GC_gcollect();
    	
        rv = NS_NewCStringInputStream(getter_AddRefs(inStr),
            NS_LITERAL_CSTRING("Memory leaks dumped."));
        if (NS_FAILED(rv)) return rv;
    }
    else {
        nsCOMPtr<nsIFile> file;
        rv = NS_GetSpecialDirectory(NS_OS_CURRENT_PROCESS_DIR, 
                                    getter_AddRefs(file));       
        if (NS_FAILED(rv)) return rv;

        rv = file->AppendNative(NS_LITERAL_CSTRING("bloatlogs"));
        if (NS_FAILED(rv)) return rv;

        bool exists;
        rv = file->Exists(&exists);
        if (NS_FAILED(rv)) return rv;

        if (!exists) {
            // On all the platforms that I know use permissions,
            // directories need to have the executable flag set
            // if you want to do anything inside the directory.
            rv = file->Create(nsIFile::DIRECTORY_TYPE, 0755);
            if (NS_FAILED(rv)) return rv;
        }

        nsCAutoString dumpFileName;
        if (statType == nsTraceRefcntImpl::ALL_STATS)
            dumpFileName.AssignLiteral("all-");
        else
            dumpFileName.AssignLiteral("new-");
        PRExplodedTime expTime;
        PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &expTime);
        char time[128];
        PR_FormatTimeUSEnglish(time, 128, "%Y-%m-%d-%H%M%S.txt", &expTime);
        dumpFileName += time;
        rv = file->AppendNative(dumpFileName);
        if (NS_FAILED(rv)) return rv;

        FILE* out;
        rv = file->OpenANSIFileDesc("w", &out);
        if (NS_FAILED(rv)) return rv;

        rv = nsTraceRefcntImpl::DumpStatistics(statType, out);
        ::fclose(out);
        if (NS_FAILED(rv)) return rv;

        rv = NS_NewLocalFileInputStream(getter_AddRefs(inStr), file);
        if (NS_FAILED(rv)) return rv;
    }

    nsIChannel* channel;
    rv = NS_NewInputStreamChannel(&channel, aURI, inStr,
                                  NS_LITERAL_CSTRING("text/plain"),
                                  NS_LITERAL_CSTRING("utf-8"));
    if (NS_FAILED(rv)) return rv;

    *result = channel;
    return rv;
}

NS_IMETHODIMP
nsAboutBloat::GetURIFlags(nsIURI *aURI, PRUint32 *result)
{
    *result = 0;
    return NS_OK;
}

nsresult
nsAboutBloat::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsAboutBloat* about = new nsAboutBloat();
    if (about == nullptr)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(about);
    nsresult rv = about->QueryInterface(aIID, aResult);
    NS_RELEASE(about);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
#endif /* NS_BUILD_REFCNT_LOGGING */
