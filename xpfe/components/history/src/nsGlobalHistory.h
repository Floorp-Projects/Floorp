/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author(s):
 *   Chris Waterson <waterson@netscape.com>
 *
 * Contributor(s): 
 */

#ifndef nsglobalhistory__h____
#define nsglobalhistory__h____

#include "nsMdbPtr.h"
#include "mdb.h"
#include "nsIGlobalHistory.h"
#include "nsIObserver.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsISupportsArray.h"
#include "nsIStringBundle.h"
#include "nsWeakReference.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsCOMPtr.h"
#include "nsAWritableString.h"
#include "nsITimer.h"

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
  nsIMdbEnv*   mEnv;
  nsIMdbTable* mTable;

  nsIMdbTableRowCursor* mCursor;
  nsIMdbRow*            mCurrent;

  nsMdbTableEnumerator();
  virtual ~nsMdbTableEnumerator();

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

//----------------------------------------------------------------------
//
// nsGlobalHistory
//
//   This class is the browser's implementation of the
//   nsIGlobalHistory interface.
//

class nsGlobalHistory : nsSupportsWeakReference,
                        public nsIGlobalHistory,
                        public nsIObserver,
                        public nsIRDFDataSource,
                        public nsIRDFRemoteDataSource
{
public:
  // nsISupports methods 
  NS_DECL_ISUPPORTS

  // nsIGlobalHistory
  NS_DECL_NSIGLOBALHISTORY

  // nsIObserver - for observing prefs changes
  NS_DECL_NSIOBSERVER

  // nsIRDFDataSource
  NS_DECL_NSIRDFDATASOURCE

  // nsIRDFRemoteDataSource
  NS_DECL_NSIRDFREMOTEDATASOURCE

  NS_METHOD Init();

  nsGlobalHistory(void);
  virtual ~nsGlobalHistory();

  // these must be public so that the callbacks can call them
  PRBool MatchExpiration(nsIMdbRow *row, PRInt64* expirationDate);
  PRBool MatchHost(nsIMdbRow *row, matchHost_t *hostInfo);

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
  nsresult GetRootDayQueries(nsISimpleEnumerator **aResult);
  nsresult GetFindUriName(const char *aURL, nsIRDFNode **aResult);
  nsresult CreateFindEnumerator(nsIRDFResource *aSource,
                                nsISimpleEnumerator **aResult);
  static nsresult FindUrlToTokenList(const char *aURL, nsVoidArray& aResult);
  static void FreeTokenList(nsVoidArray& tokens);
  static PRBool IsFindResource(nsIRDFResource *aResource);
  nsresult TokenListToSearchQuery(const nsVoidArray& tokens,
                                  searchQuery& aResult);

  // caching of PR_Now() so we don't call it every time we do
  // a history query
  PRInt64   mLastNow;           // cache the last PR_Now()
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
  
  mdb_scope  kToken_HistoryRowScope;
  mdb_kind   kToken_HistoryKind;

  mdb_column kToken_URLColumn;
  mdb_column kToken_ReferrerColumn;
  mdb_column kToken_LastVisitDateColumn;
  mdb_column kToken_FirstVisitDateColumn;
  mdb_column kToken_VisitCountColumn;
  mdb_column kToken_NameColumn;
  mdb_column kToken_HostnameColumn;

  //
  // AddPage-oriented stuff
  //
  nsresult AddPageToDatabase(const char *aURL,
                             const char *aReferrerURL,
                             PRInt64 aDate);
  nsresult AddExistingPageToDatabase(nsIMdbRow *row,
                                     PRInt64 aDate,
                                     PRInt64 *aOldDate,
                                     PRInt32 *aOldCount);
  nsresult AddNewPageToDatabase(const char *aURL,
                                const char *aReferrerURL,
                                PRInt64 aDate);
  //
  // generic routines for setting/retrieving various datatypes
  //
  nsresult SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRInt64& aValue);
  nsresult SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRInt32 aValue);
  nsresult SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const char *aValue);
  nsresult SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRUnichar *aValue);

  nsresult GetRowValue(nsIMdbRow *aRow, mdb_column aCol, nsAWritableString& aResult);
  nsresult GetRowValue(nsIMdbRow *aRow, mdb_column aCol, nsAWritableCString& aResult);
  nsresult GetRowValue(nsIMdbRow *aRow, mdb_column aCol, PRInt64* aResult);
  nsresult GetRowValue(nsIMdbRow *aRow, mdb_column aCol, PRInt32* aResult);

  nsresult FindRow(mdb_column aCol, const char *aURL, nsIMdbRow **aResult);

  //
  // misc unrelated stuff
  //
  nsCOMPtr<nsIStringBundle> mBundle;
  nsresult SaveLastPageVisited(const char *);

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

  //
  // custom enumerators
  //

  // URLEnumerator - for searching for a specific set of rows which
  // match a particular column
  class URLEnumerator : public nsMdbTableEnumerator
  {
  protected:
    mdb_column mURLColumn;
    mdb_column mSelectColumn;
    void*      mSelectValue;
    PRInt32    mSelectValueLen;

    virtual ~URLEnumerator();

  public:
    URLEnumerator(mdb_column aURLColumn,
                  mdb_column aSelectColumn = mdb_column(0),
                  void* aSelectValue = nsnull,
                  PRInt32 aSelectValueLen = 0) :
      mURLColumn(aURLColumn),
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
    SearchEnumerator(nsIMdbStore* aStore,
                     searchQuery *aQuery,
                     mdb_column aURLColumn) :
      mQuery(aQuery),
      mURLColumn(aURLColumn),
      mStore(aStore)
    {}

    virtual ~SearchEnumerator();

  protected:
    searchQuery *mQuery;
    nsHashtable mUniqueRows;
    nsIMdbStore *mStore;
    mdb_column mURLColumn;

    
    nsCString mFindUriPrefix;

    void GetFindUriPrefix(nsAWritableCString& aPrefix);
    
    virtual PRBool IsResult(nsIMdbRow* aRow);
    virtual nsresult ConvertToISupports(nsIMdbRow* aRow,
                                        nsISupports** aResult);
    
    PRBool RowMatches(nsIMdbRow* aRow, searchQuery *aQuery);
  };

  friend class URLEnumerator;
  friend class SearchEnumerator;
};

#endif // nsglobalhistory__h____
