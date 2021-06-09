/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheHashUtils__h__
#define CacheHashUtils__h__

#include "nsISupports.h"
#include "mozilla/Types.h"
#include "prnetdb.h"
#include "nsPrintfCString.h"

#define LOGSHA1(x)                                         \
  PR_htonl((reinterpret_cast<const uint32_t*>(x))[0]),     \
      PR_htonl((reinterpret_cast<const uint32_t*>(x))[1]), \
      PR_htonl((reinterpret_cast<const uint32_t*>(x))[2]), \
      PR_htonl((reinterpret_cast<const uint32_t*>(x))[3]), \
      PR_htonl((reinterpret_cast<const uint32_t*>(x))[4])

#define SHA1STRING(x) \
  (nsPrintfCString("%08x%08x%08x%08x%08x", LOGSHA1(x)).get())

namespace mozilla {

class OriginAttributes;

namespace net {

class CacheHash : public nsISupports {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  using Hash16_t = uint16_t;
  using Hash32_t = uint32_t;

  static Hash32_t Hash(const char* aData, uint32_t aSize,
                       uint32_t aInitval = 0);
  static Hash16_t Hash16(const char* aData, uint32_t aSize,
                         uint32_t aInitval = 0);

  explicit CacheHash(uint32_t aInitval = 0);

  void Update(const char* aData, uint32_t aLen);
  Hash32_t GetHash();
  Hash16_t GetHash16();

 private:
  virtual ~CacheHash() = default;

  void Feed(uint32_t aVal, uint8_t aLen = 4);

  static const uint32_t kGoldenRation = 0x9e3779b9;

  uint32_t mA{kGoldenRation}, mB{kGoldenRation}, mC;
  uint8_t mPos{0};
  uint32_t mBuf{0};
  uint8_t mBufPos{0};
  uint32_t mLength{0};
  bool mFinalized{false};
};

using OriginAttrsHash = uint64_t;

OriginAttrsHash GetOriginAttrsHash(const mozilla::OriginAttributes& aOA);

}  // namespace net
}  // namespace mozilla

#endif
