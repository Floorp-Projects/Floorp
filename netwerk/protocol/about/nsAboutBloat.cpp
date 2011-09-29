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
#include "nsILocalFile.h"

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
            clear = PR_TRUE;
        else if (param.EqualsLiteral("leaks"))
            leaks = PR_TRUE;
    }

    nsCOMPtr<nsIInputStream> inStr;
    if (clear) {
        nsTraceRefcntImpl::ResetStatistics();

        const char* msg = "Bloat statistics cleared.";
        rv = NS_NewCStringInputStream(getter_AddRefs(inStr), nsDependentCString(msg));
        if (NS_FAILED(rv)) return rv;
    }
    else if (leaks) {
        // dump the current set of leaks.
        GC_gcollect();
    	
        const char* msg = "Memory leaks dumped.";
        rv = NS_NewCStringInputStream(getter_AddRefs(inStr), nsDependentCString(msg));
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
        nsCOMPtr<nsILocalFile> lfile = do_QueryInterface(file);
        if (lfile == nsnull)
            return NS_ERROR_FAILURE;
        rv = lfile->OpenANSIFileDesc("w", &out);
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
    if (about == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(about);
    nsresult rv = about->QueryInterface(aIID, aResult);
    NS_RELEASE(about);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
#endif /* NS_BUILD_REFCNT_LOGGING */
