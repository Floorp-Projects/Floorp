/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
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
 *   Robert John Churchill      <rjc@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
  Implementation for an internet search RDF data store.
 */

#include "nsInternetSearchService.h"
#include "nscore.h"
#include "nsIEnumerator.h"
#include "nsIRDFObserver.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIServiceManager.h"
#include "nsVoidArray.h"  // XXX introduces dependency on raptorbase
#include "nsXPIDLString.h"
#include "nsRDFCID.h"
#include "plhash.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"
#include "prio.h"
#include "prlog.h"
#include "rdf.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCOMArray.h"
#include "nsCRT.h"
#include "nsEnumeratorUtils.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsITextToSubURI.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIFileChannel.h"
#include "nsIHttpChannel.h"
#include "nsIUploadChannel.h"
#include "nsIInputStream.h"
#include "nsIBookmarksService.h"
#include "nsIStringBundle.h"
#include "nsIObserverService.h"
#include "nsIURL.h"
#include "nsILocalFile.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsIPrefLocalizedString.h"

#ifdef	XP_MAC
#include <Files.h>
#include <Timer.h>
#include <Gestalt.h>
#endif

#ifdef	XP_WIN
#include "windef.h"
#include "winbase.h"
#endif

#ifdef	DEBUG
// #define	DEBUG_SEARCH_OUTPUT	1
// #define	DEBUG_SEARCH_UPDATES	1
#endif

#define MAX_SEARCH_RESULTS_ALLOWED  100

#define POSTHEADER_PREFIX "Content-type: application/x-www-form-urlencoded\r\nContent-Length: "
#define POSTHEADER_SUFFIX "\r\n\r\n"
#define SEARCH_PROPERTIES "chrome://communicator/locale/search/search-panel.properties"
#ifdef MOZ_PHOENIX
#define SEARCHCONFIG_PROPERTIES "chrome://browser/content/searchconfig.properties"
#define INTL_PROPERTIES "chrome://global/locale/intl.properties"
#else
#define SEARCHCONFIG_PROPERTIES "chrome://navigator/content/searchconfig.properties"
#define INTL_PROPERTIES "chrome://navigator/locale/navigator.properties"
#endif

static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerCID,             NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,        NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,    NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,         NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kTextToSubURICID,             NS_TEXTTOSUBURI_CID);
static NS_DEFINE_CID(kPrefCID,                     NS_PREF_CID);

static const char kURINC_SearchEngineRoot[]                   = "NC:SearchEngineRoot";
static const char kURINC_SearchResultsSitesRoot[]             = "NC:SearchResultsSitesRoot";
static const char kURINC_LastSearchRoot[]                     = "NC:LastSearchRoot";
static const char kURINC_SearchCategoryRoot[]                 = "NC:SearchCategoryRoot";
static const char kURINC_SearchCategoryPrefix[]               = "NC:SearchCategory?category=";
static const char kURINC_SearchCategoryEnginePrefix[]         = "NC:SearchCategory?engine=";
static const char kURINC_SearchCategoryEngineBasenamePrefix[] = "NC:SearchCategory?engine=urn:search:engine:";
 
static const char kURINC_FilterSearchURLsRoot[]       = "NC:FilterSearchURLsRoot";
static const char kURINC_FilterSearchSitesRoot[]      = "NC:FilterSearchSitesRoot";
static const char kSearchCommand[]                    = "http://home.netscape.com/NC-rdf#command?";

int	PR_CALLBACK searchModePrefCallback(const char *pref, void *aClosure);

// helper routine because we need to rewrite this to use string
// iterators.. this replaces the old nsString::Find

static PRInt32 nsString_Find(const nsAString& aPattern,
                             const nsAString& aSource,
                             PRBool aIgnoreCase = PR_FALSE,
                             PRInt32 aOffset = 0, PRInt32 aCount = -1)
{
    nsAString::const_iterator start, end;
    aSource.BeginReading(start);
    aSource.EndReading(end);

    // now adjust for the parameters
    start.advance(aOffset);
    if (aCount>0) {
	end = start;		// note that start may have been advanced!
	end.advance(aCount);
    }
    PRBool found;
    if (aIgnoreCase)
	found = FindInReadable(aPattern, start, end,
			       nsCaseInsensitiveStringComparator());
    else
	found = FindInReadable(aPattern, start, end);

    if (!found)
	return kNotFound;

    nsAString::const_iterator originalStart;
    aSource.BeginReading(originalStart);
    return Distance(originalStart, start);
}

class	InternetSearchContext : public nsIInternetSearchContext
{
public:
			InternetSearchContext(PRUint32 contextType, nsIRDFResource *aParent, nsIRDFResource *aEngine,
				nsIUnicodeDecoder *aUnicodeDecoder, const PRUnichar *hint);
	virtual		~InternetSearchContext(void);
	NS_METHOD	Init();

	NS_DECL_ISUPPORTS
	NS_DECL_NSIINTERNETSEARCHCONTEXT

private:
	PRUint32			mContextType;
	nsCOMPtr<nsIRDFResource>	mParent;
	nsCOMPtr<nsIRDFResource>	mEngine;
	nsCOMPtr<nsIUnicodeDecoder>	mUnicodeDecoder;
	nsString			mBuffer;
	nsString			mHint;
};



InternetSearchContext::~InternetSearchContext(void)
{
}



InternetSearchContext::InternetSearchContext(PRUint32 contextType, nsIRDFResource *aParent, nsIRDFResource *aEngine,
				nsIUnicodeDecoder *aUnicodeDecoder, const PRUnichar *hint)
	: mContextType(contextType), mParent(aParent), mEngine(aEngine), mUnicodeDecoder(aUnicodeDecoder), mHint(hint)
{
}



NS_IMETHODIMP
InternetSearchContext::Init()
{
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::GetContextType(PRUint32 *aContextType)
{
	*aContextType = mContextType;
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::GetUnicodeDecoder(nsIUnicodeDecoder **decoder)
{
	*decoder = mUnicodeDecoder;
	NS_IF_ADDREF(*decoder);
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::GetEngine(nsIRDFResource **node)
{
	*node = mEngine;
	NS_IF_ADDREF(*node);
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::GetHintConst(const PRUnichar **hint)
{
	*hint = mHint.get();
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::GetParent(nsIRDFResource **node)
{
	*node = mParent;
	NS_IF_ADDREF(*node);
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::AppendBytes(const char *buffer, PRInt32 numBytes)
{
	mBuffer.AppendWithConversion(buffer, numBytes);
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::AppendUnicodeBytes(const PRUnichar *buffer, PRInt32 numUniBytes)
{
	mBuffer.Append(buffer, numUniBytes);
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::GetBufferConst(const PRUnichar **buffer)
{
	*buffer = mBuffer.get();
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::GetBufferLength(PRInt32 *bufferLen)
{
	*bufferLen = mBuffer.Length();
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchContext::Truncate()
{
	mBuffer.Truncate();
	return(NS_OK);
}



NS_IMPL_THREADSAFE_ISUPPORTS1(InternetSearchContext, nsIInternetSearchContext)



nsresult
NS_NewInternetSearchContext(PRUint32 contextType, nsIRDFResource *aParent, nsIRDFResource *aEngine,
			    nsIUnicodeDecoder *aUnicodeDecoder, const PRUnichar *hint, nsIInternetSearchContext **aResult)
{
	 InternetSearchContext *result =
		 new InternetSearchContext(contextType, aParent, aEngine, aUnicodeDecoder, hint);

	 if (! result)
		 return NS_ERROR_OUT_OF_MEMORY;

	 nsresult rv = result->Init();
	 if (NS_FAILED(rv)) {
		 delete result;
		 return rv;
	 }

	 NS_ADDREF(result);
	 *aResult = result;
	 return NS_OK;
}





static const char		kEngineProtocol[] = "engine://";
static const char		kSearchProtocol[] = "internetsearch:";

#ifdef	DEBUG_SEARCH_UPDATES
#define	SEARCH_UPDATE_TIMEOUT	10000		// fire every 10 seconds
#else
#define	SEARCH_UPDATE_TIMEOUT	60000		// fire every 60 seconds
#endif


int PR_CALLBACK
searchModePrefCallback(const char *pref, void *aClosure)
{
	InternetSearchDataSource *searchDS = NS_STATIC_CAST(InternetSearchDataSource *, aClosure);
	if (!searchDS)	return(NS_OK);

	if (searchDS->prefs)
	{
		searchDS->prefs->GetIntPref(pref, &searchDS->gBrowserSearchMode);
#ifdef	DEBUG
		printf("searchModePrefCallback: '%s' = %d\n", pref, searchDS->gBrowserSearchMode);
#endif
		searchDS->Assert(searchDS->kNC_LastSearchRoot, searchDS->kNC_LastSearchMode, searchDS->kTrueLiteral, PR_TRUE);
	}
	return(NS_OK);
}

static	nsIRDFService		*gRDFService = nsnull;
static	nsIRDFContainerUtils	*gRDFC = nsnull;
PRInt32				InternetSearchDataSource::gRefCnt = 0;
PRInt32				InternetSearchDataSource::gBrowserSearchMode = 0;
nsIRDFDataSource		*InternetSearchDataSource::mInner = nsnull;
nsCOMPtr<nsISupportsArray>	InternetSearchDataSource::mUpdateArray;
nsCOMPtr<nsIRDFDataSource>	InternetSearchDataSource::mLocalstore;
nsCOMPtr<nsIRDFDataSource>	InternetSearchDataSource::categoryDataSource;
PRBool				InternetSearchDataSource::gEngineListBuilt = PR_FALSE;
#ifdef MOZ_PHOENIX
PRBool        InternetSearchDataSource::gReorderedEngineList = PR_FALSE;
#endif
nsCOMPtr<nsILoadGroup>		InternetSearchDataSource::mBackgroundLoadGroup;
nsCOMPtr<nsILoadGroup>		InternetSearchDataSource::mLoadGroup;
nsCOMPtr<nsIPref>		InternetSearchDataSource::prefs;

nsIRDFResource			*InternetSearchDataSource::kNC_SearchResult;
nsIRDFResource			*InternetSearchDataSource::kNC_SearchEngineRoot;
nsIRDFResource			*InternetSearchDataSource::kNC_LastSearchRoot;
nsIRDFResource			*InternetSearchDataSource::kNC_LastSearchMode;
nsIRDFResource			*InternetSearchDataSource::kNC_SearchCategoryRoot;
nsIRDFResource			*InternetSearchDataSource::kNC_SearchResultsSitesRoot;
nsIRDFResource			*InternetSearchDataSource::kNC_FilterSearchURLsRoot;
nsIRDFResource			*InternetSearchDataSource::kNC_FilterSearchSitesRoot;
nsIRDFResource			*InternetSearchDataSource::kNC_SearchType;
nsIRDFResource			*InternetSearchDataSource::kNC_Ref;
nsIRDFResource			*InternetSearchDataSource::kNC_Child;
nsIRDFResource			*InternetSearchDataSource::kNC_Title;
nsIRDFResource			*InternetSearchDataSource::kNC_Data;
nsIRDFResource			*InternetSearchDataSource::kNC_Name;
nsIRDFResource			*InternetSearchDataSource::kNC_Description;
nsIRDFResource			*InternetSearchDataSource::kNC_Version;
nsIRDFResource			*InternetSearchDataSource::kNC_actionButton;
nsIRDFResource			*InternetSearchDataSource::kNC_actionBar;
nsIRDFResource			*InternetSearchDataSource::kNC_searchForm;
nsIRDFResource			*InternetSearchDataSource::kNC_LastText;
nsIRDFResource			*InternetSearchDataSource::kNC_URL;
nsIRDFResource			*InternetSearchDataSource::kRDF_InstanceOf;
nsIRDFResource			*InternetSearchDataSource::kRDF_type;
nsIRDFResource			*InternetSearchDataSource::kNC_loading;
nsIRDFResource			*InternetSearchDataSource::kNC_HTML;
nsIRDFResource			*InternetSearchDataSource::kNC_Icon;
nsIRDFResource			*InternetSearchDataSource::kNC_StatusIcon;
nsIRDFResource			*InternetSearchDataSource::kNC_Banner;
nsIRDFResource			*InternetSearchDataSource::kNC_Site;
nsIRDFResource			*InternetSearchDataSource::kNC_Relevance;
nsIRDFResource			*InternetSearchDataSource::kNC_RelevanceSort;
nsIRDFResource			*InternetSearchDataSource::kNC_Date;
nsIRDFResource			*InternetSearchDataSource::kNC_PageRank;
nsIRDFResource			*InternetSearchDataSource::kNC_Engine;
nsIRDFResource			*InternetSearchDataSource::kNC_Price;
nsIRDFResource			*InternetSearchDataSource::kNC_PriceSort;
nsIRDFResource			*InternetSearchDataSource::kNC_Availability;
nsIRDFResource			*InternetSearchDataSource::kNC_BookmarkSeparator;
nsIRDFResource			*InternetSearchDataSource::kNC_Update;
nsIRDFResource			*InternetSearchDataSource::kNC_UpdateIcon;
nsIRDFResource			*InternetSearchDataSource::kNC_UpdateCheckDays;
nsIRDFResource			*InternetSearchDataSource::kWEB_LastPingDate;
nsIRDFResource			*InternetSearchDataSource::kWEB_LastPingModDate;
nsIRDFResource			*InternetSearchDataSource::kWEB_LastPingContentLen;

nsIRDFResource			*InternetSearchDataSource::kNC_SearchCommand_AddToBookmarks;
nsIRDFResource			*InternetSearchDataSource::kNC_SearchCommand_AddQueryToBookmarks;
nsIRDFResource			*InternetSearchDataSource::kNC_SearchCommand_FilterResult;
nsIRDFResource			*InternetSearchDataSource::kNC_SearchCommand_FilterSite;
nsIRDFResource			*InternetSearchDataSource::kNC_SearchCommand_ClearFilters;

nsIRDFLiteral			*InternetSearchDataSource::kTrueLiteral;
InternetSearchDataSource::InternetSearchDataSource(void)
{
	if (gRefCnt++ == 0)
	{
		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
			NS_GET_IID(nsIRDFService), (nsISupports**) &gRDFService);
		PR_ASSERT(NS_SUCCEEDED(rv));

		rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
						  NS_GET_IID(nsIRDFContainerUtils),
						  (nsISupports**) &gRDFC);

		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF container utils");

		gRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_SearchEngineRoot),
                             &kNC_SearchEngineRoot);
		gRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_LastSearchRoot),
                             &kNC_LastSearchRoot);
		gRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_SearchResultsSitesRoot),
                             &kNC_SearchResultsSitesRoot);
		gRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_FilterSearchURLsRoot),
                             &kNC_FilterSearchURLsRoot);
		gRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_FilterSearchSitesRoot),
                             &kNC_FilterSearchSitesRoot);
		gRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_SearchCategoryRoot),
                             &kNC_SearchCategoryRoot);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "SearchMode"),
                             &kNC_LastSearchMode);

		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "searchtype"),
                             &kNC_SearchType);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "SearchResult"),
                             &kNC_SearchResult);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "ref"),
                             &kNC_Ref);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "child"),
                             &kNC_Child);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "title"),
                             &kNC_Title);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "data"),
                             &kNC_Data);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Name"),
                             &kNC_Name);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Description"),
                             &kNC_Description);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Version"),
                             &kNC_Version);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "actionButton"),
                             &kNC_actionButton);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "actionBar"),
                             &kNC_actionBar);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "searchForm"),
                             &kNC_searchForm);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "LastText"),
                             &kNC_LastText);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "URL"),
                             &kNC_URL);
		gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "instanceOf"),
                             &kRDF_InstanceOf);
		gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "type"),
                             &kRDF_type);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "loading"),
                             &kNC_loading);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "HTML"),
                             &kNC_HTML);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Icon"),
                             &kNC_Icon);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "StatusIcon"),
                             &kNC_StatusIcon);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Banner"),
                             &kNC_Banner);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Site"),
                             &kNC_Site);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Relevance"),
                             &kNC_Relevance);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Relevance?sort=true"),
                             &kNC_RelevanceSort);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Date"),
                             &kNC_Date);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "PageRank"),
                             &kNC_PageRank);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Engine"),
                             &kNC_Engine);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Price"),
                             &kNC_Price);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Price?sort=true"),
                             &kNC_PriceSort);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Availability"),
                             &kNC_Availability);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "BookmarkSeparator"),
                             &kNC_BookmarkSeparator);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Update"),
                             &kNC_Update);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "UpdateIcon"),
                             &kNC_UpdateIcon);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "UpdateCheckDays"),
                             &kNC_UpdateCheckDays);
		gRDFService->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "LastPingDate"),
                             &kWEB_LastPingDate);
		gRDFService->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "LastPingModDate"),
                             &kWEB_LastPingModDate);
		gRDFService->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "LastPingContentLen"),
                             &kWEB_LastPingContentLen);

		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=addtobookmarks"),
                             &kNC_SearchCommand_AddToBookmarks);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=addquerytobookmarks"),
                             &kNC_SearchCommand_AddQueryToBookmarks);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=filterresult"),
                             &kNC_SearchCommand_FilterResult);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=filtersite"),
                             &kNC_SearchCommand_FilterSite);
		gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=clearfilters"),
                             &kNC_SearchCommand_ClearFilters);

		gRDFService->GetLiteral(NS_LITERAL_STRING("true").get(), &kTrueLiteral);

		rv = nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref), getter_AddRefs(prefs));
		if (NS_SUCCEEDED(rv) && (prefs))
		{
			prefs->RegisterCallback("browser.search.mode", searchModePrefCallback, this);
			prefs->GetIntPref("browser.search.mode", &gBrowserSearchMode);
		}
	}
}



InternetSearchDataSource::~InternetSearchDataSource (void)
{
	if (--gRefCnt == 0)
	{
		NS_IF_RELEASE(kNC_SearchResult);
		NS_IF_RELEASE(kNC_SearchEngineRoot);
		NS_IF_RELEASE(kNC_LastSearchRoot);
		NS_IF_RELEASE(kNC_LastSearchMode);
		NS_IF_RELEASE(kNC_SearchCategoryRoot);
		NS_IF_RELEASE(kNC_SearchResultsSitesRoot);
		NS_IF_RELEASE(kNC_FilterSearchURLsRoot);
		NS_IF_RELEASE(kNC_FilterSearchSitesRoot);
		NS_IF_RELEASE(kNC_SearchType);
		NS_IF_RELEASE(kNC_Ref);
		NS_IF_RELEASE(kNC_Child);
		NS_IF_RELEASE(kNC_Title);
		NS_IF_RELEASE(kNC_Data);
		NS_IF_RELEASE(kNC_Name);
		NS_IF_RELEASE(kNC_Description);
		NS_IF_RELEASE(kNC_Version);
		NS_IF_RELEASE(kNC_actionButton);
		NS_IF_RELEASE(kNC_actionBar);
		NS_IF_RELEASE(kNC_searchForm);
		NS_IF_RELEASE(kNC_LastText);
		NS_IF_RELEASE(kNC_URL);
		NS_IF_RELEASE(kRDF_InstanceOf);
		NS_IF_RELEASE(kRDF_type);
		NS_IF_RELEASE(kNC_loading);
		NS_IF_RELEASE(kNC_HTML);
		NS_IF_RELEASE(kNC_Icon);
		NS_IF_RELEASE(kNC_StatusIcon);
		NS_IF_RELEASE(kNC_Banner);
		NS_IF_RELEASE(kNC_Site);
		NS_IF_RELEASE(kNC_Relevance);
		NS_IF_RELEASE(kNC_RelevanceSort);
		NS_IF_RELEASE(kNC_Date);
		NS_IF_RELEASE(kNC_PageRank);
		NS_IF_RELEASE(kNC_Engine);
		NS_IF_RELEASE(kNC_Price);
		NS_IF_RELEASE(kNC_PriceSort);
		NS_IF_RELEASE(kNC_Availability);
		NS_IF_RELEASE(kNC_BookmarkSeparator);
		NS_IF_RELEASE(kNC_Update);
		NS_IF_RELEASE(kNC_UpdateIcon);
		NS_IF_RELEASE(kNC_UpdateCheckDays);
		NS_IF_RELEASE(kWEB_LastPingDate);
		NS_IF_RELEASE(kWEB_LastPingModDate);
		NS_IF_RELEASE(kWEB_LastPingContentLen);

		NS_IF_RELEASE(kNC_SearchCommand_AddToBookmarks);
		NS_IF_RELEASE(kNC_SearchCommand_AddQueryToBookmarks);
		NS_IF_RELEASE(kNC_SearchCommand_FilterResult);
		NS_IF_RELEASE(kNC_SearchCommand_FilterSite);
		NS_IF_RELEASE(kNC_SearchCommand_ClearFilters);

		NS_IF_RELEASE(kTrueLiteral);

		NS_IF_RELEASE(mInner);

		mUpdateArray = nsnull;
		mLocalstore = nsnull;
		mBackgroundLoadGroup = nsnull;
		mLoadGroup = nsnull;
		categoryDataSource = nsnull;		

		if (mTimer)
		{
			// be sure to cancel the timer, as it holds a
			// weak reference back to InternetSearchDataSource
			mTimer->Cancel();
			mTimer = nsnull;
		}

		if (prefs)
		{
			prefs->UnregisterCallback("browser.search.mode", searchModePrefCallback, this);
			prefs = nsnull;
		}

		if (gRDFC)
		{
			nsServiceManager::ReleaseService(kRDFContainerUtilsCID, gRDFC);
			gRDFC = nsnull;
		}

		if (gRDFService)
		{
			gRDFService->UnregisterDataSource(this);

			nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
			gRDFService = nsnull;
		}
	}
}



nsresult
InternetSearchDataSource::GetSearchEngineToPing(nsIRDFResource **theEngine, nsCString &updateURL)
{
	nsresult	rv = NS_OK;

	*theEngine = nsnull;
	updateURL.Truncate();

	if (!mUpdateArray)	return(NS_OK);

	PRUint32	numEngines = 0;
	if (NS_FAILED(rv = mUpdateArray->Count(&numEngines)))	return(rv);
	if (numEngines < 1)	return(NS_OK);

	nsCOMPtr<nsISupports>	isupports = mUpdateArray->ElementAt(0);

	// note: important to remove element from array
	mUpdateArray->RemoveElementAt(0);
	if (isupports)
	{
		nsCOMPtr<nsIRDFResource> aRes (do_QueryInterface(isupports));
		if (aRes)
		{
			if (isSearchCategoryEngineURI(aRes))
			{
				nsCOMPtr<nsIRDFResource>	trueEngine;
				rv = resolveSearchCategoryEngineURI(aRes, getter_AddRefs(trueEngine));
				if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE))	return(rv);
				if (!trueEngine)	return(NS_RDF_NO_VALUE);

				aRes = trueEngine;
			}

			if (!aRes)	return(NS_OK);

			*theEngine = aRes.get();
			NS_ADDREF(*theEngine);

			// get update URL
			nsCOMPtr<nsIRDFNode>	aNode;
			if (NS_SUCCEEDED(rv = mInner->GetTarget(aRes, kNC_Update, PR_TRUE, getter_AddRefs(aNode)))
				&& (rv != NS_RDF_NO_VALUE))
			{
				nsCOMPtr<nsIRDFLiteral>	aLiteral (do_QueryInterface(aNode));
				if (aLiteral)
				{
					const PRUnichar	*updateUni = nsnull;
					aLiteral->GetValueConst(&updateUni);
					if (updateUni)
					{
						updateURL.AssignWithConversion(updateUni);
					}
				}
			}
		}
	}
	return(rv);
}



void
InternetSearchDataSource::FireTimer(nsITimer* aTimer, void* aClosure)
{
	InternetSearchDataSource *search = NS_STATIC_CAST(InternetSearchDataSource *, aClosure);
	if (!search)	return;

  if (!search->busySchedule)
	{
		nsresult			rv;
		nsCOMPtr<nsIRDFResource>	searchURI;
		nsCAutoString			updateURL;
		if (NS_FAILED(rv = search->GetSearchEngineToPing(getter_AddRefs(searchURI), updateURL)))
			return;
		if (!searchURI)			return;
		if (updateURL.IsEmpty())	return;

		search->busyResource = searchURI;

		nsCOMPtr<nsIInternetSearchContext>	engineContext;
		if (NS_FAILED(rv = NS_NewInternetSearchContext(nsIInternetSearchContext::ENGINE_UPDATE_CONTEXT,
			nsnull, searchURI, nsnull, nsnull, getter_AddRefs(engineContext))))
			return;
		if (!engineContext)	return;

		nsCOMPtr<nsIURI>	uri;
		if (NS_FAILED(rv = NS_NewURI(getter_AddRefs(uri), updateURL.get())))	return;

		nsCOMPtr<nsIChannel>	channel;
		if (NS_FAILED(rv = NS_NewChannel(getter_AddRefs(channel), uri, nsnull)))	return;

		channel->SetLoadFlags(nsIRequest::VALIDATE_ALWAYS);

		nsCOMPtr<nsIHttpChannel> httpChannel (do_QueryInterface(channel));
		if (!httpChannel)	return;

		// rjc says: just check "HEAD" info for whether a search file has changed
        httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("HEAD"));
		if (NS_SUCCEEDED(rv = channel->AsyncOpen(search, engineContext)))
		{
			search->busySchedule = PR_TRUE;

#ifdef	DEBUG_SEARCH_UPDATES
			printf("    InternetSearchDataSource::FireTimer - Pinging '%s'\n", (char *)updateURL);
#endif
		}
	}
#ifdef	DEBUG_SEARCH_UPDATES
else
	{
	printf("    InternetSearchDataSource::FireTimer - busy pinging.\n");
	}
#endif
}



PRBool
InternetSearchDataSource::isEngineURI(nsIRDFResource *r)
{
	PRBool		isEngineURIFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	r->GetValueConst(&uri);
	if ((uri) && (!strncmp(uri, kEngineProtocol, sizeof(kEngineProtocol) - 1)))
	{
		isEngineURIFlag = PR_TRUE;
	}
	return(isEngineURIFlag);
}



PRBool
InternetSearchDataSource::isSearchURI(nsIRDFResource *r)
{
	PRBool		isSearchURIFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	r->GetValueConst(&uri);
	if ((uri) && (!strncmp(uri, kSearchProtocol, sizeof(kSearchProtocol) - 1)))
	{
		isSearchURIFlag = PR_TRUE;
	}
	return(isSearchURIFlag);
}



PRBool
InternetSearchDataSource::isSearchCategoryURI(nsIRDFResource *r)
{
	PRBool		isSearchCategoryURIFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	r->GetValueConst(&uri);
	if ((uri) && (!strncmp(uri, kURINC_SearchCategoryPrefix, sizeof(kURINC_SearchCategoryPrefix) - 1)))
	{
		isSearchCategoryURIFlag = PR_TRUE;
	}
	return(isSearchCategoryURIFlag);
}



PRBool
InternetSearchDataSource::isSearchCategoryEngineURI(nsIRDFResource *r)
{
	PRBool		isSearchCategoryEngineURIFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	r->GetValueConst(&uri);
	if ((uri) && (!strncmp(uri, kURINC_SearchCategoryEnginePrefix, sizeof(kURINC_SearchCategoryEnginePrefix) - 1)))
	{
		isSearchCategoryEngineURIFlag = PR_TRUE;
	}
	return(isSearchCategoryEngineURIFlag);
}



PRBool
InternetSearchDataSource::isSearchCategoryEngineBasenameURI(nsIRDFNode *r)
{
	PRBool		isSearchCategoryEngineBasenameURIFlag = PR_FALSE;

	nsCOMPtr<nsIRDFResource> aRes (do_QueryInterface(r));
	if (aRes)
	{
		const char	*uri = nsnull;
		aRes->GetValueConst(&uri);
		if ((uri) && (!nsCRT::strncmp(uri, kURINC_SearchCategoryEngineBasenamePrefix,
			(int)sizeof(kURINC_SearchCategoryEngineBasenamePrefix) - 1)))
		{
			isSearchCategoryEngineBasenameURIFlag = PR_TRUE;
		}
	}
	else
	{
		nsCOMPtr<nsIRDFLiteral>	aLit (do_QueryInterface(r));
		if (aLit)
		{
			const	PRUnichar	*uriUni = nsnull;
			aLit->GetValueConst(&uriUni);
			if ((uriUni) && (!nsCRT::strncmp(uriUni,
							 NS_ConvertASCIItoUCS2(kURINC_SearchCategoryEngineBasenamePrefix).get(),
				(int)sizeof(kURINC_SearchCategoryEngineBasenamePrefix) - 1)))
			{
				isSearchCategoryEngineBasenameURIFlag = PR_TRUE;
			}
		}
	}
	return(isSearchCategoryEngineBasenameURIFlag);
}



PRBool
InternetSearchDataSource::isSearchCommand(nsIRDFResource *r)
{
	PRBool		isSearchCommandFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	if (NS_SUCCEEDED(r->GetValueConst( &uri )) && (uri))
	{
		if (!strncmp(uri, kSearchCommand, sizeof(kSearchCommand) - 1))
		{
			isSearchCommandFlag = PR_TRUE;
		}
	}
	return(isSearchCommandFlag);
}



nsresult
InternetSearchDataSource::resolveSearchCategoryEngineURI(nsIRDFResource *engine, nsIRDFResource **trueEngine)
{
	*trueEngine = nsnull;

	if ((!categoryDataSource) || (!mInner))	return(NS_ERROR_UNEXPECTED);

	nsresult	rv;
	const char	*uriUni = nsnull;
	if (NS_FAILED(rv = engine->GetValueConst(&uriUni)))	return(rv);
	if (!uriUni)	return(NS_ERROR_NULL_POINTER);

	nsAutoString		uri;
	uri.AssignWithConversion(uriUni);
	if (uri.Find(kURINC_SearchCategoryEngineBasenamePrefix) !=0)	return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIRDFLiteral>	basenameLiteral;
	if (NS_FAILED(rv = gRDFService->GetLiteral(uri.get(),
			getter_AddRefs(basenameLiteral))))	return(rv);

	nsCOMPtr<nsIRDFResource>	catSrc;
	rv = mInner->GetSource(kNC_URL, basenameLiteral, PR_TRUE, getter_AddRefs(catSrc));
	if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE))	return(rv);
	if (!catSrc)		return(NS_ERROR_UNEXPECTED);

	*trueEngine = catSrc;
	NS_IF_ADDREF(*trueEngine);
	return(NS_OK);
}



////////////////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(InternetSearchDataSource)
NS_IMPL_RELEASE(InternetSearchDataSource)

NS_INTERFACE_MAP_BEGIN(InternetSearchDataSource)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInternetSearchService)
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIInternetSearchService)
   NS_INTERFACE_MAP_ENTRY(nsIRDFDataSource)
   NS_INTERFACE_MAP_ENTRY(nsIObserver)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END


NS_METHOD
InternetSearchDataSource::Init()
{
	nsresult	rv = NS_ERROR_OUT_OF_MEMORY;

	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
		nsnull, NS_GET_IID(nsIRDFDataSource), (void **)&mInner)))
		return(rv);

	// get localstore, as we'll be using it
	if (NS_FAILED(rv = gRDFService->GetDataSource("rdf:local-store", getter_AddRefs(mLocalstore))))
		return(rv);

	if (NS_FAILED(rv = NS_NewISupportsArray(getter_AddRefs(mUpdateArray))))
		return(rv);

	// register this as a named data source with the service manager
	if (NS_FAILED(rv = gRDFService->RegisterDataSource(this, PR_FALSE)))
		return(rv);

	rv = NS_NewLoadGroup(getter_AddRefs(mLoadGroup), nsnull);
	PR_ASSERT(NS_SUCCEEDED(rv));

	if (!mTimer)
	{
		busySchedule = PR_FALSE;
		mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
		if (mTimer)
		{
			mTimer->InitWithFuncCallback(InternetSearchDataSource::FireTimer, this,
                                                     SEARCH_UPDATE_TIMEOUT, nsITimer::TYPE_REPEATING_SLACK);
			// Note: don't addref "this" as we'll cancel the timer in the
			//       InternetSearchDataSource destructor
		}
	}

	gEngineListBuilt = PR_FALSE;

	// Register as a profile change obsevrer
  nsCOMPtr<nsIObserverService> observerService = 
           do_GetService("@mozilla.org/observer-service;1", &rv);
  if (observerService) {
    observerService->AddObserver(this, "profile-before-change", PR_TRUE);
    observerService->AddObserver(this, "profile-do-change", PR_TRUE);
  }

	return(rv);
}



NS_METHOD
InternetSearchDataSource::DeferredInit()
{
	nsresult	rv = NS_OK;

  if (!gEngineListBuilt)
	{
		gEngineListBuilt = PR_TRUE;

		// get available search engines
		nsCOMPtr<nsIFile>			nativeDir;
		if (NS_SUCCEEDED(rv = GetSearchFolder(getter_AddRefs(nativeDir))))
		{
			rv = GetSearchEngineList(nativeDir, PR_FALSE, PR_FALSE);
			
			// read in category list
			rv = GetCategoryList();
		}

#ifdef	XP_MAC
		// on Mac, use system's search files too
      	nsCOMPtr<nsIFile> macSearchDir;

        rv = NS_GetSpecialDirectory(NS_MAC_INTERNET_SEARCH_DIR, getter_AddRefs(macSearchDir));
        if (NS_SUCCEEDED(rv))
        {
      		// Mac OS X doesn't have file types set for search files, so don't check them
      		long response;
      		OSErr err = ::Gestalt(gestaltSystemVersion, &response);
      		PRBool checkFileType = (!err) && (response >= 0x00001000) ? PR_FALSE : PR_TRUE;
      		rv = GetSearchEngineList(macSearchDir, PR_TRUE, checkFileType);
      	}
#endif
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::GetURI(char **uri)
{
	NS_PRECONDITION(uri != nsnull, "null ptr");
	if (! uri)
		return NS_ERROR_NULL_POINTER;

	if ((*uri = nsCRT::strdup("rdf:internetsearch")) == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}



NS_IMETHODIMP
InternetSearchDataSource::GetSource(nsIRDFResource* property,
                                nsIRDFNode* target,
                                PRBool tv,
                                nsIRDFResource** source /* out */)
{
	NS_PRECONDITION(property != nsnull, "null ptr");
	if (! property)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(target != nsnull, "null ptr");
	if (! target)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	*source = nsnull;
	return NS_RDF_NO_VALUE;
}



NS_IMETHODIMP
InternetSearchDataSource::GetSources(nsIRDFResource *property,
                                 nsIRDFNode *target,
                                 PRBool tv,
                                 nsISimpleEnumerator **sources /* out */)
{
	nsresult	rv = NS_RDF_NO_VALUE;

	if (mInner)
	{
		rv = mInner->GetSources(property, target, tv, sources);
	}
	else
	{
		rv = NS_NewEmptyEnumerator(sources);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::GetTarget(nsIRDFResource *source,
                                nsIRDFResource *property,
                                PRBool tv,
                                nsIRDFNode **target /* out */)
{
	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(property != nsnull, "null ptr");
	if (! property)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(target != nsnull, "null ptr");
	if (! target)
		return NS_ERROR_NULL_POINTER;

	*target = nsnull;

	nsresult		rv = NS_RDF_NO_VALUE;

	// we only have positive assertions in the internet search data source.
	if (! tv)
		return(rv);

	if ((isSearchCategoryURI(source)) && (categoryDataSource))
	{
		const char	*uri = nsnull;
		source->GetValueConst(&uri);
		if (!uri)	return(NS_ERROR_UNEXPECTED);
		nsAutoString	catURI;
		catURI.AssignWithConversion(uri);

		nsCOMPtr<nsIRDFResource>	category;
		nsCAutoString			caturiC;
		caturiC.AssignWithConversion(catURI);
		if (NS_FAILED(rv = gRDFService->GetResource(caturiC,
			getter_AddRefs(category))))
			return(rv);

		rv = categoryDataSource->GetTarget(category, property, tv, target);
		return(rv);
	}

	if (isSearchCategoryEngineURI(source))
	{
		nsCOMPtr<nsIRDFResource>	trueEngine;
		rv = resolveSearchCategoryEngineURI(source, getter_AddRefs(trueEngine));
		if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE))	return(rv);
		if (!trueEngine)	return(NS_RDF_NO_VALUE);

		source = trueEngine;
	}

	if (isSearchURI(source) && (property == kNC_Child))
	{
		// fake out the generic builder (i.e. return anything in this case)
		// so that search containers never appear to be empty
		*target = source;
		NS_ADDREF(source);
		return(NS_OK);
	}

	if (isSearchCommand(source) && (property == kNC_Name))
	{
    nsresult rv;
    nsCOMPtr<nsIStringBundleService>
    stringService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv) && stringService) {

      nsCOMPtr<nsIStringBundle> bundle;
      rv = stringService->CreateBundle(SEARCH_PROPERTIES, getter_AddRefs(bundle));
      if (NS_SUCCEEDED(rv) && bundle) {

        nsXPIDLString valUni;
        nsAutoString name;

        if (source == kNC_SearchCommand_AddToBookmarks)
          name.AssignLiteral("addtobookmarks");
        else if (source == kNC_SearchCommand_AddQueryToBookmarks)
          name.AssignLiteral("addquerytobookmarks");
        else if (source == kNC_SearchCommand_FilterResult)
          name.AssignLiteral("excludeurl");
        else if (source == kNC_SearchCommand_FilterSite)
          name.AssignLiteral("excludedomain");
        else if (source == kNC_SearchCommand_ClearFilters)
          name.AssignLiteral("clearfilters");

        rv = bundle->GetStringFromName(name.get(), getter_Copies(valUni));
        if (NS_SUCCEEDED(rv) && valUni && *valUni) {
          *target = nsnull;
          nsCOMPtr<nsIRDFLiteral> literal;
          if (NS_FAILED(rv = gRDFService->GetLiteral(valUni, getter_AddRefs(literal))))
            return rv;
          *target = literal;
          NS_IF_ADDREF(*target);
          return rv;
        }
      }
    }
  }

	if (isEngineURI(source))
	{
		// if we're asking for info on a search engine, (deferred) load it if needed
		nsCOMPtr<nsIRDFLiteral>	dataLit;
		FindData(source, getter_AddRefs(dataLit));
	}

	if (mInner)
	{
		rv = mInner->GetTarget(source, property, tv, target);
	}

	return(rv);
}

#ifdef MOZ_PHOENIX
void
InternetSearchDataSource::ReorderEngineList()
{
  // XXXben - a temporary, inelegant solution to search list ordering until 
  //          after 1.0 when we replace all of this with something better 
  //          suited to our needs. 
  nsresult rv;
  nsCOMArray<nsIRDFResource> engineList;

  // Load all data for sherlock files...
  nsCOMPtr<nsISimpleEnumerator> engines;
  rv = GetTargets(kNC_SearchEngineRoot, kNC_Child, PR_TRUE, getter_AddRefs(engines));
  if (NS_FAILED(rv)) return; // Not Fatal. 

  do {
    PRBool hasMore;
    engines->HasMoreElements(&hasMore);
    if (!hasMore)
      break;

    nsCOMPtr<nsISupports> supp;
    engines->GetNext(getter_AddRefs(supp));
    nsCOMPtr<nsIRDFResource> engineResource(do_QueryInterface(supp));

    nsCOMPtr<nsIRDFLiteral> data;
    FindData(engineResource, getter_AddRefs(data));
  }
  while (PR_TRUE);

  // Build the ordered items first...
  nsCOMPtr<nsIPrefBranch> pserv(do_QueryInterface(prefs));
  char prefNameBuf[1096];
  PRInt32 i = 0; 
  do {
    ++i;
    sprintf(prefNameBuf, "browser.search.order.%d", i);

    nsCOMPtr<nsIPrefLocalizedString> engineName;
    rv = pserv->GetComplexValue(prefNameBuf, 
                                NS_GET_IID(nsIPrefLocalizedString),
                                getter_AddRefs(engineName));
    if (NS_FAILED(rv)) break;

    nsXPIDLString data;
    engineName->GetData(getter_Copies(data));

    nsCOMPtr<nsIRDFLiteral> engineNameLiteral;
    gRDFService->GetLiteral(data, getter_AddRefs(engineNameLiteral));

    nsCOMPtr<nsIRDFResource> engineResource;
    rv = mInner->GetSource(kNC_Name, engineNameLiteral, PR_TRUE, getter_AddRefs(engineResource));
    if (NS_FAILED(rv)) continue;

    engineList.AppendObject(engineResource);
  }
  while (PR_TRUE);

  // Now add the rest...
  rv = GetTargets(kNC_SearchEngineRoot, kNC_Child, PR_TRUE, getter_AddRefs(engines));
  if (NS_FAILED(rv)) return; // Not Fatal. 

  do {
    PRBool hasMore;
    engines->HasMoreElements(&hasMore);
    if (!hasMore)
      break;

    nsCOMPtr<nsISupports> supp;
    engines->GetNext(getter_AddRefs(supp));
    nsCOMPtr<nsIRDFResource> engineResource(do_QueryInterface(supp));

    if (engineList.IndexOfObject(engineResource) == -1) 
      engineList.AppendObject(engineResource);

    // Unhook the item from the list, so we can rebuild it in the right
    // order. Failures are benign.
    Unassert(kNC_SearchEngineRoot, kNC_Child, engineResource);
  }
  while (PR_TRUE);

  PRInt32 engineCount = engineList.Count();
  for (i = 0; i < engineCount; ++i)
    Assert(kNC_SearchEngineRoot, kNC_Child, engineList[i], PR_TRUE);

  gReorderedEngineList = PR_TRUE;
}
#endif

NS_IMETHODIMP
InternetSearchDataSource::GetTargets(nsIRDFResource *source,
                           nsIRDFResource *property,
                           PRBool tv,
                           nsISimpleEnumerator **targets /* out */)
{
	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(property != nsnull, "null ptr");
	if (! property)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(targets != nsnull, "null ptr");
	if (! targets)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_RDF_NO_VALUE;

	// we only have positive assertions in the internet search data source.
	if (! tv)
		return(rv);

	if ((isSearchCategoryURI(source)) && (categoryDataSource))
	{
		const char	*uri = nsnull;
		source->GetValueConst(&uri);
		if (!uri)	return(NS_ERROR_UNEXPECTED);
		nsAutoString	catURI;
		catURI.AssignWithConversion(uri);

		nsCOMPtr<nsIRDFResource>	category;
		nsCAutoString			caturiC;
		caturiC.AssignWithConversion(catURI);
		if (NS_FAILED(rv = gRDFService->GetResource(caturiC,
			getter_AddRefs(category))))
			return(rv);

		rv = categoryDataSource->GetTargets(category, property, tv, targets);
		return(rv);
	}

	if (isSearchCategoryEngineURI(source))
	{
		nsCOMPtr<nsIRDFResource>	trueEngine;
		rv = resolveSearchCategoryEngineURI(source, getter_AddRefs(trueEngine));
		if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE))	return(rv);
		if (!trueEngine)	return(NS_RDF_NO_VALUE);

		source = trueEngine;
	}

	if (mInner)
	{	
		// defer search engine discovery until needed; small startup time improvement
    if ((source == kNC_SearchEngineRoot || isSearchURI(source)) &&
        property == kNC_Child && !gEngineListBuilt)
		{
			DeferredInit();
		}

    rv = mInner->GetTargets(source, property, tv, targets);
	}
	if (isSearchURI(source))
	{
		if (property == kNC_Child)
		{
			PRBool		doNetworkRequest = PR_TRUE;
			if (NS_SUCCEEDED(rv) && (targets))
			{
				// check and see if we already have data for the search in question;
				// if we do, BeginSearchRequest() won't bother doing the search again,
				// otherwise it will kickstart it
				PRBool		hasResults = PR_FALSE;
        if (NS_SUCCEEDED((*targets)->HasMoreElements(&hasResults)) &&
            hasResults)
				{
					doNetworkRequest = PR_FALSE;
				}
			}
			BeginSearchRequest(source, doNetworkRequest);
		}
	}
	return(rv);
}



nsresult
InternetSearchDataSource::GetCategoryList()
{
	nsIRDFDataSource	*ds = nsnull;
	nsresult rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
			nsnull, NS_GET_IID(nsIRDFDataSource), (void**) &ds);
	if (NS_FAILED(rv))	return(rv);
	if (!ds)		return(NS_ERROR_UNEXPECTED);

	categoryDataSource = do_QueryInterface(ds);
	NS_RELEASE(ds);
	ds = nsnull;
	if (!categoryDataSource)	return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIRDFRemoteDataSource> remoteCategoryDataSource (do_QueryInterface(categoryDataSource));
	if (!remoteCategoryDataSource)	return(NS_ERROR_UNEXPECTED);

	// get search.rdf
		
  nsCOMPtr<nsIFile> searchFile;
  nsCAutoString searchFileURLSpec;

  rv = NS_GetSpecialDirectory(NS_APP_SEARCH_50_FILE, getter_AddRefs(searchFile));
  if (NS_FAILED(rv)) return rv;
  NS_GetURLSpecFromFile(searchFile, searchFileURLSpec);
  if (NS_FAILED(rv)) return rv;
	rv = remoteCategoryDataSource->Init(searchFileURLSpec.get());
  if (NS_FAILED(rv)) return rv;
    
	// synchronous read
	rv = remoteCategoryDataSource->Refresh(PR_TRUE);
	if (NS_FAILED(rv))	return(rv);

	// ensure that all search engine references actually exist; if not, forget about them

	PRBool				isDirtyFlag = PR_FALSE;
	nsCOMPtr<nsIRDFContainer> 	categoryRoot;
	rv = gRDFC->MakeSeq(categoryDataSource, kNC_SearchCategoryRoot, getter_AddRefs(categoryRoot));
	if (NS_FAILED(rv))	return(rv);
	if (!categoryRoot)	return(NS_ERROR_UNEXPECTED);

	rv = categoryRoot->Init(categoryDataSource, kNC_SearchCategoryRoot);
	if (NS_FAILED(rv))	return(rv);

	PRInt32		numCategories = 0;
	rv = categoryRoot->GetCount(&numCategories);
	if (NS_FAILED(rv))	return(rv);

	// Note: one-based
	for (PRInt32 catLoop=1; catLoop <= numCategories; catLoop++)
	{
		nsCOMPtr<nsIRDFResource>	aCategoryOrdinal;
		rv = gRDFC->IndexToOrdinalResource(catLoop,
			getter_AddRefs(aCategoryOrdinal));
		if (NS_FAILED(rv))	break;
		if (!aCategoryOrdinal)	break;

		nsCOMPtr<nsIRDFNode>		aCategoryNode;
		rv = categoryDataSource->GetTarget(kNC_SearchCategoryRoot, aCategoryOrdinal,
			PR_TRUE, getter_AddRefs(aCategoryNode));
		if (NS_FAILED(rv))	break;
		nsCOMPtr<nsIRDFResource> aCategoryRes (do_QueryInterface(aCategoryNode));
		if (!aCategoryRes)	break;
		const	char			*catResURI = nsnull;
		aCategoryRes->GetValueConst(&catResURI);
		if (!catResURI)		break;
		nsAutoString		categoryStr;
		categoryStr.AssignWithConversion(kURINC_SearchCategoryPrefix);
		categoryStr.AppendWithConversion(catResURI);

		nsCOMPtr<nsIRDFResource>	searchCategoryRes;
		if (NS_FAILED(rv = gRDFService->GetUnicodeResource(categoryStr,
			getter_AddRefs(searchCategoryRes))))	break;

		nsCOMPtr<nsIRDFContainer> categoryContainer;
		rv = gRDFC->MakeSeq(categoryDataSource, searchCategoryRes,
			getter_AddRefs(categoryContainer));
		if (NS_FAILED(rv))	continue;

		rv = categoryContainer->Init(categoryDataSource, searchCategoryRes);
		if (NS_FAILED(rv))	return(rv);

		PRInt32		numEngines = 0;
		rv = categoryContainer->GetCount(&numEngines);
		if (NS_FAILED(rv))	break;

		// Note: one-based, and loop backwards as we might be removing entries
		for (PRInt32 engineLoop=numEngines; engineLoop >= 1; engineLoop--)
		{
			nsCOMPtr<nsIRDFResource>	aEngineOrdinal;
			rv = gRDFC->IndexToOrdinalResource(engineLoop,
				getter_AddRefs(aEngineOrdinal));
			if (NS_FAILED(rv))	break;
			if (!aEngineOrdinal)	break;

			nsCOMPtr<nsIRDFNode>		aEngineNode;
			rv = categoryDataSource->GetTarget(searchCategoryRes, aEngineOrdinal,
				PR_TRUE, getter_AddRefs(aEngineNode));
			if (NS_FAILED(rv))	break;
			nsCOMPtr<nsIRDFResource> aEngineRes (do_QueryInterface(aEngineNode));
			if (!aEngineRes)	break;

			if (isSearchCategoryEngineURI(aEngineRes))
			{
				nsCOMPtr<nsIRDFResource>	trueEngine;
				rv = resolveSearchCategoryEngineURI(aEngineRes,
					getter_AddRefs(trueEngine));
				if (NS_FAILED(rv) || (!trueEngine))
				{
					// unable to resolve, so forget about this engine reference
#ifdef	DEBUG
					const	char		*catEngineURI = nsnull;
					aEngineRes->GetValueConst(&catEngineURI);
					if (catEngineURI)
					{
						printf("**** Stale search engine reference to '%s'\n",
							catEngineURI);
					}
#endif
					nsCOMPtr<nsIRDFNode>	staleCatEngine;
					rv = categoryContainer->RemoveElementAt(engineLoop, PR_TRUE,
						getter_AddRefs(staleCatEngine));
					isDirtyFlag = PR_TRUE;
				}
			}
		}
	}

  if (isDirtyFlag)
	{
		if (remoteCategoryDataSource)
		{
			remoteCategoryDataSource->Flush();
		}
	}

	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	// we only have positive assertions in the internet search data source.
	if (! tv)
		return(rv);

	if (mInner)
	{
		rv = mInner->Assert(source, property, target, tv);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (mInner)
	{
		rv = mInner->Unassert(source, property, target);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::Change(nsIRDFResource *source,
			nsIRDFResource *property,
			nsIRDFNode *oldTarget,
			nsIRDFNode *newTarget)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (mInner)
	{
		rv = mInner->Change(source, property, oldTarget, newTarget);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::Move(nsIRDFResource *oldSource,
					   nsIRDFResource *newSource,
					   nsIRDFResource *property,
					   nsIRDFNode *target)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (mInner)
	{
		rv = mInner->Move(oldSource, newSource, property, target);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::HasAssertion(nsIRDFResource *source,
                             nsIRDFResource *property,
                             nsIRDFNode *target,
                             PRBool tv,
                             PRBool *hasAssertion /* out */)
{
	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(property != nsnull, "null ptr");
	if (! property)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(target != nsnull, "null ptr");
	if (! target)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(hasAssertion != nsnull, "null ptr");
	if (! hasAssertion)
		return NS_ERROR_NULL_POINTER;

	*hasAssertion = PR_FALSE;

	// we only have positive assertions in the internet search data source.
	if (! tv)
	{
		return NS_OK;
        }
        nsresult	rv = NS_RDF_NO_VALUE;
        
        if (mInner)
        {
		rv = mInner->HasAssertion(source, property, target, tv, hasAssertion);
	}
        return(rv);
}

NS_IMETHODIMP 
InternetSearchDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
    if (mInner) {
	return mInner->HasArcIn(aNode, aArc, result);
    }
    else {
	*result = PR_FALSE;
    }
    return NS_OK;
}

NS_IMETHODIMP 
InternetSearchDataSource::HasArcOut(nsIRDFResource *source, nsIRDFResource *aArc, PRBool *result)
{
    NS_PRECONDITION(source != nsnull, "null ptr");
    if (! source)
	return NS_ERROR_NULL_POINTER;

    nsresult rv;

    if ((source == kNC_SearchEngineRoot) || (source == kNC_LastSearchRoot) || isSearchURI(source))
    {
	*result = (aArc == kNC_Child);
	return NS_OK;
    }

    if ((isSearchCategoryURI(source)) && (categoryDataSource))
    {
	const char	*uri = nsnull;
	source->GetValueConst(&uri);
	if (!uri)	return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIRDFResource>	category;
	if (NS_FAILED(rv = gRDFService->GetResource(nsDependentCString(uri),
						    getter_AddRefs(category))))
	    return(rv);

	return categoryDataSource->HasArcOut(source, aArc, result);
    }

    if (isSearchCategoryEngineURI(source))
    {
	nsCOMPtr<nsIRDFResource>	trueEngine;
	rv = resolveSearchCategoryEngineURI(source, getter_AddRefs(trueEngine));
	if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE))	return(rv);
	if (!trueEngine) {
	    *result = PR_FALSE;
	    return NS_OK;
	}
	source = trueEngine;
    }

    if (isEngineURI(source))
    {
	// if we're asking for info on a search engine, (deferred) load it if needed
	nsCOMPtr<nsIRDFLiteral>	dataLit;
	FindData(source, getter_AddRefs(dataLit));
    }

    if (mInner)
    {
	return mInner->HasArcOut(source, aArc, result);
    }

    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
InternetSearchDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsISimpleEnumerator ** labels /* out */)
{
	nsresult	rv;

	if (mInner)
	{
		rv = mInner->ArcLabelsIn(node, labels);
		return(rv);
	}
	else
	{
		rv = NS_NewEmptyEnumerator(labels);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::ArcLabelsOut(nsIRDFResource *source,
                             nsISimpleEnumerator **labels /* out */)
{
	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(labels != nsnull, "null ptr");
	if (! labels)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	if ((source == kNC_SearchEngineRoot) || (source == kNC_LastSearchRoot) || isSearchURI(source))
	{
            nsCOMPtr<nsISupportsArray> array;
            rv = NS_NewISupportsArray(getter_AddRefs(array));
            if (NS_FAILED(rv)) return rv;

            array->AppendElement(kNC_Child);

            nsISimpleEnumerator* result = new nsArrayEnumerator(array);
            if (! result)
                return NS_ERROR_OUT_OF_MEMORY;

            NS_ADDREF(result);
            *labels = result;
            return(NS_OK);
	}

	if ((isSearchCategoryURI(source)) && (categoryDataSource))
	{
		const char	*uri = nsnull;
		source->GetValueConst(&uri);
		if (!uri)	return(NS_ERROR_UNEXPECTED);

		nsCOMPtr<nsIRDFResource>	category;
		if (NS_FAILED(rv = gRDFService->GetResource(nsDependentCString(uri),
			getter_AddRefs(category))))
			return(rv);

		rv = categoryDataSource->ArcLabelsOut(category, labels);
		return(rv);
	}

	if (isSearchCategoryEngineURI(source))
	{
		nsCOMPtr<nsIRDFResource>	trueEngine;
		rv = resolveSearchCategoryEngineURI(source, getter_AddRefs(trueEngine));
		if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE))	return(rv);
		if (!trueEngine)	return(NS_RDF_NO_VALUE);

		source = trueEngine;
	}

	if (isEngineURI(source))
	{
		// if we're asking for info on a search engine, (deferred) load it if needed
		nsCOMPtr<nsIRDFLiteral>	dataLit;
		FindData(source, getter_AddRefs(dataLit));
	}

	if (mInner)
	{
		rv = mInner->ArcLabelsOut(source, labels);
		return(rv);
	}

	return NS_NewEmptyEnumerator(labels);
}



NS_IMETHODIMP
InternetSearchDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
	nsresult	rv = NS_RDF_NO_VALUE;

	if (mInner)
	{
		rv = mInner->GetAllResources(aCursor);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::AddObserver(nsIRDFObserver *aObserver)
{
	nsresult	rv = NS_OK;

	if (mInner)
	{
		rv = mInner->AddObserver(aObserver);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
	nsresult	rv = NS_OK;

	if (mInner)
	{
		rv = mInner->RemoveObserver(aObserver);
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::GetAllCmds(nsIRDFResource* source,
                                     nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
	nsCOMPtr<nsISupportsArray>	cmdArray;
	nsresult			rv;
	rv = NS_NewISupportsArray(getter_AddRefs(cmdArray));
	if (NS_FAILED(rv))	return(rv);

	// check if we have any filters, enable command to clear them
	PRBool				haveFilters = PR_FALSE;

	if (mLocalstore)
	{
		nsCOMPtr<nsISimpleEnumerator>	cursor;
		PRBool				hasMore = PR_FALSE;

		// check kNC_FilterSearchURLsRoot
		if (NS_SUCCEEDED(rv = mLocalstore->GetTargets(kNC_FilterSearchURLsRoot, kNC_Child,
			PR_TRUE, getter_AddRefs(cursor))))
		{
      if (NS_SUCCEEDED(cursor->HasMoreElements(&hasMore)) && hasMore)
			{
				haveFilters = PR_TRUE;
			}
		}
    if (!haveFilters)
		{
			// check kNC_FilterSearchSitesRoot
			if (NS_SUCCEEDED(rv = mLocalstore->GetTargets(kNC_FilterSearchSitesRoot, kNC_Child,
				PR_TRUE, getter_AddRefs(cursor))))
			{
        if (NS_SUCCEEDED(cursor->HasMoreElements(&hasMore)) && hasMore)
				{
					haveFilters = PR_TRUE;
				}
			}
		}
	}

	PRBool				isSearchResult = PR_FALSE;
  rv = mInner->HasAssertion(source, kRDF_type, kNC_SearchResult, PR_TRUE,
          &isSearchResult);
  if (NS_SUCCEEDED(rv) && isSearchResult)
	{
		nsCOMPtr<nsIRDFDataSource>	datasource;
		if (NS_SUCCEEDED(rv = gRDFService->GetDataSource("rdf:bookmarks", getter_AddRefs(datasource))))
		{
			nsCOMPtr<nsIBookmarksService> bookmarks (do_QueryInterface(datasource));
			if (bookmarks)
			{
				char *uri = getSearchURI(source);
				if (uri)
				{
					PRBool	isBookmarkedFlag = PR_FALSE;
          rv = bookmarks->IsBookmarked(uri, &isBookmarkedFlag);
          if (NS_SUCCEEDED(rv) && !isBookmarkedFlag)
					{
						cmdArray->AppendElement(kNC_SearchCommand_AddToBookmarks);
					}
					Recycle(uri);
				}
			}
		}
		cmdArray->AppendElement(kNC_SearchCommand_AddQueryToBookmarks);
		cmdArray->AppendElement(kNC_BookmarkSeparator);

		// if this is a search result, and it isn't filtered, enable command to be able to filter it
		PRBool				isURLFiltered = PR_FALSE;
    rv = mInner->HasAssertion(kNC_FilterSearchURLsRoot, kNC_Child, source,
                              PR_TRUE, &isURLFiltered);
    if (NS_SUCCEEDED(rv) && !isURLFiltered)
		{
			cmdArray->AppendElement(kNC_SearchCommand_FilterResult);
		}

		// XXX (should check site) if this is a search result site, and the site isn't filtered,
		//     enable command to filter it
		cmdArray->AppendElement(kNC_SearchCommand_FilterSite);

    if (haveFilters)
		{
			cmdArray->AppendElement(kNC_BookmarkSeparator);
			cmdArray->AppendElement(kNC_SearchCommand_ClearFilters);
		}
	}
	else if (isSearchURI(source) || (source == kNC_LastSearchRoot))
	{
    if (haveFilters)
		{
			cmdArray->AppendElement(kNC_SearchCommand_ClearFilters);
		}
	}

	// always append a separator last (due to aggregation of commands from multiple datasources)
	cmdArray->AppendElement(kNC_BookmarkSeparator);

	nsISimpleEnumerator		*result = new nsArrayEnumerator(cmdArray);
	if (!result)
		return(NS_ERROR_OUT_OF_MEMORY);
	NS_ADDREF(result);
	*commands = result;
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                       nsIRDFResource*   aCommand,
                                       nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                       PRBool* aResult)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}



char *
InternetSearchDataSource::getSearchURI(nsIRDFResource *src)
{
	char	*uri = nsnull;

	if (src)
	{
		nsresult		rv;
		nsCOMPtr<nsIRDFNode>	srcNode;
		if (NS_SUCCEEDED(rv = mInner->GetTarget(src, kNC_URL, PR_TRUE, getter_AddRefs(srcNode))))
		{
			nsCOMPtr<nsIRDFLiteral>	urlLiteral (do_QueryInterface(srcNode));
			if (urlLiteral)
			{
				const PRUnichar	*uriUni = nsnull;
				urlLiteral->GetValueConst(&uriUni);
				if (uriUni)
				{
					nsAutoString	uriString(uriUni);
					uri = ToNewUTF8String(uriString);
				}
			}
		}
	}
	return(uri);
}



nsresult
InternetSearchDataSource::addToBookmarks(nsIRDFResource *src)
{
	if (!src)	return(NS_ERROR_UNEXPECTED);
	if (!mInner)	return(NS_ERROR_UNEXPECTED);

	nsresult		rv;
	nsCOMPtr<nsIRDFNode>	nameNode;
	// Note: nameLiteral needs to stay in scope for the life of "name"
	nsCOMPtr<nsIRDFLiteral>	nameLiteral;
	const PRUnichar		*name = nsnull;
	if (NS_SUCCEEDED(rv = mInner->GetTarget(src, kNC_Name, PR_TRUE, getter_AddRefs(nameNode))))
	{
		nameLiteral = do_QueryInterface(nameNode);
		if (nameLiteral)
		{
			nameLiteral->GetValueConst(&name);
		}
	}

	nsCOMPtr<nsIRDFDataSource>	datasource;
	if (NS_SUCCEEDED(rv = gRDFService->GetDataSource("rdf:bookmarks", getter_AddRefs(datasource))))
	{
		nsCOMPtr<nsIBookmarksService> bookmarks (do_QueryInterface(datasource));
		if (bookmarks)
		{
			char	*uri = getSearchURI(src);
			if (uri)
			{
				rv = bookmarks->AddBookmarkImmediately(NS_ConvertUTF8toUTF16(uri).get(),
                                               name, nsIBookmarksService::BOOKMARK_SEARCH_TYPE, nsnull);
				Recycle(uri);
			}
		}
	}

	return(NS_OK);
}



nsresult
InternetSearchDataSource::addQueryToBookmarks(nsIRDFResource *src)
{
	if (!src)	return(NS_ERROR_UNEXPECTED);
	if (!mInner)	return(NS_ERROR_UNEXPECTED);

	nsresult rv;
	nsCOMPtr<nsIRDFNode>	refNode;
	if (NS_FAILED(rv = mInner->GetTarget(kNC_LastSearchRoot, kNC_Ref, PR_TRUE,
		getter_AddRefs(refNode))))
		return(rv);
	nsCOMPtr<nsIRDFLiteral>	urlLiteral (do_QueryInterface(refNode));
	if (!urlLiteral)
	  return(NS_ERROR_UNEXPECTED);
	const PRUnichar	*uriUni = nsnull;
	urlLiteral->GetValueConst(&uriUni);

	nsCOMPtr<nsIRDFNode>	textNode;
	if (NS_FAILED(rv = mInner->GetTarget(kNC_LastSearchRoot, kNC_LastText, PR_TRUE,
		getter_AddRefs(textNode))))
		return(rv);
	nsCOMPtr<nsIRDFLiteral> textLiteral = do_QueryInterface(textNode);
	nsXPIDLString value;
	if (textLiteral)
	{
		const PRUnichar *textUni = nsnull;
		textLiteral->GetValueConst(&textUni);
  	nsAutoString name;
		name.Assign(textUni);
		// replace pluses with spaces
		name.ReplaceChar(PRUnichar('+'), PRUnichar(' '));

  	nsCOMPtr<nsIStringBundleService>
  	stringService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
  	if (NS_SUCCEEDED(rv) && stringService)
  	{
  		nsCOMPtr<nsIStringBundle> bundle;
  		rv = stringService->CreateBundle(SEARCH_PROPERTIES, getter_AddRefs(bundle));
  		if (bundle)
  		{
  			const PRUnichar *strings[] = { name.get() };
  			rv = bundle->FormatStringFromName(NS_LITERAL_STRING("searchTitle").get(), strings, 1, 
  				getter_Copies(value));
			}
		}
	}

	nsCOMPtr<nsIRDFDataSource>	datasource;
	if (NS_SUCCEEDED(rv = gRDFService->GetDataSource("rdf:bookmarks", getter_AddRefs(datasource))))
	{
		nsCOMPtr<nsIBookmarksService> bookmarks (do_QueryInterface(datasource));
		if (bookmarks)
			rv = bookmarks->AddBookmarkImmediately(uriUni, value.get(),
                                             nsIBookmarksService::BOOKMARK_SEARCH_TYPE, nsnull);
	}

	return(NS_OK);
}



nsresult
InternetSearchDataSource::filterResult(nsIRDFResource *aResource)
{
	if (!aResource)	return(NS_ERROR_UNEXPECTED);
	if (!mInner)	return(NS_ERROR_UNEXPECTED);

	// remove all anonymous resources which have this as a #URL
	char	*uri = getSearchURI(aResource);
	if (!uri)	return(NS_ERROR_UNEXPECTED);
	nsAutoString	url;
	url.AssignWithConversion(uri);
	Recycle(uri);

	nsresult			rv;
	nsCOMPtr<nsIRDFLiteral>	urlLiteral;
	if (NS_FAILED(rv = gRDFService->GetLiteral(url.get(), getter_AddRefs(urlLiteral)))
		|| (urlLiteral == nsnull))	return(NS_ERROR_UNEXPECTED);

	// add aResource into a list of nodes to filter out
	PRBool	alreadyFiltered = PR_FALSE;
  rv = mLocalstore->HasAssertion(kNC_FilterSearchURLsRoot, kNC_Child,
         urlLiteral, PR_TRUE, &alreadyFiltered);
  if (NS_SUCCEEDED(rv) && alreadyFiltered)
	{
		// already filtered, nothing to do
		return(rv);
	}

	// filter this URL out
	mLocalstore->Assert(kNC_FilterSearchURLsRoot, kNC_Child, urlLiteral, PR_TRUE);

	// flush localstore
	nsCOMPtr<nsIRDFRemoteDataSource> remoteLocalStore (do_QueryInterface(mLocalstore));
	if (remoteLocalStore)
	{
		remoteLocalStore->Flush();
	}

	nsCOMPtr<nsISimpleEnumerator>	anonArcs;
	if (NS_SUCCEEDED(rv = mInner->GetSources(kNC_URL, urlLiteral, PR_TRUE,
		getter_AddRefs(anonArcs))))
	{
		PRBool			hasMoreAnonArcs = PR_TRUE;
    while (hasMoreAnonArcs)
		{
      if (NS_FAILED(anonArcs->HasMoreElements(&hasMoreAnonArcs)) ||
          !hasMoreAnonArcs)
        break;
			nsCOMPtr<nsISupports>	anonArc;
			if (NS_FAILED(anonArcs->GetNext(getter_AddRefs(anonArc))))
				break;
			nsCOMPtr<nsIRDFResource> anonChild (do_QueryInterface(anonArc));
			if (!anonChild)	continue;

			PRBool	isSearchResult = PR_FALSE;
      rv = mInner->HasAssertion(anonChild, kRDF_type, kNC_SearchResult,
                                PR_TRUE, &isSearchResult);
      if (NS_FAILED(rv) || !isSearchResult)
				continue;

			nsCOMPtr<nsIRDFResource>	anonParent;
			if (NS_FAILED(rv = mInner->GetSource(kNC_Child, anonChild, PR_TRUE,
				getter_AddRefs(anonParent))))	continue;
			if (!anonParent)	continue;

			mInner->Unassert(anonParent, kNC_Child, anonChild);
		}
	}

	return(NS_OK);
}



nsresult
InternetSearchDataSource::filterSite(nsIRDFResource *aResource)
{
	if (!aResource)	return(NS_ERROR_UNEXPECTED);
	if (!mInner)	return(NS_ERROR_UNEXPECTED);

	char	*uri = getSearchURI(aResource);
	if (!uri)	return(NS_ERROR_UNEXPECTED);
	nsAutoString	host;
	host.AssignWithConversion(uri);
	Recycle(uri);

	// determine site (host name)
	PRInt32		slashOffset1 = host.Find("://");
	if (slashOffset1 < 1)			return(NS_ERROR_UNEXPECTED);
	PRInt32 	slashOffset2 = host.FindChar(PRUnichar('/'), slashOffset1 + 3);
	if (slashOffset2 <= slashOffset1)	return(NS_ERROR_UNEXPECTED);
	host.Truncate(slashOffset2 + 1);

	nsresult			rv;
	nsCOMPtr<nsIRDFLiteral>	urlLiteral;
	if (NS_FAILED(rv = gRDFService->GetLiteral(host.get(), getter_AddRefs(urlLiteral)))
		|| (urlLiteral == nsnull))	return(NS_ERROR_UNEXPECTED);

	// add aResource into a list of nodes to filter out
	PRBool	alreadyFiltered = PR_FALSE;
  rv = mLocalstore->HasAssertion(kNC_FilterSearchSitesRoot, kNC_Child,
         urlLiteral, PR_TRUE, &alreadyFiltered);
  if (NS_SUCCEEDED(rv) && alreadyFiltered)
	{
		// already filtered, nothing to do
		return(rv);
	}

	// filter this site out
	mLocalstore->Assert(kNC_FilterSearchSitesRoot, kNC_Child, urlLiteral, PR_TRUE);

	// flush localstore
	nsCOMPtr<nsIRDFRemoteDataSource> remoteLocalStore (do_QueryInterface(mLocalstore));
	if (remoteLocalStore)
	{
		remoteLocalStore->Flush();
	}

	// remove all anonymous resources which have this as a site

	nsCOMPtr<nsISupportsArray>	array;
	nsCOMPtr<nsIRDFResource>	aRes;
	nsCOMPtr<nsISimpleEnumerator>	cursor;
	PRBool				hasMore;

	rv = NS_NewISupportsArray(getter_AddRefs(array));
	if (NS_FAILED(rv)) return rv;

	if (NS_FAILED(rv = GetAllResources(getter_AddRefs(cursor))))	return(rv);
	if (!cursor)	return(NS_ERROR_UNEXPECTED);

	hasMore = PR_TRUE;
  while (hasMore)
	{
		if (NS_FAILED(rv = cursor->HasMoreElements(&hasMore)))	return(rv);
    if (!hasMore)
      break;
		
		nsCOMPtr<nsISupports>	isupports;
		if (NS_FAILED(rv = cursor->GetNext(getter_AddRefs(isupports))))
				return(rv);
		if (!isupports)	return(NS_ERROR_UNEXPECTED);
		aRes = do_QueryInterface(isupports);
		if (!aRes)	return(NS_ERROR_UNEXPECTED);

		if ((aRes.get() == kNC_LastSearchRoot) || (isSearchURI(aRes)))
		{
			array->AppendElement(aRes);
		}
	}

	PRUint32	count;
	if (NS_FAILED(rv = array->Count(&count)))	return(rv);
	for (PRUint32 loop=0; loop<count; loop++)
	{
		nsCOMPtr<nsISupports>	element = array->ElementAt(loop);
		if (!element)	break;
		nsCOMPtr<nsIRDFResource> aSearchRoot (do_QueryInterface(element));
		if (!aSearchRoot)	break;

		if (NS_SUCCEEDED(rv = mInner->GetTargets(aSearchRoot, kNC_Child,
			PR_TRUE, getter_AddRefs(cursor))))
		{
			hasMore = PR_TRUE;
      while (hasMore)
			{
        if (NS_FAILED(cursor->HasMoreElements(&hasMore)) || !hasMore)
          break;

				nsCOMPtr<nsISupports>		arc;
				if (NS_FAILED(cursor->GetNext(getter_AddRefs(arc))))
					break;
				aRes = do_QueryInterface(arc);
				if (!aRes)	break;

				uri = getSearchURI(aRes);
				if (!uri)	return(NS_ERROR_UNEXPECTED);
				nsAutoString	site;
				site.AssignWithConversion(uri);
				Recycle(uri);

				// determine site (host name)
				slashOffset1 = site.Find("://");
				if (slashOffset1 < 1)			return(NS_ERROR_UNEXPECTED);
				slashOffset2 = site.FindChar(PRUnichar('/'), slashOffset1 + 3);
				if (slashOffset2 <= slashOffset1)	return(NS_ERROR_UNEXPECTED);
				site.Truncate(slashOffset2 + 1);

				if (site.Equals(host, nsCaseInsensitiveStringComparator()))
				{
					mInner->Unassert(aSearchRoot, kNC_Child, aRes);
				}
			}
		}
	}

	return(NS_OK);
}



nsresult
InternetSearchDataSource::clearFilters(void)
{
	if (!mInner)	return(NS_ERROR_UNEXPECTED);

	nsresult			rv;
	nsCOMPtr<nsISimpleEnumerator>	arcs;
	PRBool				hasMore = PR_TRUE;
	nsCOMPtr<nsISupports>		arc;

	// remove all filtered URLs
	if (NS_SUCCEEDED(rv = mLocalstore->GetTargets(kNC_FilterSearchURLsRoot, kNC_Child,
		PR_TRUE, getter_AddRefs(arcs))))
	{
		hasMore = PR_TRUE;
    while (hasMore)
		{
      if (NS_FAILED(arcs->HasMoreElements(&hasMore)) || !hasMore)
				break;
			if (NS_FAILED(arcs->GetNext(getter_AddRefs(arc))))
				break;

			nsCOMPtr<nsIRDFLiteral>	filterURL (do_QueryInterface(arc));
			if (!filterURL)	continue;
			
			mLocalstore->Unassert(kNC_FilterSearchURLsRoot, kNC_Child, filterURL);
		}
	}

	// remove all filtered sites
	if (NS_SUCCEEDED(rv = mLocalstore->GetTargets(kNC_FilterSearchSitesRoot, kNC_Child,
		PR_TRUE, getter_AddRefs(arcs))))
	{
		hasMore = PR_TRUE;
    while (hasMore)
		{
      if (NS_FAILED(arcs->HasMoreElements(&hasMore)) || !hasMore)
				break;
			if (NS_FAILED(arcs->GetNext(getter_AddRefs(arc))))
				break;

			nsCOMPtr<nsIRDFLiteral>	filterSiteLiteral (do_QueryInterface(arc));
			if (!filterSiteLiteral)	continue;
			
			mLocalstore->Unassert(kNC_FilterSearchSitesRoot, kNC_Child, filterSiteLiteral);
		}
	}

	// flush localstore
	nsCOMPtr<nsIRDFRemoteDataSource> remoteLocalStore (do_QueryInterface(mLocalstore));
	if (remoteLocalStore)
	{
		remoteLocalStore->Flush();
	}

	return(NS_OK);
}



PRBool
InternetSearchDataSource::isSearchResultFiltered(const nsString &hrefStr)
{
	PRBool		filterStatus = PR_FALSE;
	nsresult	rv;

	const PRUnichar	*hrefUni = hrefStr.get();
	if (!hrefUni)	return(filterStatus);

	// check if this specific URL is to be filtered out
	nsCOMPtr<nsIRDFLiteral>	hrefLiteral;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(hrefUni, getter_AddRefs(hrefLiteral))))
	{
		if (NS_SUCCEEDED(rv = mLocalstore->HasAssertion(kNC_FilterSearchURLsRoot,
			kNC_Child, hrefLiteral, PR_TRUE, &filterStatus)))
		{
      if (filterStatus)
			{
				return(filterStatus);
			}
		}
	}

	// check if this specific Site is to be filtered out

	// determine site (host name)
	nsAutoString	host(hrefStr);
	PRInt32		slashOffset1 = host.Find("://");
	if (slashOffset1 < 1)			return(NS_ERROR_UNEXPECTED);
	PRInt32 	slashOffset2 = host.FindChar(PRUnichar('/'), slashOffset1 + 3);
	if (slashOffset2 <= slashOffset1)	return(NS_ERROR_UNEXPECTED);
	host.Truncate(slashOffset2 + 1);

	nsCOMPtr<nsIRDFLiteral>	urlLiteral;
	if (NS_FAILED(rv = gRDFService->GetLiteral(host.get(), getter_AddRefs(urlLiteral)))
		|| (urlLiteral == nsnull))	return(NS_ERROR_UNEXPECTED);

	rv = mLocalstore->HasAssertion(kNC_FilterSearchSitesRoot, kNC_Child, urlLiteral,
		PR_TRUE, &filterStatus);

	return(filterStatus);
}



NS_IMETHODIMP
InternetSearchDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	nsresult		rv = NS_OK;
	PRInt32			loop;
	PRUint32		numSources;
	if (NS_FAILED(rv = aSources->Count(&numSources)))	return(rv);
	if (numSources < 1)
	{
		return(NS_ERROR_ILLEGAL_VALUE);
	}

	for (loop=((PRInt32)numSources)-1; loop>=0; loop--)
	{
		nsCOMPtr<nsISupports>	aSource = aSources->ElementAt(loop);
		if (!aSource)	return(NS_ERROR_NULL_POINTER);
		nsCOMPtr<nsIRDFResource> src (do_QueryInterface(aSource));
		if (!src)	return(NS_ERROR_NO_INTERFACE);

		if (aCommand == kNC_SearchCommand_AddToBookmarks)
		{
			if (NS_FAILED(rv = addToBookmarks(src)))
				return(rv);
		}
		else if (aCommand == kNC_SearchCommand_AddQueryToBookmarks)
		{
		  if (NS_FAILED(rv = addQueryToBookmarks(src)))
		    return(rv);
		}
		else if (aCommand == kNC_SearchCommand_FilterResult)
		{
			if (NS_FAILED(rv = filterResult(src)))
				return(rv);
		}
		else if (aCommand == kNC_SearchCommand_FilterSite)
		{
			if (NS_FAILED(rv = filterSite(src)))
				return(rv);
		}
		else if (aCommand == kNC_SearchCommand_ClearFilters)
		{
			if (NS_FAILED(rv = clearFilters()))
				return(rv);
		}
	}
	return(NS_OK);
}

NS_IMETHODIMP
InternetSearchDataSource::BeginUpdateBatch()
{
        return mInner->BeginUpdateBatch();
}

NS_IMETHODIMP
InternetSearchDataSource::EndUpdateBatch()
{
        return mInner->EndUpdateBatch();
}

NS_IMETHODIMP
InternetSearchDataSource::AddSearchEngine(const char *engineURL, const char *iconURL,
					  const PRUnichar *suggestedTitle, const PRUnichar *suggestedCategory)
{
	NS_PRECONDITION(engineURL != nsnull, "null ptr");
	if (!engineURL)	return(NS_ERROR_NULL_POINTER);
	// Note: iconURL, suggestedTitle & suggestedCategory
	// can be null or empty strings, which is OK

#ifdef	DEBUG_SEARCH_OUTPUT
	printf("AddSearchEngine: engine='%s'\n", engineURL);
#endif

	nsresult	rv = NS_OK;

	// mBackgroundLoadGroup is a dynamically created singleton
	if (!mBackgroundLoadGroup)
	{
		if (NS_FAILED(rv = NS_NewLoadGroup(getter_AddRefs(mBackgroundLoadGroup), nsnull)))
			return(rv);
		if (!mBackgroundLoadGroup)
			return(NS_ERROR_UNEXPECTED);
	}

	// download engine
	nsCOMPtr<nsIInternetSearchContext>	engineContext;
	if (NS_FAILED(rv = NS_NewInternetSearchContext(nsIInternetSearchContext::ENGINE_DOWNLOAD_CONTEXT,
		nsnull, nsnull, nsnull, suggestedCategory, getter_AddRefs(engineContext))))
		return(rv);
	if (!engineContext)	return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIURI>	engineURI;
	if (NS_FAILED(rv = NS_NewURI(getter_AddRefs(engineURI), engineURL)))
		return(rv);

	nsCOMPtr<nsIChannel>	engineChannel;
	if (NS_FAILED(rv = NS_NewChannel(getter_AddRefs(engineChannel), engineURI, nsnull, mBackgroundLoadGroup)))
		return(rv);
    
	if (NS_FAILED(rv = engineChannel->AsyncOpen(this, engineContext)))
		return(rv);

	// download icon
	nsCOMPtr<nsIInternetSearchContext>	iconContext;
	if (NS_FAILED(rv = NS_NewInternetSearchContext(nsIInternetSearchContext::ICON_DOWNLOAD_CONTEXT,
		nsnull, nsnull, nsnull, nsnull, getter_AddRefs(iconContext))))
		return(rv);
	if (!iconContext)	return(NS_ERROR_UNEXPECTED);

	if (iconURL && (*iconURL))
	{
		nsCOMPtr<nsIURI>	iconURI;
		if (NS_FAILED(rv = NS_NewURI(getter_AddRefs(iconURI), iconURL)))
			return(rv);

		nsCOMPtr<nsIChannel>	iconChannel;
		if (NS_FAILED(rv = NS_NewChannel(getter_AddRefs(iconChannel), iconURI, nsnull, mBackgroundLoadGroup)))
			return(rv);
		if (NS_FAILED(rv = iconChannel->AsyncOpen(this, iconContext)))
			return(rv);
	}
	return(NS_OK);
}



nsresult
InternetSearchDataSource::saveContents(nsIChannel* channel, nsIInternetSearchContext *context, PRUint32	contextType)
{
	nsresult	rv = NS_OK;

	if (!channel)	return(NS_ERROR_UNEXPECTED);
	if (!context)	return(NS_ERROR_UNEXPECTED);

	// get real URI
	nsCOMPtr<nsIURI>	channelURI;
	if (NS_FAILED(rv = channel->GetURI(getter_AddRefs(channelURI))))
		return(rv);
	if (!channelURI)
		return(NS_ERROR_NULL_POINTER);

	nsCAutoString baseName;
	if (NS_FAILED(rv = channelURI->GetSpec(baseName)))
		return(rv);

	PRInt32			slashOffset = baseName.RFindChar(PRUnichar('/'));
	if (slashOffset < 0)		return(NS_ERROR_UNEXPECTED);
	baseName.Cut(0, slashOffset+1);
	if (baseName.IsEmpty())	return(NS_ERROR_UNEXPECTED);

	// make sure that search engines are .src files
	PRInt32	extensionOffset;
	if (contextType == nsIInternetSearchContext::ENGINE_DOWNLOAD_CONTEXT ||
		contextType == nsIInternetSearchContext::ENGINE_UPDATE_CONTEXT)
	{
		extensionOffset = baseName.RFind(".src", PR_TRUE);
		if ((extensionOffset < 0) || (extensionOffset != (PRInt32)(baseName.Length()-4)))
		{
			return(NS_ERROR_UNEXPECTED);
		}
	}

	nsCOMPtr<nsIFile>	outFile;
	if (NS_FAILED(rv = GetSearchFolder(getter_AddRefs(outFile))))		return(rv);

	const PRUnichar	*dataBuf = nsnull;
	if (NS_FAILED(rv = context->GetBufferConst(&dataBuf)))	return(rv);

	// if no data, then nothing to do
	// Note: do this before opening file, as it would be truncated
	PRInt32		bufferLength = 0;
	if (NS_FAILED(context->GetBufferLength(&bufferLength)))	return(rv);
	if (bufferLength < 1)	return(NS_OK);
	
	rv = outFile->Append(NS_ConvertUTF8toUCS2(baseName));
	if (NS_FAILED(rv)) return rv;
	
	// save data to file
	// Note: write out one character at a time, as we might be dealing
	//       with binary data (such as 0x00) [especially for images]
    //
    // XXX - It appears that this is done in order to discard the upper
    // byte of each PRUnichar.  I hope that's OK!!
    //
	outFile->Remove(PR_FALSE);

    nsCOMPtr<nsIOutputStream> outputStream, fileOutputStream;
    rv = NS_NewLocalFileOutputStream(getter_AddRefs(fileOutputStream), outFile);
    if (NS_FAILED(rv)) return rv;
    rv = NS_NewBufferedOutputStream(getter_AddRefs(outputStream), fileOutputStream, 4096);
    if (NS_FAILED(rv)) return rv;

    PRUint32 bytesWritten;
    for (PRInt32 loop=0; loop < bufferLength; loop++)
    {
        const char b = (const char) dataBuf[loop];
        outputStream->Write(&b, 1, &bytesWritten);
    }
    outputStream->Flush();		
    outputStream->Close();

    if (contextType == nsIInternetSearchContext::ENGINE_DOWNLOAD_CONTEXT ||
        contextType == nsIInternetSearchContext::ENGINE_UPDATE_CONTEXT)
    {
#ifdef	XP_MAC
        // set appropriate Mac file type/creator for search engine files
        nsCOMPtr<nsILocalFileMac> macFile(do_QueryInterface(outFile));
        if (macFile) {
          macFile->SetFileType('issp');
          macFile->SetFileCreator('fndf');
        }
#endif

        // check suggested category hint
        const PRUnichar	*hintUni = nsnull;
        rv = context->GetHintConst(&hintUni);

        // update graph with various required info
        SaveEngineInfoIntoGraph(outFile, nsnull, hintUni, dataBuf, PR_FALSE, PR_FALSE);
    }
    else if (contextType == nsIInternetSearchContext::ICON_DOWNLOAD_CONTEXT)
    {
        // update graph with icon info
        SaveEngineInfoIntoGraph(nsnull, outFile, nsnull, nsnull, PR_FALSE, PR_FALSE);
    }

	// after we're all done with the data buffer, get rid of it
	context->Truncate();

	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::GetInternetSearchURL(const char *searchEngineURI,
  const PRUnichar *searchStr, PRInt16 direction, PRUint16 pageNumber, 
  PRUint16 *whichButtons, char **resultURL)
{
	if (!resultURL)	return(NS_ERROR_NULL_POINTER);
	*resultURL = nsnull;

	// if we haven't already, load in the engines
  if (!gEngineListBuilt)
    DeferredInit();

	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	engine;
	if (NS_FAILED(rv = gRDFService->GetResource(nsDependentCString(searchEngineURI), getter_AddRefs(engine))))
		return(rv);
	if (!engine)	return(NS_ERROR_UNEXPECTED);

	// if its a engine from a search category, then get its "#Name",
	// and try to map from that back to the real engine reference
	if (isSearchCategoryEngineURI(engine))
	{
		nsCOMPtr<nsIRDFResource>	trueEngine;
		rv = resolveSearchCategoryEngineURI(engine, getter_AddRefs(trueEngine));
		if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE))	return(rv);
		if (!trueEngine)	return(NS_RDF_NO_VALUE);

		engine = trueEngine;
	}

	nsCOMPtr<nsIRDFLiteral>	dataLit;
	rv = FindData(engine, getter_AddRefs(dataLit));
	if (NS_FAILED(rv) ||
	    (rv == NS_RDF_NO_VALUE))	return(rv);
	if (!dataLit)	return(NS_ERROR_UNEXPECTED);

	const PRUnichar	*dataUni = nsnull;
	dataLit->GetValueConst(&dataUni);
	if (!dataUni)	return(NS_RDF_NO_VALUE);

	nsAutoString	 text(searchStr), encodingStr, queryEncodingStr;

	// first look for "search/queryCharset"... if that isn't specified,
	// then fall back to looking for "search/queryEncoding" (a decimal)
	GetData(dataUni, "search", 0, "queryCharset", queryEncodingStr);
       if (queryEncodingStr.IsEmpty())
	{
		GetData(dataUni, "search", 0, "queryEncoding", encodingStr);		// decimal string values
		MapEncoding(encodingStr, queryEncodingStr);
	}

	if (!queryEncodingStr.IsEmpty())
	{
 		// remember query charset string
 		mQueryEncodingStr = queryEncodingStr;
		// convert from escaped-UTF_8, to unicode, and then to
		// the charset indicated by the dataset in question

		char	*utf8data = ToNewUTF8String(text);
		if (utf8data)
		{
			nsCOMPtr<nsITextToSubURI> textToSubURI = 
			         do_GetService(kTextToSubURICID, &rv);
			if (NS_SUCCEEDED(rv) && (textToSubURI))
			{
				PRUnichar	*uni = nsnull;
				if (NS_SUCCEEDED(rv = textToSubURI->UnEscapeAndConvert("UTF-8", utf8data, &uni)) && (uni))
				{
					char		*charsetData = nsnull;
					nsCAutoString	queryencodingstrC;
					queryencodingstrC.AssignWithConversion(queryEncodingStr);
					if (NS_SUCCEEDED(rv = textToSubURI->ConvertAndEscape(queryencodingstrC.get(), uni, &charsetData)) && (charsetData))
					{
						text.AssignWithConversion(charsetData);
						Recycle(charsetData);
					}
					Recycle(uni);
				}
			}
			Recycle(utf8data);
		}
	}
	
	nsAutoString	action, input, method, userVar, name;
	if (NS_FAILED(rv = GetData(dataUni, "search", 0, "action", action)))
	    return(rv);
	if (NS_FAILED(rv = GetData(dataUni, "search", 0, "method", method)))
	    return(rv);
	if (NS_FAILED(rv = GetData(dataUni, "search", 0, "name", name)))
	    return(rv);
	if (NS_FAILED(rv = GetInputs(dataUni, name, userVar, text, input, direction, pageNumber, whichButtons)))
	    return(rv);
	if (input.IsEmpty())				return(NS_ERROR_UNEXPECTED);

	// we can only handle HTTP GET
	if (!method.LowerCaseEqualsLiteral("get"))	return(NS_ERROR_UNEXPECTED);
	// HTTP Get method support
	action += NS_LITERAL_STRING("?") + input;

	// return a copy of the resulting search URL
	*resultURL = ToNewCString(action);
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchDataSource::RememberLastSearchText(const PRUnichar *escapedSearchStr)
{
	nsresult		rv;

	nsCOMPtr<nsIRDFNode>	textNode;
	if (NS_SUCCEEDED(rv = mInner->GetTarget(kNC_LastSearchRoot, kNC_LastText, PR_TRUE,
		getter_AddRefs(textNode))))
	{
		if (escapedSearchStr != nsnull)
		{
			nsresult		temprv;
			nsCOMPtr<nsIRDFLiteral>	textLiteral;
			if (NS_SUCCEEDED(temprv = gRDFService->GetLiteral(escapedSearchStr, getter_AddRefs(textLiteral))))
			{
				if (rv != NS_RDF_NO_VALUE)
				{
					mInner->Change(kNC_LastSearchRoot, kNC_LastText, textNode, textLiteral);
				}
				else
				{
					mInner->Assert(kNC_LastSearchRoot, kNC_LastText, textLiteral, PR_TRUE);
				}
			}
		}
		else if (rv != NS_RDF_NO_VALUE)
		{
			rv = mInner->Unassert(kNC_LastSearchRoot, kNC_LastText, textNode);
		}
	}
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::FindInternetSearchResults(const char *url, PRBool *searchInProgress)
{
	*searchInProgress = PR_FALSE;

	if (!mInner)	return(NS_OK);

	// if the url doesn't look like a HTTP GET query, just return,
	// otherwise strip off the query data
	nsAutoString		shortURL;
	shortURL.AssignWithConversion(url);
	PRInt32			optionsOffset;
	if ((optionsOffset = shortURL.FindChar(PRUnichar('?'))) < 0)	return(NS_OK);
	shortURL.Truncate(optionsOffset);

	// if we haven't already, load in the engines
  if (!gEngineListBuilt)
    DeferredInit();

	// look in available engines to see if any of them appear
	// to match this url (minus the GET options)
	PRBool				foundEngine = PR_FALSE;
	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	engine;
	nsCOMPtr<nsISimpleEnumerator>	arcs;
	nsAutoString			engineURI;
	nsCOMPtr<nsIRDFLiteral>		dataLit;
	const PRUnichar			*dataUni = nsnull;

	if (NS_SUCCEEDED(rv = mInner->GetTargets(kNC_SearchEngineRoot, kNC_Child,
		PR_TRUE, getter_AddRefs(arcs))))
	{
		PRBool			hasMore = PR_TRUE;
    while (hasMore)
		{
      if (NS_FAILED(arcs->HasMoreElements(&hasMore)) || !hasMore)
				break;
			nsCOMPtr<nsISupports>	arc;
			if (NS_FAILED(arcs->GetNext(getter_AddRefs(arc))))
				break;

			engine = do_QueryInterface(arc);
			if (!engine)	continue;

			const	char	*uri = nsnull;
			engine->GetValueConst(&uri);
			if (uri)
			{
				engineURI.AssignWithConversion(uri);
			}

			if (NS_FAILED(rv = FindData(engine, getter_AddRefs(dataLit))) ||
				(rv == NS_RDF_NO_VALUE))	continue;
			if (!dataLit)	continue;

			dataLit->GetValueConst(&dataUni);
			if (!dataUni)	continue;

			nsAutoString		action;
			if (NS_FAILED(rv = GetData(dataUni, "search", 0, "action", action)))	continue;
			if (shortURL.Equals(action, nsCaseInsensitiveStringComparator()))
			{
				foundEngine = PR_TRUE;
				break;
			}

			// extension for engines which can have multiple "actions"
			if (NS_FAILED(rv = GetData(dataUni, "browser", 0, "alsomatch", action)))	continue;
			if (nsString_Find(shortURL, action, PR_TRUE) >= 0)
			{
				foundEngine = PR_TRUE;
				break;
			}
		}
	}
  if (foundEngine)
	{
		nsAutoString	searchURL;
		searchURL.AssignWithConversion(url);

		// look for query option which is the string the user is searching for
 		nsAutoString	userVar, inputUnused, engineNameStr;
  	GetData(dataUni, "search", 0, "name", engineNameStr);

    if (NS_FAILED(rv = GetInputs(dataUni, engineNameStr, userVar, nsAutoString(), inputUnused, 0, 0, 0)))  return(rv);
		if (userVar.IsEmpty())	return(NS_RDF_NO_VALUE);

		nsAutoString	queryStr;
		queryStr = NS_LITERAL_STRING("?") +
		           userVar +
		           NS_LITERAL_STRING("=");

		PRInt32		queryOffset;
		if ((queryOffset = nsString_Find(queryStr, searchURL, PR_TRUE )) < 0)
		{
			queryStr = NS_LITERAL_STRING("&") +
			           userVar +
			           NS_LITERAL_STRING("=");
			queryOffset = nsString_Find(queryStr, searchURL, PR_TRUE);
		}

		nsAutoString	searchText;
		if (queryOffset >= 0)
		{
			PRInt32		andOffset;
			searchURL.Right(searchText, searchURL.Length() - queryOffset - queryStr.Length());

			if ((andOffset = searchText.FindChar(PRUnichar('&'))) >= 0)
			{
				searchText.Truncate(andOffset);
			}
		}
		if (!searchText.IsEmpty())
		{
 			// apply charset conversion to the search text
 			if (!mQueryEncodingStr.IsEmpty())
 			{
 				nsCOMPtr<nsITextToSubURI> textToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
 				if (NS_SUCCEEDED(rv))
 				{
 					nsCAutoString	escapedSearchText;
 					escapedSearchText.AssignWithConversion(searchText);

					// encoding +'s so as to preserve distinction between + and %2B
					escapedSearchText.ReplaceSubstring("%25", "%2B25");
					escapedSearchText.ReplaceSubstring("+", "%25");

 					nsCAutoString	aCharset;
 					aCharset.AssignWithConversion(mQueryEncodingStr);
 					PRUnichar	*uni = nsnull;
 					if (NS_SUCCEEDED(rv = textToSubURI->UnEscapeAndConvert(aCharset.get(), escapedSearchText.get(), &uni)) && (uni))
 					{
 						char	*convertedSearchText = nsnull;
 						if (NS_SUCCEEDED(rv = textToSubURI->ConvertAndEscape("UTF-8", uni, &convertedSearchText)))
 						{

							// decoding +'s thereby preserving distinction between + and %2B
 							nsCAutoString unescapedSearchText(convertedSearchText);
							unescapedSearchText.ReplaceSubstring("%25", "+");
							unescapedSearchText.ReplaceSubstring("%2B25", "%25");

 							searchText.AssignWithConversion(unescapedSearchText.get());

 							Recycle(convertedSearchText);
 						}
						Recycle(uni);
 					}
 				}
 			}
			// remember the text of the last search
			RememberLastSearchText(searchText.get());

			// construct the search query uri
			engineURI.Assign(NS_LITERAL_STRING("internetsearch:engine=") + engineURI +
			                 NS_LITERAL_STRING("&text=") + searchText);

#ifdef	DEBUG_SEARCH_OUTPUT
			char	*engineMatch = ToNewCString(searchText);
			if (engineMatch)
			{
				printf("FindInternetSearchResults: search for: '%s'\n\n",
					engineMatch);
				Recycle(engineMatch);
				engineMatch = nsnull;
			}
#endif
		}
		else
		{
			// not able to determine engine URI, so just use the search URL
			engineURI = searchURL;

			// clear the text of the last search
			RememberLastSearchText(nsnull);
		}

		// update "#Ref" on LastSearchRoot
		nsCOMPtr<nsIRDFNode>	oldNode;
		if (NS_SUCCEEDED(rv = mInner->GetTarget(kNC_LastSearchRoot, kNC_Ref, PR_TRUE,
			getter_AddRefs(oldNode))))
		{
			if (!engineURI.IsEmpty())
			{
				const PRUnichar	*uriUni = engineURI.get();
				nsCOMPtr<nsIRDFLiteral>	uriLiteral;
				nsresult		temprv;
				if ((uriUni) && (NS_SUCCEEDED(temprv = gRDFService->GetLiteral(uriUni,
					getter_AddRefs(uriLiteral)))))
				{
					if (rv != NS_RDF_NO_VALUE)
					{
						rv = mInner->Change(kNC_LastSearchRoot, kNC_Ref, oldNode, uriLiteral);
					}
					else
					{
						rv = mInner->Assert(kNC_LastSearchRoot, kNC_Ref, uriLiteral, PR_TRUE);
					}
				}
			}
			else
			{
				rv = mInner->Unassert(kNC_LastSearchRoot, kNC_Ref, oldNode);
			}
		}

		// forget about any previous search results
		ClearResults(PR_FALSE);

		// do the search
		DoSearch(nsnull, engine, searchURL, EmptyString());

		*searchInProgress = PR_TRUE;
	}

	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchDataSource::ClearResults(PRBool flushLastSearchRef)
{
	if (!mInner)	return(NS_ERROR_UNEXPECTED);

	// forget any nodes under the last search root
	nsresult			rv;
	nsCOMPtr<nsISimpleEnumerator>	arcs;
	if (NS_SUCCEEDED(rv = mInner->GetTargets(kNC_LastSearchRoot, kNC_Child, PR_TRUE, getter_AddRefs(arcs))))
	{
		PRBool			hasMore = PR_TRUE;
    while (hasMore)
		{
      if (NS_FAILED(arcs->HasMoreElements(&hasMore)) || !hasMore)
				break;
			nsCOMPtr<nsISupports>	arc;
			if (NS_FAILED(arcs->GetNext(getter_AddRefs(arc))))
				break;
			nsCOMPtr<nsIRDFResource> child (do_QueryInterface(arc));
			if (child)
			{
				mInner->Unassert(kNC_LastSearchRoot, kNC_Child, child);
			}

			// *after* (so that we won't thrash the XUL template builder) unasserting
			// child node, determine if there are any other references to the child
			// node in the graph

			PRBool hasInArcs = PR_FALSE;
			nsCOMPtr<nsISimpleEnumerator>	inArcs;
			if (NS_FAILED(mInner->ArcLabelsIn(child, getter_AddRefs(inArcs))) ||
				(!inArcs))
				continue;
      if (NS_FAILED(inArcs->HasMoreElements(&hasInArcs)) || hasInArcs)
				continue;

			// no other references, so also unassert any outgoing arcs

			nsCOMPtr<nsISimpleEnumerator>	outArcs;
			if (NS_FAILED(mInner->ArcLabelsOut(child, getter_AddRefs(outArcs))) ||
				(!outArcs))
				continue;
			PRBool	hasMoreOutArcs = PR_TRUE;
      while (hasMoreOutArcs)
			{
        if (NS_FAILED(outArcs->HasMoreElements(&hasMoreOutArcs)) ||
            !hasMoreOutArcs)
					break;
				nsCOMPtr<nsISupports>	outArc;
				if (NS_FAILED(outArcs->GetNext(getter_AddRefs(outArc))))
					break;
				nsCOMPtr<nsIRDFResource> property (do_QueryInterface(outArc));
				if (!property)
					continue;
				nsCOMPtr<nsIRDFNode> target;
				if (NS_FAILED(mInner->GetTarget(child, property, PR_TRUE,
					getter_AddRefs(target))) || (!target))
					continue;
				mInner->Unassert(child, property, target);
			}
		}
	}

  if (flushLastSearchRef)
	{
		// forget the last search query
		nsCOMPtr<nsIRDFNode>	lastTarget;
		if (NS_SUCCEEDED(rv = mInner->GetTarget(kNC_LastSearchRoot, kNC_Ref,
			PR_TRUE, getter_AddRefs(lastTarget))) && (rv != NS_RDF_NO_VALUE))
		{
			nsCOMPtr<nsIRDFLiteral>	lastLiteral (do_QueryInterface(lastTarget));
			if (lastLiteral)
			{
				rv = mInner->Unassert(kNC_LastSearchRoot, kNC_Ref, lastLiteral);
			}
		}
	}

	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchDataSource::ClearResultSearchSites(void)
{
	// forget about any previous search sites

	if (mInner)
	{
		nsresult			rv;
		nsCOMPtr<nsISimpleEnumerator>	arcs;
		if (NS_SUCCEEDED(rv = mInner->GetTargets(kNC_SearchResultsSitesRoot, kNC_Child, PR_TRUE, getter_AddRefs(arcs))))
		{
			PRBool			hasMore = PR_TRUE;
      while (hasMore)
			{
        if (NS_FAILED(arcs->HasMoreElements(&hasMore)) || !hasMore)
					break;
				nsCOMPtr<nsISupports>	arc;
				if (NS_FAILED(arcs->GetNext(getter_AddRefs(arc))))
					break;
				nsCOMPtr<nsIRDFResource> child (do_QueryInterface(arc));
				if (child)
				{
					mInner->Unassert(kNC_SearchResultsSitesRoot, kNC_Child, child);
				}
			}
		}
	}
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchDataSource::GetCategoryDataSource(nsIRDFDataSource **ds)
{
	nsresult	rv;

	if (!categoryDataSource)
	{
		if (NS_FAILED(rv = GetCategoryList()))
		{
			*ds = nsnull;
			return(rv);
		}
	}
	if (categoryDataSource)
	{
		*ds = categoryDataSource.get();
		NS_IF_ADDREF(*ds);
		return(NS_OK);
	}
	*ds = nsnull;
	return(NS_ERROR_FAILURE);
}



NS_IMETHODIMP
InternetSearchDataSource::Stop()
{
	nsresult		rv;

	// cancel any outstanding connections
	if (mLoadGroup)
	{
		nsCOMPtr<nsISimpleEnumerator>	requests;
		if (NS_SUCCEEDED(rv = mLoadGroup->GetRequests(getter_AddRefs(requests))))
		{
			PRBool			more;
      while (NS_SUCCEEDED(rv = requests->HasMoreElements(&more)) && more)
			{
				nsCOMPtr<nsISupports>	isupports;
				if (NS_FAILED(rv = requests->GetNext(getter_AddRefs(isupports))))
					break;
				nsCOMPtr<nsIRequest> request (do_QueryInterface(isupports));
				if (!request)	continue;
				request->Cancel(NS_BINDING_ABORTED);
			}
		}
		mLoadGroup->Cancel(NS_BINDING_ABORTED);
	}

	// remove any loading icons
	nsCOMPtr<nsISimpleEnumerator>	arcs;
	if (NS_SUCCEEDED(rv = mInner->GetSources(kNC_loading, kTrueLiteral, PR_TRUE,
		getter_AddRefs(arcs))))
	{
		PRBool			hasMore = PR_TRUE;
    while (hasMore)
		{
      if (NS_FAILED(arcs->HasMoreElements(&hasMore)) || !hasMore)
				break;
			nsCOMPtr<nsISupports>	arc;
			if (NS_FAILED(arcs->GetNext(getter_AddRefs(arc))))
				break;
			nsCOMPtr<nsIRDFResource> src (do_QueryInterface(arc));
			if (src)
			{
				mInner->Unassert(src, kNC_loading, kTrueLiteral);
			}
		}
	}

	return(NS_OK);
}



nsresult
InternetSearchDataSource::BeginSearchRequest(nsIRDFResource *source, PRBool doNetworkRequest)
{
        nsresult		rv = NS_OK;
	const char		*sourceURI = nsnull;

	if (NS_FAILED(rv = source->GetValueConst(&sourceURI)))
		return(rv);
	nsAutoString		uri;
	uri.AssignWithConversion(sourceURI);

	if (uri.Find("internetsearch:") != 0)
		return(NS_ERROR_FAILURE);

	// forget about any previous search results
	ClearResults(PR_TRUE);

	// forget about any previous search sites
	ClearResultSearchSites();

	// remember the last search query
	const PRUnichar	*uriUni = uri.get();
	nsCOMPtr<nsIRDFLiteral>	uriLiteral;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(uriUni, getter_AddRefs(uriLiteral))))
	{
		rv = mInner->Assert(kNC_LastSearchRoot, kNC_Ref, uriLiteral, PR_TRUE);
	}

	uri.Cut(0, strlen("internetsearch:"));

	nsVoidArray	*engineArray = new nsVoidArray;
	if (!engineArray)
		return(NS_ERROR_FAILURE);

	nsAutoString	text;

	// parse up attributes

	while(!uri.IsEmpty())
	{
		nsAutoString	item;

		PRInt32 andOffset = uri.Find("&");
		if (andOffset >= 0)
		{
			uri.Left(item, andOffset);
			uri.Cut(0, andOffset + 1);
		}
		else
		{
			item = uri;
			uri.Truncate();
		}

		PRInt32 equalOffset = item.Find("=");
		if (equalOffset < 0)	break;
		
		nsAutoString	attrib, value;
		item.Left(attrib, equalOffset);
		value = item;
		value.Cut(0, equalOffset + 1);
		
		if (!attrib.IsEmpty() && !value.IsEmpty())
		{
			if (attrib.LowerCaseEqualsLiteral("engine"))
			{
				if ((value.Find(kEngineProtocol) == 0) ||
					(value.Find(kURINC_SearchCategoryEnginePrefix) == 0))
				{
					char	*val = ToNewCString(value);
					if (val)
					{
						engineArray->AppendElement(val);
					}
				}
			}
			else if (attrib.LowerCaseEqualsLiteral("text"))
			{
				text = value;
			}
		}
	}

	mInner->Assert(source, kNC_loading, kTrueLiteral, PR_TRUE);

	PRBool	requestInitiated = PR_FALSE;

	// loop over specified search engines
	while (engineArray->Count() > 0)
	{
		char *baseFilename = (char *)(engineArray->ElementAt(0));
		engineArray->RemoveElementAt(0);
		if (!baseFilename)	continue;

#ifdef	DEBUG_SEARCH_OUTPUT
		printf("Search engine to query: '%s'\n", baseFilename);
#endif

		nsCOMPtr<nsIRDFResource>	engine;
		gRDFService->GetResource(nsDependentCString(baseFilename), getter_AddRefs(engine));
		nsCRT::free(baseFilename);
		baseFilename = nsnull;
		if (!engine)	continue;

		// if its a engine from a search category, then get its "#Name",
		// and map from that back to the real engine reference; if an
		// error occurs, finish processing the rest of the engines,
		// don't just break/return out
		if (isSearchCategoryEngineURI(engine))
		{
			nsCOMPtr<nsIRDFResource>	trueEngine;
			rv = resolveSearchCategoryEngineURI(engine, getter_AddRefs(trueEngine));
			if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE))	return(rv);
			if (!trueEngine)	continue;

			engine = trueEngine;
		}

		// mark this as a search site
		if (mInner)
		{
			mInner->Assert(kNC_SearchResultsSitesRoot, kNC_Child, engine, PR_TRUE);
		}

    if (doNetworkRequest)
		{
			DoSearch(source, engine, EmptyString(), text);
			requestInitiated = PR_TRUE;
		}
	}
	
	delete engineArray;
	engineArray = nsnull;

  if (!requestInitiated)
	{
		Stop();
	}

	return(rv);
}



nsresult
InternetSearchDataSource::FindData(nsIRDFResource *engine, nsIRDFLiteral **dataLit)
{
	if (!engine)	return(NS_ERROR_NULL_POINTER);
	if (!dataLit)	return(NS_ERROR_NULL_POINTER);

	*dataLit = nsnull;

	if (!mInner)	return(NS_RDF_NO_VALUE);

	nsresult		rv;

	nsCOMPtr<nsIRDFNode>	dataTarget = nsnull;
	if (NS_SUCCEEDED((rv = mInner->GetTarget(engine, kNC_Data, PR_TRUE,
		getter_AddRefs(dataTarget)))) && (dataTarget))
	{
		nsCOMPtr<nsIRDFLiteral>	aLiteral (do_QueryInterface(dataTarget));
		if (!aLiteral)
			return(NS_ERROR_UNEXPECTED);
		*dataLit = aLiteral;
		NS_IF_ADDREF(*dataLit);
		return(NS_OK);
	}

	// don't have the data, so try and read it in

	const char	*engineURI = nsnull;
	if (NS_FAILED(rv = engine->GetValueConst(&engineURI)))
		return(rv);
	nsAutoString	engineStr;
	engineStr.AssignWithConversion(engineURI);
	if (engineStr.Find(kEngineProtocol) != 0)
		return(rv);
	engineStr.Cut(0, sizeof(kEngineProtocol) - 1);
	char	*baseFilename = ToNewCString(engineStr);
	if (!baseFilename)
		return(rv);
	baseFilename = nsUnescape(baseFilename);
	if (!baseFilename)
		return(rv);

#ifdef	DEBUG_SEARCH_OUTPUT
	printf("InternetSearchDataSource::FindData - reading in '%s'\n", baseFilename);
#endif
        nsCOMPtr<nsILocalFile> engineFile;
        rv = NS_NewNativeLocalFile(nsDependentCString(baseFilename), PR_TRUE, getter_AddRefs(engineFile));
        if (NS_FAILED(rv)) return rv;

        nsString	data;
        rv = ReadFileContents(engineFile, data);

	nsCRT::free(baseFilename);
	baseFilename = nsnull;
	if (NS_FAILED(rv))
	{
		return(rv);
	}

	// save file contents
	if (data.IsEmpty())	return(NS_ERROR_UNEXPECTED);

	rv = updateDataHintsInGraph(engine, data.get());

	nsCOMPtr<nsIRDFLiteral>	aLiteral;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(data.get(), getter_AddRefs(aLiteral))))
	{
		*dataLit = aLiteral;
		NS_IF_ADDREF(*dataLit);
	}
	
	return(rv);
}

nsresult
InternetSearchDataSource::DecodeData(const char *aCharset, const PRUnichar *aInString, PRUnichar **aOutString)
{
	nsresult rv;
    
	nsCOMPtr <nsICharsetConverterManager> charsetConv = 
          do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
	NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIUnicodeDecoder>	unicodeDecoder;
	rv = charsetConv->GetUnicodeDecoder(aCharset, getter_AddRefs(unicodeDecoder));

	// Use the sherlock default charset in case of failure
	if (NS_FAILED(rv))
		rv = charsetConv->GetUnicodeDecoderRaw("x-mac-roman", getter_AddRefs(unicodeDecoder));
	NS_ENSURE_SUCCESS(rv, rv);

	// This fixes the corruption occured in InternetSearchDataSource::ReadFileContents()
	// (eg. aValue contains "0x0082 0x00a1" for "0x82 0xa1" shift_jis double-byte char)
	NS_LossyConvertUCS2toASCII value(aInString);

	PRInt32 srcLength = value.Length();
	PRInt32 outUnicodeLen;
	rv = unicodeDecoder->GetMaxLength(value.get(), srcLength, &outUnicodeLen);
	NS_ENSURE_SUCCESS(rv, rv);

	*aOutString = NS_REINTERPRET_CAST(PRUnichar*, nsMemory::Alloc((outUnicodeLen + 1) * sizeof(PRUnichar)));
	NS_ENSURE_TRUE(*aOutString, NS_ERROR_OUT_OF_MEMORY);

	rv = unicodeDecoder->Convert(value.get(), &srcLength, *aOutString, &outUnicodeLen);
	NS_ENSURE_SUCCESS(rv, rv);
	(*aOutString)[outUnicodeLen] = (PRUnichar)'\0';

	return rv;
}

nsresult
InternetSearchDataSource::updateDataHintsInGraph(nsIRDFResource *engine, const PRUnichar *dataUni)
{
	nsresult	rv = NS_OK;

	// save/update search engine data
	nsCOMPtr<nsIRDFLiteral>	dataLiteral;
	if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(dataUni, getter_AddRefs(dataLiteral))))
	{
		updateAtom(mInner, engine, kNC_Data, dataLiteral, nsnull);
	}

	// save/update name of search engine (as specified in file)
	nsAutoString scriptCodeValue;
	const char * charsetName = MapScriptCodeToCharsetName(0);
	nsXPIDLString decodedValue;

	if (NS_SUCCEEDED(rv = GetData(dataUni, "search", 0, "sourceTextEncoding", scriptCodeValue)) && 
		!scriptCodeValue.IsEmpty())
	{
		PRInt32 err;
		PRInt32 scriptCode = scriptCodeValue.ToInteger(&err);
		if (NS_SUCCEEDED(err))
			charsetName = MapScriptCodeToCharsetName(scriptCode);
	}

	nsAutoString	nameValue;
	if (NS_SUCCEEDED(rv = GetData(dataUni, "search", 0, "name", nameValue)))
	{
		rv = DecodeData(charsetName, nameValue.get(), getter_Copies(decodedValue));
		nsCOMPtr<nsIRDFLiteral>	nameLiteral;
		if (NS_SUCCEEDED(rv) &&
				NS_SUCCEEDED(rv = gRDFService->GetLiteral(decodedValue.get(),
				                                          getter_AddRefs(nameLiteral))))
		{
			rv = updateAtom(mInner, engine, kNC_Name, nameLiteral, nsnull);
		}
	}

	// save/update description of search engine (if specified)
	nsAutoString	descValue;
	if (NS_SUCCEEDED(rv = GetData(dataUni, "search", 0, "description", descValue)))
	{
		rv = DecodeData(charsetName, descValue.get(), getter_Copies(decodedValue));
		nsCOMPtr<nsIRDFLiteral>	descLiteral;
		if (NS_SUCCEEDED(rv) &&
				NS_SUCCEEDED(rv = gRDFService->GetLiteral(decodedValue.get(),
				                                          getter_AddRefs(descLiteral))))
		{
			rv = updateAtom(mInner, engine, kNC_Description, descLiteral, nsnull);
		}
	}

	// save/update version of search engine (if specified)
	nsAutoString	versionValue;
	if (NS_SUCCEEDED(rv = GetData(dataUni, "search", 0, "version", versionValue)))
	{
		nsCOMPtr<nsIRDFLiteral>	versionLiteral;
		if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(versionValue.get(),
				getter_AddRefs(versionLiteral))))
		{
			rv = updateAtom(mInner, engine, kNC_Version, versionLiteral, nsnull);
		}
	}

	nsAutoString	buttonValue;
	if (NS_SUCCEEDED(rv = GetData(dataUni, "search", 0, "actionButton", buttonValue)))
	{
		nsCOMPtr<nsIRDFLiteral>	buttonLiteral;
		if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(buttonValue.get(),
				getter_AddRefs(buttonLiteral))))
		{
			rv = updateAtom(mInner, engine, kNC_actionButton, buttonLiteral, nsnull);
		}
	}

	nsAutoString	barValue;
	if (NS_SUCCEEDED(rv = GetData(dataUni, "search", 0, "actionBar", barValue)))
	{
		nsCOMPtr<nsIRDFLiteral>	barLiteral;
		if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(barValue.get(),
				getter_AddRefs(barLiteral))))
		{
			rv = updateAtom(mInner, engine, kNC_actionBar, barLiteral, nsnull);
		}
	}

	nsAutoString	searchFormValue;
	if (NS_SUCCEEDED(rv = GetData(dataUni, "search", 0, "searchForm", searchFormValue)))
	{
		nsCOMPtr<nsIRDFLiteral>	searchFormLiteral;
		if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(searchFormValue.get(),
				getter_AddRefs(searchFormLiteral))))
		{
			rv = updateAtom(mInner, engine, kNC_searchForm, searchFormLiteral, nsnull);
		}
	}


	PRBool	updatePrivateFiles = PR_FALSE;

  rv = mInner->HasAssertion(engine, kNC_SearchType, kNC_Engine, PR_TRUE,
                            &updatePrivateFiles);
  if (NS_SUCCEEDED(rv) && updatePrivateFiles)
	{
		// rjc says: so here is our strategy. The spec says that the "search"|"update"
		// attribute MUST refer to a binhex-ed file (i.e. ends with ".src.hqx"). We don't
		// support binhex (due to being cross-platform minded folks) and, even if we did,
		// its not enough info to figure out what the appropriate icon should be.
		// The answer: we're add additional attributes in our private <BROWSER> section
		// to re-define these values. If they aren't there, we'll fallback to the <SEARCH>
		// section values and try and make logical choices:  for example, if we have an
		// "update" URL which ends with ".src.hqx", we'll strip off the ".hqx" part and
		// check to see if it exists... which at least makes it possible for a web site
		// to put up both a binhexed version and a straight text version of the search file.

		// get update URL and # of days to check for updates
		// Note: only check for updates on our private search files
		nsAutoString	updateStr, updateIconStr, updateCheckDaysStr;

		GetData(dataUni, "browser", 0, "update", updateStr);
		if (updateStr.IsEmpty())
		{
			// fallback to trying "search"|"updateCheckDays"
			GetData(dataUni, "search", 0, "update", updateStr);

			// if we have a ".hqx" extension, strip it off
			nsAutoString	extension;
			updateStr.Right(extension, 4);
			if (extension.LowerCaseEqualsLiteral(".hqx"))
			{
				updateStr.Truncate(updateStr.Length() - 4);
			}

			// now, either way, ensure that we have a ".src" file
			updateStr.Right(extension, 4);
      if (!extension.LowerCaseEqualsLiteral(".src"))
			{
				// and if we don't, toss it
				updateStr.Truncate();
			}
		}
		else
		{
			// note: its OK if the "updateIcon" isn't specified
			GetData(dataUni, "browser", 0, "updateIcon", updateIconStr);
		}
		if (!updateStr.IsEmpty())
		{
			GetData(dataUni, "browser", 0, "updateCheckDays", updateCheckDaysStr);
			if (updateCheckDaysStr.IsEmpty())
			{
				// fallback to trying "search"|"updateCheckDays"
				GetData(dataUni, "search", 0, "updateCheckDays", updateCheckDaysStr);
			}
		}

		if (!updateStr.IsEmpty() && !updateCheckDaysStr.IsEmpty())
		{
			nsCOMPtr<nsIRDFLiteral>	updateLiteral;
			if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(updateStr.get(),
					getter_AddRefs(updateLiteral))))
			{
				rv = updateAtom(mInner, engine, kNC_Update, updateLiteral, nsnull);
			}

			PRInt32	err;
			PRInt32	updateDays = updateCheckDaysStr.ToInteger(&err);
			if ((err) || (updateDays < 1))
			{
				// default to something sane
				updateDays = 3;
			}

			nsCOMPtr<nsIRDFInt>	updateCheckDaysLiteral;
			if (NS_SUCCEEDED(rv = gRDFService->GetIntLiteral(updateDays,
					getter_AddRefs(updateCheckDaysLiteral))))
			{
				rv = updateAtom(mInner, engine, kNC_UpdateCheckDays, updateCheckDaysLiteral, nsnull);
			}

			if (!updateIconStr.IsEmpty())
			{
				nsCOMPtr<nsIRDFLiteral>	updateIconLiteral;
				if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(updateIconStr.get(),
						getter_AddRefs(updateIconLiteral))))
				{
					rv = updateAtom(mInner, engine, kNC_UpdateIcon, updateIconLiteral, nsnull);
				}
			}
		}
	}

	return(rv);
}



nsresult
InternetSearchDataSource::updateAtom(nsIRDFDataSource *db, nsIRDFResource *src,
			nsIRDFResource *prop, nsIRDFNode *newValue, PRBool *dirtyFlag)
{
	nsresult		rv;
	nsCOMPtr<nsIRDFNode>	oldValue;

	if (dirtyFlag != nsnull)
	{
		*dirtyFlag = PR_FALSE;
	}

	if (NS_SUCCEEDED(rv = db->GetTarget(src, prop, PR_TRUE, getter_AddRefs(oldValue))) &&
		(rv != NS_RDF_NO_VALUE))
	{
		rv = db->Change(src, prop, oldValue, newValue);

		if ((oldValue.get() != newValue) && (dirtyFlag != nsnull))
		{
			*dirtyFlag = PR_TRUE;
		}
	}
	else
	{
		rv = db->Assert(src, prop, newValue, PR_TRUE);
		if (dirtyFlag != nsnull)
		{
		    *dirtyFlag = PR_TRUE;
		}
	}
	return(rv);
}



struct	encodings
{
	const char	*numericEncoding;
	const char	*stringEncoding;
};



nsresult
InternetSearchDataSource::MapEncoding(const nsString &numericEncoding, 
                                      nsString &stringEncoding)
{
	// XXX we need to have a full table of numeric --> string conversions

	struct	encodings	encodingList[] =
	{
		{	"0", "x-mac-roman"	},
		{	"6", "x-mac-greek"	},
		{	"35", "x-mac-turkish"	},
		{	"513", "ISO-8859-1"	},
		{	"514", "ISO-8859-2"	},
		{	"517", "ISO-8859-5"	},
		{	"518", "ISO-8859-6"	},
		{	"519", "ISO-8859-7"	},
		{	"520", "ISO-8859-8"	},
		{	"521", "ISO-8859-9"	},
		{	"1049", "IBM864"	},
		{	"1280", "windows-1252"	},
		{	"1281", "windows-1250"	},
		{	"1282", "windows-1251"	},
		{	"1283", "windows-1253"	},
		{	"1284", "windows-1254"	},
		{	"1285", "windows-1255"	},
		{	"1286", "windows-1256"	},
		{	"1536", "us-ascii"	},
		{	"1584", "GB2312"	},
		{	"1585", "x-gbk"		},
		{	"1600", "EUC-KR"	},
		{	"2080", "ISO-2022-JP"	},
		{	"2096", "ISO-2022-CN"	},
		{	"2112", "ISO-2022-KR"	},
		{	"2336", "EUC-JP"	},
		{	"2352", "GB2312"	},
		{	"2353", "x-euc-tw"	},
		{	"2368", "EUC-KR"	},
		{	"2561", "Shift_JIS"	},
		{	"2562", "KOI8-R"	},
		{	"2563", "Big5"		},
		{	"2565", "HZ-GB-2312"	},

		{	nsnull, nsnull		}
	};

  if (!numericEncoding.IsEmpty())	{
    for (PRUint32 i = 0; encodingList[i].numericEncoding != nsnull; i++)
    {
      if (numericEncoding.EqualsASCII(encodingList[i].numericEncoding)) 
      {
        stringEncoding.AssignASCII(encodingList[i].stringEncoding);
        return NS_OK;
      }
    }
  }

  // Still no encoding, fall back to default charset if possible
  nsXPIDLString defCharset;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (prefs)
    prefs->GetLocalizedUnicharPref("intl.charset.default", getter_Copies(defCharset));

  if (!defCharset.IsEmpty())
    stringEncoding = defCharset;
  else
    // make "ISO-8859-1" as the default (not "UTF-8")
    stringEncoding.AssignLiteral("ISO-8859-1");

  return(NS_OK);
}



const char * const
InternetSearchDataSource::MapScriptCodeToCharsetName(PRUint32 aScriptCode)
{
	// Script codes listed here are taken from Inside Macintosh Text.
	// Used TECGetTextEncodingInternetName to map script code to charset name.
	// Some are edited to match with standard name (e.g. "x-mac-japanese" -> "Shift_JIS").
	// In case no converter can be found for a charset name, 
	// the default sherlock charset "x-mac-roman" should be used.
	// 
        static const char* const scriptList[] =
	{
		"x-mac-roman",           // 0
		"Shift_JIS",             // 1
		"Big5",                  // 2
		"EUC-KR",                // 3
		"X-MAC-ARABIC",          // 4
		"X-MAC-HEBREW",          // 5
		"X-MAC-GREEK",           // 6
		"X-MAC-CYRILLIC",        // 7
		"X-MAC-DEVANAGARI" ,     // 9
		"X-MAC-GURMUKHI",        // 10
		"X-MAC-GUJARATI",        // 11
		"X-MAC-ORIYA",           // 12
		"X-MAC-BENGALI",         // 13
		"X-MAC-TAMIL",           // 14
		"X-MAC-TELUGU",          // 15
		"X-MAC-KANNADA",         // 16
		"X-MAC-MALAYALAM",       // 17
		"X-MAC-SINHALESE",       // 18
		"X-MAC-BURMESE",         // 19
		"X-MAC-KHMER",           // 20
		"X-MAC-THAI",            // 21
		"X-MAC-LAOTIAN",         // 22
		"X-MAC-GEORGIAN",        // 23
		"X-MAC-ARMENIAN",        // 24
		"GB2312",                // 25
		"X-MAC-TIBETAN",         // 26
		"X-MAC-MONGOLIAN",       // 27
		"X-MAC-ETHIOPIC",        // 28
		"X-MAC-CENTRALEURROMAN", // 29
		"X-MAC-VIETNAMESE",      // 30
		"X-MAC-EXTARABIC",       // 31
 	};

        if (aScriptCode >= NS_ARRAY_LENGTH(scriptList))
          aScriptCode = 0;

	return scriptList[aScriptCode];
}


nsresult
InternetSearchDataSource::validateEngine(nsIRDFResource *engine)
{
	nsresult	rv;

#ifdef	DEBUG_SEARCH_UPDATES
	const char	*engineURI = nsnull;
	engine->GetValueConst(&engineURI);
#endif

	// get the engines "updateCheckDays" value
	nsCOMPtr<nsIRDFNode>	updateCheckDaysNode;
	rv = mInner->GetTarget(engine, kNC_UpdateCheckDays, PR_TRUE, getter_AddRefs(updateCheckDaysNode));
	if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE))	return(rv);
	nsCOMPtr<nsIRDFInt> updateCheckDaysLiteral (do_QueryInterface(updateCheckDaysNode));
	PRInt32		updateCheckDays;
	updateCheckDaysLiteral->GetValue(&updateCheckDays);
	// convert updateCheckDays from days to seconds;

#ifndef	DEBUG_SEARCH_UPDATES
	PRInt32		updateCheckSecs = updateCheckDays * (60 * 60 * 24);
#else
	// rjc note: for debugging, use as minutes instead days
	PRInt32		updateCheckSecs = updateCheckDays * 60;
#endif

	// get the current date/time [from microseconds (PRTime) to seconds]
	PRTime		now64 = PR_Now(), temp64, million;
	LL_I2L(million, PR_USEC_PER_SEC);
	LL_DIV(temp64, now64, million);
	PRInt32		now32;
	LL_L2I(now32, temp64);

	nsCOMPtr<nsIRDFNode>	aNode;
	rv = mLocalstore->GetTarget(engine, kWEB_LastPingDate, PR_TRUE, getter_AddRefs(aNode));
	if (NS_FAILED(rv))	return(rv);
	if (rv == NS_RDF_NO_VALUE)
	{
		// if we've never validated this engine before,
		// then start its epoch as of now
		validateEngineNow(engine);

#ifdef	DEBUG_SEARCH_UPDATES
		printf("    Search engine '%s' marked valid as of now.\n", engineURI);
#endif

		return(NS_OK);
	}

	// get last validate date/time
	nsCOMPtr<nsIRDFLiteral>	lastCheckLiteral (do_QueryInterface(aNode));
	if (!lastCheckLiteral)	return(NS_ERROR_UNEXPECTED);
	const PRUnichar		*lastCheckUni = nsnull;
	lastCheckLiteral->GetValueConst(&lastCheckUni);
	if (!lastCheckUni)	return(NS_ERROR_UNEXPECTED);
	nsAutoString		lastCheckStr(lastCheckUni);
	PRInt32			lastCheckInt=0, err=0;
	lastCheckInt = lastCheckStr.ToInteger(&err);
	if (err)		return(NS_ERROR_UNEXPECTED);

	// calculate duration since last validation and
	// just return if its too early to check again
	PRInt32			durationSecs = now32 - lastCheckInt;
	if (durationSecs < updateCheckSecs)
	{
#ifdef	DEBUG_SEARCH_UPDATES
		printf("    Search engine '%s' is valid for %d more seconds.\n",
			engineURI, (updateCheckSecs-durationSecs));
#endif
		return(NS_OK);
	}

	// search engine needs to be checked again, so add it into the to-be-validated array
	PRInt32		elementIndex = mUpdateArray->IndexOf(engine);
	if (elementIndex < 0)
	{
		mUpdateArray->AppendElement(engine);

#ifdef	DEBUG_SEARCH_UPDATES
		printf("    Search engine '%s' is now queued to be validated via HTTP HEAD method.\n",
			engineURI, durationSecs);
#endif
	}
	else
	{
#ifdef	DEBUG_SEARCH_UPDATES
		printf("    Search engine '%s' is already in queue to be validated.\n",
			engineURI);
#endif
	}
	return(NS_OK);
}



nsresult
InternetSearchDataSource::DoSearch(nsIRDFResource *source, nsIRDFResource *engine,
				const nsString &fullURL, const nsString &text)
{
	nsresult	rv;
	nsAutoString	textTemp(text);

	if (!mInner)	return(NS_RDF_NO_VALUE);
//	Note: source can be null
//	if (!source)	return(NS_ERROR_NULL_POINTER);
	if (!engine)	return(NS_ERROR_NULL_POINTER);

	validateEngine(engine);

	nsCOMPtr<nsIUnicodeDecoder>	unicodeDecoder;
	nsAutoString			action, methodStr, input, userVar;

	nsCOMPtr<nsIRDFLiteral>		dataLit;
	if (NS_FAILED(rv = FindData(engine, getter_AddRefs(dataLit))) ||
		(rv == NS_RDF_NO_VALUE))	return(rv);

	const PRUnichar			*dataUni = nsnull;
	dataLit->GetValueConst(&dataUni);
	if (!dataUni)			return(NS_RDF_NO_VALUE);

	if (!fullURL.IsEmpty())
	{
		action.Assign(fullURL);
		methodStr.AssignLiteral("get");
	}
	else
	{
		if (NS_FAILED(rv = GetData(dataUni, "search", 0, "action", action)))	return(rv);
		if (NS_FAILED(rv = GetData(dataUni, "search", 0, "method", methodStr)))	return(rv);
	}

	nsAutoString	encodingStr, resultEncodingStr;

	// first look for "interpret/charset"... if that isn't specified,
	// then fall back to looking for "interpret/resultEncoding" (a decimal)
	GetData(dataUni, "interpret", 0, "charset", resultEncodingStr);
	if (resultEncodingStr.IsEmpty())
	{
		GetData(dataUni, "interpret", 0, "resultEncoding", encodingStr);	// decimal string values
		MapEncoding(encodingStr, resultEncodingStr);
	}
	// rjc note: ignore "interpret/resultTranslationEncoding" as well as
	// "interpret/resultTranslationFont" since we always convert results to Unicode
	if (!resultEncodingStr.IsEmpty())
	{
		nsCOMPtr <nsICharsetConverterManager> charsetConv = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
		if (NS_SUCCEEDED(rv))
		{
                  NS_LossyConvertUCS2toASCII charset(resultEncodingStr);
                  rv = charsetConv->GetUnicodeDecoder(charset.get(),
                                                      getter_AddRefs(unicodeDecoder));
		}
	}

	// first look for "search/queryCharset"... if that isn't specified,
	// then fall back to looking for "search/queryEncoding" (a decimal)
	nsAutoString    queryEncodingStr;
	GetData(dataUni, "search", 0, "queryCharset", queryEncodingStr);
	if (queryEncodingStr.IsEmpty())
	{
		GetData(dataUni, "search", 0, "queryEncoding", encodingStr);		// decimal string values
		MapEncoding(encodingStr, queryEncodingStr);
	}
	if (!queryEncodingStr.IsEmpty())
	{
		// convert from escaped-UTF_8, to unicode, and then to
		// the charset indicated by the dataset in question

		char	*utf8data = ToNewUTF8String(textTemp);
		if (utf8data)
		{
			nsCOMPtr<nsITextToSubURI> textToSubURI = 
			         do_GetService(kTextToSubURICID, &rv);
			if (NS_SUCCEEDED(rv) && (textToSubURI))
			{
				PRUnichar	*uni = nsnull;
				if (NS_SUCCEEDED(rv = textToSubURI->UnEscapeAndConvert("UTF-8", utf8data, &uni)) && (uni))
				{
					char		*charsetData = nsnull;
					nsCAutoString	queryencodingstrC;
					queryencodingstrC.AssignWithConversion(queryEncodingStr);
					if (NS_SUCCEEDED(rv = textToSubURI->ConvertAndEscape(queryencodingstrC.get(), uni, &charsetData))
					        && (charsetData))
					{
						textTemp.AssignWithConversion(charsetData);
						Recycle(charsetData);
					}
					Recycle(uni);
				}
			}
			Recycle(utf8data);
		}
	}

	if (fullURL.IsEmpty() && methodStr.LowerCaseEqualsLiteral("get"))
	{
    nsAutoString engineNameStr;
  	GetData(dataUni, "search", 0, "name", engineNameStr);

    if (NS_FAILED(rv = GetInputs(dataUni, engineNameStr, userVar, textTemp, input, 0, 0, 0)))  return(rv);
		if (input.IsEmpty())				return(NS_ERROR_UNEXPECTED);

		// HTTP Get method support
		action += NS_LITERAL_STRING("?") + input;
	}

	nsCOMPtr<nsIInternetSearchContext>	context;
	if (NS_FAILED(rv = NS_NewInternetSearchContext(nsIInternetSearchContext::WEB_SEARCH_CONTEXT,
		source, engine, unicodeDecoder, nsnull, getter_AddRefs(context))))
		return(rv);
	if (!context)	return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIURI>	url;
	if (NS_SUCCEEDED(rv = NS_NewURI(getter_AddRefs(url), action)))
	{
		nsCOMPtr<nsIChannel>	channel;
		if (NS_SUCCEEDED(rv = NS_NewChannel(getter_AddRefs(channel), url, nsnull, mLoadGroup)))
		{

			// send a "MultiSearch" header
			nsCOMPtr<nsIHttpChannel> httpMultiChannel (do_QueryInterface(channel));
			if (httpMultiChannel)
			{
                httpMultiChannel->SetRequestHeader(NS_LITERAL_CSTRING("MultiSearch"),
                                                   NS_LITERAL_CSTRING("true"),
                                                   PR_FALSE);
			}

			// get it just from the cache if we can (do not validate)
			channel->SetLoadFlags(nsIRequest::LOAD_FROM_CACHE);

			if (methodStr.LowerCaseEqualsLiteral("post"))
			{
				nsCOMPtr<nsIHttpChannel> httpChannel (do_QueryInterface(channel));
				if (httpChannel)
				{
				    httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("POST"));
				    
				    // construct post data to send
				    nsAutoString	postStr;
				    postStr.AssignASCII(POSTHEADER_PREFIX);
				    postStr.AppendInt(input.Length(), 10);
				    postStr.AppendASCII(POSTHEADER_SUFFIX);
				    postStr += input;
				    
				    nsCOMPtr<nsIInputStream>	postDataStream;
				    nsCAutoString			poststrC;
				    poststrC.AssignWithConversion(postStr);
				    if (NS_SUCCEEDED(rv = NS_NewPostDataStream(getter_AddRefs(postDataStream),
									       PR_FALSE, poststrC, 0)))
				    {
					nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
					NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");
					uploadChannel->SetUploadStream(postDataStream, EmptyCString(), -1);
				    }
				}
			}
			
			nsCOMPtr<nsIRequest> request;
      rv = channel->AsyncOpen(this, context);
		}
	}

	// dispose of any last HTML results page
	if (mInner)
	{
		nsCOMPtr<nsIRDFNode>	htmlNode;
		if (NS_SUCCEEDED(rv = mInner->GetTarget(engine, kNC_HTML, PR_TRUE, getter_AddRefs(htmlNode)))
			&& (rv != NS_RDF_NO_VALUE))
		{
			rv = mInner->Unassert(engine, kNC_HTML, htmlNode);
		}
	}

	// start "loading" animation for this search engine
	if (NS_SUCCEEDED(rv) && (mInner))
	{
		// remove status icon so that loading icon style can be used
		nsCOMPtr<nsIRDFNode>		engineIconNode = nsnull;
		mInner->GetTarget(engine, kNC_StatusIcon, PR_TRUE, getter_AddRefs(engineIconNode));
		if (engineIconNode)
		{
			rv = mInner->Unassert(engine, kNC_StatusIcon, engineIconNode);
		}

		mInner->Assert(engine, kNC_loading, kTrueLiteral, PR_TRUE);
	}

	return(rv);
}



nsresult
InternetSearchDataSource::GetSearchFolder(nsIFile **searchDir)
{
  NS_ENSURE_ARG_POINTER(searchDir);
  *searchDir = nsnull;
	
  nsCOMPtr<nsIFile> aDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_SEARCH_DIR, getter_AddRefs(aDir));
  if (NS_FAILED(rv)) return rv;
  
  *searchDir = aDir;
  NS_ADDREF(*searchDir);
  return NS_OK;
}



nsresult
InternetSearchDataSource::SaveEngineInfoIntoGraph(nsIFile *file, nsIFile *icon,
		const PRUnichar *categoryHint, const PRUnichar *dataUni, PRBool isSystemSearchFile,
		PRBool checkMacFileType)
{
	nsresult			rv = NS_OK;

	if (!file && !icon)	return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIRDFResource>	searchRes;
	nsCOMPtr<nsIRDFResource>	categoryRes;
	nsCOMPtr<nsIFile>		native;

	if (icon != nsnull)
	{
		native = icon;
	}

	if (file != nsnull)
	{
		native = file;
	}

  PRBool exists;
  rv = native->Exists(&exists);
	if (NS_FAILED(rv)) return(rv);
	if (!exists) return(NS_ERROR_UNEXPECTED);

	nsAutoString basename;
	rv = native->GetLeafName(basename);
	if (NS_FAILED(rv)) return rv;

	// ensure that the basename points to the search engine file
	PRInt32		extensionOffset;
	if ((extensionOffset = basename.RFindChar(PRUnichar('.'))) > 0)
	{
		basename.Truncate(extensionOffset);
		basename.AppendLiteral(".src");
	}

  nsCAutoString filePath;
  rv = native->GetNativePath(filePath);
  if (NS_FAILED(rv)) return rv;
  
	nsAutoString	searchURL;
	searchURL.AssignASCII(kEngineProtocol);
	char		*uriCescaped = nsEscape(filePath.get(), url_Path);
	if (!uriCescaped)	return(NS_ERROR_NULL_POINTER);
	searchURL.AppendASCII(uriCescaped);
	nsCRT::free(uriCescaped);

	if ((extensionOffset = searchURL.RFindChar(PRUnichar('.'))) > 0)
	{
		searchURL.Truncate(extensionOffset);
		searchURL.AppendLiteral(".src");
	}

	if (NS_FAILED(rv = gRDFService->GetUnicodeResource(searchURL,
		getter_AddRefs(searchRes))))	return(rv);

	// save the basename reference
	if (!basename.IsEmpty())
	{
		basename.Insert(NS_ConvertASCIItoUTF16(kURINC_SearchCategoryEngineBasenamePrefix), 0);

		if (NS_FAILED(rv = gRDFService->GetUnicodeResource(basename,
			getter_AddRefs(categoryRes))))	return(rv);

		nsCOMPtr<nsIRDFLiteral>	searchLiteral;
		if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(basename.get(),
				getter_AddRefs(searchLiteral))))
		{
			if (file)
			{
				updateAtom(mInner, searchRes, kNC_URL, searchLiteral, nsnull);
			}
		}
	}

	if (!searchRes)		return(NS_ERROR_UNEXPECTED);
	if (!categoryRes)	return(NS_ERROR_UNEXPECTED);

	nsAutoString	iconURL;
	if (icon)
	{
		nsCAutoString iconFileURL;
		if (NS_FAILED(rv = NS_GetURLSpecFromFile(icon, iconFileURL)))
			return(rv);
		AppendUTF8toUTF16(iconFileURL, iconURL);
	}
#ifdef XP_MAC
	else if (file)
	{
		nsCAutoString  fileURL;
		if (NS_FAILED(rv = NS_GetURLSpecFromFile(file,fileURL)))
			return(rv);
		iconURL.AssignLiteral("moz-icon:");
		AppendUTF8toUTF16(fileURL, iconURL);
	}
#endif

	// save icon url (if we have one)
	if (iconURL.Length() > 0)
	{
		nsCOMPtr<nsIRDFLiteral>	iconLiteral;
		if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(iconURL.get(),
				getter_AddRefs(iconLiteral))))
		{
			updateAtom(mInner, searchRes, kNC_Icon, iconLiteral, nsnull);
		}
	}

  if (!isSystemSearchFile)
	{
		// mark our private search files, so that we can distinguish
		// between ours and any that are included with the OS
		updateAtom(mInner, searchRes, kNC_SearchType, kNC_Engine, nsnull);
	}

	if (dataUni != nsnull)
	{
		updateDataHintsInGraph(searchRes, dataUni);

		// finally, if we have a category hint, add this new engine into the category (if it exists)
		if (categoryHint && categoryDataSource)
		{
			nsCOMPtr<nsIRDFLiteral>	catLiteral;
			rv = gRDFService->GetLiteral(categoryHint, getter_AddRefs(catLiteral));

			nsCOMPtr<nsIRDFResource>	catSrc;
			if (catLiteral)
			{
				rv = categoryDataSource->GetSource(kNC_Title, catLiteral,
					PR_TRUE, getter_AddRefs(catSrc));
			}

			const char		*catURI = nsnull;
			if (catSrc)					
			{
				rv = catSrc->GetValueConst(&catURI);
			}

			nsCOMPtr<nsIRDFResource>	catRes;
			if (catURI)
			{
				nsAutoString	catList;
				catList.AssignASCII(kURINC_SearchCategoryPrefix);
				catList.AppendWithConversion(catURI);
				gRDFService->GetUnicodeResource(catList, getter_AddRefs(catRes));
			}

			nsCOMPtr<nsIRDFContainer> container;
			if (catRes)
			{
				rv = nsComponentManager::CreateInstance(kRDFContainerCID,
									nsnull,
									NS_GET_IID(nsIRDFContainer),
									getter_AddRefs(container));
			}
			if (container)
			{
				rv = container->Init(categoryDataSource, catRes);
				if (NS_SUCCEEDED(rv))
				{
					rv = gRDFC->MakeSeq(categoryDataSource, catRes, nsnull);
				}
				if (NS_SUCCEEDED(rv))
				{
					PRInt32		searchIndex = -1;
					if (NS_SUCCEEDED(rv = container->IndexOf(categoryRes, &searchIndex))
						&& (searchIndex < 0))
					{
						rv = container->AppendElement(categoryRes);
					}
				}
				if (NS_SUCCEEDED(rv))
				{
					// flush categoryDataSource
					nsCOMPtr<nsIRDFRemoteDataSource>	remoteCategoryStore;
					remoteCategoryStore = do_QueryInterface(categoryDataSource);
					if (remoteCategoryStore)
					{
						remoteCategoryStore->Flush();
					}
				}
			}
		}
	}

	// Note: add the child relationship last
	PRBool	hasChildFlag = PR_FALSE;
  rv = mInner->HasAssertion(kNC_SearchEngineRoot, kNC_Child, searchRes,
                            PR_TRUE, &hasChildFlag);
  if (NS_SUCCEEDED(rv) && !hasChildFlag)
	{
		mInner->Assert(kNC_SearchEngineRoot, kNC_Child, searchRes, PR_TRUE);
	}

	return(NS_OK);
}

nsresult
InternetSearchDataSource::GetSearchEngineList(nsIFile *searchDir,
              PRBool isSystemSearchFile, PRBool checkMacFileType)
{
        nsresult			rv = NS_OK;

    if (!mInner)
    {
    	return(NS_RDF_NO_VALUE);
    }

    PRBool hasMore = PR_FALSE;
    nsCOMPtr<nsISimpleEnumerator> dirIterator;
    rv = searchDir->GetDirectoryEntries(getter_AddRefs(dirIterator));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFile> dirEntry;
	while ((rv = dirIterator->HasMoreElements(&hasMore)) == NS_OK && hasMore)
	{
		rv = dirIterator->GetNext((nsISupports**)getter_AddRefs(dirEntry));
        if (NS_FAILED(rv))
          continue;

        // Ignore hidden files/directories
        PRBool isHidden;
        rv = dirEntry->IsHidden(&isHidden);
        if (NS_FAILED(rv) || isHidden)
          continue;

        PRBool isDirectory;
        rv = dirEntry->IsDirectory(&isDirectory);
        if (NS_FAILED(rv))
          continue;
        if (isDirectory)
        {
          GetSearchEngineList(dirEntry, isSystemSearchFile, checkMacFileType);
          continue;
        }

        // Skip over empty files
        // Note: use GetFileSize even on Mac [instead of GetFileSizeWithResFork()]
        // as we want the size of ONLY the data fork
        PRInt64 fileSize;
        rv = dirEntry->GetFileSize(&fileSize);
        if (NS_FAILED(rv) || (fileSize == 0))
            continue;
    
        nsAutoString uri;
        rv = dirEntry->GetPath(uri);
        if (NS_FAILED(rv))
        continue;

		PRInt32		len = uri.Length();
		if (len < 5)
		{
			continue;
		}

#ifdef	XP_MAC
    if (checkMacFileType)
		{
            nsCOMPtr<nsILocalFileMac> macFile(do_QueryInterface(dirEntry));
            if (!macFile)
                continue;
            OSType type, creator;
            rv = macFile->GetFileType(&type);
            if (NS_FAILED(rv) || type != 'issp')
                continue;
            rv = macFile->GetFileCreator(&creator);     // Do we really care about creator?  
            if (NS_FAILED(rv) || creator != 'fndf')
                continue;  
		}
#endif

		// check the extension (must be ".src")
		nsAutoString	extension;
		if ((uri.Right(extension, 4) != 4) || (!extension.LowerCaseEqualsLiteral(".src")))
		{
			continue;
		}

		// check for various icons
		PRBool		foundIconFlag = PR_FALSE;
		nsAutoString	temp;
		
		nsCOMPtr<nsILocalFile> iconFile, loopFile;

                static const char *extensions[] = {
                        ".gif",
                        ".jpg",
                        ".jpeg",
                        ".png",
                        nsnull,
                };

                for (int ext_count = 0; extensions[ext_count] != nsnull; ext_count++) {
                        temp = Substring(uri, 0, uri.Length()-4);
                        temp.Append(NS_ConvertASCIItoUCS2(extensions[ext_count]));
                        rv = NS_NewLocalFile(temp, PR_TRUE, getter_AddRefs(loopFile));
                        if (NS_FAILED(rv)) return rv;
                        rv = loopFile->Exists(&foundIconFlag);
                        if (NS_FAILED(rv)) return rv;
                        if (!foundIconFlag) continue;
                        rv = loopFile->IsFile(&foundIconFlag);
                        if (NS_FAILED(rv)) return rv;
                        if (foundIconFlag) 
                        {
                                iconFile = loopFile;
                                break;
                        } 
                }
		
		SaveEngineInfoIntoGraph(dirEntry, iconFile, nsnull, nsnull, isSystemSearchFile, checkMacFileType);
	}

#ifdef MOZ_PHOENIX
    if (!gReorderedEngineList)
      ReorderEngineList();
#endif
	return(rv);
}

nsresult
InternetSearchDataSource::ReadFileContents(nsILocalFile *localFile, nsString& sourceContents)
{
	nsresult			rv = NS_ERROR_FAILURE;
	PRInt64				contentsLen, total = 0;
	char				*contents;

        NS_ENSURE_ARG_POINTER(localFile);

        sourceContents.Truncate();

        rv = localFile->GetFileSize(&contentsLen);
        if (NS_FAILED(rv)) return rv;
        if (contentsLen > 0)
        {
                contents = new char [contentsLen + 1];
                if (contents)
                {
                        nsCOMPtr<nsIInputStream> inputStream;
                        rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), localFile);
                        if (NS_FAILED(rv)) {
                        	delete [] contents;
                        	return rv;
                        }
                        PRUint32 howMany;
                        while (total < contentsLen) {
                                rv = inputStream->Read(contents+total, 
                                                       PRUint32(contentsLen),
                                                       &howMany);
                                if (NS_FAILED(rv)) {
                                        delete [] contents;
                                        return rv;
                                }
                                total += howMany;
                        }
                        if (total == contentsLen)
		        {
				contents[contentsLen] = '\0';
				sourceContents.AssignWithConversion(contents, contentsLen);
				rv = NS_OK;
		        }
	        	delete [] contents;
	        	contents = nsnull;
		}
	}
	return(rv);
}



nsresult
InternetSearchDataSource::GetNumInterpretSections(const PRUnichar *dataUni, PRUint32 &numInterpretSections)
{
	numInterpretSections = 0;

	nsString	buffer(dataUni);

	NS_NAMED_LITERAL_STRING(section, "<interpret");
	PRBool		inSection = PR_FALSE;

	while(!buffer.IsEmpty())
	{
		PRInt32 eol = buffer.FindCharInSet("\r\n", 0);
		if (eol < 0)	break;
		nsAutoString	line;
		if (eol > 0)
		{
			buffer.Left(line, eol);
		}
		buffer.Cut(0, eol+1);
		if (line.IsEmpty())	continue;		// skip empty lines
		if (line[0] == PRUnichar('#'))	continue;	// skip comments
		line.Trim(" \t");
    if (!inSection)
		{
			PRInt32	sectionOffset = nsString_Find(section, line, PR_TRUE);
			if (sectionOffset < 0)	continue;
			line.Cut(0, sectionOffset + section.Length() + 1);
			inSection = PR_TRUE;
			++numInterpretSections;			// increment # of sections
		}
		line.Trim(" \t");
		PRInt32	len = line.Length();
		if (len > 0)
		{
			if (line[len-1] == PRUnichar('>'))
			{
				inSection = PR_FALSE;
				line.SetLength(len-1);
			}
		}
	}
	return(NS_OK);
}


nsresult
InternetSearchDataSource::GetData(const PRUnichar *dataUni, const char *sectionToFind, PRUint32 sectionNum,
				  const char *attribToFind, nsString &value)
{
	nsString	buffer(dataUni);

	nsresult	rv = NS_RDF_NO_VALUE;
	PRBool		inSection = PR_FALSE;

	nsAutoString	section;
	section.AssignLiteral("<");
	section.AppendWithConversion(sectionToFind);

	while(!buffer.IsEmpty())
	{
		PRInt32 eol = buffer.FindCharInSet("\r\n", 0);
		if (eol < 0)	break;
		nsAutoString	line;
		if (eol > 0)
		{
			buffer.Left(line, eol);
		}
		buffer.Cut(0, eol+1);
		if (line.IsEmpty())	continue;		// skip empty lines
		if (line[0] == PRUnichar('#'))	continue;	// skip comments
		line.Trim(" \t");
    if (!inSection)
		{
			PRInt32	sectionOffset = nsString_Find(section, line, PR_TRUE);
			if (sectionOffset < 0)	continue;
			if (sectionNum > 0)
			{
				--sectionNum;
				continue;
			}
			line.Cut(0, sectionOffset + section.Length() + 1);
			inSection = PR_TRUE;
		}
		line.Trim(" \t");
		PRInt32	len = line.Length();
		if (len > 0)
		{
			if (line[len-1] == PRUnichar('>'))
			{
				inSection = PR_FALSE;
				line.SetLength(len-1);
			}
		}
		PRInt32 equal = line.FindChar(PRUnichar('='));
		if (equal < 0)	continue;			// skip lines with no equality
		
		nsAutoString	attrib;
		if (equal > 0)
		{
			line.Left(attrib, equal /* - 1 */);
		}
		attrib.Trim(" \t");
		if (attrib.EqualsIgnoreCase(attribToFind))
		{
			line.Cut(0, equal+1);
			line.Trim(" \t");
			value = line;

			// strip of any enclosing quotes and trailing comments
			if ((value[0] == PRUnichar('\"')) || (value[0] == PRUnichar('\'')))
			{
				PRUnichar quoteChar = value[0];
				value.Cut(0,1);
				if (!value.IsEmpty())
				{
					PRInt32 quoteEnd = value.FindChar(quoteChar);
					if (quoteEnd >= 0)
					{
						value.Truncate(quoteEnd);
					}
				}
			}
			else
			{
				PRInt32 commentOffset = value.FindCharInSet("# \t", 0);
				if (commentOffset >= 0)
				{
					value.Truncate(commentOffset);
				}
				value.Trim(" \t");
			}
			rv = NS_OK;
			break;
		}
	}
	return(rv);
}



nsresult
InternetSearchDataSource::GetInputs(const PRUnichar *dataUni, nsString &engineName, nsString &userVar,
  const nsString &text, nsString &input, PRInt16 direction, PRUint16 pageNumber,  PRUint16 *whichButtons)
{
	nsString	buffer(dataUni);

	nsresult	rv = NS_OK;
	PRBool		inSection = PR_FALSE;
  PRBool        inDirInput; // directional input: "inputnext" or "inputprev"

	while(!buffer.IsEmpty())
	{
		PRInt32 eol = buffer.FindCharInSet("\r\n", 0);
		if (eol < 0)	break;
		nsAutoString	line;
		if (eol > 0)
		{
			buffer.Left(line, eol);
		}
		buffer.Cut(0, eol+1);
		if (line.IsEmpty())	continue;		// skip empty lines
		if (line[0] == PRUnichar('#'))	continue;	// skip comments
		line.Trim(" \t");
    if (!inSection)
		{
			if (line[0] != PRUnichar('<'))	continue;
			line.Cut(0, 1);
			inSection = PR_TRUE;
		}
		PRInt32	len = line.Length();
		if (len > 0)
		{
			if (line[len-1] == PRUnichar('>'))
			{
				inSection = PR_FALSE;
				line.SetLength(len-1);
			}
		}
    if (inSection)
      continue;

		// look for inputs
		if (line.Find("input", PR_TRUE) == 0)
		{
      line.Cut(0, 5);

      // look for "inputnext" or "inputprev"
      inDirInput = PR_FALSE;

      if (line.Find("next", PR_TRUE) == 0)
      {
        inDirInput = PR_TRUE;
        if (whichButtons)
          *whichButtons |= kHaveNext;
      }

      if (line.Find("prev", PR_TRUE) == 0)
      {
        inDirInput = PR_TRUE;
        if (whichButtons)
          *whichButtons |= kHavePrev;
      }

      if (inDirInput)
        line.Cut(0, 4);
      
			line.Trim(" \t");
			
			// first look for name attribute
			nsAutoString	nameAttrib;

			PRInt32	nameOffset = line.Find("name", PR_TRUE);
			if (nameOffset >= 0)
			{
				PRInt32 equal = line.FindChar(PRUnichar('='), nameOffset);
				if (equal >= 0)
				{
					PRInt32	startQuote = line.FindChar(PRUnichar('\"'), equal + 1);
					if (startQuote >= 0)
					{
						PRInt32	endQuote = line.FindChar(PRUnichar('\"'), startQuote + 1);
						if (endQuote > 0)
						{
							line.Mid(nameAttrib, startQuote+1, endQuote-startQuote-1);
							line.Cut(0, endQuote + 1);
						}
					}
					else
					{
						nameAttrib = line;
						nameAttrib.Cut(0, equal+1);
						nameAttrib.Trim(" \t");
						PRInt32 space = nameAttrib.FindCharInSet(" \t", 0);
						if (space > 0)
						{
							nameAttrib.Truncate(space);
							line.Cut(0, equal+1+space);
						}
						else
						{
							line.Truncate();
						}
					}
				}
			}
			if (nameAttrib.IsEmpty())	continue;

			// first look for value attribute
			nsAutoString	valueAttrib;

      PRInt32  valueOffset;
      if (!inDirInput)
        valueOffset = line.Find("value", PR_TRUE);
      else
        valueOffset = line.Find("factor", PR_TRUE);
			if (valueOffset >= 0)
			{
				PRInt32 equal = line.FindChar(PRUnichar('='), valueOffset);
				if (equal >= 0)
				{
					PRInt32	startQuote = line.FindChar(PRUnichar('\"'), equal + 1);
					if (startQuote >= 0)
					{
						PRInt32	endQuote = line.FindChar(PRUnichar('\"'), startQuote + 1);
						if (endQuote >= 0)
						{
							line.Mid(valueAttrib, startQuote+1, endQuote-startQuote-1);
						}
					}
					else
					{
						// if value attribute's "value" isn't quoted, get the first word... ?
						valueAttrib = line;
						valueAttrib.Cut(0, equal+1);
						valueAttrib.Trim(" \t");
						PRInt32 space = valueAttrib.FindCharInSet(" \t>", 0);
						if (space > 0)
						{
							valueAttrib.Truncate(space);
						}
					}
				}
			}
			else if (line.Find("user", PR_TRUE) >= 0)
			{
				userVar = nameAttrib;
				valueAttrib.Assign(text);
			}
			
			// XXX should ignore if  mode=browser  is specified
			// XXX need to do this better
			if (line.RFind("mode=browser", PR_TRUE) >= 0)
				continue;

			if (!nameAttrib.IsEmpty() && !valueAttrib.IsEmpty())
			{
				if (!input.IsEmpty())
				{
					input.AppendLiteral("&");
				}
				input += nameAttrib;
        input.AppendLiteral("=");
        if (!inDirInput)
				input += valueAttrib;
        else
          input.AppendInt( computeIndex(valueAttrib, pageNumber, direction) );
			}
		}
	}

  // Now add the default vs. non-default parameters, which come from 
  // preferences and are part of the pre-configuration.
  nsCOMPtr<nsIPrefService> pserv(do_QueryInterface(prefs));
  if (pserv) 
  {
    // If the pref "browser.search.defaultenginename" has a user value, that
    // means the user has changed the engine in the list. This implies that
    // the selected engine was _not_ configured as the default by the 
    // distributor, because if the value of the pref matched, prefHasUserValue
    // would return false. 
    PRBool engineIsNotDefault = PR_FALSE;
    nsCOMPtr<nsIPrefBranch> rootBranch(do_QueryInterface(pserv));
    nsCOMPtr<nsIPrefBranch> defaultBranch;
    pserv->GetDefaultBranch("", getter_AddRefs(defaultBranch));

    if (defaultBranch) 
    {
      nsXPIDLString defaultEngineNameStr;
      nsCOMPtr<nsIPrefLocalizedString> defaultEngineName;
      rv = defaultBranch->GetComplexValue("browser.search.defaultenginename", 
                                          NS_GET_IID(nsIPrefLocalizedString),
                                          getter_AddRefs(defaultEngineName));
      defaultEngineName->GetData(getter_Copies(defaultEngineNameStr));

      nsXPIDLString selectedEngineNameStr;
      nsCOMPtr<nsIPrefLocalizedString> selectedEngineName;
      rv = rootBranch->GetComplexValue("browser.search.selectedEngine", 
                                       NS_GET_IID(nsIPrefLocalizedString),
                                       getter_AddRefs(selectedEngineName));
      if (selectedEngineName) {
        selectedEngineName->GetData(getter_Copies(selectedEngineNameStr));
        engineIsNotDefault = !defaultEngineNameStr.Equals(selectedEngineNameStr);
      }
      else {
        engineIsNotDefault = PR_FALSE; // The selected engine *is* the default
                                       // since the user has not changed the
                                       // selected item in the list causing
                                       // the selectedEngine pref to be set.
      }
    }

    PRInt32 i = 0;
    char prefNameBuf[1096];
    do
    {
      ++i;
      sprintf(prefNameBuf, "browser.search.param.%s.%d.", 
              NS_ConvertUCS2toUTF8(engineName).get(), i);

      nsCOMPtr<nsIPrefBranch> pb;
      rv = pserv->GetBranch(prefNameBuf, getter_AddRefs(pb));
      if (NS_FAILED(rv)) 
        break;

      nsCOMPtr<nsIPrefLocalizedString> parameter;
      nsXPIDLString parameterStr;
      rv = pb->GetComplexValue(engineIsNotDefault ? "custom" : "default", 
                               NS_GET_IID(nsIPrefLocalizedString), 
                               getter_AddRefs(parameter));
      if (NS_FAILED(rv))
        break;

      parameter->GetData(getter_Copies(parameterStr));
      
      if (!parameterStr.IsEmpty()) 
      {
        if (!input.IsEmpty())
          input.Append(NS_LITERAL_STRING("&"));
        input += parameterStr;
      }
    }
    while (1);

    // Now add the release identifier
    nsCOMPtr<nsIStringBundleService> stringService(do_GetService(NS_STRINGBUNDLE_CONTRACTID));
    nsCOMPtr<nsIStringBundle> bundle;
    rv = stringService->CreateBundle(SEARCHCONFIG_PROPERTIES, getter_AddRefs(bundle));
    nsCOMPtr<nsIStringBundle> intlBundle;
    rv = stringService->CreateBundle(INTL_PROPERTIES, getter_AddRefs(intlBundle));

    nsXPIDLString langName;
    intlBundle->GetStringFromName(NS_LITERAL_STRING("general.useragent.locale").get(), 
                                  getter_Copies(langName));

    nsAutoString keyTemplate(NS_LITERAL_STRING("browser.search.param."));
    keyTemplate += engineName;
    keyTemplate.Append(NS_LITERAL_STRING(".release"));

    nsXPIDLString releaseValue;
    const PRUnichar* strings[] = { langName.get() };
    bundle->FormatStringFromName(keyTemplate.get(), strings, 1, getter_Copies(releaseValue));

    if (!releaseValue.IsEmpty()) 
    {
      if (!input.IsEmpty())
        input.Append(NS_LITERAL_STRING("&"));
      input += releaseValue;
    }

    // Now add order parameters.
    nsCOMPtr<nsIPrefBranch> pb;
    rv = pserv->GetBranch("", getter_AddRefs(pb));
    if (NS_FAILED(rv)) return rv;

    i = 0;
    do {
      ++i;
      sprintf(prefNameBuf, "browser.search.order.%d", i);

      nsCOMPtr<nsIPrefLocalizedString> orderEngineName;
      rv = pb->GetComplexValue(prefNameBuf, 
                               NS_GET_IID(nsIPrefLocalizedString),
                               getter_AddRefs(orderEngineName));
      if (NS_FAILED(rv)) 
        break;

      nsXPIDLString orderEngineNameStr;
      orderEngineName->GetData(getter_Copies(orderEngineNameStr));
      if (orderEngineNameStr.Equals(engineName))
        break;
    }
    while (PR_TRUE);

    if (NS_SUCCEEDED(rv))
    {
      sprintf(prefNameBuf, "browser.search.order.%s.%d",
              NS_ConvertUCS2toUTF8(engineName).get(), i);
      
      nsCOMPtr<nsIPrefLocalizedString> orderParam;
      rv = rootBranch->GetComplexValue(prefNameBuf, 
                                       NS_GET_IID(nsIPrefLocalizedString),
                                       getter_AddRefs(orderParam));
      if (NS_FAILED(rv))
      {
        sprintf(prefNameBuf, "browser.search.order.%s",
                NS_ConvertUCS2toUTF8(engineName).get());
        rv = rootBranch->GetComplexValue(prefNameBuf, 
                                         NS_GET_IID(nsIPrefLocalizedString),
                                         getter_AddRefs(orderParam));
      }
    
      if (NS_SUCCEEDED(rv)) 
      {
        nsXPIDLString orderParamStr;
        orderParam->GetData(getter_Copies(orderParamStr));

        if (!orderParamStr.IsEmpty())
        {
          if (!input.IsEmpty())
            input.Append(NS_LITERAL_STRING("&"));
          input += orderParamStr;
        }
      }
    }

    rv = NS_OK;
  }

	return(rv);
}

PRInt32
InternetSearchDataSource::computeIndex(nsAutoString &factor, 
                                       PRUint16 page, PRInt16 direction)
{
  // XXX get page
  PRInt32 errorCode, index = 0;
  PRInt32 factorInt = factor.ToInteger(&errorCode);
  
  if (NS_SUCCEEDED(errorCode))
  {
    // if factor is garbled assume 10
    if (factorInt <= 0)
      factorInt = 10;

    if (direction < 0)
    {
      // don't pass back a negative index!
      if (0 <= (page - 1))
        --page;
    }
    index = factorInt * page;
  }

  return index;
}



nsresult
InternetSearchDataSource::GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult)
{
        const char	*uri = nsnull;
	source->GetValueConst( &uri );
	nsAutoString	url;
	url.AssignWithConversion(uri);
	nsIRDFLiteral	*literal;
	gRDFService->GetLiteral(url.get(), &literal);
        *aResult = literal;
        return NS_OK;
}



// stream observer methods



NS_IMETHODIMP
InternetSearchDataSource::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
#ifdef	DEBUG_SEARCH_OUTPUT
	printf("InternetSearchDataSourceCallback::OnStartRequest entered.\n");
#endif
	return(NS_OK);
}



NS_IMETHODIMP
InternetSearchDataSource::OnDataAvailable(nsIRequest *request, nsISupports *ctxt,
				nsIInputStream *aIStream, PRUint32 sourceOffset, PRUint32 aLength)
{
	if (!ctxt)	return(NS_ERROR_NO_INTERFACE);
	nsCOMPtr<nsIInternetSearchContext> context (do_QueryInterface(ctxt));
	if (!context)	return(NS_ERROR_NO_INTERFACE);

	nsresult	rv = NS_OK;

	if (aLength < 1)	return(rv);

	PRUint32	count;
	char		*buffer = new char[ aLength ];
	if (!buffer)	return(NS_ERROR_OUT_OF_MEMORY);

	if (NS_FAILED(rv = aIStream->Read(buffer, aLength, &count)) || count == 0)
	{
#ifdef	DEBUG
		printf("Search datasource read failure.\n");
#endif
		delete []buffer;
		return(rv);
	}
	if (count != aLength)
	{
#ifdef	DEBUG
		printf("Search datasource read # of bytes failure.\n");
#endif
		delete []buffer;
		return(NS_ERROR_UNEXPECTED);
	}

	nsCOMPtr<nsIUnicodeDecoder>	decoder;
	context->GetUnicodeDecoder(getter_AddRefs(decoder));
	if (decoder)
	{
		char			*aBuffer = buffer;
		PRInt32			unicharBufLen = 0;
		decoder->GetMaxLength(aBuffer, aLength, &unicharBufLen);
		PRUnichar		*unichars = new PRUnichar [ unicharBufLen+1 ];
		do
		{
			PRInt32		srcLength = aLength;
			PRInt32		unicharLength = unicharBufLen;
			rv = decoder->Convert(aBuffer, &srcLength, unichars, &unicharLength);
			unichars[unicharLength]=0;  //add this since the unicode converters can't be trusted to do so.

			// Move the nsParser.cpp 00 -> space hack to here so it won't break UCS2 file

			// Hack Start
			for(PRInt32 i=0;i < unicharLength; i++)
			{
				if(0x0000 == unichars[i])
				{
					unichars[i] = PRUnichar(' ');
				}
			}
			// Hack End

			context->AppendUnicodeBytes(unichars, unicharLength);
			// if we failed, we consume one byte by replace it with U+FFFD
			// and try conversion again.
			if(NS_FAILED(rv))
			{
				decoder->Reset();
				unsigned char	smallBuf[2];
				smallBuf[0] = 0xFF;
				smallBuf[1] = 0xFD;
				context->AppendBytes( (const char *)&smallBuf, 2L);
				if(((PRUint32) (srcLength + 1)) > aLength)
					srcLength = aLength;
				else 
					srcLength++;
				aBuffer += srcLength;
				aLength -= srcLength;
			}
		} while (NS_FAILED(rv) && (aLength > 0));
		delete [] unichars;
		unichars = nsnull;
	}
	else
	{
		context->AppendBytes(buffer, aLength);
	}

	delete [] buffer;
	buffer = nsnull;
	return(rv);
}



NS_IMETHODIMP
InternetSearchDataSource::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                                        nsresult status)
{
	if (!mInner)	return(NS_OK);

  nsCOMPtr<nsIChannel> channel (do_QueryInterface(request));
	nsCOMPtr<nsIInternetSearchContext> context (do_QueryInterface(ctxt));
	if (!ctxt)	return(NS_ERROR_NO_INTERFACE);

	nsresult	rv;
	PRUint32	contextType = 0;
	if (NS_FAILED(rv = context->GetContextType(&contextType)))		return(rv);

	if (contextType == nsIInternetSearchContext::WEB_SEARCH_CONTEXT)
	{
		// continue to process nsIInternetSearchContext::WEB_SEARCH_CONTEXT
		rv = webSearchFinalize(channel, context);
	}
	else if (contextType == nsIInternetSearchContext::ENGINE_DOWNLOAD_CONTEXT ||
		 contextType == nsIInternetSearchContext::ICON_DOWNLOAD_CONTEXT)
	{
		nsCOMPtr<nsIHttpChannel> httpChannel (do_QueryInterface(channel));
		if (!httpChannel)	return(NS_ERROR_UNEXPECTED);

		// check HTTP status to ensure success
		PRUint32	httpStatus = 0;
		if (NS_SUCCEEDED(rv = httpChannel->GetResponseStatus(&httpStatus)) &&
			(httpStatus == 200))
		{
			rv = saveContents(channel, context, contextType);
		}
	}
	else if (contextType == nsIInternetSearchContext::ENGINE_UPDATE_CONTEXT)
	{
		nsCOMPtr<nsIRDFResource>	theEngine;
		if (NS_FAILED(rv = context->GetEngine(getter_AddRefs(theEngine))))	return(rv);
		if (!theEngine)	return(NS_ERROR_NO_INTERFACE);

#ifdef	DEBUG_SEARCH_UPDATES
		const char	*engineURI = nsnull;
		theEngine->GetValueConst(&engineURI);
#endif

		// free up "busy" info now & mark as non-busy
		busySchedule = PR_FALSE;
		busyResource = nsnull;

		// we only have HTTP "HEAD" information when doing updates
		nsCOMPtr<nsIHttpChannel> httpChannel (do_QueryInterface(channel));
		if (!httpChannel)	return(NS_ERROR_UNEXPECTED);

		// check HTTP status to ensure success
		PRUint32	httpStatus = 0;
		if (NS_FAILED(rv = httpChannel->GetResponseStatus(&httpStatus)))
			return(rv);
		if (httpStatus != 200)	return(NS_ERROR_UNEXPECTED);

		// get last-modified & content-length info
		nsCAutoString			lastModValue, contentLengthValue;

        if (NS_FAILED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Last-Modified"), lastModValue)))
            lastModValue.Truncate();
        if (NS_FAILED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Content-Length"), contentLengthValue)))
            contentLengthValue.Truncate();

		// should we fetch the entire file?
		PRBool		updateSearchEngineFile = PR_FALSE;

		// save out new search engine state data into localstore
		PRBool			tempDirty = PR_FALSE;
		nsCOMPtr<nsIRDFLiteral>	newValue;
		if (!lastModValue.IsEmpty())
		{
			gRDFService->GetLiteral(NS_ConvertASCIItoUCS2(lastModValue).get(),
				getter_AddRefs(newValue));
			if (newValue)
			{
				updateAtom(mLocalstore, theEngine, kWEB_LastPingModDate, newValue,
					&tempDirty);
        if (tempDirty)
          updateSearchEngineFile = PR_TRUE;
			}
		}
		if (!contentLengthValue.IsEmpty())
		{
			gRDFService->GetLiteral(NS_ConvertASCIItoUCS2(contentLengthValue).get(),
				getter_AddRefs(newValue));
			if (newValue)
			{
				updateAtom(mLocalstore, theEngine, kWEB_LastPingContentLen, newValue,
					&tempDirty);
        if (tempDirty)
				{
					updateSearchEngineFile = PR_TRUE;
				}
				else
				{
					// worst case: compare local data size vs. remote content size
					nsCOMPtr<nsIRDFLiteral>	dataLit;
					if (NS_SUCCEEDED(rv = FindData(theEngine, getter_AddRefs(dataLit))) &&
						(rv != NS_RDF_NO_VALUE))
					{
						const PRUnichar	*dataUni = nsnull;
						dataLit->GetValueConst(&dataUni);
						nsAutoString	dataStr(dataUni);
						PRInt32		dataLen=dataStr.Length();
#ifdef	DEBUG_SEARCH_UPDATES
						printf("    Search engine='%s' data length='%d'\n", engineURI, dataLen);
#endif
						PRInt32	contentLen=0, err=0;
						contentLen = contentLengthValue.ToInteger(&err);
						if ((!err) && (dataLen != contentLen))
						{
#ifdef	DEBUG_SEARCH_UPDATES
							printf("    Search engine '%s' data length != remote content length, so update\n",
								engineURI, dataLen);
#endif
							updateSearchEngineFile = PR_TRUE;
						}
					}
				}
			}
		}

		// mark now as the last time we stat'ted the search engine
		validateEngineNow(theEngine);

    if (updateSearchEngineFile)
		{
#ifdef	DEBUG_SEARCH_UPDATES
			printf("    Search engine='%s' needs updating, so fetching it\n", engineURI);
#endif
			// get update URL
			nsCString		updateURL;
			nsCOMPtr<nsIRDFNode>	aNode;
			if (NS_SUCCEEDED(rv = mInner->GetTarget(theEngine, kNC_Update, PR_TRUE, getter_AddRefs(aNode)))
				&& (rv != NS_RDF_NO_VALUE))
			{
				nsCOMPtr<nsIRDFLiteral>	aLiteral (do_QueryInterface(aNode));
				if (aLiteral)
				{
					const PRUnichar	*updateUni = nsnull;
					aLiteral->GetValueConst(&updateUni);
					if (updateUni)
					{
						updateURL.AssignWithConversion(updateUni);
					}
				}
			}

			// get update icon
			nsCString		updateIconURL;
			if (NS_SUCCEEDED(rv = mInner->GetTarget(theEngine, kNC_UpdateIcon, PR_TRUE, getter_AddRefs(aNode)))
				&& (rv != NS_RDF_NO_VALUE))
			{
				nsCOMPtr<nsIRDFLiteral>	aIconLiteral (do_QueryInterface(aNode));
				if (aIconLiteral)
				{
					const PRUnichar	*updateIconUni = nsnull;
					aIconLiteral->GetValueConst(&updateIconUni);
					if (updateIconUni)
					{
						updateIconURL.AssignWithConversion(updateIconUni);
					}
				}
			}

			// download it!
			AddSearchEngine(updateURL.get(), updateIconURL.get(), nsnull, nsnull);
		}
		else
		{
#ifdef	DEBUG_SEARCH_UPDATES
			printf("    Search engine='%s' is current and requires no updating\n", engineURI);
#endif
		}
	}
	else
	{
		rv = NS_ERROR_UNEXPECTED;
	}
	return(rv);
}



nsresult
InternetSearchDataSource::validateEngineNow(nsIRDFResource *engine)
{
	// get the current date/time [from microseconds (PRTime) to seconds]
	// also, need to convert from 64 bites to 32 bites as we don't have an easy
	// way to convert from 64 bit number to a string like we do for 32 bit numbers
	PRTime		now64 = PR_Now(), temp64, million;
	LL_I2L(million, PR_USEC_PER_SEC);
	LL_DIV(temp64, now64, million);
	PRInt32		now32;
	LL_L2I(now32, temp64);

	// validate this engine as of now (save as string
	// literal as that's all RDF can currently serialize)
	nsAutoString	nowStr;
	nowStr.AppendInt(now32);

	nsresult		rv;
	nsCOMPtr<nsIRDFLiteral>	nowLiteral;
	if (NS_FAILED(rv = gRDFService->GetLiteral(nowStr.get(),
			getter_AddRefs(nowLiteral))))	return(rv);
	updateAtom(mLocalstore, engine, kWEB_LastPingDate, nowLiteral, nsnull);

	// flush localstore
	nsCOMPtr<nsIRDFRemoteDataSource> remoteLocalStore (do_QueryInterface(mLocalstore));
	if (remoteLocalStore)
	{
		remoteLocalStore->Flush();
	}
	return(NS_OK);
}



nsresult
InternetSearchDataSource::webSearchFinalize(nsIChannel* channel, nsIInternetSearchContext *context)
{
	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	mParent;
	if (NS_FAILED(rv = context->GetParent(getter_AddRefs(mParent))))	return(rv);
//	Note: mParent can be null
//	if (!mParent)	return(NS_ERROR_NO_INTERFACE);

	nsCOMPtr<nsIRDFResource>	mEngine;
	if (NS_FAILED(rv = context->GetEngine(getter_AddRefs(mEngine))))	return(rv);
	if (!mEngine)	return(NS_ERROR_NO_INTERFACE);

	nsCOMPtr<nsIURI> aURL;
	rv = channel->GetURI(getter_AddRefs(aURL));
	if (NS_FAILED(rv)) return rv;

	// copy the engine's icon reference (if it has one) onto the result node
	nsCOMPtr<nsIRDFNode>		engineIconStatusNode = nsnull;
	mInner->GetTarget(mEngine, kNC_Icon, PR_TRUE, getter_AddRefs(engineIconStatusNode));
	if (engineIconStatusNode)
	{
		rv = mInner->Assert(mEngine, kNC_StatusIcon, engineIconStatusNode, PR_TRUE);
	}

	const PRUnichar	*uniBuf = nsnull;
	if (NS_SUCCEEDED(rv = context->GetBufferConst(&uniBuf)) && (uniBuf))
	{
		if (mParent && (gBrowserSearchMode>0))
		{
			// save HTML result page for this engine
			nsCOMPtr<nsIRDFLiteral>	htmlLiteral;
			if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(uniBuf, getter_AddRefs(htmlLiteral))))
			{
				rv = mInner->Assert(mEngine, kNC_HTML, htmlLiteral, PR_TRUE);
			}
		}

		// parse up HTML results
		PRInt32 uniBufLen = 0L;
		if (NS_SUCCEEDED(rv = context->GetBufferLength(&uniBufLen)))
		{
			rv = ParseHTML(aURL, mParent, mEngine, uniBuf, uniBufLen);
		}
	}
	else
	{
#ifdef	DEBUG_SEARCH_OUTPUT
		printf(" *** InternetSearchDataSourceCallback::OnStopRequest:  no data.\n\n");
#endif
	}

	// after we're all done with the html buffer, get rid of it
	context->Truncate();

	// (do this last) potentially remove the loading attribute
	mInner->Unassert(mEngine, kNC_loading, kTrueLiteral);

	if (mLoadGroup)
	{
		PRUint32	count = 0;
		if (NS_SUCCEEDED(rv = mLoadGroup->GetActiveCount(&count)))
		{
			// is this the last connection in the loadgroup?
			if (count <= 1)
			{
				Stop();
			}
		}
	}

	return(NS_OK);
}



nsresult
InternetSearchDataSource::ParseHTML(nsIURI *aURL, nsIRDFResource *mParent,
				nsIRDFResource *mEngine, const PRUnichar *htmlPage, PRInt32 htmlPageSize)
{
	// get data out of graph
	nsresult	rv;
	nsCOMPtr<nsIRDFNode>	dataNode;
	if (NS_FAILED(rv = mInner->GetTarget(mEngine, kNC_Data, PR_TRUE, getter_AddRefs(dataNode))))
	{
		return(rv);
	}
	nsCOMPtr<nsIRDFLiteral>	dataLiteral (do_QueryInterface(dataNode));
	if (!dataLiteral)	return(NS_ERROR_NULL_POINTER);

	const PRUnichar	*dataUni = nsnull;
	if (NS_FAILED(rv = dataLiteral->GetValueConst(&dataUni)))
		return(rv);
	if (!dataUni)	return(NS_ERROR_NULL_POINTER);

	// get name of this engine once
	nsAutoString	engineStr;
	GetData(dataUni, "search", 0, "name", engineStr);

	// pre-compute host (we might discard URLs that match this)
	nsCAutoString hostName;
	aURL->GetAsciiHost(hostName);

	// pre-compute server path (we might discard URLs that match this)
	nsAutoString	serverPathStr;
	nsCAutoString serverPath;
	aURL->GetPath(serverPath);
	if (!serverPath.IsEmpty())
	{
        AppendUTF8toUTF16(serverPath, serverPathStr);
        serverPath.Truncate();

		PRInt32 serverOptionsOffset = serverPathStr.FindChar(PRUnichar('?'));
		if (serverOptionsOffset >= 0)	serverPathStr.Truncate(serverOptionsOffset);
	}

	PRBool		hasPriceFlag = PR_FALSE, hasAvailabilityFlag = PR_FALSE, hasRelevanceFlag = PR_FALSE;
	PRBool		hasDateFlag = PR_FALSE;
	PRBool		skipLocalFlag = PR_FALSE, useAllHREFsFlag = PR_FALSE;
	PRInt32		pageRank = 1;
	PRUint32	numInterpretSections, numResults = 0;

	GetNumInterpretSections(dataUni, numInterpretSections);
	if (numInterpretSections < 1)
	{
		// if no <interpret> sections, use all the HREFS on the page
		numInterpretSections = 1;
		useAllHREFsFlag = PR_TRUE;
		skipLocalFlag = PR_TRUE;
	}

#ifdef	DEBUG
	PRTime		now;
#ifdef	XP_MAC
	Microseconds((UnsignedWide *)&now);
#else
	now = PR_Now();
#endif
       printf("\nStart processing search results:   %u bytes \n", htmlPageSize); 
#endif

	// need to handle multiple <interpret> sections, per spec
	for (PRUint32 interpretSectionNum=0; interpretSectionNum < numInterpretSections; interpretSectionNum++)
	{
		nsAutoString	resultListStartStr, resultListEndStr;
		nsAutoString	resultItemStartStr, resultItemEndStr;
		nsAutoString	relevanceStartStr, relevanceEndStr;
		nsAutoString	bannerStartStr, bannerEndStr, skiplocalStr;
		nsAutoString	priceStartStr, priceEndStr, availStartStr, availEndStr;
		nsAutoString	dateStartStr, dateEndStr;
		nsAutoString	nameStartStr, nameEndStr;
		nsAutoString	emailStartStr, emailEndStr;
		nsAutoString	browserResultTypeStr;
		browserResultTypeStr.AssignLiteral("result");		// default to "result"

		// use a nsDependentString so that "htmlPage" data isn't copied
		nsDependentString  htmlResults(htmlPage, htmlPageSize);
		PRUint32           startIndex = 0L, stopIndex = htmlPageSize;

    if (!useAllHREFsFlag)
		{
			GetData(dataUni, "interpret", interpretSectionNum, "resultListStart", resultListStartStr);
			GetData(dataUni, "interpret", interpretSectionNum, "resultListEnd", resultListEndStr);
			GetData(dataUni, "interpret", interpretSectionNum, "resultItemStart", resultItemStartStr);
			GetData(dataUni, "interpret", interpretSectionNum, "resultItemEnd", resultItemEndStr);
			GetData(dataUni, "interpret", interpretSectionNum, "relevanceStart", relevanceStartStr);
			GetData(dataUni, "interpret", interpretSectionNum, "relevanceEnd", relevanceEndStr);
			GetData(dataUni, "interpret", interpretSectionNum, "bannerStart", bannerStartStr);
			GetData(dataUni, "interpret", interpretSectionNum, "bannerEnd", bannerEndStr);
			GetData(dataUni, "interpret", interpretSectionNum, "skiplocal", skiplocalStr);
			skipLocalFlag = (skiplocalStr.LowerCaseEqualsLiteral("true")) ? PR_TRUE : PR_FALSE;

			// shopping channel support
			GetData(dataUni, "interpret", interpretSectionNum, "priceStart", priceStartStr);
			GetData(dataUni, "interpret", interpretSectionNum, "priceEnd", priceEndStr);
			GetData(dataUni, "interpret", interpretSectionNum, "availStart", availStartStr);
			GetData(dataUni, "interpret", interpretSectionNum, "availEnd", availEndStr);

			// news channel support
			GetData(dataUni, "interpret", interpretSectionNum, "dateStart", dateStartStr);
			GetData(dataUni, "interpret", interpretSectionNum, "dateEnd", dateEndStr);

			// people channel support
			GetData(dataUni, "interpret", interpretSectionNum, "nameStart", nameStartStr);
			GetData(dataUni, "interpret", interpretSectionNum, "nameEnd", nameEndStr);
			GetData(dataUni, "interpret", interpretSectionNum, "emailStart", emailStartStr);
			GetData(dataUni, "interpret", interpretSectionNum, "emailEnd", emailEndStr);

			// special browser support
			GetData(dataUni, "interpret", interpretSectionNum, "browserResultType", browserResultTypeStr);
			if (browserResultTypeStr.IsEmpty())
			{
				browserResultTypeStr.AssignLiteral("result");	// default to "result"
			}
		}

		// look for banner
		nsCOMPtr<nsIRDFLiteral>	bannerLiteral;
		if ((!bannerStartStr.IsEmpty()) && (!bannerEndStr.IsEmpty()))
		{
			PRInt32	bannerStart = nsString_Find(bannerStartStr, htmlResults, PR_TRUE);
			if (bannerStart >= 0)
			{
				startIndex = bannerStart;

				PRInt32	bannerEnd = nsString_Find(bannerEndStr, htmlResults, PR_TRUE, bannerStart + bannerStartStr.Length());
				if (bannerEnd > bannerStart)
				{
					stopIndex = bannerEnd - 1;

					nsAutoString	htmlBanner;
					htmlResults.Mid(htmlBanner, bannerStart, bannerEnd - bannerStart - 1);
					if (!htmlBanner.IsEmpty())
					{
						const PRUnichar	*bannerUni = htmlBanner.get();
						if (bannerUni)
						{
							gRDFService->GetLiteral(bannerUni, getter_AddRefs(bannerLiteral));
						}
					}
				}
			}
		}

		if (!resultListStartStr.IsEmpty())
		{
			PRInt32	resultListStart = nsString_Find(resultListStartStr, htmlResults, PR_TRUE);
			if (resultListStart >= 0)
			{
        startIndex = resultListStart + resultListStartStr.Length();
			}
      else if (!useAllHREFsFlag)
			{
				// if we have multiple <INTERPRET> sections but can't find the startIndex
				// of a result list block, just continue on with the the next block
				continue;
			}
		}
		if (!resultListEndStr.IsEmpty())
		{
			// rjc note: use RFind to find the LAST
			// occurrence of resultListEndStr

		    nsAString::const_iterator originalStart, start, end;
		    htmlResults.BeginReading(start);
		    htmlResults.EndReading(end);
		    originalStart = start;
		    
		    if (RFindInReadable(resultListEndStr, start, end))
			stopIndex = Distance(originalStart, start);
		}

		PRBool	trimItemStart = PR_TRUE;
		PRBool	trimItemEnd = PR_FALSE;		// rjc note: testing shows we should NEVER trim end???

		// if resultItemStartStr is not specified, try making it just be "HREF="
		if (resultItemStartStr.IsEmpty())
		{
			resultItemStartStr.AssignLiteral("HREF=");
			trimItemStart = PR_FALSE;
		}

		// if resultItemEndStr is not specified, try making it the same as resultItemStartStr
		if (resultItemEndStr.IsEmpty())
		{
			resultItemEndStr = resultItemStartStr;
			trimItemEnd = PR_TRUE;
		}

		while(startIndex < stopIndex)
		{
			PRInt32	resultItemStart;
			resultItemStart = nsString_Find(resultItemStartStr, htmlResults, PR_TRUE, startIndex);
			if (resultItemStart < 0)	break;

			PRInt32	resultItemEnd;
      if (trimItemStart)
			{
				resultItemStart += resultItemStartStr.Length();
				resultItemEnd = nsString_Find(resultItemEndStr, htmlResults, PR_TRUE, resultItemStart);
			}
			else
			{
				resultItemEnd = nsString_Find(resultItemEndStr, htmlResults, PR_TRUE, resultItemStart + resultItemStartStr.Length());
			}

			if (resultItemEnd < 0)
			{
				resultItemEnd = stopIndex;
			}
      else if (!trimItemEnd && resultItemEnd >= 0)
			{
				resultItemEnd += resultItemEndStr.Length();
			}

			// forced to use an nsAutoString (which copies)
			// as CBufDescriptor doesn't guarantee null terminator
			if (resultItemEnd - resultItemStart - 1 <= 0)    break;
			nsAutoString resultItem(&htmlPage[resultItemStart],
				resultItemEnd - resultItemStart - 1);

			if (resultItem.IsEmpty())	break;

			// advance startIndex here so that, from this point onward, "continue" can be used
			startIndex = resultItemEnd;

#ifdef	DEBUG_SEARCH_OUTPUT
			char	*results = ToNewCString(resultItem);
			if (results)
			{
				printf("\n----- Search result: '%s'\n\n", results);
				nsCRT::free(results);
				results = nsnull;
			}
#endif

			// look for href
			PRInt32	hrefOffset = resultItem.Find("HREF=", PR_TRUE);
			if (hrefOffset < 0)
			{
#ifdef	DEBUG_SEARCH_OUTPUT
				printf("\n***** Unable to find HREF!\n\n");
#endif
				continue;
			}

			nsAutoString	hrefStr;
			PRInt32		quoteStartOffset = resultItem.FindCharInSet("\"\'>", hrefOffset);
			PRInt32		quoteEndOffset;
			if (quoteStartOffset < hrefOffset)
			{
				// handle case where HREF isn't quoted
				quoteStartOffset = hrefOffset + strlen("HREF=");
				quoteEndOffset = resultItem.FindChar('>', quoteStartOffset);
				if (quoteEndOffset < quoteStartOffset)	continue;
			}
			else
			{
				if (resultItem[quoteStartOffset] == PRUnichar('>'))
				{
					// handle case where HREF isn't quoted
					quoteEndOffset = quoteStartOffset;
					quoteStartOffset = hrefOffset + strlen("HREF=") -1;
				}
				else
				{
					quoteEndOffset = resultItem.FindCharInSet("\"\'", quoteStartOffset + 1);
					if (quoteEndOffset < hrefOffset)	continue;
				}
			}
			resultItem.Mid(hrefStr, quoteStartOffset + 1, quoteEndOffset - quoteStartOffset - 1);

			ConvertEntities(hrefStr, PR_FALSE, PR_TRUE, PR_FALSE);

			if (hrefStr.IsEmpty())	continue;

			char		*absURIStr = nsnull;
			nsCAutoString	hrefstrC;
			hrefstrC.AssignWithConversion(hrefStr);

			if (NS_SUCCEEDED(rv = NS_MakeAbsoluteURI(&absURIStr, hrefstrC.get(), aURL))
			    && (absURIStr))
			{
				hrefStr.AssignWithConversion(absURIStr);

				nsCOMPtr<nsIURI>	absURI;
				rv = NS_NewURI(getter_AddRefs(absURI), absURIStr);
				nsCRT::free(absURIStr);
				absURIStr = nsnull;

        if (absURI && skipLocalFlag)
				{
					nsCAutoString absPath;
					absURI->GetPath(absPath);
					if (!absPath.IsEmpty())
					{
                        NS_ConvertUTF8toUCS2 absPathStr(absPath);
						PRInt32 pathOptionsOffset = absPathStr.FindChar(PRUnichar('?'));
						if (pathOptionsOffset >= 0)
							absPathStr.Truncate(pathOptionsOffset);
						PRBool	pathsMatchFlag = serverPathStr.Equals(absPathStr, nsCaseInsensitiveStringComparator());
            if (pathsMatchFlag)
              continue;
					}

					if (!hostName.IsEmpty())
					{
						nsCAutoString absHost;
						absURI->GetAsciiHost(absHost);
						if (!absHost.IsEmpty())
						{
							PRBool	hostsMatchFlag = !nsCRT::strcasecmp(hostName.get(), absHost.get());
              if (hostsMatchFlag)
                continue;
						}
					}
				}
			}

			// if this result is to be filtered out, notice it now
      if (isSearchResultFiltered(hrefStr))
				continue;

			nsAutoString	site(hrefStr);

#ifdef	DEBUG_SEARCH_OUTPUT
			char *hrefCStr = ToNewCString(hrefStr);
			if (hrefCStr)
			{
				printf("HREF: '%s'\n", hrefCStr);
				nsCRT::free(hrefCStr);
				hrefCStr = nsnull;
			}
#endif

			nsCOMPtr<nsIRDFResource>	res;

// #define	OLDWAY
#ifdef	OLDWAY
			rv = gRDFService->GetResource(nsCAutoString(hrefStr), getter_AddRefs(res));
#else		
			// save HREF attribute as URL
			if (NS_SUCCEEDED(rv = gRDFService->GetAnonymousResource(getter_AddRefs(res))))
			{
				const PRUnichar	*hrefUni = hrefStr.get();
				if (hrefUni)
				{
					nsCOMPtr<nsIRDFLiteral>	hrefLiteral;
					if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(hrefUni, getter_AddRefs(hrefLiteral))))
					{
						mInner->Assert(res, kNC_URL, hrefLiteral, PR_TRUE);
					}
				}
			}
	#endif
			if (NS_FAILED(rv))	continue;
			
			// set HTML response chunk
			const PRUnichar	*htmlResponseUni = resultItem.get();
			if (htmlResponseUni && (gBrowserSearchMode>0))
			{
				nsCOMPtr<nsIRDFLiteral>	htmlResponseLiteral;
				if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(htmlResponseUni, getter_AddRefs(htmlResponseLiteral))))
				{
					if (htmlResponseLiteral)
					{
						mInner->Assert(res, kNC_HTML, htmlResponseLiteral, PR_TRUE);
					}
				}
			}
			
			// set banner (if we have one)
			if (bannerLiteral)
			{
				mInner->Assert(res, kNC_Banner, bannerLiteral, PR_TRUE);
			}

			// look for Site (if it isn't already set)
			nsCOMPtr<nsIRDFNode>		oldSiteRes = nsnull;
			mInner->GetTarget(res, kNC_Site, PR_TRUE, getter_AddRefs(oldSiteRes));
			if (!oldSiteRes)
			{
				PRInt32	protocolOffset = site.FindChar(':', 0);
				if (protocolOffset >= 0)
				{
					site.Cut(0, protocolOffset+1);
					while (site[0] == PRUnichar('/'))
					{
						site.Cut(0, 1);
					}
					PRInt32	slashOffset = site.FindChar('/', 0);
					if (slashOffset >= 0)
					{
						site.Truncate(slashOffset);
					}
					if (!site.IsEmpty())
					{
						const PRUnichar	*siteUni = site.get();
						if (siteUni)
						{
							nsCOMPtr<nsIRDFLiteral>	siteLiteral;
							if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(siteUni, getter_AddRefs(siteLiteral))))
							{
								if (siteLiteral)
								{
									mInner->Assert(res, kNC_Site, siteLiteral, PR_TRUE);
								}
							}
						}
					}
				}
			}

			// look for name
			nsAutoString	nameStr;

			if ((!nameStartStr.IsEmpty()) && (!nameEndStr.IsEmpty()))
			{
				PRInt32		nameStart;
				if ((nameStart = nsString_Find(nameStartStr, resultItem, PR_TRUE)) >= 0)
				{
					nameStart += nameStartStr.Length();
					PRInt32	nameEnd = nsString_Find(nameEndStr, resultItem, PR_TRUE, nameStart);
					if (nameEnd > nameStart)
					{
						resultItem.Mid(nameStr, nameStart, nameEnd - nameStart);
					}
				}
			}

			if (nameStr.IsEmpty())
			{
				PRInt32	anchorEnd = resultItem.FindChar('>', quoteEndOffset);
				if (anchorEnd < quoteEndOffset)
				{
#ifdef	DEBUG_SEARCH_OUTPUT
					printf("\n\nSearch: Unable to find ending > when computing name.\n\n");
#endif
					continue;
				}
				PRInt32	anchorStop = resultItem.Find("</A>", PR_TRUE, quoteEndOffset);
				if (anchorStop < anchorEnd)
				{
#ifdef	DEBUG_SEARCH_OUTPUT
					printf("\n\nSearch: Unable to find </A> tag to compute name.\n\n");
#endif
					continue;
				}
				resultItem.Mid(nameStr, anchorEnd + 1, anchorStop - anchorEnd - 1);
			}

			ConvertEntities(nameStr);

			// look for Name (if it isn't already set)
			nsCOMPtr<nsIRDFNode>		oldNameRes = nsnull;
#ifdef	OLDWAY
			mInner->GetTarget(res, kNC_Name, PR_TRUE, getter_AddRefs(oldNameRes));
#endif
			if (!oldNameRes)
			{
				if (!nameStr.IsEmpty())
				{
					const PRUnichar	*nameUni = nameStr.get();
					if (nameUni)
					{
						nsCOMPtr<nsIRDFLiteral>	nameLiteral;
						if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(nameUni, getter_AddRefs(nameLiteral))))
						{
							if (nameLiteral)
							{
								mInner->Assert(res, kNC_Name, nameLiteral, PR_TRUE);
							}
						}
					}
				}
			}

			// look for date
			if (!dateStartStr.IsEmpty())
			{
				nsAutoString	dateItem;
				PRInt32		dateStart;
				if ((dateStart = nsString_Find(dateStartStr, resultItem, PR_TRUE)) >= 0)
				{
					dateStart += dateStartStr.Length();
					PRInt32	dateEnd = nsString_Find(dateEndStr, resultItem, PR_TRUE, dateStart);
					if (dateEnd > dateStart)
					{
						resultItem.Mid(dateItem, dateStart, dateEnd - dateStart);
					}
				}
				
				// strip out any html tags
				PRInt32		ltOffset, gtOffset;
				while ((ltOffset = dateItem.FindChar(PRUnichar('<'))) >= 0)
				{
					if ((gtOffset = dateItem.FindChar(PRUnichar('>'), ltOffset)) <= ltOffset)
						break;
					dateItem.Cut(ltOffset, gtOffset - ltOffset + 1);
				}

				// strip out whitespace and any CR/LFs
				dateItem.Trim("\n\r\t ");

				if (!dateItem.IsEmpty())
				{
					const PRUnichar		*dateUni = dateItem.get();
					nsCOMPtr<nsIRDFLiteral>	dateLiteral;
					if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(dateUni, getter_AddRefs(dateLiteral))))
					{
						if (dateLiteral)
						{
							mInner->Assert(res, kNC_Date, dateLiteral, PR_TRUE);
							hasDateFlag = PR_TRUE;
						}
					}
				}
			}

			// look for price
			if (!priceStartStr.IsEmpty())
			{
				nsAutoString	priceItem;
				PRInt32		priceStart;
				if ((priceStart = nsString_Find(priceStartStr, resultItem, PR_TRUE)) >= 0)
				{
					priceStart += priceStartStr.Length();
					PRInt32	priceEnd = nsString_Find(priceEndStr, resultItem, PR_TRUE, priceStart);
					if (priceEnd > priceStart)
					{
						resultItem.Mid(priceItem, priceStart, priceEnd - priceStart);
						ConvertEntities(priceItem);
					}
				}
				if (!priceItem.IsEmpty())
				{
					const PRUnichar		*priceUni = priceItem.get();
					nsCOMPtr<nsIRDFLiteral>	priceLiteral;
					if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(priceUni, getter_AddRefs(priceLiteral))))
					{
						if (priceLiteral)
						{
							mInner->Assert(res, kNC_Price, priceLiteral, PR_TRUE);
							hasPriceFlag = PR_TRUE;
						}
					}

					PRInt32	priceCharStartOffset = priceItem.FindCharInSet("1234567890");
					if (priceCharStartOffset >= 0)
					{
						priceItem.Cut(0, priceCharStartOffset);
						PRInt32	priceErr;
						float	val = priceItem.ToFloat(&priceErr);
						if (priceItem.FindChar(PRUnichar('.')) >= 0)	val *= 100;

						nsCOMPtr<nsIRDFInt>	priceSortLiteral;
						if (NS_SUCCEEDED(rv = gRDFService->GetIntLiteral((PRInt32)val, getter_AddRefs(priceSortLiteral))))
						{
							if (priceSortLiteral)
							{
								mInner->Assert(res, kNC_PriceSort, priceSortLiteral, PR_TRUE);
							}
						}
					}
				}
				// look for availability (if we looked for price)
				if (!availStartStr.IsEmpty())
				{
					nsAutoString	availItem;
					PRInt32		availStart;
					if ((availStart = nsString_Find(availStartStr, resultItem, PR_TRUE)) >= 0)
					{
						availStart += availStartStr.Length();
						PRInt32	availEnd = nsString_Find(availEndStr, resultItem, PR_TRUE, availStart);
						if (availEnd > availStart)
						{
							resultItem.Mid(availItem, availStart, availEnd - availStart);
							ConvertEntities(availItem);
						}
					}
					if (!availItem.IsEmpty())
					{
						const PRUnichar		*availUni = availItem.get();
						nsCOMPtr<nsIRDFLiteral>	availLiteral;
						if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(availUni, getter_AddRefs(availLiteral))))
						{
							if (availLiteral)
							{
								mInner->Assert(res, kNC_Availability, availLiteral, PR_TRUE);
								hasAvailabilityFlag = PR_TRUE;
							}
						}
					}
				}
			}

			// look for relevance
			nsAutoString	relItem;
			PRInt32		relStart;
			if ((relStart = nsString_Find(relevanceStartStr, resultItem, PR_TRUE)) >= 0)
			{
				relStart += relevanceStartStr.Length();
				PRInt32	relEnd = nsString_Find(relevanceEndStr, resultItem, PR_TRUE);
				if (relEnd > relStart)
				{
					resultItem.Mid(relItem, relStart, relEnd - relStart);
				}
			}

			// look for Relevance (if it isn't already set)
			nsCOMPtr<nsIRDFNode>		oldRelRes = nsnull;
#ifdef	OLDWAY
			mInner->GetTarget(res, kNC_Relevance, PR_TRUE, getter_AddRefs(oldRelRes));
#endif
			if (!oldRelRes)
			{
				if (!relItem.IsEmpty())
				{
					// save real relevance
					const PRUnichar	*relUni = relItem.get();
					if (relUni)
					{
						nsAutoString	relStr(relUni);
						// take out any characters that aren't numeric or "%"
						PRInt32	len = relStr.Length();
						for (PRInt32 x=len-1; x>=0; x--)
						{
							PRUnichar	ch;
							ch = relStr.CharAt(x);
							if ((ch != PRUnichar('%')) &&
								((ch < PRUnichar('0')) || (ch > PRUnichar('9'))))
							{
								relStr.Cut(x, 1);
							}
						}
						// make sure it ends with a "%"
						len = relStr.Length();
						if (len > 0)
						{
							PRUnichar	ch;
							ch = relStr.CharAt(len - 1);
							if (ch != PRUnichar('%'))
							{
								relStr += PRUnichar('%');
							}
							relItem = relStr;
							hasRelevanceFlag = PR_TRUE;
						}
						else
						{
							relItem.Truncate();
						}
					}
				}
				if (relItem.IsEmpty())
				{
					relItem.AssignLiteral("-");
				}

				const PRUnichar *relItemUni = relItem.get();
				nsCOMPtr<nsIRDFLiteral>	relLiteral;
				if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(relItemUni, getter_AddRefs(relLiteral))))
				{
					if (relLiteral)
					{
						mInner->Assert(res, kNC_Relevance, relLiteral, PR_TRUE);
					}
				}

				if ((!relItem.IsEmpty()) && (!relItem.EqualsLiteral("-")))
				{
					// If its a percentage, remove "%"
					if (relItem[relItem.Length()-1] == PRUnichar('%'))
					{
						relItem.Cut(relItem.Length()-1, 1);
					}

					// left-pad with "0"s and set special sorting value
					nsAutoString	zero;
					zero.AssignLiteral("000");
					if (relItem.Length() < 3)
					{
						relItem.Insert(zero.get(), 0, zero.Length() - relItem.Length()); 
					}
				}
				else
				{
					relItem.AssignLiteral("000");
				}

				const PRUnichar	*relSortUni = relItem.get();
				if (relSortUni)
				{
					nsCOMPtr<nsIRDFLiteral>	relSortLiteral;
					if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(relSortUni, getter_AddRefs(relSortLiteral))))
					{
						if (relSortLiteral)
						{
							mInner->Assert(res, kNC_RelevanceSort, relSortLiteral, PR_TRUE);
						}
					}
				}
			}

			// set reference to engine this came from (if it isn't already set)
			nsCOMPtr<nsIRDFNode>		oldEngineRes = nsnull;
#ifdef	OLDWAY
			mInner->GetTarget(res, kNC_Engine, PR_TRUE, getter_AddRefs(oldEngineRes));
#endif
			if (!oldEngineRes)
			{
				if (!engineStr.IsEmpty())
				{
					const PRUnichar		*engineUni = engineStr.get();
					nsCOMPtr<nsIRDFLiteral>	engineLiteral;
					if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(engineUni, getter_AddRefs(engineLiteral))))
					{
						if (engineLiteral)
						{
							mInner->Assert(res, kNC_Engine, engineLiteral, PR_TRUE);
						}
					}
				}
			}

			// copy the engine's icon reference (if it has one) onto the result node
			nsCOMPtr<nsIRDFNode>		engineIconNode = nsnull;
			mInner->GetTarget(mEngine, kNC_Icon, PR_TRUE, getter_AddRefs(engineIconNode));

			// if no branding icon, use some default icons
			nsAutoString	iconChromeDefault;

			if (browserResultTypeStr.LowerCaseEqualsLiteral("category"))
				iconChromeDefault.AssignLiteral("chrome://communicator/skin/search/category.gif");
			else if ((browserResultTypeStr.LowerCaseEqualsLiteral("result")) && (!engineIconNode))
				iconChromeDefault.AssignLiteral("chrome://communicator/skin/search/result.gif");

			if (!iconChromeDefault.IsEmpty())
			{
				const PRUnichar		*iconUni = iconChromeDefault.get();
				nsCOMPtr<nsIRDFLiteral>	iconLiteral;
				if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(iconUni, getter_AddRefs(iconLiteral))))
				{
					if (iconLiteral)
					{
						mInner->Assert(res, kNC_Icon, iconLiteral, PR_TRUE);
					}
				}
			}
			else if (engineIconNode)
			{
				rv = mInner->Assert(res, kNC_Icon, engineIconNode, PR_TRUE);
			}

			// set result page rank
			nsCOMPtr<nsIRDFInt>	pageRankLiteral;
			if (NS_SUCCEEDED(rv = gRDFService->GetIntLiteral(pageRank++, getter_AddRefs(pageRankLiteral))))
			{
				rv = mInner->Assert(res, kNC_PageRank, pageRankLiteral, PR_TRUE);
			}

			// set the result type
			if (!browserResultTypeStr.IsEmpty())
			{
				const PRUnichar		*resultTypeUni = browserResultTypeStr.get();
				nsCOMPtr<nsIRDFLiteral>	resultTypeLiteral;
				if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(resultTypeUni, getter_AddRefs(resultTypeLiteral))))
				{
					if (resultTypeLiteral)
					{
						mInner->Assert(res, kNC_SearchType, resultTypeLiteral, PR_TRUE);
					}
				}
			}

			// set the node type
			rv = mInner->Assert(res, kRDF_type, kNC_SearchResult, PR_TRUE);

			// Note: always add in parent-child relationship last!  (if it isn't already set)
#ifdef	OLDWAY
			PRBool		parentHasChildFlag = PR_FALSE;
			if (mParent)
			{
				mInner->HasAssertion(mParent, kNC_Child, res, PR_TRUE, &parentHasChildFlag);
			}
      if (!parentHasChildFlag)
#endif
			{
				if (mParent)
				{
					rv = mInner->Assert(mParent, kNC_Child, res, PR_TRUE);
				}
			}

			// Persist this under kNC_LastSearchRoot
			if (mInner)
			{
				rv = mInner->Assert(kNC_LastSearchRoot, kNC_Child, res, PR_TRUE);
			}

			++numResults;

			// place a cap on the # of results we can process
			if (numResults >= MAX_SEARCH_RESULTS_ALLOWED)
				break;

		}
	}

	// set hints so that the appropriate columns can be displayed
	if (mParent)
	{
    if (hasPriceFlag)
      SetHint(mParent, kNC_Price);
    if (hasAvailabilityFlag)
      SetHint(mParent, kNC_Availability);
    if (hasRelevanceFlag)
      SetHint(mParent, kNC_Relevance);
    if (hasDateFlag)
      SetHint(mParent, kNC_Date);
	}

#ifdef	DEBUG
	PRTime		now2;
#ifdef	XP_MAC
	Microseconds((UnsignedWide *)&now2);
#else
	now2 = PR_Now();
#endif
	PRUint64	loadTime64;
	LL_SUB(loadTime64, now2, now);
	PRUint32	loadTime32;
	LL_L2UI(loadTime32, loadTime64);
	printf("Finished processing search results  (%u microseconds)\n", loadTime32);
#endif

	return(NS_OK);
}



nsresult
InternetSearchDataSource::SetHint(nsIRDFResource *mParent, nsIRDFResource *hintRes)
{
	if (!mInner)	return(NS_OK);

	nsresult	rv;
	PRBool		hasAssertionFlag = PR_FALSE;
  rv = mInner->HasAssertion(mParent, hintRes, kTrueLiteral, PR_TRUE,
                            &hasAssertionFlag);
  if (NS_SUCCEEDED(rv) && !hasAssertionFlag)
	{
		rv = mInner->Assert(mParent, hintRes, kTrueLiteral, PR_TRUE);
	}
	return(rv);
}



nsresult
InternetSearchDataSource::ConvertEntities(nsString &nameStr, PRBool removeHTMLFlag,
					PRBool removeCRLFsFlag, PRBool trimWhiteSpaceFlag)
{
	PRInt32	startOffset = 0, ampOffset, semiOffset, offset;

	// do this before converting entities
  if (removeHTMLFlag)
	{
		// munge out anything inside of HTML "<" / ">" tags
		while ((offset = nameStr.FindChar(PRUnichar('<'), 0)) >= 0)
		{
			PRInt32	offsetEnd = nameStr.FindChar(PRUnichar('>'), offset+1);
			if (offsetEnd <= offset)	break;
			nameStr.Cut(offset, offsetEnd - offset + 1);
		}
	}
	while ((ampOffset = nameStr.FindChar(PRUnichar('&'), startOffset)) >= 0)
	{
		if ((semiOffset = nameStr.FindChar(PRUnichar(';'), ampOffset+1)) <= ampOffset)
			break;

		nsAutoString	entityStr;
		nameStr.Mid(entityStr, ampOffset, semiOffset-ampOffset+1);
		nameStr.Cut(ampOffset, semiOffset-ampOffset+1);

		PRUnichar	entityChar = 0;
		if (entityStr.LowerCaseEqualsLiteral("&quot;"))	entityChar = PRUnichar('\"');
		else if (entityStr.LowerCaseEqualsLiteral("&amp;"))	entityChar = PRUnichar('&');
		else if (entityStr.LowerCaseEqualsLiteral("&nbsp;"))	entityChar = PRUnichar(' ');
		else if (entityStr.LowerCaseEqualsLiteral("&lt;"))		entityChar = PRUnichar('<');
		else if (entityStr.LowerCaseEqualsLiteral("&gt;"))		entityChar = PRUnichar('>');
		else if (entityStr.LowerCaseEqualsLiteral("&iexcl;"))	entityChar = PRUnichar(161);
		else if (entityStr.LowerCaseEqualsLiteral("&cent;"))	entityChar = PRUnichar(162);
		else if (entityStr.LowerCaseEqualsLiteral("&pound;"))	entityChar = PRUnichar(163);
		else if (entityStr.LowerCaseEqualsLiteral("&curren;"))	entityChar = PRUnichar(164);
		else if (entityStr.LowerCaseEqualsLiteral("&yen;"))	entityChar = PRUnichar(165);
		else if (entityStr.LowerCaseEqualsLiteral("&brvbar;"))	entityChar = PRUnichar(166);
		else if (entityStr.LowerCaseEqualsLiteral("&sect;"))	entityChar = PRUnichar(167);
		else if (entityStr.LowerCaseEqualsLiteral("&uml;"))	entityChar = PRUnichar(168);
		else if (entityStr.LowerCaseEqualsLiteral("&copy;"))	entityChar = PRUnichar(169);
		else if (entityStr.LowerCaseEqualsLiteral("&ordf;"))	entityChar = PRUnichar(170);
		else if (entityStr.LowerCaseEqualsLiteral("&laquo;"))	entityChar = PRUnichar(171);
		else if (entityStr.LowerCaseEqualsLiteral("&not;"))	entityChar = PRUnichar(172);
		else if (entityStr.LowerCaseEqualsLiteral("&shy;"))	entityChar = PRUnichar(173);
		else if (entityStr.LowerCaseEqualsLiteral("&reg;"))	entityChar = PRUnichar(174);
		else if (entityStr.LowerCaseEqualsLiteral("&macr;"))	entityChar = PRUnichar(175);
		else if (entityStr.LowerCaseEqualsLiteral("&deg;"))	entityChar = PRUnichar(176);
		else if (entityStr.LowerCaseEqualsLiteral("&plusmn;"))	entityChar = PRUnichar(177);
		else if (entityStr.LowerCaseEqualsLiteral("&sup2;"))	entityChar = PRUnichar(178);
		else if (entityStr.LowerCaseEqualsLiteral("&sup3;"))	entityChar = PRUnichar(179);
		else if (entityStr.LowerCaseEqualsLiteral("&acute;"))	entityChar = PRUnichar(180);
		else if (entityStr.LowerCaseEqualsLiteral("&micro;"))	entityChar = PRUnichar(181);
		else if (entityStr.LowerCaseEqualsLiteral("&para;"))	entityChar = PRUnichar(182);
		else if (entityStr.LowerCaseEqualsLiteral("&middot;"))	entityChar = PRUnichar(183);
		else if (entityStr.LowerCaseEqualsLiteral("&cedil;"))	entityChar = PRUnichar(184);
		else if (entityStr.LowerCaseEqualsLiteral("&sup1;"))	entityChar = PRUnichar(185);
		else if (entityStr.LowerCaseEqualsLiteral("&ordm;"))	entityChar = PRUnichar(186);
		else if (entityStr.LowerCaseEqualsLiteral("&raquo;"))	entityChar = PRUnichar(187);
		else if (entityStr.LowerCaseEqualsLiteral("&frac14;"))	entityChar = PRUnichar(188);
		else if (entityStr.LowerCaseEqualsLiteral("&frac12;"))	entityChar = PRUnichar(189);
		else if (entityStr.LowerCaseEqualsLiteral("&frac34;"))	entityChar = PRUnichar(190);
		else if (entityStr.LowerCaseEqualsLiteral("&iquest;"))	entityChar = PRUnichar(191);
		else if (entityStr.LowerCaseEqualsLiteral("&agrave;"))	entityChar = PRUnichar(192);
		else if (entityStr.LowerCaseEqualsLiteral("&aacute;"))	entityChar = PRUnichar(193);
		else if (entityStr.LowerCaseEqualsLiteral("&acirc;"))	entityChar = PRUnichar(194);
		else if (entityStr.LowerCaseEqualsLiteral("&atilde;"))	entityChar = PRUnichar(195);
		else if (entityStr.LowerCaseEqualsLiteral("&auml;"))	entityChar = PRUnichar(196);
		else if (entityStr.LowerCaseEqualsLiteral("&aring;"))	entityChar = PRUnichar(197);
		else if (entityStr.LowerCaseEqualsLiteral("&aelig;"))	entityChar = PRUnichar(198);
		else if (entityStr.LowerCaseEqualsLiteral("&ccedil;"))	entityChar = PRUnichar(199);
		else if (entityStr.LowerCaseEqualsLiteral("&egrave;"))	entityChar = PRUnichar(200);
		else if (entityStr.LowerCaseEqualsLiteral("&eacute;"))	entityChar = PRUnichar(201);
		else if (entityStr.LowerCaseEqualsLiteral("&ecirc;"))	entityChar = PRUnichar(202);
		else if (entityStr.LowerCaseEqualsLiteral("&euml;"))	entityChar = PRUnichar(203);
		else if (entityStr.LowerCaseEqualsLiteral("&igrave;"))	entityChar = PRUnichar(204);
		else if (entityStr.LowerCaseEqualsLiteral("&iacute;"))	entityChar = PRUnichar(205);
		else if (entityStr.LowerCaseEqualsLiteral("&icirc;"))	entityChar = PRUnichar(206);
		else if (entityStr.LowerCaseEqualsLiteral("&iuml;"))	entityChar = PRUnichar(207);
		else if (entityStr.LowerCaseEqualsLiteral("&eth;"))	entityChar = PRUnichar(208);
		else if (entityStr.LowerCaseEqualsLiteral("&ntilde;"))	entityChar = PRUnichar(209);
		else if (entityStr.LowerCaseEqualsLiteral("&ograve;"))	entityChar = PRUnichar(210);
		else if (entityStr.LowerCaseEqualsLiteral("&oacute;"))	entityChar = PRUnichar(211);
		else if (entityStr.LowerCaseEqualsLiteral("&ocirc;"))	entityChar = PRUnichar(212);
		else if (entityStr.LowerCaseEqualsLiteral("&otilde;"))	entityChar = PRUnichar(213);
		else if (entityStr.LowerCaseEqualsLiteral("&ouml;"))	entityChar = PRUnichar(214);
		else if (entityStr.LowerCaseEqualsLiteral("&times;"))	entityChar = PRUnichar(215);
		else if (entityStr.LowerCaseEqualsLiteral("&oslash;"))	entityChar = PRUnichar(216);
		else if (entityStr.LowerCaseEqualsLiteral("&ugrave;"))	entityChar = PRUnichar(217);
		else if (entityStr.LowerCaseEqualsLiteral("&uacute;"))	entityChar = PRUnichar(218);
		else if (entityStr.LowerCaseEqualsLiteral("&ucirc;"))	entityChar = PRUnichar(219);
		else if (entityStr.LowerCaseEqualsLiteral("&uuml;"))	entityChar = PRUnichar(220);
		else if (entityStr.LowerCaseEqualsLiteral("&yacute;"))	entityChar = PRUnichar(221);
		else if (entityStr.LowerCaseEqualsLiteral("&thorn;"))	entityChar = PRUnichar(222);
		else if (entityStr.LowerCaseEqualsLiteral("&szlig;"))	entityChar = PRUnichar(223);
		else if (entityStr.LowerCaseEqualsLiteral("&agrave;"))	entityChar = PRUnichar(224);
		else if (entityStr.LowerCaseEqualsLiteral("&aacute;"))	entityChar = PRUnichar(225);
		else if (entityStr.LowerCaseEqualsLiteral("&acirc;"))	entityChar = PRUnichar(226);
		else if (entityStr.LowerCaseEqualsLiteral("&atilde;"))	entityChar = PRUnichar(227);
		else if (entityStr.LowerCaseEqualsLiteral("&auml;"))	entityChar = PRUnichar(228);
		else if (entityStr.LowerCaseEqualsLiteral("&aring;"))	entityChar = PRUnichar(229);
		else if (entityStr.LowerCaseEqualsLiteral("&aelig;"))	entityChar = PRUnichar(230);
		else if (entityStr.LowerCaseEqualsLiteral("&ccedil;"))	entityChar = PRUnichar(231);
		else if (entityStr.LowerCaseEqualsLiteral("&egrave;"))	entityChar = PRUnichar(232);
		else if (entityStr.LowerCaseEqualsLiteral("&eacute;"))	entityChar = PRUnichar(233);
		else if (entityStr.LowerCaseEqualsLiteral("&ecirc;"))	entityChar = PRUnichar(234);
		else if (entityStr.LowerCaseEqualsLiteral("&euml;"))	entityChar = PRUnichar(235);
		else if (entityStr.LowerCaseEqualsLiteral("&igrave;"))	entityChar = PRUnichar(236);
		else if (entityStr.LowerCaseEqualsLiteral("&iacute;"))	entityChar = PRUnichar(237);
		else if (entityStr.LowerCaseEqualsLiteral("&icirc;"))	entityChar = PRUnichar(238);
		else if (entityStr.LowerCaseEqualsLiteral("&iuml;"))	entityChar = PRUnichar(239);
		else if (entityStr.LowerCaseEqualsLiteral("&eth;"))	entityChar = PRUnichar(240);
		else if (entityStr.LowerCaseEqualsLiteral("&ntilde;"))	entityChar = PRUnichar(241);
		else if (entityStr.LowerCaseEqualsLiteral("&ograve;"))	entityChar = PRUnichar(242);
		else if (entityStr.LowerCaseEqualsLiteral("&oacute;"))	entityChar = PRUnichar(243);
		else if (entityStr.LowerCaseEqualsLiteral("&ocirc;"))	entityChar = PRUnichar(244);
		else if (entityStr.LowerCaseEqualsLiteral("&otilde;"))	entityChar = PRUnichar(245);
		else if (entityStr.LowerCaseEqualsLiteral("&ouml;"))	entityChar = PRUnichar(246);
		else if (entityStr.LowerCaseEqualsLiteral("&divide;"))	entityChar = PRUnichar(247);
		else if (entityStr.LowerCaseEqualsLiteral("&oslash;"))	entityChar = PRUnichar(248);
		else if (entityStr.LowerCaseEqualsLiteral("&ugrave;"))	entityChar = PRUnichar(249);
		else if (entityStr.LowerCaseEqualsLiteral("&uacute;"))	entityChar = PRUnichar(250);
		else if (entityStr.LowerCaseEqualsLiteral("&ucirc;"))	entityChar = PRUnichar(251);
		else if (entityStr.LowerCaseEqualsLiteral("&uuml;"))	entityChar = PRUnichar(252);
		else if (entityStr.LowerCaseEqualsLiteral("&yacute;"))	entityChar = PRUnichar(253);
		else if (entityStr.LowerCaseEqualsLiteral("&thorn;"))	entityChar = PRUnichar(254);
		else if (entityStr.LowerCaseEqualsLiteral("&yuml;"))	entityChar = PRUnichar(255);

		startOffset = ampOffset;
		if (entityChar != 0)
		{
			nameStr.Insert(entityChar, ampOffset);
			++startOffset;
		}
	}

  if (removeCRLFsFlag)
	{
		// cut out any CRs or LFs
		while ((offset = nameStr.FindCharInSet("\n\r", 0)) >= 0)
		{
			nameStr.Cut(offset, 1);
		}
	}

  if (trimWhiteSpaceFlag)
	{
		// trim name
		nameStr.Trim(" \t");
	}

	return(NS_OK);
}

NS_IMETHODIMP
InternetSearchDataSource::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
    nsresult rv = NS_OK;

    if (!nsCRT::strcmp(aTopic, "profile-before-change"))
    {
        // The profile is about to change.
        categoryDataSource = nsnull;

        if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("shutdown-cleanse").get()))
        {
            // Delete search.rdf
            nsCOMPtr<nsIFile> searchFile;
            rv = NS_GetSpecialDirectory(NS_APP_SEARCH_50_FILE, getter_AddRefs(searchFile));
            if (NS_SUCCEEDED(rv))
                rv = searchFile->Remove(PR_FALSE);
        }
    }
    else if (!nsCRT::strcmp(aTopic, "profile-do-change"))
    {
        // The profile has aleady changed.
        if (!categoryDataSource)
          GetCategoryList();
    }

    return rv;
}
