/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCertTree.h"

#include "ScopedNSSTypes.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/intl/AppDateTimeFormat.h"
#include "nsArray.h"
#include "nsArrayUtils.h"
#include "nsHashKeys.h"
#include "nsISupportsPrimitives.h"
#include "nsIX509CertDB.h"
#include "nsIX509Cert.h"
#include "nsIX509CertValidity.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertificate.h"
#include "nsComponentManagerUtils.h"
#include "nsNSSCertificateDB.h"
#include "nsNSSHelper.h"
#include "nsReadableUtils.h"
#include "nsTHashtable.h"
#include "nsUnicharUtils.h"
#include "nsXPCOMCID.h"
#include "nsString.h"
#include "nsTreeColumns.h"
#include "mozpkix/pkixtypes.h"

using namespace mozilla;

extern LazyLogModule gPIPNSSLog;

// treeArrayElStr
//
// structure used to hold map of tree.  Each thread (an organization
// field from a cert) has an element in the array.  The numChildren field
// stores the number of certs corresponding to that thread.
struct treeArrayElStr {
  nsString orgName;    /* heading for thread                   */
  bool open;           /* toggle open state for thread         */
  int32_t certIndex;   /* index into cert array for 1st cert   */
  int32_t numChildren; /* number of chidren (certs) for thread */
};

CompareCacheHashEntryPtr::CompareCacheHashEntryPtr() {
  entry = new CompareCacheHashEntry;
}

CompareCacheHashEntryPtr::~CompareCacheHashEntryPtr() { delete entry; }

CompareCacheHashEntry::CompareCacheHashEntry() : key(nullptr), mCritInit() {
  for (int i = 0; i < max_criterions; ++i) {
    mCritInit[i] = false;
    mCrit[i].SetIsVoid(true);
  }
}

static bool CompareCacheMatchEntry(const PLDHashEntryHdr* hdr,
                                   const void* key) {
  const CompareCacheHashEntryPtr* entryPtr =
      static_cast<const CompareCacheHashEntryPtr*>(hdr);
  return entryPtr->entry->key == key;
}

static void CompareCacheInitEntry(PLDHashEntryHdr* hdr, const void* key) {
  new (hdr) CompareCacheHashEntryPtr();
  CompareCacheHashEntryPtr* entryPtr =
      static_cast<CompareCacheHashEntryPtr*>(hdr);
  entryPtr->entry->key = (void*)key;
}

static void CompareCacheClearEntry(PLDHashTable* table, PLDHashEntryHdr* hdr) {
  CompareCacheHashEntryPtr* entryPtr =
      static_cast<CompareCacheHashEntryPtr*>(hdr);
  entryPtr->~CompareCacheHashEntryPtr();
}

static const PLDHashTableOps gMapOps = {
    PLDHashTable::HashVoidPtrKeyStub, CompareCacheMatchEntry,
    PLDHashTable::MoveEntryStub, CompareCacheClearEntry, CompareCacheInitEntry};

NS_IMPL_ISUPPORTS(nsCertTreeDispInfo, nsICertTreeItem)

nsCertTreeDispInfo::~nsCertTreeDispInfo() = default;

NS_IMETHODIMP
nsCertTreeDispInfo::GetCert(nsIX509Cert** aCert) {
  NS_ENSURE_ARG(aCert);
  nsCOMPtr<nsIX509Cert> cert = mCert;
  cert.forget(aCert);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCertTree, nsICertTree, nsITreeView)

nsCertTree::nsCertTree()
    : mTreeArray(nullptr),
      mNumOrgs(0),
      mNumRows(0),
      mCompareCache(&gMapOps, sizeof(CompareCacheHashEntryPtr),
                    kInitialCacheLength) {
  mCellText = nullptr;
}

void nsCertTree::ClearCompareHash() {
  mCompareCache.ClearAndPrepareForLength(kInitialCacheLength);
}

nsCertTree::~nsCertTree() { delete[] mTreeArray; }

void nsCertTree::FreeCertArray() { mDispInfo.Clear(); }

CompareCacheHashEntry* nsCertTree::getCacheEntry(void* cache, void* aCert) {
  PLDHashTable& aCompareCache = *static_cast<PLDHashTable*>(cache);
  auto entryPtr = static_cast<CompareCacheHashEntryPtr*>(
      aCompareCache.Add(aCert, fallible));
  return entryPtr ? entryPtr->entry : nullptr;
}

void nsCertTree::RemoveCacheEntry(void* key) { mCompareCache.Remove(key); }

// CountOrganizations
//
// Count the number of different organizations encountered in the cert
// list.
int32_t nsCertTree::CountOrganizations() {
  uint32_t i, certCount;
  certCount = mDispInfo.Length();
  if (certCount == 0) return 0;
  nsCOMPtr<nsIX509Cert> orgCert = mDispInfo.ElementAt(0)->mCert;
  nsCOMPtr<nsIX509Cert> nextCert = nullptr;
  int32_t orgCount = 1;
  for (i = 1; i < certCount; i++) {
    nextCert = mDispInfo.SafeElementAt(i, nullptr)->mCert;
    // XXX we assume issuer org is always criterion 1
    if (CmpBy(&mCompareCache, orgCert, nextCert, sort_IssuerOrg, sort_None,
              sort_None) != 0) {
      orgCert = nextCert;
      orgCount++;
    }
  }
  return orgCount;
}

// GetThreadDescAtIndex
//
// If the row at index is an organization thread, return the collection
// associated with that thread.  Otherwise, return null.
treeArrayEl* nsCertTree::GetThreadDescAtIndex(int32_t index) {
  int i, idx = 0;
  if (index < 0) return nullptr;
  for (i = 0; i < mNumOrgs; i++) {
    if (index == idx) {
      return &mTreeArray[i];
    }
    if (mTreeArray[i].open) {
      idx += mTreeArray[i].numChildren;
    }
    idx++;
    if (idx > index) break;
  }
  return nullptr;
}

//  GetCertAtIndex
//
//  If the row at index is a cert, return that cert.  Otherwise, return null.
already_AddRefed<nsIX509Cert> nsCertTree::GetCertAtIndex(
    int32_t index, int32_t* outAbsoluteCertOffset) {
  RefPtr<nsCertTreeDispInfo> certdi(
      GetDispInfoAtIndex(index, outAbsoluteCertOffset));
  if (!certdi) return nullptr;

  nsCOMPtr<nsIX509Cert> ret = certdi->mCert;
  return ret.forget();
}

//  If the row at index is a cert, return that cert.  Otherwise, return null.
already_AddRefed<nsCertTreeDispInfo> nsCertTree::GetDispInfoAtIndex(
    int32_t index, int32_t* outAbsoluteCertOffset) {
  int i, idx = 0, cIndex = 0, nc;
  if (index < 0) return nullptr;
  // Loop over the threads
  for (i = 0; i < mNumOrgs; i++) {
    if (index == idx) return nullptr;  // index is for thread
    idx++;                             // get past the thread
    nc = (mTreeArray[i].open) ? mTreeArray[i].numChildren : 0;
    if (index < idx + nc) {  // cert is within range of this thread
      int32_t certIndex = cIndex + index - idx;
      if (outAbsoluteCertOffset) *outAbsoluteCertOffset = certIndex;
      RefPtr<nsCertTreeDispInfo> certdi(
          mDispInfo.SafeElementAt(certIndex, nullptr));
      if (certdi) {
        return certdi.forget();
      }
      break;
    }
    if (mTreeArray[i].open) idx += mTreeArray[i].numChildren;
    cIndex += mTreeArray[i].numChildren;
    if (idx > index) break;
  }
  return nullptr;
}

nsCertTree::nsCertCompareFunc nsCertTree::GetCompareFuncFromCertType(
    uint32_t aType) {
  switch (aType) {
    case nsIX509Cert::ANY_CERT:
    case nsIX509Cert::USER_CERT:
      return CmpUserCert;
    case nsIX509Cert::EMAIL_CERT:
      return CmpEmailCert;
    case nsIX509Cert::CA_CERT:
    default:
      return CmpCACert;
  }
}

nsresult nsCertTree::GetCertsByTypeFromCertList(
    const nsTArray<RefPtr<nsIX509Cert>>& aCertList, uint32_t aWantedType,
    nsCertCompareFunc aCertCmpFn, void* aCertCmpFnArg) {
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("GetCertsByTypeFromCertList"));

  nsTHashtable<nsCStringHashKey> allHostPortOverrideKeys;

  if (aWantedType == nsIX509Cert::SERVER_CERT) {
    return NS_ERROR_INVALID_ARG;
  }

  int count = 0;
  for (const auto& cert : aCertList) {
    bool wantThisCert = (aWantedType == nsIX509Cert::ANY_CERT);

    if (!wantThisCert) {
      uint32_t thisCertType;
      nsresult rv = cert->GetCertType(&thisCertType);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (thisCertType == aWantedType) {
        wantThisCert = true;
      }
    }

    if (wantThisCert) {
      int InsertPosition = 0;
      for (; InsertPosition < count; ++InsertPosition) {
        nsCOMPtr<nsIX509Cert> otherCert = nullptr;
        RefPtr<nsCertTreeDispInfo> elem(
            mDispInfo.SafeElementAt(InsertPosition, nullptr));
        if (elem) {
          otherCert = elem->mCert;
        }
        if ((*aCertCmpFn)(aCertCmpFnArg, cert, otherCert) < 0) {
          break;
        }
      }
      nsCertTreeDispInfo* certdi = new nsCertTreeDispInfo(cert);
      mDispInfo.InsertElementAt(InsertPosition, certdi);
      ++count;
      ++InsertPosition;
    }
  }

  return NS_OK;
}

// LoadCerts
//
// Load all of the certificates in the DB for this type.  Sort them
// by token, organization, then common name.
NS_IMETHODIMP
nsCertTree::LoadCertsFromCache(const nsTArray<RefPtr<nsIX509Cert>>& aCache,
                               uint32_t aType) {
  if (mTreeArray) {
    FreeCertArray();
    delete[] mTreeArray;
    mTreeArray = nullptr;
    mNumRows = 0;
  }
  ClearCompareHash();

  nsresult rv = GetCertsByTypeFromCertList(
      aCache, aType, GetCompareFuncFromCertType(aType), &mCompareCache);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return UpdateUIContents();
}

nsresult nsCertTree::UpdateUIContents() {
  uint32_t count = mDispInfo.Length();
  mNumOrgs = CountOrganizations();
  mTreeArray = new treeArrayEl[mNumOrgs];

  mCellText = nsArrayBase::Create();

  if (count) {
    uint32_t j = 0;
    nsCOMPtr<nsIX509Cert> orgCert = mDispInfo.ElementAt(j)->mCert;
    for (int32_t i = 0; i < mNumOrgs; i++) {
      nsString& orgNameRef = mTreeArray[i].orgName;
      if (!orgCert) {
        GetPIPNSSBundleString("CertOrgUnknown", orgNameRef);
      } else {
        orgCert->GetIssuerOrganization(orgNameRef);
        if (orgNameRef.IsEmpty()) orgCert->GetCommonName(orgNameRef);
      }
      mTreeArray[i].open = true;
      mTreeArray[i].certIndex = j;
      mTreeArray[i].numChildren = 1;
      if (++j >= count) break;
      nsCOMPtr<nsIX509Cert> nextCert =
          mDispInfo.SafeElementAt(j, nullptr)->mCert;
      while (0 == CmpBy(&mCompareCache, orgCert, nextCert, sort_IssuerOrg,
                        sort_None, sort_None)) {
        mTreeArray[i].numChildren++;
        if (++j >= count) break;
        nextCert = mDispInfo.SafeElementAt(j, nullptr)->mCert;
      }
      orgCert = nextCert;
    }
  }
  if (mTree) {
    mTree->BeginUpdateBatch();
    mTree->RowCountChanged(0, -mNumRows);
  }
  mNumRows = count + mNumOrgs;
  if (mTree) mTree->EndUpdateBatch();
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::DeleteEntryObject(uint32_t index) {
  if (!mTreeArray) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIX509CertDB> certdb =
      do_GetService("@mozilla.org/security/x509certdb;1");
  if (!certdb) {
    return NS_ERROR_FAILURE;
  }

  int i;
  uint32_t idx = 0, cIndex = 0, nc;
  // Loop over the threads
  for (i = 0; i < mNumOrgs; i++) {
    if (index == idx) return NS_OK;  // index is for thread
    idx++;                           // get past the thread
    nc = (mTreeArray[i].open) ? mTreeArray[i].numChildren : 0;
    if (index < idx + nc) {  // cert is within range of this thread
      int32_t certIndex = cIndex + index - idx;

      RefPtr<nsCertTreeDispInfo> certdi(
          mDispInfo.SafeElementAt(certIndex, nullptr));
      if (certdi) {
        nsCOMPtr<nsIX509Cert> cert = certdi->mCert;
        RemoveCacheEntry(cert);
        certdb->DeleteCertificate(cert);
      }

      mDispInfo.RemoveElementAt(certIndex);

      delete[] mTreeArray;
      mTreeArray = nullptr;
      return UpdateUIContents();
    }
    if (mTreeArray[i].open) idx += mTreeArray[i].numChildren;
    cIndex += mTreeArray[i].numChildren;
    if (idx > index) break;
  }
  return NS_ERROR_FAILURE;
}

//////////////////////////////////////////////////////////////////////////////
//
//  Begin nsITreeView methods
//
/////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsCertTree::GetCert(uint32_t aIndex, nsIX509Cert** _cert) {
  NS_ENSURE_ARG(_cert);
  *_cert = GetCertAtIndex(aIndex).take();
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::GetTreeItem(uint32_t aIndex, nsICertTreeItem** _treeitem) {
  NS_ENSURE_ARG(_treeitem);

  RefPtr<nsCertTreeDispInfo> certdi(GetDispInfoAtIndex(aIndex));
  if (!certdi) return NS_ERROR_FAILURE;

  *_treeitem = certdi;
  NS_IF_ADDREF(*_treeitem);
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::GetRowCount(int32_t* aRowCount) {
  if (!mTreeArray) return NS_ERROR_NOT_INITIALIZED;
  uint32_t count = 0;
  for (int32_t i = 0; i < mNumOrgs; i++) {
    if (mTreeArray[i].open) {
      count += mTreeArray[i].numChildren;
    }
    count++;
  }
  *aRowCount = count;
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::GetSelection(nsITreeSelection** aSelection) {
  *aSelection = mSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::SetSelection(nsITreeSelection* aSelection) {
  mSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::GetRowProperties(int32_t index, nsAString& aProps) { return NS_OK; }

NS_IMETHODIMP
nsCertTree::GetCellProperties(int32_t row, nsTreeColumn* col,
                              nsAString& aProps) {
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::GetColumnProperties(nsTreeColumn* col, nsAString& aProps) {
  return NS_OK;
}
NS_IMETHODIMP
nsCertTree::IsContainer(int32_t index, bool* _retval) {
  if (!mTreeArray) return NS_ERROR_NOT_INITIALIZED;
  treeArrayEl* el = GetThreadDescAtIndex(index);
  if (el) {
    *_retval = true;
  } else {
    *_retval = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::IsContainerOpen(int32_t index, bool* _retval) {
  if (!mTreeArray) return NS_ERROR_NOT_INITIALIZED;
  treeArrayEl* el = GetThreadDescAtIndex(index);
  if (el && el->open) {
    *_retval = true;
  } else {
    *_retval = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::IsContainerEmpty(int32_t index, bool* _retval) {
  *_retval = !mTreeArray;
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::IsSeparator(int32_t index, bool* _retval) {
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::GetParentIndex(int32_t rowIndex, int32_t* _retval) {
  if (!mTreeArray) return NS_ERROR_NOT_INITIALIZED;
  int i, idx = 0;
  for (i = 0; i < mNumOrgs && idx < rowIndex; i++, idx++) {
    if (mTreeArray[i].open) {
      if (rowIndex <= idx + mTreeArray[i].numChildren) {
        *_retval = idx;
        return NS_OK;
      }
      idx += mTreeArray[i].numChildren;
    }
  }
  *_retval = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::HasNextSibling(int32_t rowIndex, int32_t afterIndex,
                           bool* _retval) {
  if (!mTreeArray) return NS_ERROR_NOT_INITIALIZED;

  int i, idx = 0;
  for (i = 0; i < mNumOrgs && idx <= rowIndex; i++, idx++) {
    if (mTreeArray[i].open) {
      idx += mTreeArray[i].numChildren;
      if (afterIndex <= idx) {
        *_retval = afterIndex < idx;
        return NS_OK;
      }
    }
  }
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::GetLevel(int32_t index, int32_t* _retval) {
  if (!mTreeArray) return NS_ERROR_NOT_INITIALIZED;
  treeArrayEl* el = GetThreadDescAtIndex(index);
  if (el) {
    *_retval = 0;
  } else {
    *_retval = 1;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::GetImageSrc(int32_t row, nsTreeColumn* col, nsAString& _retval) {
  _retval.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::GetCellValue(int32_t row, nsTreeColumn* col, nsAString& _retval) {
  _retval.Truncate();
  return NS_OK;
}

static void PRTimeToLocalDateString(PRTime time, nsAString& result) {
  PRExplodedTime explodedTime;
  PR_ExplodeTime(time, PR_LocalTimeParameters, &explodedTime);
  intl::DateTimeFormat::StyleBag style;
  style.date = Some(intl::DateTimeFormat::Style::Long);
  style.time = Nothing();
  Unused << intl::AppDateTimeFormat::Format(style, &explodedTime, result);
}

NS_IMETHODIMP
nsCertTree::GetCellText(int32_t row, nsTreeColumn* col, nsAString& _retval) {
  if (!mTreeArray) return NS_ERROR_NOT_INITIALIZED;

  nsresult rv = NS_OK;
  _retval.Truncate();

  const nsAString& colID = col->GetId();

  treeArrayEl* el = GetThreadDescAtIndex(row);
  if (el) {
    if (u"certcol"_ns.Equals(colID))
      _retval.Assign(el->orgName);
    else
      _retval.Truncate();
    return NS_OK;
  }

  int32_t absoluteCertOffset;
  RefPtr<nsCertTreeDispInfo> certdi(
      GetDispInfoAtIndex(row, &absoluteCertOffset));
  if (!certdi) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIX509Cert> cert = certdi->mCert;

  int32_t colIndex = col->Index();
  uint32_t arrayIndex = absoluteCertOffset + colIndex * (mNumRows - mNumOrgs);
  uint32_t arrayLength = 0;
  if (mCellText) {
    mCellText->GetLength(&arrayLength);
  }
  if (arrayIndex < arrayLength) {
    nsCOMPtr<nsISupportsString> myString(
        do_QueryElementAt(mCellText, arrayIndex));
    if (myString) {
      myString->GetData(_retval);
      return NS_OK;
    }
  }

  if (u"certcol"_ns.Equals(colID)) {
    if (!cert) {
      rv = GetPIPNSSBundleString("CertNotStored", _retval);
    } else {
      rv = cert->GetDisplayName(_retval);
    }
  } else if (u"tokencol"_ns.Equals(colID) && cert) {
    rv = cert->GetTokenName(_retval);
  } else if (u"emailcol"_ns.Equals(colID) && cert) {
    rv = cert->GetEmailAddress(_retval);
  } else if (u"issuedcol"_ns.Equals(colID) && cert) {
    nsCOMPtr<nsIX509CertValidity> validity;

    rv = cert->GetValidity(getter_AddRefs(validity));
    if (NS_SUCCEEDED(rv)) {
      PRTime notBefore;
      rv = validity->GetNotBefore(&notBefore);
      if (NS_SUCCEEDED(rv)) {
        PRTimeToLocalDateString(notBefore, _retval);
      }
    }
  } else if (u"expiredcol"_ns.Equals(colID) && cert) {
    nsCOMPtr<nsIX509CertValidity> validity;

    rv = cert->GetValidity(getter_AddRefs(validity));
    if (NS_SUCCEEDED(rv)) {
      PRTime notAfter;
      rv = validity->GetNotAfter(&notAfter);
      if (NS_SUCCEEDED(rv)) {
        PRTimeToLocalDateString(notAfter, _retval);
      }
    }
  } else if (u"serialnumcol"_ns.Equals(colID) && cert) {
    rv = cert->GetSerialNumber(_retval);
  } else {
    return NS_ERROR_FAILURE;
  }
  if (mCellText) {
    nsCOMPtr<nsISupportsString> text(
        do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    text->SetData(_retval);
    mCellText->ReplaceElementAt(text, arrayIndex);
  }
  return rv;
}

NS_IMETHODIMP
nsCertTree::SetTree(mozilla::dom::XULTreeElement* tree) {
  mTree = tree;
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::ToggleOpenState(int32_t index) {
  if (!mTreeArray) return NS_ERROR_NOT_INITIALIZED;
  treeArrayEl* el = GetThreadDescAtIndex(index);
  if (el) {
    el->open = !el->open;
    int32_t newChildren = (el->open) ? el->numChildren : -el->numChildren;
    if (mTree) {
      mTree->RowCountChanged(index + 1, newChildren);
      mTree->InvalidateRow(index);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::CycleHeader(nsTreeColumn* col) { return NS_OK; }

NS_IMETHODIMP
nsCertTree::SelectionChangedXPCOM() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
nsCertTree::CycleCell(int32_t row, nsTreeColumn* col) { return NS_OK; }

NS_IMETHODIMP
nsCertTree::IsEditable(int32_t row, nsTreeColumn* col, bool* _retval) {
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::SetCellValue(int32_t row, nsTreeColumn* col,
                         const nsAString& value) {
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::SetCellText(int32_t row, nsTreeColumn* col,
                        const nsAString& value) {
  return NS_OK;
}

//
// CanDrop
//
NS_IMETHODIMP nsCertTree::CanDrop(int32_t index, int32_t orientation,
                                  mozilla::dom::DataTransfer* aDataTransfer,
                                  bool* _retval) {
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;

  return NS_OK;
}

//
// Drop
//
NS_IMETHODIMP nsCertTree::Drop(int32_t row, int32_t orient,
                               mozilla::dom::DataTransfer* aDataTransfer) {
  return NS_OK;
}

//
// IsSorted
//
// ...
//
NS_IMETHODIMP nsCertTree::IsSorted(bool* _retval) {
  *_retval = false;
  return NS_OK;
}

#define RETURN_NOTHING

void nsCertTree::CmpInitCriterion(nsIX509Cert* cert,
                                  CompareCacheHashEntry* entry,
                                  sortCriterion crit, int32_t level) {
  NS_ENSURE_TRUE(cert && entry, RETURN_NOTHING);

  entry->mCritInit[level] = true;
  nsString& str = entry->mCrit[level];

  switch (crit) {
    case sort_IssuerOrg:
      cert->GetIssuerOrganization(str);
      if (str.IsEmpty()) cert->GetCommonName(str);
      break;
    case sort_Org:
      cert->GetOrganization(str);
      break;
    case sort_Token:
      cert->GetTokenName(str);
      break;
    case sort_CommonName:
      cert->GetCommonName(str);
      break;
    case sort_IssuedDateDescending: {
      nsresult rv;
      nsCOMPtr<nsIX509CertValidity> validity;
      PRTime notBefore;

      rv = cert->GetValidity(getter_AddRefs(validity));
      if (NS_SUCCEEDED(rv)) {
        rv = validity->GetNotBefore(&notBefore);
      }

      if (NS_SUCCEEDED(rv)) {
        PRExplodedTime explodedTime;
        PR_ExplodeTime(notBefore, PR_GMTParameters, &explodedTime);
        char datebuf[20];  // 4 + 2 + 2 + 2 + 2 + 2 + 1 = 15
        if (0 != PR_FormatTime(datebuf, sizeof(datebuf), "%Y%m%d%H%M%S",
                               &explodedTime)) {
          str = NS_ConvertASCIItoUTF16(nsDependentCString(datebuf));
        }
      }
    } break;
    case sort_Email:
      cert->GetEmailAddress(str);
      break;
    case sort_None:
    default:
      break;
  }
}

int32_t nsCertTree::CmpByCrit(nsIX509Cert* a, CompareCacheHashEntry* ace,
                              nsIX509Cert* b, CompareCacheHashEntry* bce,
                              sortCriterion crit, int32_t level) {
  NS_ENSURE_TRUE(a && ace && b && bce, 0);

  if (!ace->mCritInit[level]) {
    CmpInitCriterion(a, ace, crit, level);
  }

  if (!bce->mCritInit[level]) {
    CmpInitCriterion(b, bce, crit, level);
  }

  nsString& str_a = ace->mCrit[level];
  nsString& str_b = bce->mCrit[level];

  int32_t result;
  if (!str_a.IsVoid() && !str_b.IsVoid())
    result = Compare(str_a, str_b, nsCaseInsensitiveStringComparator);
  else
    result = str_a.IsVoid() ? (str_b.IsVoid() ? 0 : -1) : 1;

  if (sort_IssuedDateDescending == crit) result *= -1;  // reverse compare order

  return result;
}

int32_t nsCertTree::CmpBy(void* cache, nsIX509Cert* a, nsIX509Cert* b,
                          sortCriterion c0, sortCriterion c1,
                          sortCriterion c2) {
  // This will be called when comparing items for display sorting.
  // Some items might have no cert associated, so either a or b is null.
  // We want all those orphans show at the top of the list,
  // so we treat a null cert as "smaller" by returning -1.
  // We don't try to sort within the group of no-cert entries,
  // so we treat them as equal wrt sort order.

  if (!a && !b) return 0;

  if (!a) return -1;

  if (!b) return 1;

  NS_ENSURE_TRUE(cache && a && b, 0);

  CompareCacheHashEntry* ace = getCacheEntry(cache, a);
  CompareCacheHashEntry* bce = getCacheEntry(cache, b);

  int32_t cmp;
  cmp = CmpByCrit(a, ace, b, bce, c0, 0);
  if (cmp != 0) return cmp;

  if (c1 != sort_None) {
    cmp = CmpByCrit(a, ace, b, bce, c1, 1);
    if (cmp != 0) return cmp;

    if (c2 != sort_None) {
      return CmpByCrit(a, ace, b, bce, c2, 2);
    }
  }

  return cmp;
}

int32_t nsCertTree::CmpCACert(void* cache, nsIX509Cert* a, nsIX509Cert* b) {
  // XXX we assume issuer org is always criterion 1
  return CmpBy(cache, a, b, sort_IssuerOrg, sort_Org, sort_Token);
}

int32_t nsCertTree::CmpUserCert(void* cache, nsIX509Cert* a, nsIX509Cert* b) {
  // XXX we assume issuer org is always criterion 1
  return CmpBy(cache, a, b, sort_IssuerOrg, sort_Token,
               sort_IssuedDateDescending);
}

int32_t nsCertTree::CmpEmailCert(void* cache, nsIX509Cert* a, nsIX509Cert* b) {
  // XXX we assume issuer org is always criterion 1
  return CmpBy(cache, a, b, sort_IssuerOrg, sort_Email, sort_CommonName);
}
