/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCertOverrideService_h
#define nsCertOverrideService_h

#include <utility>

#include "mozilla/HashFunctions.h"
#include "mozilla/Mutex.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/TypedEnumBits.h"
#include "nsIAsyncShutdown.h"
#include "nsICertOverrideService.h"
#include "nsIFile.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "nsWeakReference.h"
#include "secoidt.h"

class nsCertOverride final : public nsICertOverride {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICERTOVERRIDE

  enum class OverrideBits {
    None = 0,
    Untrusted = nsICertOverrideService::ERROR_UNTRUSTED,
    Mismatch = nsICertOverrideService::ERROR_MISMATCH,
    Time = nsICertOverrideService::ERROR_TIME,
  };

  nsCertOverride()
      : mPort(-1), mIsTemporary(false), mOverrideBits(OverrideBits::None) {}

  nsCString mAsciiHost;
  int32_t mPort;
  OriginAttributes mOriginAttributes;
  bool mIsTemporary;  // true: session only, false: stored on disk
  nsCString mFingerprint;
  OverrideBits mOverrideBits;
  nsCString mDBKey;
  nsCOMPtr<nsIX509Cert> mCert;

  static void convertBitsToString(OverrideBits ob, nsACString& str);
  static void convertStringToBits(const nsACString& str, OverrideBits& ob);

 private:
  ~nsCertOverride() = default;
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(nsCertOverride::OverrideBits)

// hash entry class
class nsCertOverrideEntry final : public PLDHashEntryHdr {
 public:
  // Hash methods
  typedef const char* KeyType;
  typedef const char* KeyTypePointer;

  // do nothing with aHost - we require mHead to be set before we're live!
  explicit nsCertOverrideEntry(KeyTypePointer aHostWithPortUTF8) {}

  nsCertOverrideEntry(nsCertOverrideEntry&& toMove)
      : PLDHashEntryHdr(std::move(toMove)),
        mSettings(std::move(toMove.mSettings)),
        mKeyString(std::move(toMove.mKeyString)) {}

  ~nsCertOverrideEntry() = default;

  KeyType GetKey() const { return KeyStringPtr(); }

  KeyTypePointer GetKeyPointer() const { return KeyStringPtr(); }

  bool KeyEquals(KeyTypePointer aKey) const {
    return !strcmp(KeyStringPtr(), aKey);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }

  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return mozilla::HashString(aKey);
  }

  enum { ALLOW_MEMMOVE = false };

  // get methods
  inline const nsCString& KeyString() const { return mKeyString; }

  inline KeyTypePointer KeyStringPtr() const { return mKeyString.get(); }

  RefPtr<nsCertOverride> mSettings;
  nsCString mKeyString;
};

class nsCertOverrideService final : public nsICertOverrideService,
                                    public nsIObserver,
                                    public nsSupportsWeakReference,
                                    public nsIAsyncShutdownBlocker {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICERTOVERRIDESERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER

  nsCertOverrideService();

  nsresult Init();
  void RemoveAllTemporaryOverrides();

  // Concatenates host name and the port number. If the port number is -1 then
  // port 443 is automatically used. This method ensures there is always a port
  // number separated with colon.
  static void GetHostWithPort(const nsACString& aHostName, int32_t aPort,
                              nsACString& aRetval);

  // Concatenates host name, port number, and origin attributes.
  static void GetKeyString(const nsACString& aHostName, int32_t aPort,
                           const OriginAttributes& aOriginAttributes,
                           nsACString& aRetval);

  void AssertOnTaskQueue() const {
    MOZ_ASSERT(mWriterTaskQueue->IsOnCurrentThread());
  }

  void RemoveShutdownBlocker();

 private:
  ~nsCertOverrideService();

  mozilla::Mutex mMutex;
  bool mDisableAllSecurityCheck;
  nsCOMPtr<nsIFile> mSettingsFile;
  nsTHashtable<nsCertOverrideEntry> mSettingsTable;

  void CountPermanentOverrideTelemetry(
      const mozilla::MutexAutoLock& aProofOfLock);

  void RemoveAllFromMemory();
  nsresult Read(const mozilla::MutexAutoLock& aProofOfLock);
  nsresult Write(const mozilla::MutexAutoLock& aProofOfLock);
  nsresult AddEntryToList(const nsACString& host, int32_t port,
                          const OriginAttributes& aOriginAttributes,
                          nsIX509Cert* aCert, const bool aIsTemporary,
                          const nsACString& fingerprint,
                          nsCertOverride::OverrideBits ob,
                          const nsACString& dbKey,
                          const mozilla::MutexAutoLock& aProofOfLock);

  RefPtr<mozilla::TaskQueue> mWriterTaskQueue;

  // Only accessed on the main thread
  uint64_t mPendingWriteCount;
};

#define NS_CERTOVERRIDE_CID                          \
  { /* 67ba681d-5485-4fff-952c-2ee337ffdcd6 */       \
    0x67ba681d, 0x5485, 0x4fff, {                    \
      0x95, 0x2c, 0x2e, 0xe3, 0x37, 0xff, 0xdc, 0xd6 \
    }                                                \
  }

#endif  // nsCertOverrideService_h
