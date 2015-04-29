/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCertTree.h"

#include "pkix/pkixtypes.h"
#include "nsNSSComponent.h" // for PIPNSS string bundle calls.
#include "nsITreeColumns.h"
#include "nsIX509Cert.h"
#include "nsIX509CertValidity.h"
#include "nsIX509CertDB.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsNSSCertificate.h"
#include "nsNSSCertHelper.h"
#include "nsIMutableArray.h"
#include "nsArrayUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsXPCOMCID.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

#include "prlog.h"

using namespace mozilla;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

static NS_DEFINE_CID(kCertOverrideCID, NS_CERTOVERRIDE_CID);

// treeArrayElStr
//
// structure used to hold map of tree.  Each thread (an organization
// field from a cert) has an element in the array.  The numChildren field
// stores the number of certs corresponding to that thread.
struct treeArrayElStr {
  nsString   orgName;     /* heading for thread                   */
  bool       open;        /* toggle open state for thread         */
  int32_t    certIndex;   /* index into cert array for 1st cert   */
  int32_t    numChildren; /* number of chidren (certs) for thread */
};

CompareCacheHashEntryPtr::CompareCacheHashEntryPtr()
{
  entry = new CompareCacheHashEntry;
}

CompareCacheHashEntryPtr::~CompareCacheHashEntryPtr()
{
  delete entry;
}

CompareCacheHashEntry::CompareCacheHashEntry()
:key(nullptr)
{
  for (int i = 0; i < max_criterions; ++i) {
    mCritInit[i] = false;
  }
}

static bool
CompareCacheMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                         const void *key)
{
  const CompareCacheHashEntryPtr *entryPtr = static_cast<const CompareCacheHashEntryPtr*>(hdr);
  return entryPtr->entry->key == key;
}

static void
CompareCacheInitEntry(PLDHashEntryHdr *hdr, const void *key)
{
  new (hdr) CompareCacheHashEntryPtr();
  CompareCacheHashEntryPtr *entryPtr = static_cast<CompareCacheHashEntryPtr*>(hdr);
  entryPtr->entry->key = (void*)key;
}

static void
CompareCacheClearEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  CompareCacheHashEntryPtr *entryPtr = static_cast<CompareCacheHashEntryPtr*>(hdr);
  entryPtr->~CompareCacheHashEntryPtr();
}

static const PLDHashTableOps gMapOps = {
  PL_DHashVoidPtrKeyStub,
  CompareCacheMatchEntry,
  PL_DHashMoveEntryStub,
  CompareCacheClearEntry,
  CompareCacheInitEntry
};

NS_IMPL_ISUPPORTS0(nsCertAddonInfo)
NS_IMPL_ISUPPORTS(nsCertTreeDispInfo, nsICertTreeItem)

nsCertTreeDispInfo::nsCertTreeDispInfo()
:mAddonInfo(nullptr)
,mTypeOfEntry(direct_db)
,mPort(-1)
,mOverrideBits(nsCertOverride::ob_None)
,mIsTemporary(true)
{
}

nsCertTreeDispInfo::nsCertTreeDispInfo(nsCertTreeDispInfo &other)
{
  mAddonInfo = other.mAddonInfo;
  mTypeOfEntry = other.mTypeOfEntry;
  mAsciiHost = other.mAsciiHost;
  mPort = other.mPort;
  mOverrideBits = other.mOverrideBits;
  mIsTemporary = other.mIsTemporary;
  mCert = other.mCert;
}

nsCertTreeDispInfo::~nsCertTreeDispInfo()
{
}

NS_IMETHODIMP
nsCertTreeDispInfo::GetCert(nsIX509Cert **_cert)
{
  NS_ENSURE_ARG(_cert);
  if (mCert) {
    // we may already have the cert for temporary overrides
    *_cert = mCert;
    NS_IF_ADDREF(*_cert);
    return NS_OK;
  }
  if (mAddonInfo) {
    *_cert = mAddonInfo->mCert.get();
    NS_IF_ADDREF(*_cert);
  }
  else {
    *_cert = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCertTreeDispInfo::GetHostPort(nsAString &aHostPort)
{
  nsAutoCString hostPort;
  nsCertOverrideService::GetHostWithPort(mAsciiHost, mPort, hostPort);
  aHostPort = NS_ConvertUTF8toUTF16(hostPort);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCertTree, nsICertTree, nsITreeView)

nsCertTree::nsCertTree() : mTreeArray(nullptr)
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  mNSSComponent = do_GetService(kNSSComponentCID);
  mOverrideService = do_GetService("@mozilla.org/security/certoverride;1");
  // Might be a different service if someone is overriding the contract
  nsCOMPtr<nsICertOverrideService> origCertOverride =
    do_GetService(kCertOverrideCID);
  mOriginalOverrideService =
    static_cast<nsCertOverrideService*>(origCertOverride.get());
  mCellText = nullptr;
}

void nsCertTree::ClearCompareHash()
{
  if (mCompareCache.IsInitialized()) {
    PL_DHashTableFinish(&mCompareCache);
  }
}

nsresult nsCertTree::InitCompareHash()
{
  ClearCompareHash();
  PL_DHashTableInit(&mCompareCache, &gMapOps,
                    sizeof(CompareCacheHashEntryPtr), 64);
  return NS_OK;
}

nsCertTree::~nsCertTree()
{
  ClearCompareHash();
  delete [] mTreeArray;
}

void
nsCertTree::FreeCertArray()
{
  mDispInfo.Clear();
}

CompareCacheHashEntry *
nsCertTree::getCacheEntry(void *cache, void *aCert)
{
  PLDHashTable &aCompareCache = *reinterpret_cast<PLDHashTable*>(cache);
  CompareCacheHashEntryPtr *entryPtr = static_cast<CompareCacheHashEntryPtr*>
    (PL_DHashTableAdd(&aCompareCache, aCert, fallible));
  return entryPtr ? entryPtr->entry : nullptr;
}

void nsCertTree::RemoveCacheEntry(void *key)
{
  PL_DHashTableRemove(&mCompareCache, key);
}

// CountOrganizations
//
// Count the number of different organizations encountered in the cert
// list.
int32_t
nsCertTree::CountOrganizations()
{
  uint32_t i, certCount;
  certCount = mDispInfo.Length();
  if (certCount == 0) return 0;
  nsCOMPtr<nsIX509Cert> orgCert = nullptr;
  nsCertAddonInfo *addonInfo = mDispInfo.ElementAt(0)->mAddonInfo;
  if (addonInfo) {
    orgCert = addonInfo->mCert;
  }
  nsCOMPtr<nsIX509Cert> nextCert = nullptr;
  int32_t orgCount = 1;
  for (i=1; i<certCount; i++) {
    nextCert = nullptr;
    addonInfo = mDispInfo.SafeElementAt(i, nullptr)->mAddonInfo;
    if (addonInfo) {
      nextCert = addonInfo->mCert;
    }
    // XXX we assume issuer org is always criterion 1
    if (CmpBy(&mCompareCache, orgCert, nextCert, sort_IssuerOrg, sort_None, sort_None) != 0) {
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
treeArrayEl *
nsCertTree::GetThreadDescAtIndex(int32_t index)
{
  int i, idx=0;
  if (index < 0) return nullptr;
  for (i=0; i<mNumOrgs; i++) {
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
already_AddRefed<nsIX509Cert>
nsCertTree::GetCertAtIndex(int32_t index, int32_t *outAbsoluteCertOffset)
{
  RefPtr<nsCertTreeDispInfo> certdi(
    GetDispInfoAtIndex(index, outAbsoluteCertOffset));
  if (!certdi)
    return nullptr;

  nsCOMPtr<nsIX509Cert> ret;
  if (certdi->mCert) {
    ret = certdi->mCert;
  } else if (certdi->mAddonInfo) {
    ret = certdi->mAddonInfo->mCert;
  }
  return ret.forget();
}

//  If the row at index is a cert, return that cert.  Otherwise, return null.
TemporaryRef<nsCertTreeDispInfo>
nsCertTree::GetDispInfoAtIndex(int32_t index, 
                               int32_t *outAbsoluteCertOffset)
{
  int i, idx = 0, cIndex = 0, nc;
  if (index < 0) return nullptr;
  // Loop over the threads
  for (i=0; i<mNumOrgs; i++) {
    if (index == idx) return nullptr; // index is for thread
    idx++; // get past the thread
    nc = (mTreeArray[i].open) ? mTreeArray[i].numChildren : 0;
    if (index < idx + nc) { // cert is within range of this thread
      int32_t certIndex = cIndex + index - idx;
      if (outAbsoluteCertOffset)
        *outAbsoluteCertOffset = certIndex;
      RefPtr<nsCertTreeDispInfo> certdi(mDispInfo.SafeElementAt(certIndex,
                                                                nullptr));
      if (certdi) {
        return certdi.forget();
      }
      break;
    }
    if (mTreeArray[i].open)
      idx += mTreeArray[i].numChildren;
    cIndex += mTreeArray[i].numChildren;
    if (idx > index) break;
  }
  return nullptr;
}

nsCertTree::nsCertCompareFunc
nsCertTree::GetCompareFuncFromCertType(uint32_t aType)
{
  switch (aType) {
    case nsIX509Cert::ANY_CERT:
    case nsIX509Cert::USER_CERT:
      return CmpUserCert;
    case nsIX509Cert::CA_CERT:
      return CmpCACert;
    case nsIX509Cert::EMAIL_CERT:
      return CmpEmailCert;
    case nsIX509Cert::SERVER_CERT:
    default:
      return CmpWebSiteCert;
  }
}

struct nsCertAndArrayAndPositionAndCounterAndTracker
{
  RefPtr<nsCertAddonInfo> certai;
  nsTArray< RefPtr<nsCertTreeDispInfo> > *array;
  int position;
  int counter;
  nsTHashtable<nsCStringHashKey> *tracker;
};

// Used to enumerate host:port overrides that match a stored
// certificate, creates and adds a display-info-object to the
// provided array. Increments insert position and entry counter.
// We remove the given key from the tracker, which is used to 
// track entries that have not yet been handled.
// The created display-info references the cert, so make a note
// of that by incrementing the cert usage counter.
static void
MatchingCertOverridesCallback(const nsCertOverride &aSettings,
                              void *aUserData)
{
  nsCertAndArrayAndPositionAndCounterAndTracker *cap = 
    (nsCertAndArrayAndPositionAndCounterAndTracker*)aUserData;
  if (!cap)
    return;

  nsCertTreeDispInfo *certdi = new nsCertTreeDispInfo;
  if (certdi) {
    if (cap->certai)
      cap->certai->mUsageCount++;
    certdi->mAddonInfo = cap->certai;
    certdi->mTypeOfEntry = nsCertTreeDispInfo::host_port_override;
    certdi->mAsciiHost = aSettings.mAsciiHost;
    certdi->mPort = aSettings.mPort;
    certdi->mOverrideBits = aSettings.mOverrideBits;
    certdi->mIsTemporary = aSettings.mIsTemporary;
    certdi->mCert = aSettings.mCert;
    cap->array->InsertElementAt(cap->position, certdi);
    cap->position++;
    cap->counter++;
  }

  // this entry is now associated to a displayed cert, remove
  // it from the list of remaining entries
  nsAutoCString hostPort;
  nsCertOverrideService::GetHostWithPort(aSettings.mAsciiHost, aSettings.mPort, hostPort);
  cap->tracker->RemoveEntry(hostPort);
}

// Used to collect a list of the (unique) host:port keys
// for all stored overrides.
static void
CollectAllHostPortOverridesCallback(const nsCertOverride &aSettings,
                                    void *aUserData)
{
  nsTHashtable<nsCStringHashKey> *collectorTable =
    (nsTHashtable<nsCStringHashKey> *)aUserData;
  if (!collectorTable)
    return;

  nsAutoCString hostPort;
  nsCertOverrideService::GetHostWithPort(aSettings.mAsciiHost, aSettings.mPort, hostPort);
  collectorTable->PutEntry(hostPort);
}

struct nsArrayAndPositionAndCounterAndTracker
{
  nsTArray< RefPtr<nsCertTreeDispInfo> > *array;
  int position;
  int counter;
  nsTHashtable<nsCStringHashKey> *tracker;
};

// Used when enumerating the stored host:port overrides where
// no associated certificate was found in the NSS database.
static void
AddRemaningHostPortOverridesCallback(const nsCertOverride &aSettings,
                                     void *aUserData)
{
  nsArrayAndPositionAndCounterAndTracker *cap = 
    (nsArrayAndPositionAndCounterAndTracker*)aUserData;
  if (!cap)
    return;

  nsAutoCString hostPort;
  nsCertOverrideService::GetHostWithPort(aSettings.mAsciiHost, aSettings.mPort, hostPort);
  if (!cap->tracker->GetEntry(hostPort))
    return;

  // This entry is not associated to any stored cert,
  // so we still need to display it.

  nsCertTreeDispInfo *certdi = new nsCertTreeDispInfo;
  if (certdi) {
    certdi->mAddonInfo = nullptr;
    certdi->mTypeOfEntry = nsCertTreeDispInfo::host_port_override;
    certdi->mAsciiHost = aSettings.mAsciiHost;
    certdi->mPort = aSettings.mPort;
    certdi->mOverrideBits = aSettings.mOverrideBits;
    certdi->mIsTemporary = aSettings.mIsTemporary;
    certdi->mCert = aSettings.mCert;
    cap->array->InsertElementAt(cap->position, certdi);
    cap->position++;
    cap->counter++;
  }
}

nsresult
nsCertTree::GetCertsByTypeFromCertList(CERTCertList *aCertList,
                                       uint32_t aWantedType,
                                       nsCertCompareFunc  aCertCmpFn,
                                       void *aCertCmpFnArg)
{
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("GetCertsByTypeFromCertList"));
  if (!aCertList)
    return NS_ERROR_FAILURE;

  if (!mOriginalOverrideService)
    return NS_ERROR_FAILURE;

  nsTHashtable<nsCStringHashKey> allHostPortOverrideKeys;

  if (aWantedType == nsIX509Cert::SERVER_CERT) {
    mOriginalOverrideService->
      EnumerateCertOverrides(nullptr, 
                             CollectAllHostPortOverridesCallback, 
                             &allHostPortOverrideKeys);
  }

  CERTCertListNode *node;
  int count = 0;
  for (node = CERT_LIST_HEAD(aCertList);
       !CERT_LIST_END(node, aCertList);
       node = CERT_LIST_NEXT(node)) {

    bool wantThisCert = (aWantedType == nsIX509Cert::ANY_CERT);
    bool wantThisCertIfNoOverrides = false;
    bool wantThisCertIfHaveOverrides = false;
    bool addOverrides = false;

    if (!wantThisCert) {
      uint32_t thisCertType = getCertType(node->cert);

      // The output from getCertType is a "guess", which can be wrong.
      // The guess is based on stored trust flags, but for the host:port
      // overrides, we are storing certs without any trust flags associated.
      // So we must check whether the cert really belongs to the 
      // server, email or unknown tab. We will lookup the cert in the override
      // list to come to the decision. Unfortunately, the lookup in the
      // override list is quite expensive. Therefore we are using this 
      // lengthy if/else statement to minimize 
      // the number of override-list-lookups.

      if (aWantedType == nsIX509Cert::SERVER_CERT
          && thisCertType == nsIX509Cert::UNKNOWN_CERT) {
        // This unknown cert was stored without trust
        // Are there host:port based overrides stored?
        // If yes, display them.
        addOverrides = true;
      }
      else
      if (aWantedType == nsIX509Cert::UNKNOWN_CERT
          && thisCertType == nsIX509Cert::UNKNOWN_CERT) {
        // This unknown cert was stored without trust.
        // If there are associated overrides, do not show as unknown.
        // If there are no associated overrides, display as unknown.
        wantThisCertIfNoOverrides = true;
      }
      else
      if (aWantedType == nsIX509Cert::SERVER_CERT
          && thisCertType == nsIX509Cert::SERVER_CERT) {
        // This server cert is explicitly marked as a web site peer, 
        // with or without trust, but editable, so show it
        wantThisCert = true;
        // Are there host:port based overrides stored?
        // If yes, display them.
        addOverrides = true;
      }
      else
      if (aWantedType == nsIX509Cert::SERVER_CERT
          && thisCertType == nsIX509Cert::EMAIL_CERT) {
        // This cert might have been categorized as an email cert
        // because it carries an email address. But is it really one?
        // Our cert categorization is uncertain when it comes to
        // distinguish between email certs and web site certs.
        // So, let's see if we have an override for that cert
        // and if there is, conclude it's really a web site cert.
        addOverrides = true;
      }
      else
      if (aWantedType == nsIX509Cert::EMAIL_CERT
          && thisCertType == nsIX509Cert::EMAIL_CERT) {
        // This cert might have been categorized as an email cert
        // because it carries an email address. But is it really one?
        // Our cert categorization is uncertain when it comes to
        // distinguish between email certs and web site certs.
        // So, let's see if we have an override for that cert
        // and if there is, conclude it's really a web site cert.
        wantThisCertIfNoOverrides = true;
      }
      else
      if (thisCertType == aWantedType) {
        wantThisCert = true;
      }
    }

    nsCOMPtr<nsIX509Cert> pipCert = nsNSSCertificate::Create(node->cert);
    if (!pipCert)
      return NS_ERROR_OUT_OF_MEMORY;

    if (wantThisCertIfNoOverrides || wantThisCertIfHaveOverrides) {
      uint32_t ocount = 0;
      nsresult rv = 
        mOverrideService->IsCertUsedForOverrides(pipCert, 
                                                 true, // we want temporaries
                                                 true, // we want permanents
                                                 &ocount);
      if (wantThisCertIfNoOverrides) {
        if (NS_FAILED(rv) || ocount == 0) {
          // no overrides for this cert
          wantThisCert = true;
        }
      }

      if (wantThisCertIfHaveOverrides) {
        if (NS_SUCCEEDED(rv) && ocount > 0) {
          // there are overrides for this cert
          wantThisCert = true;
        }
      }
    }

    RefPtr<nsCertAddonInfo> certai(new nsCertAddonInfo);
    certai->mCert = pipCert;
    certai->mUsageCount = 0;

    if (wantThisCert || addOverrides) {
      int InsertPosition = 0;
      for (; InsertPosition < count; ++InsertPosition) {
        nsCOMPtr<nsIX509Cert> cert = nullptr;
        RefPtr<nsCertTreeDispInfo> elem(
          mDispInfo.SafeElementAt(InsertPosition, nullptr));
        if (elem && elem->mAddonInfo) {
          cert = elem->mAddonInfo->mCert;
        }
        if ((*aCertCmpFn)(aCertCmpFnArg, pipCert, cert) < 0) {
          break;
        }
      }
      if (wantThisCert) {
        nsCertTreeDispInfo *certdi = new nsCertTreeDispInfo;
        certdi->mAddonInfo = certai;
        certai->mUsageCount++;
        certdi->mTypeOfEntry = nsCertTreeDispInfo::direct_db;
        // not necessary: certdi->mAsciiHost.Clear(); certdi->mPort = -1;
        certdi->mOverrideBits = nsCertOverride::ob_None;
        certdi->mIsTemporary = false;
        mDispInfo.InsertElementAt(InsertPosition, certdi);
        ++count;
        ++InsertPosition;
      }
      if (addOverrides) {
        nsCertAndArrayAndPositionAndCounterAndTracker cap;
        cap.certai = certai;
        cap.array = &mDispInfo;
        cap.position = InsertPosition;
        cap.counter = 0;
        cap.tracker = &allHostPortOverrideKeys;

        mOriginalOverrideService->
          EnumerateCertOverrides(pipCert, MatchingCertOverridesCallback, &cap);
        count += cap.counter;
      }
    }
  }

  if (aWantedType == nsIX509Cert::SERVER_CERT) {
    nsArrayAndPositionAndCounterAndTracker cap;
    cap.array = &mDispInfo;
    cap.position = 0;
    cap.counter = 0;
    cap.tracker = &allHostPortOverrideKeys;
    mOriginalOverrideService->
      EnumerateCertOverrides(nullptr, AddRemaningHostPortOverridesCallback, &cap);
  }

  return NS_OK;
}

nsresult 
nsCertTree::GetCertsByType(uint32_t           aType,
                           nsCertCompareFunc  aCertCmpFn,
                           void              *aCertCmpFnArg)
{
  nsNSSShutDownPreventionLock locker;
  nsCOMPtr<nsIInterfaceRequestor> cxt = new PipUIContext();
  ScopedCERTCertList certList(PK11_ListCerts(PK11CertListUnique, cxt));
  return GetCertsByTypeFromCertList(certList.get(), aType, aCertCmpFn,
                                    aCertCmpFnArg);
}

nsresult
nsCertTree::GetCertsByTypeFromCache(nsIX509CertList   *aCache,
                                    uint32_t           aType,
                                    nsCertCompareFunc  aCertCmpFn,
                                    void              *aCertCmpFnArg)
{
  NS_ENSURE_ARG_POINTER(aCache);
  // GetRawCertList checks for NSS shutdown since we can't do it ourselves here
  // easily. We still have to acquire a shutdown prevention lock to prevent NSS
  // shutting down after GetRawCertList has returned. While cumbersome, this is
  // at least mostly correct. The rest of this implementation doesn't even go
  // this far in attempting to check for or prevent NSS shutdown at the
  // appropriate times. If this were reimplemented at a higher level using
  // more encapsulated types that handled NSS shutdown themselves, we wouldn't
  // be having these kinds of problems.
  nsNSSShutDownPreventionLock locker;
  CERTCertList *certList = reinterpret_cast<CERTCertList*>(aCache->GetRawCertList());
  if (!certList)
    return NS_ERROR_FAILURE;
  return GetCertsByTypeFromCertList(certList, aType, aCertCmpFn, aCertCmpFnArg);
}

// LoadCerts
//
// Load all of the certificates in the DB for this type.  Sort them
// by token, organization, then common name.
NS_IMETHODIMP
nsCertTree::LoadCertsFromCache(nsIX509CertList *aCache, uint32_t aType)
{
  if (mTreeArray) {
    FreeCertArray();
    delete [] mTreeArray;
    mTreeArray = nullptr;
    mNumRows = 0;
  }
  nsresult rv = InitCompareHash();
  if (NS_FAILED(rv)) return rv;

  rv = GetCertsByTypeFromCache(aCache, aType, 
                               GetCompareFuncFromCertType(aType), &mCompareCache);
  if (NS_FAILED(rv)) return rv;
  return UpdateUIContents();
}

NS_IMETHODIMP 
nsCertTree::LoadCerts(uint32_t aType)
{
  if (mTreeArray) {
    FreeCertArray();
    delete [] mTreeArray;
    mTreeArray = nullptr;
    mNumRows = 0;
  }
  nsresult rv = InitCompareHash();
  if (NS_FAILED(rv)) return rv;

  rv = GetCertsByType(aType, 
                      GetCompareFuncFromCertType(aType), &mCompareCache);
  if (NS_FAILED(rv)) return rv;
  return UpdateUIContents();
}

nsresult
nsCertTree::UpdateUIContents()
{
  uint32_t count = mDispInfo.Length();
  mNumOrgs = CountOrganizations();
  mTreeArray = new treeArrayEl[mNumOrgs];

  mCellText = do_CreateInstance(NS_ARRAY_CONTRACTID);

if (count) {
  uint32_t j = 0;
  nsCOMPtr<nsIX509Cert> orgCert = nullptr;
  nsCertAddonInfo *addonInfo = mDispInfo.ElementAt(j)->mAddonInfo;
  if (addonInfo) {
    orgCert = addonInfo->mCert;
  }
  for (int32_t i=0; i<mNumOrgs; i++) {
    nsString &orgNameRef = mTreeArray[i].orgName;
    if (!orgCert) {
      mNSSComponent->GetPIPNSSBundleString("CertOrgUnknown", orgNameRef);
    }
    else {
      orgCert->GetIssuerOrganization(orgNameRef);
      if (orgNameRef.IsEmpty())
        orgCert->GetCommonName(orgNameRef);
    }
    mTreeArray[i].open = true;
    mTreeArray[i].certIndex = j;
    mTreeArray[i].numChildren = 1;
    if (++j >= count) break;
    nsCOMPtr<nsIX509Cert> nextCert = nullptr;
    nsCertAddonInfo *addonInfo = mDispInfo.SafeElementAt(j, nullptr)->mAddonInfo;
    if (addonInfo) {
      nextCert = addonInfo->mCert;
    }
    while (0 == CmpBy(&mCompareCache, orgCert, nextCert, sort_IssuerOrg, sort_None, sort_None)) {
      mTreeArray[i].numChildren++;
      if (++j >= count) break;
      nextCert = nullptr;
      addonInfo = mDispInfo.SafeElementAt(j, nullptr)->mAddonInfo;
      if (addonInfo) {
        nextCert = addonInfo->mCert;
      }
    }
    orgCert = nextCert;
  }
}
  if (mTree) {
    mTree->BeginUpdateBatch();
    mTree->RowCountChanged(0, -mNumRows);
  }
  mNumRows = count + mNumOrgs;
  if (mTree)
    mTree->EndUpdateBatch();
  return NS_OK;
}

NS_IMETHODIMP 
nsCertTree::DeleteEntryObject(uint32_t index)
{
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
  for (i=0; i<mNumOrgs; i++) {
    if (index == idx)
      return NS_OK; // index is for thread
    idx++; // get past the thread
    nc = (mTreeArray[i].open) ? mTreeArray[i].numChildren : 0;
    if (index < idx + nc) { // cert is within range of this thread
      int32_t certIndex = cIndex + index - idx;

      bool canRemoveEntry = false;
      RefPtr<nsCertTreeDispInfo> certdi(mDispInfo.SafeElementAt(certIndex,
                                                                nullptr));
      
      // We will remove the element from the visual tree.
      // Only if we have a certdi, then we can check for additional actions.
      nsCOMPtr<nsIX509Cert> cert = nullptr;
      if (certdi) {
        if (certdi->mAddonInfo) {
          cert = certdi->mAddonInfo->mCert;
        }
        nsCertAddonInfo *addonInfo = certdi->mAddonInfo ? certdi->mAddonInfo : nullptr;
        if (certdi->mTypeOfEntry == nsCertTreeDispInfo::host_port_override) {
          mOverrideService->ClearValidityOverride(certdi->mAsciiHost, certdi->mPort);
          if (addonInfo) {
            addonInfo->mUsageCount--;
            if (addonInfo->mUsageCount == 0) {
              // The certificate stored in the database is no longer
              // referenced by any other object displayed.
              // That means we no longer need to keep it around
              // and really can remove it.
              canRemoveEntry = true;
            }
          } 
        }
        else {
          if (addonInfo && addonInfo->mUsageCount > 1) {
            // user is trying to delete a perm trusted cert,
            // although there are still overrides stored,
            // so, we keep the cert, but remove the trust

            ScopedCERTCertificate nsscert(cert->GetCert());

            if (nsscert) {
              CERTCertTrust trust;
              memset((void*)&trust, 0, sizeof(trust));
            
              SECStatus srv = CERT_DecodeTrustString(&trust, ""); // no override 
              if (srv == SECSuccess) {
                CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), nsscert.get(),
                                     &trust);
              }
            }
          }
          else {
            canRemoveEntry = true;
          }
        }
      }

      mDispInfo.RemoveElementAt(certIndex);

      if (canRemoveEntry) {
        RemoveCacheEntry(cert);
        certdb->DeleteCertificate(cert);
      }

      delete [] mTreeArray;
      mTreeArray = nullptr;
      return UpdateUIContents();
    }
    if (mTreeArray[i].open)
      idx += mTreeArray[i].numChildren;
    cIndex += mTreeArray[i].numChildren;
    if (idx > index)
      break;
  }
  return NS_ERROR_FAILURE;
}

//////////////////////////////////////////////////////////////////////////////
//
//  Begin nsITreeView methods
//
/////////////////////////////////////////////////////////////////////////////

/* nsIX509Cert getCert(in unsigned long index); */
NS_IMETHODIMP
nsCertTree::GetCert(uint32_t aIndex, nsIX509Cert **_cert)
{
  NS_ENSURE_ARG(_cert);
  *_cert = GetCertAtIndex(aIndex).take();
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::GetTreeItem(uint32_t aIndex, nsICertTreeItem **_treeitem)
{
  NS_ENSURE_ARG(_treeitem);

  RefPtr<nsCertTreeDispInfo> certdi(GetDispInfoAtIndex(aIndex));
  if (!certdi)
    return NS_ERROR_FAILURE;

  *_treeitem = certdi;
  NS_IF_ADDREF(*_treeitem);
  return NS_OK;
}

NS_IMETHODIMP
nsCertTree::IsHostPortOverride(uint32_t aIndex, bool *_retval)
{
  NS_ENSURE_ARG(_retval);

  RefPtr<nsCertTreeDispInfo> certdi(GetDispInfoAtIndex(aIndex));
  if (!certdi)
    return NS_ERROR_FAILURE;

  *_retval = (certdi->mTypeOfEntry == nsCertTreeDispInfo::host_port_override);
  return NS_OK;
}

/* readonly attribute long rowCount; */
NS_IMETHODIMP 
nsCertTree::GetRowCount(int32_t *aRowCount)
{
  if (!mTreeArray)
    return NS_ERROR_NOT_INITIALIZED;
  uint32_t count = 0;
  for (int32_t i=0; i<mNumOrgs; i++) {
    if (mTreeArray[i].open) {
      count += mTreeArray[i].numChildren;
    }
    count++;
  }
  *aRowCount = count;
  return NS_OK;
}

/* attribute nsITreeSelection selection; */
NS_IMETHODIMP 
nsCertTree::GetSelection(nsITreeSelection * *aSelection)
{
  *aSelection = mSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP 
nsCertTree::SetSelection(nsITreeSelection * aSelection)
{
  mSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP 
nsCertTree::GetRowProperties(int32_t index, nsAString& aProps)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsCertTree::GetCellProperties(int32_t row, nsITreeColumn* col, 
                              nsAString& aProps)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsCertTree::GetColumnProperties(nsITreeColumn* col, nsAString& aProps)
{
  return NS_OK;
}
/* boolean isContainer (in long index); */
NS_IMETHODIMP 
nsCertTree::IsContainer(int32_t index, bool *_retval)
{
  if (!mTreeArray)
    return NS_ERROR_NOT_INITIALIZED;
  treeArrayEl *el = GetThreadDescAtIndex(index);
  if (el) {
    *_retval = true;
  } else {
    *_retval = false;
  }
  return NS_OK;
}

/* boolean isContainerOpen (in long index); */
NS_IMETHODIMP 
nsCertTree::IsContainerOpen(int32_t index, bool *_retval)
{
  if (!mTreeArray)
    return NS_ERROR_NOT_INITIALIZED;
  treeArrayEl *el = GetThreadDescAtIndex(index);
  if (el && el->open) {
    *_retval = true;
  } else {
    *_retval = false;
  }
  return NS_OK;
}

/* boolean isContainerEmpty (in long index); */
NS_IMETHODIMP 
nsCertTree::IsContainerEmpty(int32_t index, bool *_retval)
{
  *_retval = !mTreeArray;
  return NS_OK;
}

/* boolean isSeparator (in long index); */
NS_IMETHODIMP 
nsCertTree::IsSeparator(int32_t index, bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

/* long getParentIndex (in long rowIndex); */
NS_IMETHODIMP 
nsCertTree::GetParentIndex(int32_t rowIndex, int32_t *_retval)
{
  if (!mTreeArray)
    return NS_ERROR_NOT_INITIALIZED;
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

/* boolean hasNextSibling (in long rowIndex, in long afterIndex); */
NS_IMETHODIMP 
nsCertTree::HasNextSibling(int32_t rowIndex, int32_t afterIndex, 
                               bool *_retval)
{
  if (!mTreeArray)
    return NS_ERROR_NOT_INITIALIZED;

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

/* long getLevel (in long index); */
NS_IMETHODIMP 
nsCertTree::GetLevel(int32_t index, int32_t *_retval)
{
  if (!mTreeArray)
    return NS_ERROR_NOT_INITIALIZED;
  treeArrayEl *el = GetThreadDescAtIndex(index);
  if (el) {
    *_retval = 0;
  } else {
    *_retval = 1;
  }
  return NS_OK;
}

/* Astring getImageSrc (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsCertTree::GetImageSrc(int32_t row, nsITreeColumn* col, 
                        nsAString& _retval)
{
  _retval.Truncate();
  return NS_OK;
}

/* long getProgressMode (in long row, in nsITreeColumn col); */
NS_IMETHODIMP
nsCertTree::GetProgressMode(int32_t row, nsITreeColumn* col, int32_t* _retval)
{
  return NS_OK;
}

/* Astring getCellValue (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsCertTree::GetCellValue(int32_t row, nsITreeColumn* col, 
                         nsAString& _retval)
{
  _retval.Truncate();
  return NS_OK;
}

/* Astring getCellText (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsCertTree::GetCellText(int32_t row, nsITreeColumn* col, 
                        nsAString& _retval)
{
  if (!mTreeArray)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv;
  _retval.Truncate();

  const char16_t* colID;
  col->GetIdConst(&colID);

  treeArrayEl *el = GetThreadDescAtIndex(row);
  if (el) {
    if (NS_LITERAL_STRING("certcol").Equals(colID))
      _retval.Assign(el->orgName);
    else
      _retval.Truncate();
    return NS_OK;
  }

  int32_t absoluteCertOffset;
  RefPtr<nsCertTreeDispInfo> certdi(GetDispInfoAtIndex(row, &absoluteCertOffset));
  if (!certdi)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIX509Cert> cert = certdi->mCert;
  if (!cert && certdi->mAddonInfo) {
    cert = certdi->mAddonInfo->mCert;
  }

  int32_t colIndex;
  col->GetIndex(&colIndex);
  uint32_t arrayIndex=absoluteCertOffset+colIndex*(mNumRows-mNumOrgs);
  uint32_t arrayLength=0;
  if (mCellText) {
    mCellText->GetLength(&arrayLength);
  }
  if (arrayIndex < arrayLength) {
    nsCOMPtr<nsISupportsString> myString(do_QueryElementAt(mCellText, arrayIndex));
    if (myString) {
      myString->GetData(_retval);
      return NS_OK;
    }
  }

  if (NS_LITERAL_STRING("certcol").Equals(colID)) {
    if (!cert) {
      mNSSComponent->GetPIPNSSBundleString("CertNotStored", _retval);
    }
    else {
      rv = cert->GetCommonName(_retval);
      if (NS_FAILED(rv) || _retval.IsEmpty()) {
        // kaie: I didn't invent the idea to cut off anything before 
        //       the first colon. :-)
        nsAutoString nick;
        rv = cert->GetNickname(nick);
        
        nsAString::const_iterator start, end, end2;
        nick.BeginReading(start);
        nick.EndReading(end);
        end2 = end;
  
        if (FindInReadable(NS_LITERAL_STRING(":"), start, end)) {
          // found. end points to the first char after the colon,
          // that's what we want.
          _retval = Substring(end, end2);
        }
        else {
          _retval = nick;
        }
      }
    }
  } else if (NS_LITERAL_STRING("tokencol").Equals(colID) && cert) {
    rv = cert->GetTokenName(_retval);
  } else if (NS_LITERAL_STRING("emailcol").Equals(colID) && cert) {
    rv = cert->GetEmailAddress(_retval);
  } else if (NS_LITERAL_STRING("purposecol").Equals(colID) && mNSSComponent && cert) {
    uint32_t verified;

    nsAutoString theUsages;
    rv = cert->GetUsagesString(false, &verified, theUsages); // allow OCSP
    if (NS_FAILED(rv)) {
      verified = nsIX509Cert::NOT_VERIFIED_UNKNOWN;
    }

    switch (verified) {
      case nsIX509Cert::VERIFIED_OK:
        _retval = theUsages;
        break;

      case nsIX509Cert::CERT_REVOKED:
        rv = mNSSComponent->GetPIPNSSBundleString("VerifyRevoked", _retval);
        break;
      case nsIX509Cert::CERT_EXPIRED:
        rv = mNSSComponent->GetPIPNSSBundleString("VerifyExpired", _retval);
        break;
      case nsIX509Cert::CERT_NOT_TRUSTED:
        rv = mNSSComponent->GetPIPNSSBundleString("VerifyNotTrusted", _retval);
        break;
      case nsIX509Cert::ISSUER_NOT_TRUSTED:
        rv = mNSSComponent->GetPIPNSSBundleString("VerifyIssuerNotTrusted", _retval);
        break;
      case nsIX509Cert::ISSUER_UNKNOWN:
        rv = mNSSComponent->GetPIPNSSBundleString("VerifyIssuerUnknown", _retval);
        break;
      case nsIX509Cert::INVALID_CA:
        rv = mNSSComponent->GetPIPNSSBundleString("VerifyInvalidCA", _retval);
        break;
      case nsIX509Cert::SIGNATURE_ALGORITHM_DISABLED:
        rv = mNSSComponent->GetPIPNSSBundleString("VerifyDisabledAlgorithm", _retval);
        break;
      case nsIX509Cert::NOT_VERIFIED_UNKNOWN:
      case nsIX509Cert::USAGE_NOT_ALLOWED:
      default:
        rv = mNSSComponent->GetPIPNSSBundleString("VerifyUnknown", _retval);
        break;
    }
  } else if (NS_LITERAL_STRING("issuedcol").Equals(colID) && cert) {
    nsCOMPtr<nsIX509CertValidity> validity;

    rv = cert->GetValidity(getter_AddRefs(validity));
    if (NS_SUCCEEDED(rv)) {
      validity->GetNotBeforeLocalDay(_retval);
    }
  } else if (NS_LITERAL_STRING("expiredcol").Equals(colID) && cert) {
    nsCOMPtr<nsIX509CertValidity> validity;

    rv = cert->GetValidity(getter_AddRefs(validity));
    if (NS_SUCCEEDED(rv)) {
      validity->GetNotAfterLocalDay(_retval);
    }
  } else if (NS_LITERAL_STRING("serialnumcol").Equals(colID) && cert) {
    rv = cert->GetSerialNumber(_retval);


  } else if (NS_LITERAL_STRING("overridetypecol").Equals(colID)) {
    // default to classic permanent-trust
    nsCertOverride::OverrideBits ob = nsCertOverride::ob_Untrusted;
    if (certdi->mTypeOfEntry == nsCertTreeDispInfo::host_port_override) {
      ob = certdi->mOverrideBits;
    }
    nsAutoCString temp;
    nsCertOverride::convertBitsToString(ob, temp);
    _retval = NS_ConvertUTF8toUTF16(temp);
  } else if (NS_LITERAL_STRING("sitecol").Equals(colID)) {
    if (certdi->mTypeOfEntry == nsCertTreeDispInfo::host_port_override) {
      nsAutoCString hostPort;
      nsCertOverrideService::GetHostWithPort(certdi->mAsciiHost, certdi->mPort, hostPort);
      _retval = NS_ConvertUTF8toUTF16(hostPort);
    }
    else {
      _retval = NS_LITERAL_STRING("*");
    }
  } else if (NS_LITERAL_STRING("lifetimecol").Equals(colID)) {
    const char *stringID = 
      (certdi->mIsTemporary) ? "CertExceptionTemporary" : "CertExceptionPermanent";
    rv = mNSSComponent->GetPIPNSSBundleString(stringID, _retval);
  } else if (NS_LITERAL_STRING("typecol").Equals(colID) && cert) {
    uint32_t type = nsIX509Cert::UNKNOWN_CERT;
    rv = cert->GetCertType(&type);

    switch (type) {
    case nsIX509Cert::USER_CERT:
        rv = mNSSComponent->GetPIPNSSBundleString("CertUser", _retval);
	break;
    case nsIX509Cert::CA_CERT:
        rv = mNSSComponent->GetPIPNSSBundleString("CertCA", _retval);
	break;
    case nsIX509Cert::SERVER_CERT:
        rv = mNSSComponent->GetPIPNSSBundleString("CertSSL", _retval);
	break;
    case nsIX509Cert::EMAIL_CERT:
        rv = mNSSComponent->GetPIPNSSBundleString("CertEmail", _retval);
	break;
    default:
        rv = mNSSComponent->GetPIPNSSBundleString("CertUnknown", _retval);
	break;
    }

  } else {
    return NS_ERROR_FAILURE;
  }
  if (mCellText) {
    nsCOMPtr<nsISupportsString> text(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    text->SetData(_retval);
    mCellText->ReplaceElementAt(text, arrayIndex, false);
  }
  return rv;
}

/* void setTree (in nsITreeBoxObject tree); */
NS_IMETHODIMP 
nsCertTree::SetTree(nsITreeBoxObject *tree)
{
  mTree = tree;
  return NS_OK;
}

/* void toggleOpenState (in long index); */
NS_IMETHODIMP 
nsCertTree::ToggleOpenState(int32_t index)
{
  if (!mTreeArray)
    return NS_ERROR_NOT_INITIALIZED;
  treeArrayEl *el = GetThreadDescAtIndex(index);
  if (el) {
    el->open = !el->open;
    int32_t newChildren = (el->open) ? el->numChildren : -el->numChildren;
    if (mTree) mTree->RowCountChanged(index + 1, newChildren);
  }
  return NS_OK;
}

/* void cycleHeader (in nsITreeColumn); */
NS_IMETHODIMP 
nsCertTree::CycleHeader(nsITreeColumn* col)
{
  return NS_OK;
}

/* void selectionChanged (); */
NS_IMETHODIMP 
nsCertTree::SelectionChanged()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cycleCell (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsCertTree::CycleCell(int32_t row, nsITreeColumn* col)
{
  return NS_OK;
}

/* boolean isEditable (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsCertTree::IsEditable(int32_t row, nsITreeColumn* col, bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

/* boolean isSelectable (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsCertTree::IsSelectable(int32_t row, nsITreeColumn* col, bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

/* void setCellValue (in long row, in nsITreeColumn col, in AString value); */
NS_IMETHODIMP 
nsCertTree::SetCellValue(int32_t row, nsITreeColumn* col, 
                         const nsAString& value)
{
  return NS_OK;
}

/* void setCellText (in long row, in nsITreeColumn col, in AString value); */
NS_IMETHODIMP 
nsCertTree::SetCellText(int32_t row, nsITreeColumn* col, 
                        const nsAString& value)
{
  return NS_OK;
}

/* void performAction (in wstring action); */
NS_IMETHODIMP 
nsCertTree::PerformAction(const char16_t *action)
{
  return NS_OK;
}

/* void performActionOnRow (in wstring action, in long row); */
NS_IMETHODIMP 
nsCertTree::PerformActionOnRow(const char16_t *action, int32_t row)
{
  return NS_OK;
}

/* void performActionOnCell (in wstring action, in long row, 
 *                           in wstring colID); 
 */
NS_IMETHODIMP 
nsCertTree::PerformActionOnCell(const char16_t *action, int32_t row, 
                                nsITreeColumn* col)
{
  return NS_OK;
}

#ifdef DEBUG_CERT_TREE
void
nsCertTree::dumpMap()
{
  for (int i=0; i<mNumOrgs; i++) {
    nsAutoString org(mTreeArray[i].orgName);
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("ORG[%s]", NS_LossyConvertUTF16toASCII(org).get()));
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("OPEN[%d]", mTreeArray[i].open));
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("INDEX[%d]", mTreeArray[i].certIndex));
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("NCHILD[%d]", mTreeArray[i].numChildren));
  }
  for (int i=0; i<mNumRows; i++) {
    treeArrayEl *el = GetThreadDescAtIndex(i);
    if (el) {
      nsAutoString td(el->orgName);
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("thread desc[%d]: %s", i, NS_LossyConvertUTF16toASCII(td).get()));
    }
    nsCOMPtr<nsIX509Cert> ct = GetCertAtIndex(i);
    if (ct) {
      char16_t *goo;
      ct->GetCommonName(&goo);
      nsAutoString doo(goo);
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("cert [%d]: %s", i, NS_LossyConvertUTF16toASCII(doo).get()));
    }
  }
}
#endif

//
// CanDrop
//
NS_IMETHODIMP nsCertTree::CanDrop(int32_t index, int32_t orientation,
                                  nsIDOMDataTransfer* aDataTransfer, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;
  
  return NS_OK;
}


//
// Drop
//
NS_IMETHODIMP nsCertTree::Drop(int32_t row, int32_t orient, nsIDOMDataTransfer* aDataTransfer)
{
  return NS_OK;
}


//
// IsSorted
//
// ...
//
NS_IMETHODIMP nsCertTree::IsSorted(bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

#define RETURN_NOTHING

void 
nsCertTree::CmpInitCriterion(nsIX509Cert *cert, CompareCacheHashEntry *entry,
                             sortCriterion crit, int32_t level)
{
  NS_ENSURE_TRUE(cert && entry, RETURN_NOTHING);

  entry->mCritInit[level] = true;
  nsXPIDLString &str = entry->mCrit[level];
  
  switch (crit) {
    case sort_IssuerOrg:
      cert->GetIssuerOrganization(str);
      if (str.IsEmpty())
        cert->GetCommonName(str);
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
    case sort_IssuedDateDescending:
      {
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
          char datebuf[20]; // 4 + 2 + 2 + 2 + 2 + 2 + 1 = 15
          if (0 != PR_FormatTime(datebuf, sizeof(datebuf), "%Y%m%d%H%M%S", &explodedTime)) {
            str = NS_ConvertASCIItoUTF16(nsDependentCString(datebuf));
          }
        }
      }
      break;
    case sort_Email:
      cert->GetEmailAddress(str);
      break;
    case sort_None:
    default:
      break;
  }
}

int32_t
nsCertTree::CmpByCrit(nsIX509Cert *a, CompareCacheHashEntry *ace, 
                      nsIX509Cert *b, CompareCacheHashEntry *bce, 
                      sortCriterion crit, int32_t level)
{
  NS_ENSURE_TRUE(a && ace && b && bce, 0);

  if (!ace->mCritInit[level]) {
    CmpInitCriterion(a, ace, crit, level);
  }

  if (!bce->mCritInit[level]) {
    CmpInitCriterion(b, bce, crit, level);
  }

  nsXPIDLString &str_a = ace->mCrit[level];
  nsXPIDLString &str_b = bce->mCrit[level];

  int32_t result;
  if (str_a && str_b)
    result = Compare(str_a, str_b, nsCaseInsensitiveStringComparator());
  else
    result = !str_a ? (!str_b ? 0 : -1) : 1;

  if (sort_IssuedDateDescending == crit)
    result *= -1; // reverse compare order

  return result;
}

int32_t
nsCertTree::CmpBy(void *cache, nsIX509Cert *a, nsIX509Cert *b, 
                  sortCriterion c0, sortCriterion c1, sortCriterion c2)
{
  // This will be called when comparing items for display sorting.
  // Some items might have no cert associated, so either a or b is null.
  // We want all those orphans show at the top of the list,
  // so we treat a null cert as "smaller" by returning -1.
  // We don't try to sort within the group of no-cert entries,
  // so we treat them as equal wrt sort order.

  if (!a && !b)
    return 0;

  if (!a)
    return -1;

  if (!b)
    return 1;

  NS_ENSURE_TRUE(cache && a && b, 0);

  CompareCacheHashEntry *ace = getCacheEntry(cache, a);
  CompareCacheHashEntry *bce = getCacheEntry(cache, b);

  int32_t cmp;
  cmp = CmpByCrit(a, ace, b, bce, c0, 0);
  if (cmp != 0)
    return cmp;

  if (c1 != sort_None) {
    cmp = CmpByCrit(a, ace, b, bce, c1, 1);
    if (cmp != 0)
      return cmp;
    
    if (c2 != sort_None) {
      return CmpByCrit(a, ace, b, bce, c2, 2);
    }
  }

  return cmp;
}

int32_t
nsCertTree::CmpCACert(void *cache, nsIX509Cert *a, nsIX509Cert *b)
{
  // XXX we assume issuer org is always criterion 1
  return CmpBy(cache, a, b, sort_IssuerOrg, sort_Org, sort_Token);
}

int32_t
nsCertTree::CmpWebSiteCert(void *cache, nsIX509Cert *a, nsIX509Cert *b)
{
  // XXX we assume issuer org is always criterion 1
  return CmpBy(cache, a, b, sort_IssuerOrg, sort_CommonName, sort_None);
}

int32_t
nsCertTree::CmpUserCert(void *cache, nsIX509Cert *a, nsIX509Cert *b)
{
  // XXX we assume issuer org is always criterion 1
  return CmpBy(cache, a, b, sort_IssuerOrg, sort_Token, sort_IssuedDateDescending);
}

int32_t
nsCertTree::CmpEmailCert(void *cache, nsIX509Cert *a, nsIX509Cert *b)
{
  // XXX we assume issuer org is always criterion 1
  return CmpBy(cache, a, b, sort_IssuerOrg, sort_Email, sort_CommonName);
}

