/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NS_CERTTREE_H_
#define _NS_CERTTREE_H_

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIServiceManager.h"
#include "nsICertTree.h"
#include "nsITreeView.h"
#include "nsITreeBoxObject.h"
#include "nsITreeSelection.h"
#include "nsISupportsArray.h"
#include "nsIMutableArray.h"
#include "nsTArray.h"
#include "pldhash.h"
#include "nsIX509CertDB.h"
#include "nsCertOverrideService.h"
#include "mozilla/Attributes.h"


typedef struct treeArrayElStr treeArrayEl;

struct CompareCacheHashEntry {
  enum { max_criterions = 3 };
  CompareCacheHashEntry();

  void *key; // no ownership
  bool mCritInit[max_criterions];
  nsXPIDLString mCrit[max_criterions];
};

struct CompareCacheHashEntryPtr : PLDHashEntryHdr {
  CompareCacheHashEntryPtr();
  ~CompareCacheHashEntryPtr();
  CompareCacheHashEntry *entry;
};

class nsCertAddonInfo MOZ_FINAL : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  nsCertAddonInfo() : mUsageCount(0) {}

  nsRefPtr<nsIX509Cert> mCert;
  // how many display entries reference this?
  // (and therefore depend on the underlying cert)
  PRInt32 mUsageCount;
};

class nsCertTreeDispInfo : public nsICertTreeItem
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICERTTREEITEM

  nsCertTreeDispInfo();
  nsCertTreeDispInfo(nsCertTreeDispInfo &other);
  virtual ~nsCertTreeDispInfo();

  nsRefPtr<nsCertAddonInfo> mAddonInfo;
  enum {
    direct_db, host_port_override
  } mTypeOfEntry;
  nsCString mAsciiHost;
  PRInt32 mPort;
  nsCertOverride::OverrideBits mOverrideBits;
  bool mIsTemporary;
  nsCOMPtr<nsIX509Cert> mCert;
};

class nsCertTree : public nsICertTree
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICERTTREE
  NS_DECL_NSITREEVIEW

  nsCertTree();
  virtual ~nsCertTree();

  enum sortCriterion { sort_IssuerOrg, sort_Org, sort_Token, 
    sort_CommonName, sort_IssuedDateDescending, sort_Email, sort_None };

protected:
  nsresult InitCompareHash();
  void ClearCompareHash();
  void RemoveCacheEntry(void *key);

  typedef int (*nsCertCompareFunc)(void *, nsIX509Cert *a, nsIX509Cert *b);

  static CompareCacheHashEntry *getCacheEntry(void *cache, void *aCert);
  static void CmpInitCriterion(nsIX509Cert *cert, CompareCacheHashEntry *entry,
                               sortCriterion crit, PRInt32 level);
  static PRInt32 CmpByCrit(nsIX509Cert *a, CompareCacheHashEntry *ace, 
                           nsIX509Cert *b, CompareCacheHashEntry *bce, 
                           sortCriterion crit, PRInt32 level);
  static PRInt32 CmpBy(void *cache, nsIX509Cert *a, nsIX509Cert *b, 
                       sortCriterion c0, sortCriterion c1, sortCriterion c2);
  static PRInt32 CmpCACert(void *cache, nsIX509Cert *a, nsIX509Cert *b);
  static PRInt32 CmpWebSiteCert(void *cache, nsIX509Cert *a, nsIX509Cert *b);
  static PRInt32 CmpUserCert(void *cache, nsIX509Cert *a, nsIX509Cert *b);
  static PRInt32 CmpEmailCert(void *cache, nsIX509Cert *a, nsIX509Cert *b);
  nsCertCompareFunc GetCompareFuncFromCertType(PRUint32 aType);
  PRInt32 CountOrganizations();

  nsresult GetCertsByType(PRUint32 aType, nsCertCompareFunc aCertCmpFn,
                          void *aCertCmpFnArg);

  nsresult GetCertsByTypeFromCache(nsINSSCertCache *aCache, PRUint32 aType,
                                   nsCertCompareFunc aCertCmpFn, void *aCertCmpFnArg);
private:
  nsTArray< nsRefPtr<nsCertTreeDispInfo> > mDispInfo;
  nsCOMPtr<nsITreeBoxObject>  mTree;
  nsCOMPtr<nsITreeSelection>  mSelection;
  treeArrayEl                *mTreeArray;
  PRInt32                         mNumOrgs;
  PRInt32                         mNumRows;
  PLDHashTable mCompareCache;
  nsCOMPtr<nsINSSComponent> mNSSComponent;
  nsCOMPtr<nsICertOverrideService> mOverrideService;
  nsRefPtr<nsCertOverrideService> mOriginalOverrideService;

  treeArrayEl *GetThreadDescAtIndex(PRInt32 _index);
  already_AddRefed<nsIX509Cert> 
    GetCertAtIndex(PRInt32 _index, PRInt32 *outAbsoluteCertOffset = nullptr);
  already_AddRefed<nsCertTreeDispInfo> 
    GetDispInfoAtIndex(PRInt32 index, PRInt32 *outAbsoluteCertOffset = nullptr);
  void FreeCertArray();
  nsresult UpdateUIContents();

  nsresult GetCertsByTypeFromCertList(CERTCertList *aCertList,
                                      PRUint32 aType,
                                      nsCertCompareFunc  aCertCmpFn,
                                      void              *aCertCmpFnArg);

  nsCOMPtr<nsIMutableArray> mCellText;

#ifdef DEBUG_CERT_TREE
  /* for debugging purposes */
  void dumpMap();
#endif
};

#endif /* _NS_CERTTREE_H_ */

