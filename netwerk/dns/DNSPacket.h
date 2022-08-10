/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DNSPacket_h__
#define mozilla_net_DNSPacket_h__

#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "pk11pub.h"
#include "ScopedNSSTypes.h"
#include "nsClassHashtable.h"
#include "nsIDNSService.h"
#include "DNS.h"
#include "DNSByTypeRecord.h"

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

enum class DNSPacketStatus : uint8_t {
  Unknown = 0,
  Success,
  KeyNotAvailable,
  KeyNotUsable,
  EncodeError,
  EncryptError,
  DecodeError,
  DecryptError,
};

class DNSPacket {
 public:
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

  DNSPacketStatus PacketStatus() const { return mStatus; }
  void SetOriginHost(const Maybe<nsCString>& aHost) { mOriginHost = aHost; }

 protected:
  // Never accept larger DOH responses than this as that would indicate
  // something is wrong. Typical ones are much smaller.
  static const unsigned int MAX_SIZE = 3200;

  nsresult PassQName(unsigned int& index, const unsigned char* aBuffer);
  nsresult GetQname(nsACString& aQname, unsigned int& aIndex,
                    const unsigned char* aBuffer);
  nsresult ParseSvcParam(unsigned int svcbIndex, uint16_t key,
                         SvcFieldValue& field, uint16_t length,
                         const unsigned char* aBuffer);
  nsresult DecodeInternal(
      nsCString& aHost, enum TrrType aType, nsCString& aCname,
      bool aAllowRFC1918, DOHresp& aResp, TypeRecordResultType& aTypeResult,
      nsClassHashtable<nsCStringHashKey, DOHresp>& aAdditionalRecords,
      uint32_t& aTTL, const unsigned char* aBuffer, uint32_t aLen);

  void SetDNSPacketStatus(DNSPacketStatus aStatus) {
    if (mStatus == DNSPacketStatus::Unknown ||
        mStatus == DNSPacketStatus::Success) {
      mStatus = aStatus;
    }
  }

  // The response buffer.
  unsigned char mResponse[MAX_SIZE]{};
  unsigned int mBodySize = 0;
  DNSPacketStatus mStatus = DNSPacketStatus::Unknown;
  Maybe<nsCString> mOriginHost;
};

class ODoHDNSPacket final : public DNSPacket {
 public:
  ODoHDNSPacket() = default;
  virtual ~ODoHDNSPacket();

  static bool ParseODoHConfigs(Span<const uint8_t> aData,
                               nsTArray<ObliviousDoHConfig>& aOut);

  virtual nsresult EncodeRequest(nsCString& aBody, const nsACString& aHost,
                                 uint16_t aType, bool aDisableECS) override;

  virtual nsresult Decode(
      nsCString& aHost, enum TrrType aType, nsCString& aCname,
      bool aAllowRFC1918, DOHresp& aResp, TypeRecordResultType& aTypeResult,
      nsClassHashtable<nsCStringHashKey, DOHresp>& aAdditionalRecords,
      uint32_t& aTTL) override;

 protected:
  bool EncryptDNSQuery(const nsACString& aQuery, uint16_t aPaddingLen,
                       const ObliviousDoHConfig& aConfig,
                       ObliviousDoHMessage& aOut);
  bool DecryptDNSResponse();

  HpkeContext* mContext = nullptr;
  UniqueSECItem mPlainQuery;
  // This struct indicates the range of decrypted responses stored in mResponse.
  struct DecryptedResponseRange {
    uint16_t mStart = 0;
    uint16_t mLength = 0;
  };
  Maybe<DecryptedResponseRange> mDecryptedResponseRange;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_DNSPacket_h__
