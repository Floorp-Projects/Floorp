/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NS_CERTTREE_H_
#define _NS_CERTTREE_H_

#include "nsCOMPtr.h"
#include "nsICertTree.h"
#include "nsITreeSelection.h"
#include "nsIMutableArray.h"
#include "nsNSSComponent.h"
#include "nsTArray.h"
#include "PLDHashTable.h"
#include "mozilla/Attributes.h"

/* Disable the "base class XXX should be explicitly initialized
   in the copy constructor" warning. */
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wextra"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wextra"
#endif  // __clang__ || __GNUC__

#include "mozilla/dom/XULTreeElement.h"

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif  // __clang__ || __GNUC__

typedef struct treeArrayElStr treeArrayEl;

struct CompareCacheHashEntry {
  enum { max_criterions = 3 };
  CompareCacheHashEntry();

  void* key;  // no ownership
  bool mCritInit[max_criterions];
  nsString mCrit[max_criterions];
};

struct CompareCacheHashEntryPtr : PLDHashEntryHdr {
  CompareCacheHashEntryPtr();
  ~CompareCacheHashEntryPtr();
  CompareCacheHashEntry* entry;
};

class nsCertTreeDispInfo : public nsICertTreeItem {
 protected:
  virtual ~nsCertTreeDispInfo();

 public:
  explicit nsCertTreeDispInfo(nsIX509Cert* aCert) : mCert(aCert) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSICERTTREEITEM

  nsCOMPtr<nsIX509Cert> mCert;
};

class nsCertTree : public nsICertTree {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICERTTREE
  NS_DECL_NSITREEVIEW

  nsCertTree();

  enum sortCriterion {
    sort_IssuerOrg,
    sort_Org,
    sort_Token,
    sort_CommonName,
    sort_IssuedDateDescending,
    sort_Email,
    sort_None
  };

 protected:
  virtual ~nsCertTree();

  void ClearCompareHash();
  void RemoveCacheEntry(void* key);

  typedef int (*nsCertCompareFunc)(void*, nsIX509Cert* a, nsIX509Cert* b);

  static CompareCacheHashEntry* getCacheEntry(void* cache, void* aCert);
  static void CmpInitCriterion(nsIX509Cert* cert, CompareCacheHashEntry* entry,
                               sortCriterion crit, int32_t level);
  static int32_t CmpByCrit(nsIX509Cert* a, CompareCacheHashEntry* ace,
                           nsIX509Cert* b, CompareCacheHashEntry* bce,
                           sortCriterion crit, int32_t level);
  static int32_t CmpBy(void* cache, nsIX509Cert* a, nsIX509Cert* b,
                       sortCriterion c0, sortCriterion c1, sortCriterion c2);
  static int32_t CmpCACert(void* cache, nsIX509Cert* a, nsIX509Cert* b);
  static int32_t CmpUserCert(void* cache, nsIX509Cert* a, nsIX509Cert* b);
  static int32_t CmpEmailCert(void* cache, nsIX509Cert* a, nsIX509Cert* b);
  nsCertCompareFunc GetCompareFuncFromCertType(uint32_t aType);
  int32_t CountOrganizations();

 private:
  static const uint32_t kInitialCacheLength = 64;

  nsTArray<RefPtr<nsCertTreeDispInfo>> mDispInfo;
  RefPtr<mozilla::dom::XULTreeElement> mTree;
  nsCOMPtr<nsITreeSelection> mSelection;
  treeArrayEl* mTreeArray;
  int32_t mNumOrgs;
  int32_t mNumRows;
  PLDHashTable mCompareCache;

  treeArrayEl* GetThreadDescAtIndex(int32_t _index);
  already_AddRefed<nsIX509Cert> GetCertAtIndex(
      int32_t _index, int32_t* outAbsoluteCertOffset = nullptr);
  already_AddRefed<nsCertTreeDispInfo> GetDispInfoAtIndex(
      int32_t index, int32_t* outAbsoluteCertOffset = nullptr);
  void FreeCertArray();
  nsresult UpdateUIContents();

  nsresult GetCertsByTypeFromCertList(
      const nsTArray<RefPtr<nsIX509Cert>>& aCertList, uint32_t aWantedType,
      nsCertCompareFunc aCertCmpFn, void* aCertCmpFnArg);

  nsCOMPtr<nsIMutableArray> mCellText;
};

#endif /* _NS_CERTTREE_H_ */
