/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*

  A global browser history implementation that also supports the RDF
  datasource interface.

*/

#include "nsCOMPtr.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsIGenericFactory.h"
#include "nsIGlobalHistory.h"
#include "nsIProfile.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsSpecialSystemDirectory.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "plhash.h"
#include "plstr.h"
#include "prprf.h"
#include "prtime.h"
#include "rdf.h"

#ifdef MOZ_BRPROF
#include "nsIBrowsingProfile.h"
#endif

////////////////////////////////////////////////////////////////////////
// Common CIDs

static NS_DEFINE_CID(kComponentManagerCID,  NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID,    NS_GENERICFACTORY_CID);
static NS_DEFINE_CID(kGlobalHistoryCID,     NS_GLOBALHISTORY_CID);
static NS_DEFINE_CID(kRDFServiceCID,        NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kProfileCID,           NS_PROFILE_CID);

#ifdef MOZ_BRPROF
static NS_DEFINE_CID(kBrowsingProfileCID,   NS_BROWSINGPROFILE_CID);
#endif

////////////////////////////////////////////////////////////////////////
// HistoryEntry
//

class HistoryEntry
{
public:
  PRTime date;
  char* url;
  char* referrer;

  HistoryEntry(PRTime& aDate, const char* aURL, const char* aReferrerURL)
    : date(aDate)
  {
    url = PL_strdup(aURL);

    if (aReferrerURL)
      referrer = PL_strdup(aReferrerURL);
    else
      referrer = nsnull;
  }

  ~HistoryEntry() {
    PL_strfree(url);

    if (referrer)
      PL_strfree(referrer);
  }
};


////////////////////////////////////////////////////////////////////////
// nsGlobalHistory
//
//   This class is the browser's implementation of the
//   nsIGlobalHistory interface.
//

class nsGlobalHistory : public nsIGlobalHistory,
                        public nsIRDFDataSource
{
public:
  // nsISupports methods 
  NS_DECL_ISUPPORTS

  // nsIGlobalHistory
  NS_IMETHOD AddPage(const char *aURL, const char *aReferrerURL, PRInt64 aDate);
  NS_IMETHOD SetPageTitle(const char *aURL, const PRUnichar *aTitle);
  NS_IMETHOD RemovePage(const char *aURL);
  NS_IMETHOD GetLastVisitDate(const char *aURL, PRInt64 *_retval);
  NS_IMETHOD GetURLCompletion(const char *aURL, char **_retval);
  NS_IMETHOD GetLastPageVisted(char **_retval);

  // nsIRDFDataSource
  NS_IMETHOD GetURI(char* *aURI);

  NS_IMETHOD GetSource(nsIRDFResource* aProperty,
                       nsIRDFNode* aTarget,
                       PRBool aTruthValue,
                       nsIRDFResource** aSource) {
    return mInner->GetSource(aProperty, aTarget, aTruthValue, aSource);
  }

  NS_IMETHOD GetSources(nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget,
                        PRBool aTruthValue,
                        nsISimpleEnumerator** aSources) {
    return mInner->GetSources(aProperty, aTarget, aTruthValue, aSources);
  }

  NS_IMETHOD GetTarget(nsIRDFResource* aSource,
                       nsIRDFResource* aProperty,
                       PRBool aTruthValue,
                       nsIRDFNode** aTarget);

  NS_IMETHOD GetTargets(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        PRBool aTruthValue,
                        nsISimpleEnumerator** aTargets) {
    return mInner->GetTargets(aSource, aProperty, aTruthValue, aTargets);
  }

  NS_IMETHOD Assert(nsIRDFResource* aSource, 
                    nsIRDFResource* aProperty, 
                    nsIRDFNode* aTarget,
                    PRBool aTruthValue) {
    // History cannot be modified
    return NS_RDF_ASSERTION_REJECTED;
  }

  NS_IMETHOD Unassert(nsIRDFResource* aSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aTarget) {
    // History cannot be modified
    return NS_RDF_ASSERTION_REJECTED;
  }

	NS_IMETHOD Change(nsIRDFResource* aSource,
                    nsIRDFResource* aProperty,
                    nsIRDFNode* aOldTarget,
                    nsIRDFNode* aNewTarget) {
    return NS_RDF_ASSERTION_REJECTED;
  }

	NS_IMETHOD Move(nsIRDFResource* aOldSource,
					nsIRDFResource* aNewSource,
					nsIRDFResource* aProperty,
                  nsIRDFNode* aTarget) {
    return NS_RDF_ASSERTION_REJECTED;
  }

  NS_IMETHOD HasAssertion(nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget,
                          PRBool aTruthValue,
                          PRBool* aHasAssertion) {
    return mInner->HasAssertion(aSource, aProperty, aTarget, aTruthValue, aHasAssertion);
  }

  NS_IMETHOD AddObserver(nsIRDFObserver* aObserver) {
    return mInner->AddObserver(aObserver);
  }

  NS_IMETHOD RemoveObserver(nsIRDFObserver* aObserver) {
    return mInner->RemoveObserver(aObserver);
  }

  NS_IMETHOD ArcLabelsIn(nsIRDFNode* aNode,
                         nsISimpleEnumerator** aLabels) {
    return mInner->ArcLabelsIn(aNode, aLabels);
  }

  NS_IMETHOD ArcLabelsOut(nsIRDFResource* aSource,
                          nsISimpleEnumerator** aLabels) {
    return mInner->ArcLabelsOut(aSource, aLabels);
  }

  NS_IMETHOD GetAllCommands(nsIRDFResource* aSource,
                            nsIEnumerator/*<nsIRDFResource>*/** aCommands) {
    return mInner->GetAllCommands(aSource, aCommands);
  }

  NS_IMETHOD GetAllCmds(nsIRDFResource* aSource,
                            nsISimpleEnumerator/*<nsIRDFResource>*/** aCommands) {
    return mInner->GetAllCmds(aSource, aCommands);
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

private:
  nsGlobalHistory(void);
  virtual ~nsGlobalHistory();

  friend NS_IMETHODIMP
  NS_NewGlobalHistory(nsISupports* aOuter, REFNSIID aIID, void** aResult);

  // The in-memory datasource that is used to hold the global history
  nsCOMPtr<nsIRDFDataSource> mInner;

  nsVoidArray        mFiles;
  nsFileSpec         mCurrentFileSpec;
  nsVoidArray        mPendingWrites; 

  
  PLHashTable*       mLastVisitDateHash; 
  PRExplodedTime     mSessionTime;

#if defined(MOZ_BRPROF)
  nsIBrowsingProfile* mBrowsingProfile;
#endif


  // Implementation Methods
  nsresult Init();

  nsresult ReadHistory();

  nsresult ReadOneHistoryFile(const nsFileSpec& aFileSpec);

  nsresult AddPageToGraph(const char* aURL,
                          const PRUnichar* aTitle,
                          const char* aReferrerURL,
                          PRUint32 aVisitCount,
                          PRTime aDate);

  nsresult AddToDateHierarchy(PRTime aDate, const char *aURL);

  nsresult GetSiteOfURL(const char* aURL, nsIRDFResource** aResource);

  nsresult UpdateLastVisitDate(nsIRDFResource* aPage, PRTime aDate);

  nsresult GetHistoryDir(nsFileSpec* aDirectory);

  // pseudo-constants. although the global history really is a
  // singleton, we'll use this metaphor to be consistent.
  static PRInt32 gRefCnt;
  static nsIRDFService* gRDFService;
  static nsIRDFResource* kNC_Page;
  static nsIRDFResource* kNC_Date;
  static nsIRDFResource* kNC_VisitCount;
  static nsIRDFResource* kNC_Title;
  static nsIRDFResource* kNC_Referrer;
  static nsIRDFResource* kNC_child;
  static nsIRDFResource* kNC_URL;
  static nsIRDFResource* kNC_HistoryBySite;
  static nsIRDFResource* kNC_HistoryByDate;

  static PLHashNumber HashPointer(const void* aKey);
};


PRInt32 nsGlobalHistory::gRefCnt;
nsIRDFService* nsGlobalHistory::gRDFService;
nsIRDFResource* nsGlobalHistory::kNC_Page;
nsIRDFResource* nsGlobalHistory::kNC_Date;
nsIRDFResource* nsGlobalHistory::kNC_VisitCount;
nsIRDFResource* nsGlobalHistory::kNC_Title;
nsIRDFResource* nsGlobalHistory::kNC_Referrer;
nsIRDFResource* nsGlobalHistory::kNC_child;
nsIRDFResource* nsGlobalHistory::kNC_URL;
nsIRDFResource* nsGlobalHistory::kNC_HistoryBySite;
nsIRDFResource* nsGlobalHistory::kNC_HistoryByDate;


////////////////////////////////////////////////////////////////////////


nsGlobalHistory::nsGlobalHistory()
  : mLastVisitDateHash(nsnull)
{
  NS_INIT_REFCNT();
}

nsGlobalHistory::~nsGlobalHistory()
{
  gRDFService->UnregisterDataSource(this);

  if (mLastVisitDateHash)
    PL_HashTableDestroy(mLastVisitDateHash);


  PRInt32 writeIndex;
  for (writeIndex = mPendingWrites.Count() - 1; writeIndex >= 0; --writeIndex) {
    HistoryEntry* entry = (HistoryEntry*) mPendingWrites.ElementAt(writeIndex);
    delete entry;
  }

#if defined(MOZ_BRPROF)
  if (mBrowsingProfile)
    nsServiceManager::ReleaseService(kBrowsingProfileCID, mBrowsingProfile);
#endif

  if (--gRefCnt == 0) {
    if (gRDFService) {
      nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
      gRDFService = nsnull;
    }

    NS_IF_RELEASE(kNC_Page);
    NS_IF_RELEASE(kNC_Date);
    NS_IF_RELEASE(kNC_VisitCount);
    NS_IF_RELEASE(kNC_Title);
    NS_IF_RELEASE(kNC_Referrer);
    NS_IF_RELEASE(kNC_child);
    NS_IF_RELEASE(kNC_URL);
    NS_IF_RELEASE(kNC_HistoryBySite);
    NS_IF_RELEASE(kNC_HistoryByDate);
  }
}



NS_IMETHODIMP
NS_NewGlobalHistory(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aOuter == nsnull, "no aggregation");
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsresult rv = NS_OK;

  nsGlobalHistory* result = new nsGlobalHistory();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = result->Init();
  if (NS_SUCCEEDED(rv))
    rv = result->QueryInterface(aIID, aResult);

  if (NS_FAILED(rv)) {
    delete result;
    *aResult = nsnull;
    return rv;
  }

  return rv;
}


////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ADDREF(nsGlobalHistory);
NS_IMPL_RELEASE(nsGlobalHistory);

NS_IMETHODIMP
nsGlobalHistory::QueryInterface(REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(nsIGlobalHistory::GetIID()) ||
      aIID.Equals(kISupportsIID)) {
    *aResult = NS_STATIC_CAST(nsIGlobalHistory*, this);
  }
  else if (aIID.Equals(nsIRDFDataSource::GetIID())) {
    *aResult = NS_STATIC_CAST(nsIRDFDataSource*, this);
  }
  else {
    *aResult = nsnull;
    return NS_NOINTERFACE;
  }

  NS_ADDREF(this);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIGlobalHistory

NS_IMETHODIMP
nsGlobalHistory::AddPage(const char *aURL, const char *aReferrerURL, PRInt64 aDate)
{
  NS_PRECONDITION(aURL != nsnull, "null ptr");
  if (! aURL)
    return NS_ERROR_NULL_POINTER;

  HistoryEntry* he = new HistoryEntry(aDate, aURL, aReferrerURL);
  if (! he)
    return NS_ERROR_OUT_OF_MEMORY;

  mPendingWrites.AppendElement(he);
  return NS_OK;
}


NS_IMETHODIMP
nsGlobalHistory::SetPageTitle(const char *aURL, const PRUnichar *aTitle)
{
  // XXX This is pretty much copied straight over from Guha's original
  // implementation. It needs to be sanitized for safety, etc.
  for (PRInt32 i = mPendingWrites.Count() - 1; i >= 0; --i) {
    HistoryEntry* wr = (HistoryEntry*) mPendingWrites.ElementAt(i);
    if (PL_strcmp(wr->url, aURL) == 0) {
      char timeBuffer[256];
      PRExplodedTime etime;
      PR_ExplodeTime(wr->date, PR_LocalTimeParameters, &etime);
      PR_FormatTimeUSEnglish(timeBuffer, 256, "%a %b %d %H:%M:%S %Y", &etime);

      nsAutoString  title(aTitle);
      char    *cTitle = title.ToNewCString();
      char *buffer = PR_smprintf("%s\t%s\t%s\t%lu\t%s\n", wr->url,
                                 ((cTitle) ? cTitle:""),
                                 ((wr->referrer) ? wr->referrer : ""),
                                 1L, timeBuffer);
      if (cTitle) {
        delete [] cTitle;
        cTitle = nsnull;
      }
      if (buffer) {
        nsOutputFileStream out(mCurrentFileSpec, PR_WRONLY | PR_CREATE_FILE | PR_APPEND, 0744);
        // XXX because PR_APPEND is broken
        out.seek(mCurrentFileSpec.GetFileSize());
        out << buffer;
        PR_smprintf_free(buffer);
      }

      mPendingWrites.RemoveElement(wr);
      AddPageToGraph(wr->url, aTitle, wr->referrer, 1L, PR_Now());
      delete wr;

      return NS_OK;
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsGlobalHistory::RemovePage(const char *aURL)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsGlobalHistory::GetLastVisitDate(const char *aURL, PRInt64 *_retval)
{
  NS_PRECONDITION(aURL != nsnull, "null ptr");
  if (! aURL)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(_retval != nsnull, "null ptr");
  if (! _retval)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;

  nsIRDFResource* page;
  rv = gRDFService->GetResource(aURL, &page);
  if (NS_FAILED(rv)) return rv;

  PRTime* date = (PRTime*) PL_HashTableLookup(mLastVisitDateHash, page);

  if (date) {
    *_retval = *date;
  }
  else {
    *_retval = LL_ZERO; // return zero if not found
  }

  NS_RELEASE(page);
  return NS_OK;
}


NS_IMETHODIMP
nsGlobalHistory::GetURLCompletion(const char *aURL, char **_retval)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGlobalHistory::GetLastPageVisted(char **_retval)
{ 
  NS_PRECONDITION(_retval != nsnull, "null ptr");
  if (! _retval)
    return NS_ERROR_NULL_POINTER;

  *_retval = PR_smprintf("http://people.netscape.com/waterson");
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource

NS_IMETHODIMP
nsGlobalHistory::GetURI(char* *aURI)
{
  NS_PRECONDITION(aURI != nsnull, "null ptr");
  if (! aURI)
    return NS_ERROR_NULL_POINTER;

  *aURI = nsXPIDLCString::Copy("rdf:history");
  if (! *aURI)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalHistory::GetTarget(nsIRDFResource* aSource,
                           nsIRDFResource* aProperty,
                           PRBool aTruthValue,
                           nsIRDFNode** aTarget)
{
  if (aTruthValue && (aProperty == kNC_URL)) {
    nsresult rv;

    nsXPIDLCString uri;
    rv = aSource->GetValue( getter_Copies(uri) );
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get source's URI");
    if (NS_FAILED(rv)) return(rv);

    nsAutoString  url(uri);
    if (url.Find("NC:") == 0)
      return NS_RDF_NO_VALUE;

    nsCOMPtr<nsIRDFLiteral>  literal;
    rv = gRDFService->GetLiteral(url.GetUnicode(), getter_AddRefs(literal));
    if (NS_FAILED(rv)) return rv;

    *aTarget = literal;
    NS_ADDREF(*aTarget);

    return NS_OK;
  }

  return mInner->GetTarget(aSource, aProperty, aTruthValue, aTarget);
}


////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
nsGlobalHistory::Init()
{
  NS_PRECONDITION(mInner == nsnull, "already initialized");
  if (mInner)
    return NS_ERROR_ALREADY_INITIALIZED;

  nsresult rv;

  if (gRefCnt++ == 0) {
    rv = nsServiceManager::GetService(kRDFServiceCID,
                                      nsIRDFService::GetIID(),
                                      (nsISupports**) &gRDFService);

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
    if (NS_FAILED(rv)) return rv;

    gRDFService->GetResource(NC_NAMESPACE_URI "Page",        &kNC_Page);
    gRDFService->GetResource(NC_NAMESPACE_URI "Date",        &kNC_Date);
    gRDFService->GetResource(NC_NAMESPACE_URI "VisitCount",  &kNC_VisitCount);
    gRDFService->GetResource(NC_NAMESPACE_URI "Name",        &kNC_Title);
    gRDFService->GetResource(NC_NAMESPACE_URI "Referrer",    &kNC_Referrer);
    gRDFService->GetResource(NC_NAMESPACE_URI "child",       &kNC_child);
    gRDFService->GetResource(NC_NAMESPACE_URI "URL",         &kNC_URL);
    gRDFService->GetResource("NC:HistoryBySite",             &kNC_HistoryBySite);
    gRDFService->GetResource("NC:HistoryByDate",             &kNC_HistoryByDate);
  }

  static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);

  rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
                                          nsnull,
                                          nsIRDFDataSource::GetIID(),
                                          getter_AddRefs(mInner));

  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create inner in-memory datasource");
  if (NS_FAILED(rv)) return rv;

  // register this as a named data source with the RDF service
  rv = gRDFService->RegisterDataSource(this, PR_FALSE);
  NS_ASSERTION(NS_SUCCEEDED(rv), "somebody already created & registered the history data source");
  if (NS_FAILED(rv)) return rv;

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

  mLastVisitDateHash = PL_NewHashTable(400, HashPointer,
                                       PL_CompareValues,
                                       PL_CompareValues,
                                       nsnull, nsnull);

  PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &mSessionTime);

  rv = ReadHistory();
  if (NS_FAILED(rv)) return rv;

  char filename[256];

  // XXX since we're not really printing an unsinged long, but
  // rather an unsigned long-long, this is sure to break at some
  // point.
  PR_snprintf(filename, sizeof(filename), "%lu.hst", PR_Now());

  rv = GetHistoryDir(&mCurrentFileSpec);
  if (NS_FAILED(rv)) return rv;

  mCurrentFileSpec += filename;
  return NS_OK;
}

nsresult
nsGlobalHistory::ReadHistory()
{
  nsresult rv;

  nsFileSpec dir;
  rv = GetHistoryDir(&dir);
  if (NS_FAILED(rv)) return rv;

  for (nsDirectoryIterator i(dir, PR_TRUE); i.Exists(); i++) {
    const nsFileSpec spec = i.Spec();

    // convert to a path so we can inspect it: we only want to read
    const char* path = (const char*) spec;

    PRInt32 pathlen = PL_strlen(path);
    if (pathlen < 4)
      continue;

    if (PL_strcasecmp(&(path[pathlen - 4]), ".hst") != 0)
      continue;

    // okay, it's a .HST file, which means we should try to parse it.
    rv = ReadOneHistoryFile(spec);

    // XXX ignore failure, continue to try to parse the rest.
  }

  return NS_OK;
}

nsresult
nsGlobalHistory::ReadOneHistoryFile(const nsFileSpec& aFileSpec)
{
  nsInputFileStream strm(aFileSpec);
  NS_ASSERTION(strm.is_open(), "unable to open history file");
  if (! strm.is_open())
    return NS_ERROR_FAILURE;

  nsRandomAccessInputStream in(strm);

  while (! in.eof() && ! in.failed()) {
    nsresult rv = NS_OK;
    char buf[256];
    char* p = buf;

    char* cursor = buf;          // our position in the buffer
    PRInt32 size = sizeof(buf);  // the size of the buffer
    PRInt32 space = sizeof(buf); // the space left in the buffer
    while (! in.readline(cursor, space)) {
      // in.readline() return PR_FALSE if there was buffer overflow,
      // or there was a catastrophe. Check to see if we're here
      // because of the latter...
      NS_ASSERTION (! in.failed(), "error reading file");
      if (in.failed()) return NS_ERROR_FAILURE;

      PRInt32 newsize = size * 2;

      // realloc
      char* newp = new char[newsize];
      if (! newp) {
        if (p != buf)
          delete[] p;

        return NS_ERROR_OUT_OF_MEMORY;
      }

      nsCRT::memcpy(newp, p, size);

      if (p != buf)
        delete[] p;

      p = newp;
      cursor = p + size - 1; // because it's null-terminated
      space = size + 1;      // ...we can re-use the '\0'
      size = newsize;
    }

    if (PL_strlen(p) > 0) do {
      char* url = p;

      char* title = PL_strchr(p, '\t');
      if (! title)
        break;

      *(title++) = '\0';

      char* referrer = PL_strchr(title, '\t');
      if (! referrer)
        break;

      *(referrer++) = '\0';

      char* visitcount = PL_strchr(referrer, '\t');
      if (! visitcount)
        break;

      *(visitcount++) = '\0';

      char* date = PL_strchr(visitcount, '\t');
      if (! date)
        break;

      *(date++) = '\0';

      PRTime time;
      PR_ParseTimeString(date, 0, &time);

      PRExplodedTime etime;
      PR_ExplodeTime(time, PR_LocalTimeParameters, &etime);

      // While we parse this file, check to see if it has any
      // history elements in it from today. If it does, then
      // we'll re-use it as the current history file.
      //
      // XXX Isn't this a little over-ambitious? I could see one
      // file growing into a monstrous thing if somebody
      // regularly stays up past midnight.
      if (etime.tm_yday == mSessionTime.tm_yday)
        mCurrentFileSpec = aFileSpec;

      rv = AddPageToGraph(url, nsAutoString(title).GetUnicode(), referrer,
                          (PRUint32) atol(visitcount), time);

    } while (0);

    if (p != buf)
      delete[] p;

    if (NS_FAILED(rv))
      return rv;
  }
  return NS_OK;
}

nsresult
nsGlobalHistory::AddPageToGraph(const char* aURL,
                                const PRUnichar* aTitle,
                                const char* aReferrerURL,
                                PRUint32 aVisitCount,
                                PRTime aDate)
{
  // This adds the page to the graph as follows. It constructs a
  // "history URL" that is the placeholder in the graph for the
  // history page. From that, the following structure is created
  //
  // [hst://http://some.url.com/foo.html]
  //  |
  //  +--[NC:Page]-->[http://some.url.com/foo.html]
  //  |
  //  +--[NC:Referrer]-->[http://the.referrer.com/page.html]
  //  |
  //  +--[NC:Date]-->("Tue Mar 12 1999 10:24:48 PST")
  //  |
  //  +--[NC:VisitCount]-->("12")
  //
  // This is then added into the site hierarchy and the date
  // hierarchies.
  nsresult rv;

  // XXX replace with nsString when nsString2 is official.
  char *historyURL = PR_smprintf("hst://%s", aURL);
  if (! historyURL)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIRDFResource> history;
  rv = gRDFService->GetResource(historyURL, getter_AddRefs(history));

  PR_smprintf_free(historyURL);

  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFResource> page;
  rv = gRDFService->GetResource(aURL, getter_AddRefs(page));
  if (NS_SUCCEEDED(rv))
    mInner->Assert(history, kNC_Page, page, PR_TRUE);

  if (aReferrerURL) {
    nsCOMPtr<nsIRDFResource> referrer;
    rv = gRDFService->GetResource(aReferrerURL, getter_AddRefs(referrer));
    if (NS_SUCCEEDED(rv))
      mInner->Assert(history, kNC_Referrer, referrer, PR_TRUE);
  }

  nsCOMPtr<nsIRDFDate> date;
  rv = gRDFService->GetDateLiteral(aDate, getter_AddRefs(date));
  if (NS_SUCCEEDED(rv))
    mInner->Assert(history, kNC_Date, date, PR_TRUE);

  nsCOMPtr<nsIRDFInt> visitCount;
  rv = gRDFService->GetIntLiteral(aVisitCount, getter_AddRefs(visitCount));
  if (NS_SUCCEEDED(rv))
    mInner->Assert(history, kNC_VisitCount, visitCount, PR_TRUE);

  nsCOMPtr<nsIRDFLiteral> title;
  rv = gRDFService->GetLiteral(aTitle, getter_AddRefs(title));
  if (NS_SUCCEEDED(rv))
    mInner->Assert(page, kNC_Title, title, PR_TRUE);

  // Add to site hierarchy
  nsCOMPtr<nsIRDFResource> site;
  rv = GetSiteOfURL(aURL, getter_AddRefs(site));
  if (NS_SUCCEEDED(rv))
    mInner->Assert(site, kNC_child, page, PR_TRUE);

  // Add to date hierarchy. XXX Use page URL instead of history URL?
  rv = AddToDateHierarchy(aDate, aURL);

  rv = UpdateLastVisitDate(page, PR_Now());

  return NS_OK;
}

nsresult
nsGlobalHistory::AddToDateHierarchy(PRTime aDate, const char *aURL)
{
  // Add the history entry into the date hierarchies. This is really
  // broken, because what you'd _really_ like to do is have these
  // folders do is figure out their contents on the fly. But, this'll
  // have to do for now.
  //
  // XXX I just copied most of this code directly from Guha's original
  // implementation: is it really the case that we want to enter the
  // _page_ URL as a child of the folder? I'd have thought it would be
  // the _history_ URL that we wanted entered into the graph.
  //
  // XXX I think this implementation is very leaky.
  //
  char timeBuffer[128], timeNameBuffer[128], dayBuffer[128], dayNameBuffer[128];
  PRExplodedTime    enow, etime;

  timeBuffer[0] = timeNameBuffer[0] = dayBuffer[0] = dayNameBuffer[0] = '\0';

  PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &enow);
  PR_ExplodeTime(aDate, PR_LocalTimeParameters, &etime);
  if ((enow.tm_year == etime.tm_year) && (enow.tm_yday == etime.tm_yday)) {
    PR_snprintf(timeBuffer, sizeof(timeBuffer), "NC:hst:date?today");
    PR_snprintf(timeNameBuffer, sizeof(timeNameBuffer), "Today");      // XXX localization
  }
  else if ((enow.tm_year == etime.tm_year) && ((enow.tm_yday - 1) == etime.tm_yday)) {
    PR_snprintf(timeBuffer, sizeof(timeBuffer), "NC:hst:date?yesterday");
    PR_snprintf(timeNameBuffer, sizeof(timeNameBuffer), "Yesterday");    // XXX localization
  }
  else if ((enow.tm_year == etime.tm_year) && ((enow.tm_yday - etime.tm_yday) < 7))  { // XXX calculation is not quite right
    PR_FormatTimeUSEnglish(timeBuffer, sizeof(timeBuffer), "NC:hst:date?%A, %B %d, %Y", &etime);
    PR_FormatTimeUSEnglish(timeNameBuffer, sizeof(timeNameBuffer), "%A", &etime);
  }
  else {
    PR_FormatTimeUSEnglish(dayBuffer, sizeof(dayBuffer), "NC:hst:day?%A, %B %d, %Y", &etime);
    PR_FormatTimeUSEnglish(dayNameBuffer, sizeof(dayNameBuffer), "%A, %B %d, %Y", &etime);
    if (etime.tm_wday > 0) {
      PRUint64  microSecsInSec, secsInDay, temp;
      LL_I2L(microSecsInSec, PR_USEC_PER_SEC);
      LL_I2L(secsInDay, (60L*60L*24L*etime.tm_wday));
      LL_MUL(temp, microSecsInSec, secsInDay);
      LL_SUB(aDate, aDate, temp);
      PR_ExplodeTime(aDate, PR_LocalTimeParameters, &etime);
    }
    PR_FormatTimeUSEnglish(timeBuffer, sizeof(timeBuffer), "NC:hst:weekof?%B %d, %Y", &etime);
    PR_FormatTimeUSEnglish(timeNameBuffer, sizeof(timeNameBuffer), "Week of %B %d, %Y", &etime);  // XXX localization
  }

  nsIRDFResource    *timeResource, *dayResource, *resource, *parent;
  parent = kNC_HistoryByDate;
  if (NS_OK != gRDFService->GetResource(aURL, &resource))
    return NS_ERROR_FAILURE;

  PRBool      found = PR_FALSE;
  //XXXwaterson: fix this
  //if (NS_OK != gRDFService->FindResource(timeBuffer, &timeResource, &found))
  //return NS_ERROR_FAILURE;
  if (! found) {
    if (NS_OK != gRDFService->GetResource(timeBuffer, &timeResource))
      return NS_ERROR_FAILURE;

    // set name of synthesized history container
    nsAutoString  ptitle(timeNameBuffer);
    nsIRDFLiteral  *titleLiteral;
    gRDFService->GetLiteral(ptitle.GetUnicode(), &titleLiteral);
    mInner->Assert(timeResource, kNC_Title, titleLiteral, 1);
  }
  mInner->Assert(parent, kNC_child, timeResource, 1);
  parent = timeResource;

  if (dayBuffer[0]) {
    found = PR_FALSE;
    // XXXwaterson: fix this
    //if (NS_OK != gRDFService->FindResource(dayBuffer, &dayResource, &found))
    //return NS_ERROR_FAILURE;
    if (! found) {
      if (NS_OK != gRDFService->GetResource(dayBuffer, &dayResource))
        return NS_ERROR_FAILURE;

      // set name of synthesized history container
      nsAutoString  dayTitle(dayNameBuffer);
      nsIRDFLiteral  *dayLiteral;
      gRDFService->GetLiteral(dayTitle.GetUnicode(), &dayLiteral);
      mInner->Assert(dayResource, kNC_Title, dayLiteral, 1);
    }
    mInner->Assert(parent, kNC_child, dayResource, 1);
    parent = dayResource;
  }

  mInner->Assert(parent, kNC_child, resource, 1);
  return(NS_OK);
}

nsresult
nsGlobalHistory::GetSiteOfURL(const char* aURL, nsIRDFResource** aSite)
{
  // Get the "site name" out of a URL.
  //
  // XXX Most of this was copied straigh over from Guha's original
  // implementation. Needs runtime checks for safety, etc.

  nsresult rv;

  char* str = PL_strstr(aURL, "://");
  char buff[256];
  if (!str)
    return NS_ERROR_FAILURE;

  str = str + 3;

  if (str[0] == '/') str++;
  char* estr = PL_strchr(str, '/');
  if (!estr) estr = str + PL_strlen(str);
  if ((estr - str) > 256)
    return NS_ERROR_FAILURE;

  memset(buff, '\0', 256);

  // Note: start with "NC:" so that URL won't be shown
  static const char kHistoryURLPrefix[] = "NC:hst:site?";
  PL_strcpy(buff, kHistoryURLPrefix);
  PL_strncat(buff, str, (estr-str));

  // Get the "site" resource
  nsCOMPtr<nsIRDFResource> site;
  rv = gRDFService->GetResource(buff, getter_AddRefs(site));
  if (NS_FAILED(rv)) return rv;

  // If it hasn't been hooked into the history-by-site hierarchy yet,
  // do so now.
  PRBool hasAssertion;
  rv = mInner->HasAssertion(kNC_HistoryBySite, kNC_child, site, PR_TRUE, &hasAssertion);
  if (NS_FAILED(rv)) return rv;

  if (! hasAssertion) {
    rv = mInner->Assert(kNC_HistoryBySite,  kNC_child, site, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    nsAutoString ptitle(buff + sizeof(kHistoryURLPrefix) - 1);  

    nsCOMPtr<nsIRDFLiteral> title;
    rv = gRDFService->GetLiteral(ptitle.GetUnicode(), getter_AddRefs(title));
    if (NS_FAILED(rv)) return rv;

    rv = mInner->Assert(site, kNC_Title, title, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
  }

  // finally, return the site.
  *aSite = site;
  NS_ADDREF(*aSite);

  return NS_OK;
}


nsresult
nsGlobalHistory::UpdateLastVisitDate(nsIRDFResource* aPage, PRTime aDate)
{
  PRTime* date = (PRTime*) PL_HashTableLookup(mLastVisitDateHash, aPage);

  if (! date) {
    date = new PRTime; // XXX Leak!
    if (! date)
      return NS_ERROR_OUT_OF_MEMORY;

    PL_HashTableAdd(mLastVisitDateHash, aPage, date);
  }

  *date = aDate;
  return NS_OK;
}


nsresult
nsGlobalHistory::GetHistoryDir(nsFileSpec* aDirectory)
{
  nsresult rv;

  NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = profile->GetCurrentProfileDir(aDirectory);
  if (NS_FAILED(rv)) return rv;

#ifdef  XP_MAC
  {
    PRBool  wasAlias = PR_FALSE;
    aDirectory->ResolveAlias(wasAlias);
  }
#endif

  return NS_OK;
}


PLHashNumber
nsGlobalHistory::HashPointer(const void* aKey)
{
  return (PLHashNumber) aKey;
}


////////////////////////////////////////////////////////////////////////
// Component Exports

extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServiceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
  NS_PRECONDITION(aFactory != nsnull, "null ptr");
  if (! aFactory)
    return NS_ERROR_NULL_POINTER;

  nsIGenericFactory::ConstructorProcPtr constructor;

  if (aClass.Equals(kGlobalHistoryCID)) {
    constructor = NS_NewGlobalHistory;
  }
  else {
    *aFactory = nsnull;
    return NS_NOINTERFACE; // XXX
  }

  nsresult rv;
  NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServiceMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIGenericFactory> factory;
  rv = compMgr->CreateInstance(kGenericFactoryCID,
                               nsnull,
                               nsIGenericFactory::GetIID(),
                               getter_AddRefs(factory));

  if (NS_FAILED(rv)) return rv;

  rv = factory->SetConstructor(constructor);
  if (NS_FAILED(rv)) return rv;

  *aFactory = factory;
  NS_ADDREF(*aFactory);
  return NS_OK;
}



extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, servMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kGlobalHistoryCID, "Global History",
                                  NS_GLOBALHISTORY_PROGID,
                                  aPath, PR_TRUE, PR_TRUE);

  rv = compMgr->RegisterComponent(kGlobalHistoryCID, "Global History",
                                  NS_GLOBALHISTORY_DATASOURCE_PROGID,
                                  aPath, PR_TRUE, PR_TRUE);

  return NS_OK;
}



extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, servMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kGlobalHistoryCID, aPath);

  return NS_OK;
}


