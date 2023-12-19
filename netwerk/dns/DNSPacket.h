/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DNSPacket_h__
#define mozilla_net_DNSPacket_h__

#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "nsClassHashtable.h"
#include "nsIDNSService.h"
#include "DNS.h"
#include "DNSByTypeRecord.h"

#include <functional>

namespace mozilla {
namespace net {

class DOHresp {
 public:
  nsresult Add(uint32_t TTL, unsigned char const* dns, unsigned int index,
               uint16_t len, bool aLocalAllowed);
  nsTArray<NetAddr> mAddresses;
  uint32_t mTtl = 0;
};

// the values map to RFC1035 type identifiers
enum TrrType {
  TRRTYPE_A = 1,
  TRRTYPE_NS = 2,
  TRRTYPE_CNAME = 5,
  TRRTYPE_AAAA = 28,
  TRRTYPE_OPT = 41,
  TRRTYPE_TXT = 16,
  TRRTYPE_HTTPSSVC = nsIDNSService::RESOLVE_TYPE_HTTPSSVC,  // 65
};

class DNSPacket {
 public:
  // Never accept larger DOH responses than this as that would indicate
  // something is wrong. Typical ones are much smaller.
  static const unsigned int MAX_SIZE = 3200;

  DNSPacket() = default;
  virtual ~DNSPacket() = default;

  Result<uint8_t, nsresult> GetRCode() const;
  Result<bool, nsresult> RecursionAvailable() const;

  // Called in order to feed data into the buffer.
  nsresult OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aInputStream,
                           uint64_t aOffset, const uint32_t aCount);

  // Encodes the name request into a buffer that represents a DNS packet
  virtual nsresult EncodeRequest(nsCString& aBody, const nsACString& aHost,
                                 uint16_t aType, bool aDisableECS);

  // Decodes the DNS response and extracts the responses, additional records,
  // etc. XXX: This should probably be refactored to reduce the number of
  // output parameters and have a common format for different record types.
  virtual nsresult Decode(
      nsCString& aHost, enum TrrType aType, nsCString& aCname,
      bool aAllowRFC1918, DOHresp& aResp, TypeRecordResultType& aTypeResult,
      nsClassHashtable<nsCStringHashKey, DOHresp>& aAdditionalRecords,
      uint32_t& aTTL);

  void SetOriginHost(const Maybe<nsCString>& aHost) { mOriginHost = aHost; }

  nsresult FillBuffer(std::function<int(unsigned char response[MAX_SIZE])>&&);

  static nsresult ParseHTTPS(uint16_t aRDLen, struct SVCB& aParsed,
                             unsigned int aIndex, const unsigned char* aBuffer,
                             unsigned int aBodySize,
                             const nsACString& aOriginHost);
  void SetNativePacket(bool aNative) { mNativePacket = aNative; }

 protected:
  nsresult PassQName(unsigned int& index, const unsigned char* aBuffer);
  static nsresult GetQname(nsACString& aQname, unsigned int& aIndex,
                           const unsigned char* aBuffer,
                           unsigned int aBodySize);
  static nsresult ParseSvcParam(unsigned int svcbIndex, uint16_t key,
                                SvcFieldValue& field, uint16_t length,
                                const unsigned char* aBuffer);
  nsresult DecodeInternal(
      nsCString& aHost, enum TrrType aType, nsCString& aCname,
      bool aAllowRFC1918, DOHresp& aResp, TypeRecordResultType& aTypeResult,
      nsClassHashtable<nsCStringHashKey, DOHresp>& aAdditionalRecords,
      uint32_t& aTTL, const unsigned char* aBuffer, uint32_t aLen);

  // The response buffer.
  unsigned char mResponse[MAX_SIZE]{};
  unsigned int mBodySize = 0;
  // True when decoding a DNS packet received from OS. Decoding will
  // not panic if packet ID is not zero.
  bool mNativePacket = false;
  nsresult mStatus = NS_OK;
  Maybe<nsCString> mOriginHost;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_DNSPacket_h__
