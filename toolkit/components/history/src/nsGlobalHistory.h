/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   Blake Ross <blaker@netscape.com>
 *   Joe Hewitt <hewitt@netscape.com>
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

#ifndef nsglobalhistory__h____
#define nsglobalhistory__h____

#include "nsIBrowserHistory.h"

#include "mdb.h"
#include "nsIObserver.h"
#include "nsIPrefBranch.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsISupportsArray.h"
#include "nsIStringBundle.h"
#include "nsWeakReference.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsITimer.h"
#include "nsIAutoCompleteSearch.h"
#include "nsIAutoCompleteResult.h"
#include "nsIAutoCompleteResultTypes.h"

//----------------------------------------------------------------------
//
//  nsMdbTableEnumerator
//
//    An nsISimpleEnumerator implementation that returns the value of
//    a column as an nsISupports. Allows for some simple selection.
//

class nsMdbTableEnumerator : public nsISimpleEnumerator
{
protected:
  nsMdbTableEnumerator();
  virtual ~nsMdbTableEnumerator();

  nsIMdbEnv*   mEnv;

private:
  // subclasses should not tweak these
  nsIMdbTable* mTable;
  nsIMdbTableRowCursor* mCursor;
  nsIMdbRow*            mCurrent;

public:
  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsISimpleEnumeratorMethods
  NS_IMETHOD HasMoreElements(PRBool* _result);
  NS_IMETHOD GetNext(nsISupports** _result);

  // Implementation methods
  virtual nsresult Init(nsIMdbEnv* aEnv, nsIMdbTable* aTable);

protected:
  virtual PRBool   IsResult(nsIMdbRow* aRow) = 0;
  virtual nsresult ConvertToISupports(nsIMdbRow* aRow, nsISupports** aResult) = 0;
};

typedef PRBool (*rowMatchCallback)(nsIMdbRow *aRow, void *closure);

struct matchHost_t;
struct searchQuery;
class searchTerm;

// Number of prefixes used in the autocomplete sort comparison function
#define AUTOCOMPLETE_PREFIX_LIST_COUNT 6
// Size of visit count boost to give to urls which are sites or paths
#define AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST 5

//----------------------------------------------------------------------
//
// nsGlobalHistory
//
//   This class is the browser's implementation of the
//   nsIGlobalHistory interface.
//


// Used to describe what prefixes shouldn't be cut from
// history urls when doing an autocomplete url comparison.
struct AutocompleteExclude {
  PRInt32 schemePrefix;
  PRInt32 hostnamePrefix;
};

class nsGlobalHistory : nsSupportsWeakReference,
                        public nsIBrowserHistory,
                        public nsIObserver,
                        public nsIRDFDataSource,
                        public nsIRDFRemoteDataSource,
                        public nsIAutoCompleteSearch
{
public:
  // nsISupports methods 
  NS_DECL_ISUPPORTS

  NS_DECL_NSIGLOBALHISTORY2
  NS_DECL_NSIBROWSERHISTORY
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIRDFDATASOURCE
  NS_DECL_NSIRDFREMOTEDATASOURCE
  NS_DECL_NSIAUTOCOMPLETESEARCH

  NS_METHOD Init();

  nsGlobalHistory(void);
  virtual ~nsGlobalHistory();

  // these must be public so that the callbacks can call them
  PRBool MatchExpiration(nsIMdbRow *row, PRInt64* expirationDate);
  PRBool MatchHost(nsIMdbRow *row, matchHost_t *hostInfo);
  PRBool RowMatches(nsIMdbRow* aRow, searchQuery *aQuery, PRBool caseSensitive);

protected:

  //
  // database junk
  //
  enum eCommitType 
  {
    kLargeCommit = 0,
    kSessionCommit = 1,
    kCompressCommit = 2
  };
  
  PRInt64   mFileSizeOnDisk;
  nsresult OpenDB();
  nsresult OpenExistingFile(nsIMdbFactory *factory, const char *filePath);
  nsresult OpenNewFile(nsIMdbFactory *factory, const char *filePath);
  nsresult CreateTokens();
  nsresult CloseDB();
  nsresult CheckHostnameEntries();
  nsresult Commit(eCommitType commitType);

  //
  // expiration/removal stuff
  //
  PRInt32   mExpireDays;
  nsresult ExpireEntries(PRBool notify);
  nsresult RemoveMatchingRows(rowMatchCallback aMatchFunc,
                              void *aClosure, PRBool notify);

  //
  // search stuff - find URL stuff, etc
  //
  nsresult GetRootDayQueries(nsISimpleEnumerator **aResult, PRBool aBySite);
  nsresult GetFindUriName(const char *aURL, nsIRDFNode **aResult);
  nsresult CreateFindEnumerator(nsIRDFResource *aSource,
                                nsISimpleEnumerator **aResult);
  
  static nsresult FindUrlToTokenList(const char *aURL, nsVoidArray& aResult);
  static void FreeTokenList(nsVoidArray& tokens);
  static void FreeSearchQuery(searchQuery& aQuery);
  static PRBool IsFindResource(nsIRDFResource *aResource);
  void GetFindUriPrefix(const searchQuery& aQuery,
                        const PRBool aDoGroupBy,
                        nsACString& aResult);
  
  nsresult TokenListToSearchQuery(const nsVoidArray& tokens,
                                  searchQuery& aResult);
  nsresult FindUrlToSearchQuery(const char *aURL, searchQuery& aResult);
  nsresult NotifyFindAssertions(nsIRDFResource *aSource, nsIMdbRow *aRow);
  nsresult NotifyFindUnassertions(nsIRDFResource *aSource, nsIMdbRow *aRow);
    
  // 
  // autocomplete stuff
  //
  PRBool mAutocompleteOnlyTyped;
  nsStringArray mIgnoreSchemes;
  nsStringArray mIgnoreHostnames;
  
  nsresult AutoCompleteTypedSearch(nsIAutoCompleteMdbResult **aResult);
  nsresult AutoCompleteSearch(const nsAString& aSearchString,
                              AutocompleteExclude* aExclude,
                              nsIAutoCompleteMdbResult* aPrevResult,
                              nsIAutoCompleteMdbResult** aResult);
  void AutoCompleteCutPrefix(nsAString& aURL, AutocompleteExclude* aExclude);
  void AutoCompleteGetExcludeInfo(const nsAString& aURL, AutocompleteExclude* aExclude);
  nsString AutoCompletePrefilter(const nsAString& aSearchString);
  PRBool AutoCompleteCompare(nsAString& aHistoryURL, 
                             const nsAString& aUserURL,
                             AutocompleteExclude* aExclude);
  PR_STATIC_CALLBACK(int)
  AutoCompleteSortComparison(nsIMdbRow *row1, nsIMdbRow *row2, void *closureVoid);

  // AutoCompleteSortClosure - used to pass info into 
  // AutoCompleteSortComparison from the NS_QuickSort() function
  struct AutoCompleteSortClosure
  {
    nsGlobalHistory* history;
    size_t prefixCount;
    const nsAFlatString* prefixes[AUTOCOMPLETE_PREFIX_LIST_COUNT];
  };

  // caching of PR_Now() so we don't call it every time we do
  // a history query
  PRInt64   mLastNow;           // cache the last PR_Now()
  PRInt32   mBatchesInProgress;
  PRBool    mNowValid;          // is mLastNow valid?
  nsCOMPtr<nsITimer> mExpireNowTimer;
  
  PRInt64 GetNow();
  void ExpireNow();
  
  static void expireNowTimer(nsITimer *aTimer, void *aClosure)
  {((nsGlobalHistory *)aClosure)->ExpireNow(); }
  
  //
  // sync stuff to write the db to disk every so often
  //
  PRBool    mDirty;             // if we've changed history
  nsCOMPtr<nsITimer> mSyncTimer;
  
  void Sync();
  nsresult SetDirty();
  
  static void fireSyncTimer(nsITimer *aTimer, void *aClosure)
  {((nsGlobalHistory *)aClosure)->Sync(); }

  //
  // RDF stuff
  //
  nsCOMPtr<nsISupportsArray> mObservers;
  
  PRBool IsURLInHistory(nsIRDFResource* aResource);
  
  nsresult NotifyAssert(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aValue);
  nsresult NotifyUnassert(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aValue);
  nsresult NotifyChange(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aOldValue, nsIRDFNode* aNewValue);

  //
  // row-oriented stuff
  //
  
  // N.B., these are MDB interfaces, _not_ XPCOM interfaces.
  nsIMdbEnv* mEnv;         // OWNER
  nsIMdbStore* mStore;     // OWNER
  nsIMdbTable* mTable;     // OWNER
  
  nsCOMPtr<nsIMdbRow> mMetaRow;
  
  mdb_scope  kToken_HistoryRowScope;
  mdb_kind   kToken_HistoryKind;

  mdb_column kToken_URLColumn;
  mdb_column kToken_ReferrerColumn;
  mdb_column kToken_LastVisitDateColumn;
  mdb_column kToken_FirstVisitDateColumn;
  mdb_column kToken_VisitCountColumn;
  mdb_column kToken_NameColumn;
  mdb_column kToken_HostnameColumn;
  mdb_column kToken_HiddenColumn;
  mdb_column kToken_TypedColumn;

  // meta-data tokens
  mdb_column kToken_LastPageVisited;

  //
  // AddPage-oriented stuff
  //
  nsresult AddPageToDatabase(nsIURI* aURI, PRBool aRedirect, PRBool aTopLevel,
                             PRInt64 aLastVisitDate);
  nsresult AddExistingPageToDatabase(nsIMdbRow *row,
                                     PRInt64 aDate,
                                     PRInt64 *aOldDate,
                                     PRInt32 *aOldCount);
  nsresult AddNewPageToDatabase(nsIURI* aURI,
                                PRInt64 aDate,
                                PRBool aRedirect,
                                PRBool aTopLevel,
                                nsIMdbRow **aResult);

  nsresult RemovePageInternal(const char *aSpec);

  //
  // generic routines for setting/retrieving various datatypes
  //
  nsresult SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRInt64& aValue);
  nsresult SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRInt32 aValue);
  nsresult SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const char *aValue);
  nsresult SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRUnichar *aValue);

  nsresult GetRowValue(nsIMdbRow *aRow, mdb_column aCol, nsAString& aResult);
  nsresult GetRowValue(nsIMdbRow *aRow, mdb_column aCol, nsACString& aResult);
  nsresult GetRowValue(nsIMdbRow *aRow, mdb_column aCol, PRInt64* aResult);
  nsresult GetRowValue(nsIMdbRow *aRow, mdb_column aCol, PRInt32* aResult);

  nsresult FindRow(mdb_column aCol, const char *aURL, nsIMdbRow **aResult);

  //
  // misc unrelated stuff
  //
  nsCOMPtr<nsIStringBundle> mBundle;

  // pseudo-constants. although the global history really is a
  // singleton, we'll use this metaphor to be consistent.
  static PRInt32 gRefCnt;
  static nsIRDFService* gRDFService;
  static nsIRDFResource* kNC_Page; // XXX do we need?
  static nsIRDFResource* kNC_Date;
  static nsIRDFResource* kNC_FirstVisitDate;
  static nsIRDFResource* kNC_VisitCount;
  static nsIRDFResource* kNC_AgeInDays;
  static nsIRDFResource* kNC_Name;
  static nsIRDFResource* kNC_NameSort;
  static nsIRDFResource* kNC_Hostname;
  static nsIRDFResource* kNC_Referrer;
  static nsIRDFResource* kNC_child;
  static nsIRDFResource* kNC_URL;  // XXX do we need?
  static nsIRDFResource* kNC_HistoryRoot;
  static nsIRDFResource* kNC_HistoryByDate;
  static nsIRDFResource* kNC_HistoryByDateAndSite;
  static nsIRDFResource* kNC_DayFolderIndex;

  static nsIMdbFactory* gMdbFactory;
  static nsIPrefBranch* gPrefBranch;
  //
  // custom enumerators
  //

  // URLEnumerator - for searching for a specific set of rows which
  // match a particular column
  class URLEnumerator : public nsMdbTableEnumerator
  {
  protected:
    mdb_column mURLColumn;
    mdb_column mHiddenColumn;
    mdb_column mSelectColumn;
    void*      mSelectValue;
    PRInt32    mSelectValueLen;

    virtual ~URLEnumerator();

  public:
    URLEnumerator(mdb_column aURLColumn,
                  mdb_column aHiddenColumn,
                  mdb_column aSelectColumn = mdb_column(0),
                  void* aSelectValue = nsnull,
                  PRInt32 aSelectValueLen = 0) :
      mURLColumn(aURLColumn),
      mHiddenColumn(aHiddenColumn),
      mSelectColumn(aSelectColumn),
      mSelectValue(aSelectValue),
      mSelectValueLen(aSelectValueLen)
    {}

  protected:
    virtual PRBool   IsResult(nsIMdbRow* aRow);
    virtual nsresult ConvertToISupports(nsIMdbRow* aRow, nsISupports** aResult);
  };

  // SearchEnumerator - for matching a set of rows based on a search query
  class SearchEnumerator : public nsMdbTableEnumerator
  {
  public:
    SearchEnumerator(searchQuery *aQuery,
                     mdb_column aHiddenColumn,
                     nsGlobalHistory *aHistory) :
      mQuery(aQuery),
      mHiddenColumn(aHiddenColumn),
      mHistory(aHistory)
    {}

    virtual ~SearchEnumerator();

  protected:
    searchQuery *mQuery;
    mdb_column mHiddenColumn;
    nsGlobalHistory *mHistory;
    nsHashtable mUniqueRows;
    
    nsCString mFindUriPrefix;

    virtual PRBool IsResult(nsIMdbRow* aRow);
    virtual nsresult ConvertToISupports(nsIMdbRow* aRow,
                                        nsISupports** aResult);
    
    PRBool RowMatches(nsIMdbRow* aRow, searchQuery *aQuery);
  };


  friend class URLEnumerator;
  friend class SearchEnumerator;
};


#endif // nsglobalhistory__h____
