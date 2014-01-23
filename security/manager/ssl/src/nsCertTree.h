/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NS_CERTTREE_H_
#define _NS_CERTTREE_H_

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsICertTree.h"
#include "nsITreeView.h"
#include "nsITreeBoxObject.h"
#include "nsITreeSelection.h"
#include "nsIMutableArray.h"
#include "nsNSSComponent.h"
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

  mozilla::RefPtr<nsIX509Cert> mCert;
  // how many display entries reference this?
  // (and therefore depend on the underlying cert)
  int32_t mUsageCount;
};

class nsCertTreeDispInfo : public nsICertTreeItem
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICERTTREEITEM

  nsCertTreeDispInfo();
  nsCertTreeDispInfo(nsCertTreeDispInfo &other);
  virtual ~nsCertTreeDispInfo();

  mozilla::RefPtr<nsCertAddonInfo> mAddonInfo;
  enum {
    direct_db, host_port_override
  } mTypeOfEntry;
  nsCString mAsciiHost;
  int32_t mPort;
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
                               sortCriterion crit, int32_t level);
  static int32_t CmpByCrit(nsIX509Cert *a, CompareCacheHashEntry *ace, 
                           nsIX509Cert *b, CompareCacheHashEntry *bce, 
                           sortCriterion crit, int32_t level);
  static int32_t CmpBy(void *cache, nsIX509Cert *a, nsIX509Cert *b, 
                       sortCriterion c0, sortCriterion c1, sortCriterion c2);
  static int32_t CmpCACert(void *cache, nsIX509Cert *a, nsIX509Cert *b);
  static int32_t CmpWebSiteCert(void *cache, nsIX509Cert *a, nsIX509Cert *b);
  static int32_t CmpUserCert(void *cache, nsIX509Cert *a, nsIX509Cert *b);
  static int32_t CmpEmailCert(void *cache, nsIX509Cert *a, nsIX509Cert *b);
  nsCertCompareFunc GetCompareFuncFromCertType(uint32_t aType);
  int32_t CountOrganizations();

  nsresult GetCertsByType(uint32_t aType, nsCertCompareFunc aCertCmpFn,
                          void *aCertCmpFnArg);

  nsresult GetCertsByTypeFromCache(nsINSSCertCache *aCache, uint32_t aType,
                                   nsCertCompareFunc aCertCmpFn, void *aCertCmpFnArg);
private:
  nsTArray< mozilla::RefPtr<nsCertTreeDispInfo> > mDispInfo;
  nsCOMPtr<nsITreeBoxObject>  mTree;
  nsCOMPtr<nsITreeSelection>  mSelection;
  treeArrayEl                *mTreeArray;
  int32_t                         mNumOrgs;
  int32_t                         mNumRows;
  PLDHashTable mCompareCache;
  nsCOMPtr<nsINSSComponent> mNSSComponent;
  nsCOMPtr<nsICertOverrideService> mOverrideService;
  mozilla::RefPtr<nsCertOverrideService> mOriginalOverrideService;

  treeArrayEl *GetThreadDescAtIndex(int32_t _index);
  already_AddRefed<nsIX509Cert> 
    GetCertAtIndex(int32_t _index, int32_t *outAbsoluteCertOffset = nullptr);
  mozilla::TemporaryRef<nsCertTreeDispInfo>
    GetDispInfoAtIndex(int32_t index, int32_t *outAbsoluteCertOffset = nullptr);
  void FreeCertArray();
  nsresult UpdateUIContents();

  nsresult GetCertsByTypeFromCertList(CERTCertList *aCertList,
                                      uint32_t aType,
                                      nsCertCompareFunc  aCertCmpFn,
                                      void              *aCertCmpFnArg);

  nsCOMPtr<nsIMutableArray> mCellText;

#ifdef DEBUG_CERT_TREE
  /* for debugging purposes */
  void dumpMap();
#endif
};

#endif /* _NS_CERTTREE_H_ */

