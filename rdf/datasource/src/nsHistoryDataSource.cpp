/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*

  Implementation for the history data source.

 */

#include <ctype.h> // for toupper()
#include <stdio.h>
#include "nscore.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFResourceFactory.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsVoidArray.h"  // XXX introduces dependency on raptorbase
#include "nsXPIDLString.h"
#include "nsRDFCID.h"
#include "rdfutil.h"
#include "nsIHistoryDataSource.h"
#include "nsIRDFService.h"
#include "plhash.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"
#include "prio.h"
#include "prtime.h"
#include "prlog.h"
#include "prlong.h"
#include "rdf.h"

#if defined(MOZ_BRPROF)
#include "nsIBrowsingProfile.h"
#endif

#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"
#include "prio.h"
#ifdef DEBUG_warren
#include "nsIBrowsingProfile.h" // XXX here to bootstrap browsing profile service
#endif

////////////////////////////////////////////////////////////////////////
// Interface IDs

static NS_DEFINE_IID(kISupportsIID,                NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,    NS_RDFINMEMORYDATASOURCE_CID);
#if defined(MOZ_BRPROF)
static NS_DEFINE_CID(kBrowsingProfileCID,          NS_BROWSINGPROFILE_CID);
#endif

////////////////////////////////////////////////////////////////////////
// RDF property & resource declarations

static const char kURIHistoryRoot[]  = "HistoryRoot";
static nsIRDFService* gRDFService = nsnull;


DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, URL);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, VisitCount);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Folder);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Title);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Page);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Date);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Referer);

DEFINE_RDF_VOCAB(WEB_NAMESPACE_URI, WEB, LastVisitDate);
DEFINE_RDF_VOCAB(WEB_NAMESPACE_URI, WEB, LastModifiedDate);



class HistoryEntry {
public:
    PRTime date;
    char* url;
    char* referer;

    HistoryEntry(PRTime& aDate, const char* aURL, const char* aRefererURL)
        : date(aDate)
    {
        url = PL_strdup(aURL);

        if (aRefererURL)
            referer = PL_strdup(aRefererURL);
        else
            referer = nsnull;
    }

    ~HistoryEntry() {
        PL_strfree(url);

        if (referer)
            PL_strfree(referer);
    }
};

typedef HistoryEntry* HE;

static PLHashNumber
rdf_HashPointer(const void* key)
{
    return (PLHashNumber) key;
}



class nsHistoryDataSource : public nsIHistoryDataSource {
protected:
    nsIRDFDataSource*  mInner;
    nsVoidArray        mFiles;
    nsFileSpec         mCurrentFileSpec;
    nsVoidArray        mPendingWrites; 
    PLHashTable*       mLastVisitDateHash; 
    PRExplodedTime     mSessionTime;

#if defined(MOZ_BRPROF)
    nsIBrowsingProfile* mBrowsingProfile;
#endif

static	  PRInt32		gRefCnt;
static    nsIRDFResource*    mResourcePage;
static    nsIRDFResource*    mResourceDate;
static    nsIRDFResource*    mResourceVisitCount;
static    nsIRDFResource*    mResourceTitle;
static    nsIRDFResource*    mResourceReferer;
static    nsIRDFResource*    mResourceChild;
static    nsIRDFResource*    mResourceURL;
static    nsIRDFResource*    mResourceHistoryBySite;
static    nsIRDFResource*    mResourceHistoryByDate;

    nsresult ReadHistory(void);
    nsresult ReadOneHistoryFile(nsInputFileStream& aStream, nsFileSpec fileSpec);
    nsresult AddPageToGraph(const char* url, const PRUnichar* title, const char* referer, PRUint32 visitCount, PRTime date);
    nsresult AddToDateHierarchy (PRTime date, const char *url);
    nsresult getSiteOfURL(const char* url, nsIRDFResource** resource);

public:
    // nsISupports
    NS_DECL_ISUPPORTS

    nsHistoryDataSource(void);
    virtual ~nsHistoryDataSource(void);

    NS_IMETHOD Init(const char* uri) ;

    NS_IMETHOD GetURI(char* *uri) {
        return mInner->GetURI(uri);
    }

    NS_IMETHOD GetSource(nsIRDFResource* property,
                         nsIRDFNode* target,
                         PRBool tv, nsIRDFResource** source) {
        return mInner->GetSource(property, target, tv, source);
    }

    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,  PRBool tv,
                          nsISimpleEnumerator** sources) {
        return mInner->GetSources(property, target, tv, sources);
    }

    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target)
	{
		if (tv && property == mResourceURL)
		{
            nsXPIDLCString uri;
			nsresult rv;
			if (NS_FAILED(rv = source->GetValue( getter_Copies(uri) )))
			{
				NS_ERROR("unable to get source's URI");
				return(rv);
			}
			nsAutoString	url(uri);
			if (url.Find("NC:") == 0)
				return(NS_RDF_NO_VALUE);
			nsIRDFLiteral	*literal;
			if (NS_FAILED(rv = gRDFService->GetLiteral(url.GetUnicode(), &literal)))
			{
				NS_ERROR("unable to construct literal for URL");
				return rv;
			}
			*target = (nsIRDFNode*)literal;
			return NS_OK;
		}
		return mInner->GetTarget(source, property, tv, target);
	}

    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsISimpleEnumerator** targets) {
        return mInner->GetTargets(source, property, tv, targets);
    }

    NS_IMETHOD Assert(nsIRDFResource* source, 
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv) {
        // History cannot be modified
        return NS_RDF_ASSERTION_REJECTED;
    }

    NS_IMETHOD Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target) {
        // History cannot be modified
        return NS_RDF_ASSERTION_REJECTED;
    }

    NS_IMETHOD HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target, PRBool tv,
                            PRBool* hasAssertion) {
        return mInner->HasAssertion(source, property, target, tv, hasAssertion);
    }

    NS_IMETHOD AddObserver(nsIRDFObserver* n) {
        return mInner->AddObserver(n);
    }

    NS_IMETHOD RemoveObserver(nsIRDFObserver* n) {
        return mInner->RemoveObserver(n);
    }

    NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                           nsISimpleEnumerator** labels) {
        return mInner->ArcLabelsIn(node, labels);
    }

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsISimpleEnumerator** labels) {
        return mInner->ArcLabelsOut(source, labels);
    }

    NS_IMETHOD Flush() {
        return mInner->Flush();
    }

    NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                              nsIEnumerator/*<nsIRDFResource>*/** commands) {
        return mInner->GetAllCommands(source, commands);
    }

    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult) {
        return mInner->IsCommandEnabled(aSources, aCommand, aArguments, aResult);
    }

    NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                         nsIRDFResource*   aCommand,
                         nsISupportsArray/*<nsIRDFResource>*/* aArguments) {
        // XXX Uh oh, this could cause problems wrt. the "dirty" flag
        // if it changes the in-memory store's internal state.
        return mInner->DoCommand(aSources, aCommand, aArguments);
    }

    NS_IMETHOD GetAllResources(nsISimpleEnumerator** aResult) {
        return mInner->GetAllResources(aResult);
    }

    NS_IMETHOD AddPage (const char* aURI, const char* aRefererURI, PRTime aTime);

    NS_IMETHOD SetPageTitle (const char* aURI, const PRUnichar* aTitle);

    NS_IMETHOD RemovePage (const char* aURI) {
        return NS_OK;
    }

    NS_IMETHOD GetLastVisitDate (const char* aURI, uint32 *aDate) {
        nsIRDFResource* res;
        PRBool found = 0;
        *aDate = 0;
        gRDFService->GetResource(aURI, &res);
        *aDate = (uint32)PL_HashTableLookup(mLastVisitDateHash, res);    
        NS_RELEASE(res);
        return NS_OK;
    }

    nsresult UpdateLastVisitDate (nsIRDFResource* res, uint32 date) {
        uint32 edate = (uint32) PL_HashTableLookup(mLastVisitDateHash, res);    
        if (edate && (edate > date)) return NS_OK;
        PL_HashTableAdd(mLastVisitDateHash, res, (void*)date);
        return NS_OK;
    }

    NS_IMETHOD CompleteURL (const char* prefix, char** preferredCompletion) {
        return NS_OK;
    }

};


PRInt32			nsHistoryDataSource::gRefCnt;

nsIRDFResource		*nsHistoryDataSource::mResourcePage;
nsIRDFResource		*nsHistoryDataSource::mResourceDate;
nsIRDFResource		*nsHistoryDataSource::mResourceVisitCount;
nsIRDFResource		*nsHistoryDataSource::mResourceTitle;
nsIRDFResource		*nsHistoryDataSource::mResourceReferer;
nsIRDFResource		*nsHistoryDataSource::mResourceChild;
nsIRDFResource		*nsHistoryDataSource::mResourceURL;
nsIRDFResource		*nsHistoryDataSource::mResourceHistoryBySite;
nsIRDFResource		*nsHistoryDataSource::mResourceHistoryByDate;



nsHistoryDataSource::nsHistoryDataSource(void)
    : mInner(nsnull),
      mLastVisitDateHash(nsnull)
#if defined(MOZ_BRPROF)
      , mBrowsingProfile(nsnull)
#endif
{
	NS_INIT_REFCNT();
	if (gRefCnt++ == 0)
	{
		nsresult	rv;
		rv = nsServiceManager::GetService(kRDFServiceCID,
                                          nsIRDFService::GetIID(),
                                          (nsISupports**) &gRDFService);

		PR_ASSERT(NS_SUCCEEDED(rv));

		gRDFService->GetResource(kURINC_Page,                    &mResourcePage);
		gRDFService->GetResource(kURINC_Date,                    &mResourceDate);
		gRDFService->GetResource(kURINC_VisitCount,              &mResourceVisitCount);
		gRDFService->GetResource(kURINC_Name,                    &mResourceTitle);
		gRDFService->GetResource(kURINC_Referer,                 &mResourceReferer);
		gRDFService->GetResource(kURINC_child,                   &mResourceChild);
		gRDFService->GetResource(kURINC_URL,                     &mResourceURL);
		gRDFService->GetResource("NC:HistoryBySite",             &mResourceHistoryBySite);
		gRDFService->GetResource("NC:HistoryByDate",             &mResourceHistoryByDate);
	}
}



nsHistoryDataSource::~nsHistoryDataSource(void)
{
	gRDFService->UnregisterDataSource(this);

	if (--gRefCnt == 0)
	{
		NS_IF_RELEASE(mInner);
		NS_IF_RELEASE(mResourcePage);
		NS_IF_RELEASE(mResourceDate);
		NS_IF_RELEASE(mResourceVisitCount);
		NS_IF_RELEASE(mResourceTitle);
		NS_IF_RELEASE(mResourceReferer);
		NS_IF_RELEASE(mResourceChild);
		NS_IF_RELEASE(mResourceURL);
		NS_IF_RELEASE(mResourceHistoryBySite);
		NS_IF_RELEASE(mResourceHistoryByDate);

		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);

#if defined(MOZ_BRPROF)
        nsServiceManager::ReleaseService(kBrowsingProfileCID, mBrowsingProfile);
#endif

		gRDFService = nsnull;
	}
    if (mLastVisitDateHash)
        PL_HashTableDestroy(mLastVisitDateHash);

    PRInt32 index;
    for (index = mPendingWrites.Count() - 1; index >= 0; --index) {
        HistoryEntry* entry = (HistoryEntry*) mPendingWrites.ElementAt(index);
        delete entry;
    }
}



NS_IMPL_ADDREF(nsHistoryDataSource);
NS_IMPL_RELEASE(nsHistoryDataSource);



NS_IMETHODIMP
nsHistoryDataSource::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(nsIHistoryDataSource::GetIID()) ||
        aIID.Equals(nsIRDFDataSource::GetIID()) ||
        aIID.Equals(nsISupports::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIHistoryDataSource*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }
}

NS_IMETHODIMP
nsHistoryDataSource::Init(const char* uri)
{
	nsresult rv;
	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
			nsnull, nsIRDFDataSource::GetIID(), (void**) &mInner)))
		return rv;

	if (NS_FAILED(rv = mInner->Init(uri)))
		return rv;

	// register this as a named data source with the RDF service
	if (NS_FAILED(rv = gRDFService->RegisterDataSource(this, PR_FALSE)))
		return rv;

#if defined(MOZ_BRPROF)
    {
        // Force the browsing profile to be loaded and initialized
        // here. Mostly we do this so that it's not on the netlib
        // thread, which seems to be broken right now.
        rv = nsServiceManager::GetService(kBrowsingProfileCID,
                                          nsIBrowsingProfile::GetIID(),
                                          (nsISupports**) &mBrowsingProfile);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get browsing profile");
        if (NS_SUCCEEDED(rv)) {
            rv = mBrowsingProfile->Init(nsnull);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to intialize browsing profile");
        }
    }
#endif

    mLastVisitDateHash = PL_NewHashTable(400, rdf_HashPointer,
                                         PL_CompareValues,
                                         PL_CompareValues,
                                         nsnull, nsnull);
 
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &mSessionTime);
    
    if (NS_FAILED(rv = ReadHistory()))
        return rv;

    char filename[256];

    // XXX since we're not really printing an unsinged long, but
    // rather an unsigned long-long, this is sure to break at some
    // point.
    PR_snprintf(filename, sizeof(filename), "%lu.hst", PR_Now());

    nsSpecialSystemDirectory historyFile(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
    historyFile += "res";
    historyFile += "rdf";
    historyFile += "History";

#ifdef	XP_MAC
	PRBool	wasAlias = PR_FALSE;
	historyFile.ResolveAlias(wasAlias);
#endif

//    historyFile.SetLeafName(filename);
    historyFile += filename;

    mCurrentFileSpec = historyFile;

#ifdef DEBUG_warren
    // XXX here to bootstrap the browsing profile code -- move later
    {
        static NS_DEFINE_CID(kBrowsingProfileCID, NS_BROWSINGPROFILE_CID);
        NS_WITH_SERVICE(nsIRDFObserver, brProf, kBrowsingProfileCID, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = AddObserver(brProf);
    }
#endif

    return rv;
}



NS_IMETHODIMP 
nsHistoryDataSource::AddPage (const char* aURI, const char* aRefererURI, PRTime aTime)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

	HistoryEntry* he = new HistoryEntry(aTime, aURI, aRefererURI);
	if (! he)
        return NS_ERROR_OUT_OF_MEMORY;

    mPendingWrites.AppendElement(he);
    return NS_OK;
}



NS_IMETHODIMP 
nsHistoryDataSource::SetPageTitle (const char* aURI, const PRUnichar* aTitle)
{
	for (PRInt32 i = mPendingWrites.Count() - 1; i >= 0; --i)
	{
		HE wr = (HE) mPendingWrites.ElementAt(i);
		if (strcmp(wr->url, aURI) == 0)
		{
			char timeBuffer[256];
			PRExplodedTime etime;
			PR_ExplodeTime(wr->date, PR_LocalTimeParameters, &etime);
			PR_FormatTimeUSEnglish(timeBuffer, 256, "%a %b %d %H:%M:%S %Y", &etime);

			nsAutoString	title(aTitle);
			char		*cTitle = title.ToNewCString();
			char *buffer = PR_smprintf("%s\t%s\t%s\t%lu\t%s\n", wr->url,
					((cTitle) ? cTitle:""),
					((wr->referer) ? wr->referer : ""),
					1L, timeBuffer);
			if (cTitle)
			{
				delete [] cTitle;
				cTitle = nsnull;
			}
			if (buffer)
			{
				nsOutputFileStream out(mCurrentFileSpec, PR_WRONLY | PR_CREATE_FILE | PR_APPEND, 0744);
				// XXX because PR_APPEND is broken
				out.seek(mCurrentFileSpec.GetFileSize());
				out << buffer;
				PR_smprintf_free(buffer);
			}

			mPendingWrites.RemoveElement(wr);
			AddPageToGraph(wr->url, aTitle, wr->referer, 1L, PR_Now());
			delete wr;

			return NS_OK;
		}    
	}
	return NS_OK;
}



PRBool
endsWith (const char* pattern, const char* uuid)
{
	short l1 = strlen(pattern);
	short l2 = strlen(uuid);
	short index;
	if (l2 < l1) return PR_FALSE;  
	for (index = 1; index <= l1; index++)
	{
		if (pattern[l1-index] != uuid[l2-index])
			return PR_FALSE;
	}  
	return PR_TRUE;
}



nsresult
nsHistoryDataSource::ReadHistory(void)
{
	nsSpecialSystemDirectory historyDir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
	historyDir += "res";
	historyDir += "rdf";
	historyDir += "History";

#ifdef	XP_MAC
	PRBool	wasAlias = PR_FALSE;
	historyDir.ResolveAlias(wasAlias);
#endif

	for (nsDirectoryIterator i(historyDir); i.Exists(); i++)
	{
		const nsNativeFileSpec	nativeSpec = (const nsNativeFileSpec &)i;
		nsFilePath		filePath(nativeSpec);
		nsFileSpec		fileSpec(filePath);
		const char		*fileURL = (const char*) fileSpec;
		if (fileURL)
		{
			if (endsWith(".hst", fileURL))
			{
				nsInputFileStream strm(fileSpec);
				if (strm.is_open())
				{
					ReadOneHistoryFile(strm, fileSpec);
				}
			}
		}
	}
	return(NS_OK);
}



nsresult
nsHistoryDataSource::ReadOneHistoryFile(nsInputFileStream& aStream, nsFileSpec fileSpec)
{
	nsresult	rv = NS_ERROR_FAILURE;
	nsAutoString	buffer;

	while (! aStream.eof() && ! aStream.failed())
	{
		nsresult rv = NS_OK;
		char c = aStream.get();
		if ((c != '\r') && (c != '\n'))
		{
			buffer += c;
		}
		else
		{
			int n = 0;
			char *title, *url, *referer, *visitcount, *date;
			PRTime time;
			PRTime fileTime = LL_ZERO;

			char *aLine = buffer.ToNewCString();
			if (aLine && PL_strlen(aLine)>0)
			{
				url = aLine;
				title = strchr(aLine, '\t') + 1;
				*(title - 1) = '\0';
				referer = strchr(title, '\t') + 1;
				*(referer - 1) = '\0';
				visitcount = strchr(referer, '\t') + 1;
				*(visitcount - 1) = '\0';
				date = strchr(visitcount, '\t') + 1;
				*(date -1 ) = '\0';
				PR_ParseTimeString (date, 0, &time);
				if (LL_IS_ZERO(fileTime))
				{
					PRExplodedTime etime;
					fileTime = time;
					PR_ExplodeTime(time, PR_LocalTimeParameters, &etime);
					if (etime.tm_yday == mSessionTime.tm_yday)
					{
						mCurrentFileSpec = fileSpec;
					}
				}

				AddPageToGraph(url, nsAutoString(title).GetUnicode(), referer,
					(PRUint32) atol(visitcount), time);
				delete [] aLine;
			}

			buffer = "";
		}        
	}
	return(rv);
}



nsresult 
nsHistoryDataSource::getSiteOfURL(const char* url, nsIRDFResource** resource)
{
	char* str = strstr(url, "://");
	char buff[256];
	nsIRDFLiteral  *titleLiteral;
	if (!str) return NS_ERROR_FAILURE;
	str = str + 3;
	if (str[0] == '/') str++;
	char* estr = strchr(str, '/');
	if (!estr) estr = str + strlen(str);
	if ((estr - str) > 256) return NS_ERROR_FAILURE;
	memset(buff, '\0', 256);

	// Note: start with "NC:" so that URL won't be shown
	static const char kHistoryURLPrefix[] = "NC:hst:site?";
	strcpy(buff, kHistoryURLPrefix);
	strncat(buff, str, (estr-str));

    // XXXwaterson: fix this
	//if (NS_OK != gRDFService->FindResource(buff, resource, &foundp)) return NS_ERROR_FAILURE;
	//if (foundp) return NS_OK;
	if (NS_OK != gRDFService->GetResource(buff, resource)) return NS_ERROR_FAILURE;
	mInner->Assert(mResourceHistoryBySite,  mResourceChild, *resource, 1);
	nsAutoString ptitle(buff + sizeof(kHistoryURLPrefix) - 1);	
	gRDFService->GetLiteral(ptitle.GetUnicode(), &titleLiteral);
	mInner->Assert(*resource, mResourceTitle, titleLiteral, 1);
	return NS_OK;
}



nsresult 
nsHistoryDataSource::AddPageToGraph(const char* url, const PRUnichar* title, 
                                    const char* referer, PRUint32 visitCount, PRTime date)
{
	nsresult	rv = NS_ERROR_OUT_OF_MEMORY;
	char *histURL = PR_smprintf("hst://%s", url);
	if (histURL)
	{
		nsIRDFResource	*siteResource;    
		rv = getSiteOfURL(url, &siteResource);
		if (rv == NS_OK)
		{
			nsCOMPtr<nsIRDFResource> pageResource, refererResource, histResource;
			nsCOMPtr<nsIRDFLiteral>  titleLiteral;
			nsCOMPtr<nsIRDFDate>     dateLiteral;
			nsCOMPtr<nsIRDFInt>      visitCountLiteral;

			gRDFService->GetResource(url, getter_AddRefs(pageResource));
			gRDFService->GetResource(histURL, getter_AddRefs(histResource));
			if (referer)
				gRDFService->GetResource(referer, getter_AddRefs(refererResource));
			gRDFService->GetDateLiteral(date, getter_AddRefs(dateLiteral));
			gRDFService->GetIntLiteral(visitCount, getter_AddRefs(visitCountLiteral));
			nsAutoString ptitle(title);
			gRDFService->GetLiteral(ptitle.GetUnicode(), getter_AddRefs(titleLiteral));
			mInner->Assert(histResource,  mResourcePage, pageResource, 1);
			mInner->Assert(histResource,  mResourceDate, dateLiteral, 1);
			mInner->Assert(histResource,  mResourceVisitCount, visitCountLiteral, 1);
			mInner->Assert(pageResource,  mResourceTitle, titleLiteral, 1);
			if (referer)
				mInner->Assert(histResource,  mResourceReferer, refererResource, 1);
			mInner->Assert(siteResource,  mResourceChild, pageResource, 1);
			AddToDateHierarchy(date, url);
			UpdateLastVisitDate (pageResource, 1/* date */);
		}
		PR_smprintf_free(histURL);
	}
	return(rv);
}



nsresult
nsHistoryDataSource::AddToDateHierarchy (PRTime date, const char *url)
{
	char			timeBuffer[128], timeNameBuffer[128], dayBuffer[128], dayNameBuffer[128];
	PRExplodedTime		enow, etime;

	timeBuffer[0] = timeNameBuffer[0] = dayBuffer[0] = dayNameBuffer[0] = '\0';

	PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &enow);
	PR_ExplodeTime(date, PR_LocalTimeParameters, &etime);
	if ((enow.tm_year == etime.tm_year) && (enow.tm_yday == etime.tm_yday))
	{
		PR_snprintf(timeBuffer, sizeof(timeBuffer), "NC:hst:date?today");
		PR_snprintf(timeNameBuffer, sizeof(timeNameBuffer), "Today");			// XXX localization
	}
	else if ((enow.tm_year == etime.tm_year) && ((enow.tm_yday - 1) == etime.tm_yday))
	{
		PR_snprintf(timeBuffer, sizeof(timeBuffer), "NC:hst:date?yesterday");
		PR_snprintf(timeNameBuffer, sizeof(timeNameBuffer), "Yesterday");		// XXX localization
	}
	else if ((enow.tm_year == etime.tm_year) && ((enow.tm_yday - etime.tm_yday) < 7))	// XXX calculation is not quite right
	{
		PR_FormatTimeUSEnglish(timeBuffer, sizeof(timeBuffer), "NC:hst:date?%A, %B %d, %Y", &etime);
		PR_FormatTimeUSEnglish(timeNameBuffer, sizeof(timeNameBuffer), "%A", &etime);
	}
	else
	{
		PR_FormatTimeUSEnglish(dayBuffer, sizeof(dayBuffer), "NC:hst:day?%A, %B %d, %Y", &etime);
		PR_FormatTimeUSEnglish(dayNameBuffer, sizeof(dayNameBuffer), "%A, %B %d, %Y", &etime);
		if (etime.tm_wday > 0)
		{
			PRUint64	microSecsInSec, secsInDay, temp;
			LL_I2L(microSecsInSec, PR_USEC_PER_SEC);
			LL_I2L(secsInDay, (60L*60L*24L*etime.tm_wday));
			LL_MUL(temp, microSecsInSec, secsInDay);
			LL_SUB(date, date, temp);
			PR_ExplodeTime(date, PR_LocalTimeParameters, &etime);
		}
		PR_FormatTimeUSEnglish(timeBuffer, sizeof(timeBuffer), "NC:hst:weekof?%B %d, %Y", &etime);
		PR_FormatTimeUSEnglish(timeNameBuffer, sizeof(timeNameBuffer), "Week of %B %d, %Y", &etime);	// XXX localization
	}

	nsIRDFResource		*timeResource, *dayResource, *resource, *parent;
	parent = mResourceHistoryByDate;
	if (NS_OK != gRDFService->GetResource(url, &resource))
		return NS_ERROR_FAILURE;

	PRBool			found = PR_FALSE;
    //XXXwaterson: fix this
	//if (NS_OK != gRDFService->FindResource(timeBuffer, &timeResource, &found))
    //return NS_ERROR_FAILURE;
	if (found == PR_FALSE)
	{
		if (NS_OK != gRDFService->GetResource(timeBuffer, &timeResource))
			return NS_ERROR_FAILURE;

		// set name of synthesized history container
		nsAutoString	ptitle(timeNameBuffer);
		nsIRDFLiteral	*titleLiteral;
		gRDFService->GetLiteral(ptitle.GetUnicode(), &titleLiteral);
		mInner->Assert(timeResource, mResourceTitle, titleLiteral, 1);
	}
	mInner->Assert(parent, mResourceChild, timeResource, 1);
	parent = timeResource;

	if (dayBuffer[0])
	{
		PRBool			found = PR_FALSE;
        // XXXwaterson: fix this
		//if (NS_OK != gRDFService->FindResource(dayBuffer, &dayResource, &found))
        //return NS_ERROR_FAILURE;
		if (found == PR_FALSE)
		{
			if (NS_OK != gRDFService->GetResource(dayBuffer, &dayResource))
				return NS_ERROR_FAILURE;

			// set name of synthesized history container
			nsAutoString	dayTitle(dayNameBuffer);
			nsIRDFLiteral	*dayLiteral;
			gRDFService->GetLiteral(dayTitle.GetUnicode(), &dayLiteral);
			mInner->Assert(dayResource, mResourceTitle, dayLiteral, 1);
		}
		mInner->Assert(parent, mResourceChild, dayResource, 1);
		parent = dayResource;
	}

	mInner->Assert(parent, mResourceChild, resource, 1);
	return(NS_OK);
}



nsresult
NS_NewHistoryDataSource(nsIHistoryDataSource** result)
{
	nsHistoryDataSource* ds = new nsHistoryDataSource();

	if (! ds)
		return NS_ERROR_NULL_POINTER;

	*result = ds;
	NS_ADDREF(*result);
	return(NS_OK);
}
