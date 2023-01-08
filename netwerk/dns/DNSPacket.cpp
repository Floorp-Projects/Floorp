/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DNSPacket.h"

#include "DNS.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_network.h"
#include "ODoHService.h"
// Put DNSLogging.h at the end to avoid LOG being overwritten by other headers.
#include "DNSLogging.h"

#include "nsIInputStream.h"

namespace mozilla {
namespace net {

static uint16_t get16bit(const unsigned char* aData, unsigned int index) {
  return ((aData[index] << 8) | aData[index + 1]);
}

static bool get16bit(const Span<const uint8_t>& aData,
                     Span<const uint8_t>::const_iterator& it,
                     uint16_t& result) {
  if (it >= aData.cend() || std::distance(it, aData.cend()) < 2) {
    return false;
  }

  result = (*it << 8) | *(it + 1);
  it += 2;
  return true;
}

static uint32_t get32bit(const unsigned char* aData, unsigned int index) {
  return (aData[index] << 24) | (aData[index + 1] << 16) |
         (aData[index + 2] << 8) | aData[index + 3];
}

// https://datatracker.ietf.org/doc/html/draft-ietf-dnsop-extended-error-16#section-4
// This is a list of errors for which we should not fallback to Do53.
// These are normally DNSSEC failures or explicit filtering performed by the
// recursive resolver.
bool hardFail(uint16_t code) {
  const uint16_t noFallbackErrors[] = {
      4,   // Forged answer (malware filtering)
      6,   // DNSSEC Boggus
      7,   // Signature expired
      8,   // Signature not yet valid
      9,   // DNSKEY Missing
      10,  // RRSIG missing
      11,  // No ZONE Key Bit set
      12,  // NSEC Missing
      17,  // Filtered
  };

  for (const auto& err : noFallbackErrors) {
    if (code == err) {
      return true;
    }
  }
  return false;
}

// static
nsresult DNSPacket::ParseSvcParam(unsigned int svcbIndex, uint16_t key,
                                  SvcFieldValue& field, uint16_t length,
                                  const unsigned char* aBuffer) {
  switch (key) {
    case SvcParamKeyMandatory: {
      if (length % 2 != 0) {
        // This key should encode a list of uint16_t
        return NS_ERROR_UNEXPECTED;
      }
      while (length > 0) {
        uint16_t mandatoryKey = get16bit(aBuffer, svcbIndex);
        length -= 2;
        svcbIndex += 2;

        if (!IsValidSvcParamKey(mandatoryKey)) {
          LOG(("The mandatory field includes a key we don't support %u",
               mandatoryKey));
          return NS_ERROR_UNEXPECTED;
        }
      }
      break;
    }
    case SvcParamKeyAlpn: {
      field.mValue = AsVariant(SvcParamAlpn());
      auto& alpnArray = field.mValue.as<SvcParamAlpn>().mValue;
      while (length > 0) {
        uint8_t alpnIdLength = aBuffer[svcbIndex++];
        length -= 1;
        if (alpnIdLength > length) {
          return NS_ERROR_UNEXPECTED;
        }

        alpnArray.AppendElement(
            nsCString((const char*)&aBuffer[svcbIndex], alpnIdLength));
        length -= alpnIdLength;
        svcbIndex += alpnIdLength;
      }
      break;
    }
    case SvcParamKeyNoDefaultAlpn: {
      if (length != 0) {
        // This key should not contain a value
        return NS_ERROR_UNEXPECTED;
      }
      field.mValue = AsVariant(SvcParamNoDefaultAlpn{});
      break;
    }
    case SvcParamKeyPort: {
      if (length != 2) {
        // This key should only encode a uint16_t
        return NS_ERROR_UNEXPECTED;
      }
      field.mValue =
          AsVariant(SvcParamPort{.mValue = get16bit(aBuffer, svcbIndex)});
      break;
    }
    case SvcParamKeyIpv4Hint: {
      if (length % 4 != 0) {
        // This key should only encode IPv4 addresses
        return NS_ERROR_UNEXPECTED;
      }

      field.mValue = AsVariant(SvcParamIpv4Hint());
      auto& ipv4array = field.mValue.as<SvcParamIpv4Hint>().mValue;
      while (length > 0) {
        NetAddr addr;
        addr.inet.family = AF_INET;
        addr.inet.port = 0;
        addr.inet.ip = ntohl(get32bit(aBuffer, svcbIndex));
        ipv4array.AppendElement(addr);
        length -= 4;
        svcbIndex += 4;
      }
      break;
    }
    case SvcParamKeyEchConfig: {
      field.mValue = AsVariant(SvcParamEchConfig{
          .mValue = nsCString((const char*)(&aBuffer[svcbIndex]), length)});
      break;
    }
    case SvcParamKeyIpv6Hint: {
      if (length % 16 != 0) {
        // This key should only encode IPv6 addresses
        return NS_ERROR_UNEXPECTED;
      }

      field.mValue = AsVariant(SvcParamIpv6Hint());
      auto& ipv6array = field.mValue.as<SvcParamIpv6Hint>().mValue;
      while (length > 0) {
        NetAddr addr;
        addr.inet6.family = AF_INET6;
        addr.inet6.port = 0;      // unknown
        addr.inet6.flowinfo = 0;  // unknown
        addr.inet6.scope_id = 0;  // unknown
        for (int i = 0; i < 16; i++, svcbIndex++) {
          addr.inet6.ip.u8[i] = aBuffer[svcbIndex];
        }
        ipv6array.AppendElement(addr);
        length -= 16;
        // no need to increase svcbIndex - we did it in the for above.
      }
      break;
    }
    case SvcParamKeyODoHConfig: {
      field.mValue = AsVariant(SvcParamODoHConfig{
          .mValue = nsCString((const char*)(&aBuffer[svcbIndex]), length)});
      break;
    }
    default: {
      // Unespected type. We'll just ignore it.
      return NS_OK;
      break;
    }
  }
  return NS_OK;
}

nsresult DNSPacket::PassQName(unsigned int& index,
                              const unsigned char* aBuffer) {
  uint8_t length;
  do {
    if (mBodySize < (index + 1)) {
      LOG(("TRR: PassQName:%d fail at index %d\n", __LINE__, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    length = static_cast<uint8_t>(aBuffer[index]);
    if ((length & 0xc0) == 0xc0) {
      // name pointer, advance over it and be done
      if (mBodySize < (index + 2)) {
        return NS_ERROR_ILLEGAL_VALUE;
      }
      index += 2;
      break;
    }
    if (length & 0xc0) {
      LOG(("TRR: illegal label length byte (%x) at index %d\n", length, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    // pass label
    if (mBodySize < (index + 1 + length)) {
      LOG(("TRR: PassQName:%d fail at index %d\n", __LINE__, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    index += 1 + length;
  } while (length);
  return NS_OK;
}

// GetQname: retrieves the qname (stores in 'aQname') and stores the index
// after qname was parsed into the 'aIndex'.
nsresult DNSPacket::GetQname(nsACString& aQname, unsigned int& aIndex,
                             const unsigned char* aBuffer) {
  uint8_t clength = 0;
  unsigned int cindex = aIndex;
  unsigned int loop = 128;    // a valid DNS name can never loop this much
  unsigned int endindex = 0;  // index position after this data
  do {
    if (cindex >= mBodySize) {
      LOG(("TRR: bad Qname packet\n"));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    clength = static_cast<uint8_t>(aBuffer[cindex]);
    if ((clength & 0xc0) == 0xc0) {
      // name pointer, get the new offset (14 bits)
      if ((cindex + 1) >= mBodySize) {
        return NS_ERROR_ILLEGAL_VALUE;
      }
      // extract the new index position for the next label
      uint16_t newpos = (clength & 0x3f) << 8 | aBuffer[cindex + 1];
      if (!endindex) {
        // only update on the first "jump"
        endindex = cindex + 2;
      }
      cindex = newpos;
      continue;
    }
    if (clength & 0xc0) {
      // any of those bits set individually is an error
      LOG(("TRR: bad Qname packet\n"));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    cindex++;

    if (clength) {
      if (!aQname.IsEmpty()) {
        aQname.Append(".");
      }
      if ((cindex + clength) > mBodySize) {
        return NS_ERROR_ILLEGAL_VALUE;
      }
      aQname.Append((const char*)(&aBuffer[cindex]), clength);
      cindex += clength;  // skip label
    }
  } while (clength && --loop);

  if (!loop) {
    LOG(("DNSPacket::DohDecode pointer loop error\n"));
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (!endindex) {
    // there was no "jump"
    endindex = cindex;
  }
  aIndex = endindex;
  return NS_OK;
}

nsresult DOHresp::Add(uint32_t TTL, unsigned char const* dns,
                      unsigned int index, uint16_t len, bool aLocalAllowed) {
  NetAddr addr;
  if (4 == len) {
    // IPv4
    addr.inet.family = AF_INET;
    addr.inet.port = 0;  // unknown
    addr.inet.ip = ntohl(get32bit(dns, index));
  } else if (16 == len) {
    // IPv6
    addr.inet6.family = AF_INET6;
    addr.inet6.port = 0;      // unknown
    addr.inet6.flowinfo = 0;  // unknown
    addr.inet6.scope_id = 0;  // unknown
    for (int i = 0; i < 16; i++, index++) {
      addr.inet6.ip.u8[i] = dns[index];
    }
  } else {
    return NS_ERROR_UNEXPECTED;
  }

  if (addr.IsIPAddrLocal() && !aLocalAllowed) {
    return NS_ERROR_FAILURE;
  }

  // While the DNS packet might return individual TTLs for each address,
  // we can only return one value in the AddrInfo class so pick the
  // lowest number.
  if (mTtl < TTL) {
    mTtl = TTL;
  }

  if (LOG_ENABLED()) {
    char buf[128];
    addr.ToStringBuffer(buf, sizeof(buf));
    LOG(("DOHresp:Add %s\n", buf));
  }
  mAddresses.AppendElement(addr);
  return NS_OK;
}

nsresult DNSPacket::OnDataAvailable(nsIRequest* aRequest,
                                    nsIInputStream* aInputStream,
                                    uint64_t aOffset, const uint32_t aCount) {
  if (aCount + mBodySize > MAX_SIZE) {
    LOG(("DNSPacket::OnDataAvailable:%d fail\n", __LINE__));
    return NS_ERROR_FAILURE;
  }
  uint32_t count;
  nsresult rv =
      aInputStream->Read((char*)mResponse + mBodySize, aCount, &count);
  if (NS_FAILED(rv)) {
    return rv;
  }
  MOZ_ASSERT(count == aCount);
  mBodySize += aCount;
  return NS_OK;
}

const uint8_t kDNS_CLASS_IN = 1;

nsresult DNSPacket::EncodeRequest(nsCString& aBody, const nsACString& aHost,
                                  uint16_t aType, bool aDisableECS) {
  aBody.Truncate();
  // Header
  aBody += '\0';
  aBody += '\0';  // 16 bit id
  aBody += 0x01;  // |QR|   Opcode  |AA|TC|RD| Set the RD bit
  aBody += '\0';  // |RA|   Z    |   RCODE   |
  aBody += '\0';
  aBody += 1;  // QDCOUNT (number of entries in the question section)
  aBody += '\0';
  aBody += '\0';  // ANCOUNT
  aBody += '\0';
  aBody += '\0';  // NSCOUNT

  char additionalRecords =
      (aDisableECS || StaticPrefs::network_trr_padding()) ? 1 : 0;
  aBody += '\0';               // ARCOUNT
  aBody += additionalRecords;  // ARCOUNT low byte for EDNS(0)

  // Question

  // The input host name should be converted to a sequence of labels, where
  // each label consists of a length octet followed by that number of
  // octets.  The domain name terminates with the zero length octet for the
  // null label of the root.
  // Followed by 16 bit QTYPE and 16 bit QCLASS

  int32_t index = 0;
  int32_t offset = 0;
  do {
    bool dotFound = false;
    int32_t labelLength;
    index = aHost.FindChar('.', offset);
    if (kNotFound != index) {
      dotFound = true;
      labelLength = index - offset;
    } else {
      labelLength = aHost.Length() - offset;
    }
    if (labelLength > 63) {
      // too long label!
      SetDNSPacketStatus(DNSPacketStatus::EncodeError);
      return NS_ERROR_ILLEGAL_VALUE;
    }
    if (labelLength > 0) {
      aBody += static_cast<unsigned char>(labelLength);
      nsDependentCSubstring label = Substring(aHost, offset, labelLength);
      aBody.Append(label);
    }
    if (!dotFound) {
      aBody += '\0';  // terminate with a final zero
      break;
    }
    offset += labelLength + 1;  // move over label and dot
  } while (true);

  aBody += static_cast<uint8_t>(aType >> 8);  // upper 8 bit TYPE
  aBody += static_cast<uint8_t>(aType);
  aBody += '\0';           // upper 8 bit CLASS
  aBody += kDNS_CLASS_IN;  // IN - "the Internet"

  if (additionalRecords) {
    // EDNS(0) is RFC 6891, ECS is RFC 7871
    aBody += '\0';  // NAME       | domain name  | MUST be 0 (root domain) |
    aBody += '\0';
    aBody += 41;  // TYPE       | u_int16_t    | OPT (41)                     |
    aBody += 16;  // CLASS      | u_int16_t    | requestor's UDP payload size |
    aBody +=
        '\0';  // advertise 4K (high-byte: 16 | low-byte: 0), ignored by DoH
    aBody += '\0';  // TTL        | u_int32_t    | extended RCODE and flags |
    aBody += '\0';
    aBody += '\0';
    aBody += '\0';

    // calculate padding length
    unsigned int paddingLen = 0;
    unsigned int rdlen = 0;
    bool padding = StaticPrefs::network_trr_padding();
    if (padding) {
      // always add padding specified in rfc 7830 when this config is enabled
      // to allow the reponse to be padded as well

      // two bytes RDLEN, 4 bytes padding header
      unsigned int packetLen = aBody.Length() + 2 + 4;
      if (aDisableECS) {
        // 8 bytes for disabling ecs
        packetLen += 8;
      }

      // clamp the padding length, because the padding extension only allows up
      // to 2^16 - 1 bytes padding and adding too much padding wastes resources
      uint32_t padTo = std::clamp<uint32_t>(
          StaticPrefs::network_trr_padding_length(), 0, 1024);

      // Calculate number of padding bytes. The second '%'-operator is necessary
      // because we prefer to add 0 bytes padding rather than padTo bytes
      if (padTo > 0) {
        paddingLen = (padTo - (packetLen % padTo)) % padTo;
      }
      // padding header + padding length
      rdlen += 4 + paddingLen;
    }
    if (aDisableECS) {
      rdlen += 8;
    }

    // RDLEN      | u_int16_t    | length of all RDATA          |
    aBody += (char)((rdlen >> 8) & 0xff);  // upper 8 bit RDLEN
    aBody += (char)(rdlen & 0xff);

    // RDATA      | octet stream | {attribute,value} pairs      |
    // The RDATA is just the ECS option setting zero subnet prefix

    if (aDisableECS) {
      aBody += '\0';  // upper 8 bit OPTION-CODE ECS
      aBody += 8;     // OPTION-CODE, 2 octets, for ECS is 8

      aBody += '\0';  // upper 8 bit OPTION-LENGTH
      aBody += 4;     // OPTION-LENGTH, 2 octets, contains the length of the
                      // payload after OPTION-LENGTH
      aBody += '\0';  // upper 8 bit FAMILY. IANA Address Family Numbers
                      // registry, not the AF_* constants!
      aBody += 1;     // FAMILY (Ipv4), 2 octets

      aBody += '\0';  // SOURCE PREFIX-LENGTH      |     SCOPE PREFIX-LENGTH |
      aBody += '\0';

      // ADDRESS, minimum number of octets == nothing because zero bits
    }

    if (padding) {
      aBody += '\0';  // upper 8 bit option OPTION-CODE PADDING
      aBody += 12;    // OPTION-CODE, 2 octets, for PADDING is 12

      // OPTION-LENGTH, 2 octets
      aBody += (char)((paddingLen >> 8) & 0xff);
      aBody += (char)(paddingLen & 0xff);
      for (unsigned int i = 0; i < paddingLen; i++) {
        aBody += '\0';
      }
    }
  }

  SetDNSPacketStatus(DNSPacketStatus::Success);
  return NS_OK;
}

Result<uint8_t, nsresult> DNSPacket::GetRCode() const {
  if (mBodySize < 12) {
    LOG(("DNSPacket::GetRCode - packet too small"));
    return Err(NS_ERROR_ILLEGAL_VALUE);
  }

  return mResponse[3] & 0x0F;
}

Result<bool, nsresult> DNSPacket::RecursionAvailable() const {
  if (mBodySize < 12) {
    LOG(("DNSPacket::GetRCode - packet too small"));
    return Err(NS_ERROR_ILLEGAL_VALUE);
  }

  return mResponse[3] & 0x80;
}

nsresult DNSPacket::DecodeInternal(
    nsCString& aHost, enum TrrType aType, nsCString& aCname, bool aAllowRFC1918,
    DOHresp& aResp, TypeRecordResultType& aTypeResult,
    nsClassHashtable<nsCStringHashKey, DOHresp>& aAdditionalRecords,
    uint32_t& aTTL, const unsigned char* aBuffer, uint32_t aLen) {
  // The response has a 12 byte header followed by the question (returned)
  // and then the answer. The answer section itself contains the name, type
  // and class again and THEN the record data.

  // www.example.com response:
  // header:
  // abcd 8180 0001 0001 0000 0000
  // the question:
  // 0377 7777 0765 7861 6d70 6c65 0363 6f6d 0000 0100 01
  // the answer:
  // 03 7777 7707 6578 616d 706c 6503 636f 6d00 0001 0001
  // 0000 0080 0004 5db8 d822

  unsigned int index = 12;
  uint8_t length;
  nsAutoCString host;
  nsresult rv;
  uint16_t extendedError = UINT16_MAX;

  LOG(("doh decode %s %d bytes\n", aHost.get(), aLen));

  aCname.Truncate();

  if (aLen < 12 || aBuffer[0] || aBuffer[1]) {
    LOG(("TRR bad incoming DOH, eject!\n"));
    return NS_ERROR_ILLEGAL_VALUE;
  }
  uint8_t rcode = mResponse[3] & 0x0F;
  LOG(("TRR Decode %s RCODE %d\n", PromiseFlatCString(aHost).get(), rcode));

  uint16_t questionRecords = get16bit(aBuffer, 4);  // qdcount
  // iterate over the single(?) host name in question
  while (questionRecords) {
    do {
      if (aLen < (index + 1)) {
        LOG(("TRR Decode 1 index: %u size: %u", index, aLen));
        return NS_ERROR_ILLEGAL_VALUE;
      }
      length = static_cast<uint8_t>(aBuffer[index]);
      if (length) {
        if (host.Length()) {
          host.Append(".");
        }
        if (aLen < (index + 1 + length)) {
          LOG(("TRR Decode 2 index: %u size: %u len: %u", index, aLen, length));
          return NS_ERROR_ILLEGAL_VALUE;
        }
        host.Append(((char*)aBuffer) + index + 1, length);
      }
      index += 1 + length;  // skip length byte + label
    } while (length);
    if (aLen < (index + 4)) {
      LOG(("TRR Decode 3 index: %u size: %u", index, aLen));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    index += 4;  // skip question's type, class
    questionRecords--;
  }

  // Figure out the number of answer records from ANCOUNT
  uint16_t answerRecords = get16bit(aBuffer, 6);

  LOG(("TRR Decode: %d answer records (%u bytes body) %s index=%u\n",
       answerRecords, aLen, host.get(), index));

  while (answerRecords) {
    nsAutoCString qname;
    rv = GetQname(qname, index, aBuffer);
    if (NS_FAILED(rv)) {
      return rv;
    }
    // 16 bit TYPE
    if (aLen < (index + 2)) {
      LOG(("TRR: Dohdecode:%d fail at index %d\n", __LINE__, index + 2));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t TYPE = get16bit(aBuffer, index);

    if ((TYPE != TRRTYPE_CNAME) && (TYPE != TRRTYPE_HTTPSSVC) &&
        (TYPE != static_cast<uint16_t>(aType))) {
      // Not the same type as was asked for nor CNAME
      LOG(("TRR: Dohdecode:%d asked for type %d got %d\n", __LINE__, aType,
           TYPE));
      return NS_ERROR_UNEXPECTED;
    }
    index += 2;

    // 16 bit class
    if (aLen < (index + 2)) {
      LOG(("TRR: Dohdecode:%d fail at index %d\n", __LINE__, index + 2));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t CLASS = get16bit(aBuffer, index);
    if (kDNS_CLASS_IN != CLASS) {
      LOG(("TRR bad CLASS (%u) at index %d\n", CLASS, index));
      return NS_ERROR_UNEXPECTED;
    }
    index += 2;

    // 32 bit TTL (seconds)
    if (aLen < (index + 4)) {
      LOG(("TRR: Dohdecode:%d fail at index %d\n", __LINE__, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint32_t TTL = get32bit(aBuffer, index);
    index += 4;

    // 16 bit RDLENGTH
    if (aLen < (index + 2)) {
      LOG(("TRR: Dohdecode:%d fail at index %d\n", __LINE__, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t RDLENGTH = get16bit(aBuffer, index);
    index += 2;

    if (aLen < (index + RDLENGTH)) {
      LOG(("TRR: Dohdecode:%d fail RDLENGTH=%d at index %d\n", __LINE__,
           RDLENGTH, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    // We check if the qname is a case-insensitive match for the host or the
    // FQDN version of the host
    bool responseMatchesQuestion =
        (qname.Length() == aHost.Length() ||
         (aHost.Length() == qname.Length() + 1 && aHost.Last() == '.')) &&
        StringBeginsWith(aHost, qname, nsCaseInsensitiveCStringComparator);

    if (responseMatchesQuestion) {
      // RDATA
      // - A (TYPE 1):  4 bytes
      // - AAAA (TYPE 28): 16 bytes
      // - NS (TYPE 2): N bytes

      switch (TYPE) {
        case TRRTYPE_A:
          if (RDLENGTH != 4) {
            LOG(("TRR bad length for A (%u)\n", RDLENGTH));
            return NS_ERROR_UNEXPECTED;
          }
          rv = aResp.Add(TTL, aBuffer, index, RDLENGTH, aAllowRFC1918);
          if (NS_FAILED(rv)) {
            LOG(
                ("TRR:DohDecode failed: local IP addresses or unknown IP "
                 "family\n"));
            return rv;
          }
          break;
        case TRRTYPE_AAAA:
          if (RDLENGTH != 16) {
            LOG(("TRR bad length for AAAA (%u)\n", RDLENGTH));
            return NS_ERROR_UNEXPECTED;
          }
          rv = aResp.Add(TTL, aBuffer, index, RDLENGTH, aAllowRFC1918);
          if (NS_FAILED(rv)) {
            LOG(("TRR got unique/local IPv6 address!\n"));
            return rv;
          }
          break;

        case TRRTYPE_NS:
          break;
        case TRRTYPE_CNAME:
          if (aCname.IsEmpty()) {
            nsAutoCString qname;
            unsigned int qnameindex = index;
            rv = GetQname(qname, qnameindex, aBuffer);
            if (NS_FAILED(rv)) {
              return rv;
            }
            if (!qname.IsEmpty()) {
              ToLowerCase(qname);
              aCname = qname;
              LOG(("DNSPacket::DohDecode CNAME host %s => %s\n", host.get(),
                   aCname.get()));
            } else {
              LOG(("DNSPacket::DohDecode empty CNAME for host %s!\n",
                   host.get()));
            }
          } else {
            LOG(("DNSPacket::DohDecode CNAME - ignoring another entry\n"));
          }
          break;
        case TRRTYPE_TXT: {
          // TXT record RRDATA sections are a series of character-strings
          // each character string is a length byte followed by that many data
          // bytes
          nsAutoCString txt;
          unsigned int txtIndex = index;
          uint16_t available = RDLENGTH;

          while (available > 0) {
            uint8_t characterStringLen = aBuffer[txtIndex++];
            available--;
            if (characterStringLen > available) {
              LOG(("DNSPacket::DohDecode MALFORMED TXT RECORD\n"));
              break;
            }
            txt.Append((const char*)(&aBuffer[txtIndex]), characterStringLen);
            txtIndex += characterStringLen;
            available -= characterStringLen;
          }

          if (!aTypeResult.is<TypeRecordTxt>()) {
            aTypeResult = AsVariant(CopyableTArray<nsCString>());
          }

          {
            auto& results = aTypeResult.as<TypeRecordTxt>();
            results.AppendElement(txt);
          }
          if (aTTL > TTL) {
            aTTL = TTL;
          }
          LOG(("DNSPacket::DohDecode TXT host %s => %s\n", host.get(),
               txt.get()));

          break;
        }
        case TRRTYPE_HTTPSSVC: {
          struct SVCB parsed;
          int32_t lastSvcParamKey = -1;

          unsigned int svcbIndex = index;
          CheckedInt<uint16_t> available = RDLENGTH;

          // Should have at least 2 bytes for the priority and one for the
          // qname length.
          if (available.value() < 3) {
            return NS_ERROR_UNEXPECTED;
          }

          parsed.mSvcFieldPriority = get16bit(aBuffer, svcbIndex);
          svcbIndex += 2;

          rv = GetQname(parsed.mSvcDomainName, svcbIndex, aBuffer);
          if (NS_FAILED(rv)) {
            return rv;
          }

          if (parsed.mSvcDomainName.IsEmpty()) {
            if (parsed.mSvcFieldPriority == 0) {
              // For AliasMode SVCB RRs, a TargetName of "." indicates that the
              // service is not available or does not exist.
              continue;
            }

            // For ServiceMode SVCB RRs, if TargetName has the value ".",
            // then the owner name of this record MUST be used as
            // the effective TargetName.
            // When the qname is port prefix name, we need to use the
            // original host name as TargetName.
            if (mOriginHost) {
              parsed.mSvcDomainName = *mOriginHost;
            } else {
              parsed.mSvcDomainName = qname;
            }
          }

          available -= (svcbIndex - index);
          if (!available.isValid()) {
            return NS_ERROR_UNEXPECTED;
          }
          while (available.value() >= 4) {
            // Every SvcFieldValues must have at least 4 bytes for the
            // SvcParamKey (2 bytes) and length of SvcParamValue (2 bytes)
            // If the length ever goes above the available data, meaning if
            // available ever underflows, then that is an error.
            struct SvcFieldValue value;
            uint16_t key = get16bit(aBuffer, svcbIndex);
            svcbIndex += 2;

            // 2.2 Clients MUST consider an RR malformed if SvcParamKeys are
            // not in strictly increasing numeric order.
            if (key <= lastSvcParamKey) {
              LOG(("SvcParamKeys not in increasing order"));
              return NS_ERROR_UNEXPECTED;
            }
            lastSvcParamKey = key;

            uint16_t len = get16bit(aBuffer, svcbIndex);
            svcbIndex += 2;

            available -= 4 + len;
            if (!available.isValid()) {
              return NS_ERROR_UNEXPECTED;
            }

            rv = ParseSvcParam(svcbIndex, key, value, len, aBuffer);
            if (NS_FAILED(rv)) {
              return rv;
            }
            svcbIndex += len;

            // If this is an unknown key, we will simply ignore it.
            // We also don't need to record SvcParamKeyMandatory
            if (key == SvcParamKeyMandatory || !IsValidSvcParamKey(key)) {
              continue;
            }

            if (value.mValue.is<SvcParamIpv4Hint>() ||
                value.mValue.is<SvcParamIpv6Hint>()) {
              parsed.mHasIPHints = true;
            }
            if (value.mValue.is<SvcParamEchConfig>()) {
              parsed.mHasEchConfig = true;
              parsed.mEchConfig = value.mValue.as<SvcParamEchConfig>().mValue;
            }
            if (value.mValue.is<SvcParamODoHConfig>()) {
              parsed.mODoHConfig = value.mValue.as<SvcParamODoHConfig>().mValue;
            }
            parsed.mSvcFieldValue.AppendElement(value);
          }

          if (aType != TRRTYPE_HTTPSSVC) {
            // Ignore the entry that we just parsed if we didn't ask for it.
            break;
          }

          // Check for AliasForm
          if (aCname.IsEmpty() && parsed.mSvcFieldPriority == 0) {
            // Alias form SvcDomainName must not have the "." value (empty)
            if (parsed.mSvcDomainName.IsEmpty()) {
              return NS_ERROR_UNEXPECTED;
            }
            aCname = parsed.mSvcDomainName;
            // If aliasForm is present, Service form must be ignored.
            aTypeResult = mozilla::AsVariant(Nothing());
            ToLowerCase(aCname);
            LOG(("DNSPacket::DohDecode HTTPSSVC AliasForm host %s => %s\n",
                 host.get(), aCname.get()));
            break;
          }

          if (!aTypeResult.is<TypeRecordHTTPSSVC>()) {
            aTypeResult = mozilla::AsVariant(CopyableTArray<SVCB>());
          }
          {
            auto& results = aTypeResult.as<TypeRecordHTTPSSVC>();
            results.AppendElement(parsed);
          }

          aTTL = TTL;
          break;
        }
        default:
          // skip unknown record types
          LOG(("TRR unsupported TYPE (%u) RDLENGTH %u\n", TYPE, RDLENGTH));
          break;
      }
    } else {
      LOG(("TRR asked for %s data but got %s\n", aHost.get(), qname.get()));
    }

    index += RDLENGTH;
    LOG(("done with record type %u len %u index now %u of %u\n", TYPE, RDLENGTH,
         index, aLen));
    answerRecords--;
  }

  // NSCOUNT
  uint16_t nsRecords = get16bit(aBuffer, 8);
  LOG(("TRR Decode: %d ns records (%u bytes body)\n", nsRecords, aLen));
  while (nsRecords) {
    rv = PassQName(index, aBuffer);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (aLen < (index + 8)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    index += 2;  // type
    index += 2;  // class
    index += 4;  // ttl

    // 16 bit RDLENGTH
    if (aLen < (index + 2)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t RDLENGTH = get16bit(aBuffer, index);
    index += 2;
    if (aLen < (index + RDLENGTH)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    index += RDLENGTH;
    LOG(("done with nsRecord now %u of %u\n", index, aLen));
    nsRecords--;
  }

  // additional resource records
  uint16_t arRecords = get16bit(aBuffer, 10);
  LOG(("TRR Decode: %d additional resource records (%u bytes body)\n",
       arRecords, aLen));

  while (arRecords) {
    nsAutoCString qname;
    rv = GetQname(qname, index, aBuffer);
    if (NS_FAILED(rv)) {
      LOG(("Bad qname for additional record"));
      return rv;
    }

    if (aLen < (index + 8)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t type = get16bit(aBuffer, index);
    index += 2;
    // The next two bytes encode class
    // (or udpPayloadSize when type is TRRTYPE_OPT)
    uint16_t cls = get16bit(aBuffer, index);
    index += 2;
    // The next 4 bytes encode TTL
    // (or extRCode + ednsVersion + flags when type is TRRTYPE_OPT)
    uint32_t ttl = get32bit(aBuffer, index);
    index += 4;
    // cls and ttl are unused when type is TRRTYPE_OPT

    // 16 bit RDLENGTH
    if (aLen < (index + 2)) {
      LOG(("Record too small"));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    uint16_t rdlength = get16bit(aBuffer, index);
    index += 2;
    if (aLen < (index + rdlength)) {
      LOG(("rdlength too big"));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    auto parseRecord = [&]() {
      LOG(("Parsing additional record type: %u", type));
      auto* entry = aAdditionalRecords.GetOrInsertNew(qname);

      switch (type) {
        case TRRTYPE_A:
          if (kDNS_CLASS_IN != cls) {
            LOG(("NOT IN - returning"));
            return;
          }
          if (rdlength != 4) {
            LOG(("TRR bad length for A (%u)\n", rdlength));
            return;
          }
          rv = entry->Add(ttl, aBuffer, index, rdlength, aAllowRFC1918);
          if (NS_FAILED(rv)) {
            LOG(
                ("TRR:DohDecode failed: local IP addresses or unknown IP "
                 "family\n"));
            return;
          }
          break;
        case TRRTYPE_AAAA:
          if (kDNS_CLASS_IN != cls) {
            LOG(("NOT IN - returning"));
            return;
          }
          if (rdlength != 16) {
            LOG(("TRR bad length for AAAA (%u)\n", rdlength));
            return;
          }
          rv = entry->Add(ttl, aBuffer, index, rdlength, aAllowRFC1918);
          if (NS_FAILED(rv)) {
            LOG(("TRR got unique/local IPv6 address!\n"));
            return;
          }
          break;
        case TRRTYPE_OPT: {  // OPT
          LOG(("Parsing opt rdlen: %u", rdlength));
          unsigned int offset = 0;
          while (offset + 2 <= rdlength) {
            uint16_t optCode = get16bit(aBuffer, index + offset);
            LOG(("optCode: %u", optCode));
            offset += 2;
            if (offset + 2 > rdlength) {
              break;
            }
            uint16_t optLen = get16bit(aBuffer, index + offset);
            LOG(("optLen: %u", optLen));
            offset += 2;
            if (offset + optLen > rdlength) {
              LOG(("offset: %u, optLen: %u, rdlen: %u", offset, optLen,
                   rdlength));
              break;
            }

            LOG(("OPT: code: %u len:%u", optCode, optLen));

            if (optCode != 15) {
              offset += optLen;
              continue;
            }

            // optCode == 15; Extended DNS error

            if (offset + 2 > rdlength || optLen < 2) {
              break;
            }
            extendedError = get16bit(aBuffer, index + offset);

            LOG(("Extended error code: %u message: %s", extendedError,
                 nsAutoCString((char*)aBuffer + index + offset + 2, optLen - 2)
                     .get()));
            offset += optLen;
          }
          break;
        }
        default:
          break;
      }
    };

    parseRecord();

    index += rdlength;
    LOG(("done with additional rr now %u of %u\n", index, aLen));
    arRecords--;
  }

  if (index != aLen) {
    LOG(("DohDecode failed to parse entire response body, %u out of %u bytes\n",
         index, aLen));
    // failed to parse 100%, do not continue
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (aType == TRRTYPE_NS && rcode != 0) {
    return NS_ERROR_UNKNOWN_HOST;
  }

  if ((aType != TRRTYPE_NS) && aCname.IsEmpty() && aResp.mAddresses.IsEmpty() &&
      aTypeResult.is<TypeRecordEmpty>()) {
    // no entries were stored!
    LOG(("TRR: No entries were stored!\n"));

    if (extendedError != UINT16_MAX && hardFail(extendedError)) {
      return NS_ERROR_DEFINITIVE_UNKNOWN_HOST;
    }
    return NS_ERROR_UNKNOWN_HOST;
  }

  // https://tools.ietf.org/html/draft-ietf-dnsop-svcb-httpssvc-03#page-14
  // If one or more SVCB records of ServiceForm SvcRecordType are returned for
  // HOST, clients should select the highest-priority option with acceptable
  // parameters.
  if (aTypeResult.is<TypeRecordHTTPSSVC>()) {
    auto& results = aTypeResult.as<TypeRecordHTTPSSVC>();
    results.Sort();
  }

  return NS_OK;
}

//
// DohDecode() collects the TTL and the IP addresses in the response
//
nsresult DNSPacket::Decode(
    nsCString& aHost, enum TrrType aType, nsCString& aCname, bool aAllowRFC1918,
    DOHresp& aResp, TypeRecordResultType& aTypeResult,
    nsClassHashtable<nsCStringHashKey, DOHresp>& aAdditionalRecords,
    uint32_t& aTTL) {
  nsresult rv =
      DecodeInternal(aHost, aType, aCname, aAllowRFC1918, aResp, aTypeResult,
                     aAdditionalRecords, aTTL, mResponse, mBodySize);
  SetDNSPacketStatus(NS_SUCCEEDED(rv) ? DNSPacketStatus::Success
                                      : DNSPacketStatus::DecodeError);
  return rv;
}

static SECItem* CreateRawConfig(const ObliviousDoHConfig& aConfig) {
  SECItem* item(::SECITEM_AllocItem(nullptr, nullptr,
                                    8 + aConfig.mContents.mPublicKey.Length()));
  if (!item) {
    return nullptr;
  }

  uint16_t index = 0;
  NetworkEndian::writeUint16(&item->data[index], aConfig.mContents.mKemId);
  index += 2;
  NetworkEndian::writeUint16(&item->data[index], aConfig.mContents.mKdfId);
  index += 2;
  NetworkEndian::writeUint16(&item->data[index], aConfig.mContents.mAeadId);
  index += 2;
  uint16_t keyLength = aConfig.mContents.mPublicKey.Length();
  NetworkEndian::writeUint16(&item->data[index], keyLength);
  index += 2;
  memcpy(&item->data[index], aConfig.mContents.mPublicKey.Elements(),
         aConfig.mContents.mPublicKey.Length());
  return item;
}

static bool CreateConfigId(ObliviousDoHConfig& aConfig) {
  SECStatus rv;
  CK_HKDF_PARAMS params = {0};
  SECItem paramsi = {siBuffer, (unsigned char*)&params, sizeof(params)};

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return false;
  }

  UniqueSECItem rawConfig(CreateRawConfig(aConfig));
  if (!rawConfig) {
    return false;
  }

  UniquePK11SymKey configKey(PK11_ImportDataKey(slot.get(), CKM_HKDF_DATA,
                                                PK11_OriginUnwrap, CKA_DERIVE,
                                                rawConfig.get(), nullptr));
  if (!configKey) {
    return false;
  }

  params.bExtract = CK_TRUE;
  params.bExpand = CK_TRUE;
  params.prfHashMechanism = CKM_SHA256;
  params.ulSaltType = CKF_HKDF_SALT_NULL;
  params.pInfo = (unsigned char*)&hODoHConfigID[0];
  params.ulInfoLen = strlen(hODoHConfigID);
  UniquePK11SymKey derived(PK11_DeriveWithFlags(
      configKey.get(), CKM_HKDF_DATA, &paramsi, CKM_HKDF_DERIVE, CKA_DERIVE,
      SHA256_LENGTH, CKF_SIGN | CKF_VERIFY));

  rv = PK11_ExtractKeyValue(derived.get());
  if (rv != SECSuccess) {
    return false;
  }

  SECItem* derivedItem = PK11_GetKeyData(derived.get());
  if (!derivedItem) {
    return false;
  }

  if (derivedItem->len != SHA256_LENGTH) {
    return false;
  }

  aConfig.mConfigId.AppendElements(derivedItem->data, derivedItem->len);
  return true;
}

// static
bool ODoHDNSPacket::ParseODoHConfigs(Span<const uint8_t> aData,
                                     nsTArray<ObliviousDoHConfig>& aOut) {
  // struct {
  //     uint16 kem_id;
  //     uint16 kdf_id;
  //     uint16 aead_id;
  //     opaque public_key<1..2^16-1>;
  //  } ObliviousDoHConfigContents;
  //
  //  struct {
  //     uint16 version;
  //     uint16 length;
  //     select (ObliviousDoHConfig.version) {
  //        case 0xff03: ObliviousDoHConfigContents contents;
  //     }
  //  } ObliviousDoHConfig;
  //
  //  ObliviousDoHConfig ObliviousDoHConfigs<1..2^16-1>;

  Span<const uint8_t>::const_iterator it = aData.begin();
  uint16_t length = 0;
  if (!get16bit(aData, it, length)) {
    return false;
  }

  if (length != aData.Length() - 2) {
    return false;
  }

  nsTArray<ObliviousDoHConfig> result;
  static const uint32_t kMinimumConfigContentLength = 12;
  while (std::distance(it, aData.cend()) > kMinimumConfigContentLength) {
    ObliviousDoHConfig config;
    if (!get16bit(aData, it, config.mVersion)) {
      return false;
    }

    if (!get16bit(aData, it, config.mLength)) {
      return false;
    }

    if (std::distance(it, aData.cend()) < config.mLength) {
      return false;
    }

    if (!get16bit(aData, it, config.mContents.mKemId)) {
      return false;
    }

    if (!get16bit(aData, it, config.mContents.mKdfId)) {
      return false;
    }

    if (!get16bit(aData, it, config.mContents.mAeadId)) {
      return false;
    }

    uint16_t keyLength = 0;
    if (!get16bit(aData, it, keyLength)) {
      return false;
    }

    if (!keyLength || std::distance(it, aData.cend()) < keyLength) {
      return false;
    }

    config.mContents.mPublicKey.AppendElements(Span(it, it + keyLength));
    it += keyLength;

    CreateConfigId(config);

    // Check if the version of the config is supported and validate its content.
    if (config.mVersion == ODOH_VERSION &&
        PK11_HPKE_ValidateParameters(
            static_cast<HpkeKemId>(config.mContents.mKemId),
            static_cast<HpkeKdfId>(config.mContents.mKdfId),
            static_cast<HpkeAeadId>(config.mContents.mAeadId)) == SECSuccess) {
      result.AppendElement(std::move(config));
    } else {
      LOG(("ODoHDNSPacket::ParseODoHConfigs got an invalid config"));
    }
  }

  aOut = std::move(result);
  return true;
}

ODoHDNSPacket::~ODoHDNSPacket() { PK11_HPKE_DestroyContext(mContext, true); }

nsresult ODoHDNSPacket::EncodeRequest(nsCString& aBody, const nsACString& aHost,
                                      uint16_t aType, bool aDisableECS) {
  nsAutoCString queryBody;
  nsresult rv = DNSPacket::EncodeRequest(queryBody, aHost, aType, aDisableECS);
  if (NS_FAILED(rv)) {
    SetDNSPacketStatus(DNSPacketStatus::EncodeError);
    return rv;
  }

  if (!gODoHService->ODoHConfigs()) {
    SetDNSPacketStatus(DNSPacketStatus::KeyNotAvailable);
    return NS_ERROR_FAILURE;
  }

  if (gODoHService->ODoHConfigs()->IsEmpty()) {
    SetDNSPacketStatus(DNSPacketStatus::KeyNotUsable);
    return NS_ERROR_FAILURE;
  }

  // We only use the first ODoHConfig.
  const ObliviousDoHConfig& config = (*gODoHService->ODoHConfigs())[0];

  ObliviousDoHMessage message;
  // The spec didn't recommand padding length for encryption, let's use 0 here.
  if (!EncryptDNSQuery(queryBody, 0, config, message)) {
    SetDNSPacketStatus(DNSPacketStatus::EncryptError);
    return NS_ERROR_FAILURE;
  }

  aBody.Truncate();
  aBody += message.mType;
  uint16_t keyIdLength = message.mKeyId.Length();
  aBody += static_cast<uint8_t>(keyIdLength >> 8);
  aBody += static_cast<uint8_t>(keyIdLength);
  aBody.Append(reinterpret_cast<const char*>(message.mKeyId.Elements()),
               keyIdLength);
  uint16_t messageLen = message.mEncryptedMessage.Length();
  aBody += static_cast<uint8_t>(messageLen >> 8);
  aBody += static_cast<uint8_t>(messageLen);
  aBody.Append(
      reinterpret_cast<const char*>(message.mEncryptedMessage.Elements()),
      messageLen);

  SetDNSPacketStatus(DNSPacketStatus::Success);
  return NS_OK;
}

/*
 * def encrypt_query_body(pkR, key_id, Q_plain):
 *     enc, context = SetupBaseS(pkR, "odoh query")
 *     aad = 0x01 || len(key_id) || key_id
 *     ct = context.Seal(aad, Q_plain)
 *     Q_encrypted = enc || ct
 *     return Q_encrypted
 */
bool ODoHDNSPacket::EncryptDNSQuery(const nsACString& aQuery,
                                    uint16_t aPaddingLen,
                                    const ObliviousDoHConfig& aConfig,
                                    ObliviousDoHMessage& aOut) {
  mContext = PK11_HPKE_NewContext(
      static_cast<HpkeKemId>(aConfig.mContents.mKemId),
      static_cast<HpkeKdfId>(aConfig.mContents.mKdfId),
      static_cast<HpkeAeadId>(aConfig.mContents.mAeadId), nullptr, nullptr);
  if (!mContext) {
    LOG(("ODoHDNSPacket::EncryptDNSQuery create context failed"));
    return false;
  }

  SECKEYPublicKey* pkR;
  SECStatus rv =
      PK11_HPKE_Deserialize(mContext, aConfig.mContents.mPublicKey.Elements(),
                            aConfig.mContents.mPublicKey.Length(), &pkR);
  if (rv != SECSuccess) {
    return false;
  }

  UniqueSECItem hpkeInfo(
      ::SECITEM_AllocItem(nullptr, nullptr, strlen(kODoHQuery)));
  if (!hpkeInfo) {
    return false;
  }

  memcpy(hpkeInfo->data, kODoHQuery, strlen(kODoHQuery));

  rv = PK11_HPKE_SetupS(mContext, nullptr, nullptr, pkR, hpkeInfo.get());
  if (rv != SECSuccess) {
    LOG(("ODoHDNSPacket::EncryptDNSQuery setupS failed"));
    return false;
  }

  const SECItem* hpkeEnc = PK11_HPKE_GetEncapPubKey(mContext);
  if (!hpkeEnc) {
    return false;
  }

  // aad = 0x01 || len(key_id) || key_id
  UniqueSECItem aad(::SECITEM_AllocItem(nullptr, nullptr,
                                        1 + 2 + aConfig.mConfigId.Length()));
  if (!aad) {
    return false;
  }

  aad->data[0] = ODOH_QUERY;
  NetworkEndian::writeUint16(&aad->data[1], aConfig.mConfigId.Length());
  memcpy(&aad->data[3], aConfig.mConfigId.Elements(),
         aConfig.mConfigId.Length());

  // struct {
  //     opaque dns_message<1..2^16-1>;
  //     opaque padding<0..2^16-1>;
  // } ObliviousDoHMessagePlaintext;
  SECItem* odohPlainText(::SECITEM_AllocItem(
      nullptr, nullptr, 2 + aQuery.Length() + 2 + aPaddingLen));
  if (!odohPlainText) {
    return false;
  }

  mPlainQuery.reset(odohPlainText);
  memset(mPlainQuery->data, 0, mPlainQuery->len);
  NetworkEndian::writeUint16(&mPlainQuery->data[0], aQuery.Length());
  memcpy(&mPlainQuery->data[2], aQuery.BeginReading(), aQuery.Length());
  NetworkEndian::writeUint16(&mPlainQuery->data[2 + aQuery.Length()],
                             aPaddingLen);

  SECItem* chCt = nullptr;
  rv = PK11_HPKE_Seal(mContext, aad.get(), mPlainQuery.get(), &chCt);
  if (rv != SECSuccess) {
    LOG(("ODoHDNSPacket::EncryptDNSQuery seal failed"));
    return false;
  }

  UniqueSECItem ct(chCt);

  aOut.mType = ODOH_QUERY;
  aOut.mKeyId.AppendElements(aConfig.mConfigId);
  aOut.mEncryptedMessage.AppendElements(Span(hpkeEnc->data, hpkeEnc->len));
  aOut.mEncryptedMessage.AppendElements(Span(ct->data, ct->len));

  return true;
}

nsresult ODoHDNSPacket::Decode(
    nsCString& aHost, enum TrrType aType, nsCString& aCname, bool aAllowRFC1918,
    DOHresp& aResp, TypeRecordResultType& aTypeResult,
    nsClassHashtable<nsCStringHashKey, DOHresp>& aAdditionalRecords,
    uint32_t& aTTL) {
  // This function could be called multiple times when we are checking CNAME
  // records, but we only need to decrypt the response once.
  if (!mDecryptedResponseRange) {
    if (!DecryptDNSResponse()) {
      SetDNSPacketStatus(DNSPacketStatus::DecryptError);
      return NS_ERROR_FAILURE;
    }

    uint32_t index = 0;
    uint16_t responseLength = get16bit(mResponse, index);
    index += 2;

    if (mBodySize < (index + responseLength)) {
      SetDNSPacketStatus(DNSPacketStatus::DecryptError);
      return NS_ERROR_ILLEGAL_VALUE;
    }

    DecryptedResponseRange range;
    range.mStart = index;
    range.mLength = responseLength;

    index += responseLength;
    uint16_t paddingLen = get16bit(mResponse, index);

    if (static_cast<unsigned int>(4 + responseLength + paddingLen) !=
        mBodySize) {
      SetDNSPacketStatus(DNSPacketStatus::DecryptError);
      return NS_ERROR_ILLEGAL_VALUE;
    }

    mDecryptedResponseRange.emplace(range);
  }

  nsresult rv = DecodeInternal(aHost, aType, aCname, aAllowRFC1918, aResp,
                               aTypeResult, aAdditionalRecords, aTTL,
                               &mResponse[mDecryptedResponseRange->mStart],
                               mDecryptedResponseRange->mLength);
  SetDNSPacketStatus(NS_SUCCEEDED(rv) ? DNSPacketStatus::Success
                                      : DNSPacketStatus::DecodeError);
  return rv;
}

static bool CreateObliviousDoHMessage(const unsigned char* aData,
                                      unsigned int aLength,
                                      ObliviousDoHMessage& aOut) {
  if (aLength < 5) {
    return false;
  }

  unsigned int index = 0;
  aOut.mType = static_cast<ObliviousDoHMessageType>(aData[index++]);

  uint16_t keyIdLength = get16bit(aData, index);
  index += 2;
  if (aLength < (index + keyIdLength)) {
    return false;
  }

  aOut.mKeyId.AppendElements(Span(aData + index, keyIdLength));
  index += keyIdLength;

  uint16_t messageLen = get16bit(aData, index);
  index += 2;
  if (aLength < (index + messageLen)) {
    return false;
  }

  aOut.mEncryptedMessage.AppendElements(Span(aData + index, messageLen));
  return true;
}

static SECStatus HKDFExtract(SECItem* aSalt, PK11SymKey* aIkm,
                             UniquePK11SymKey& aOutKey) {
  CK_HKDF_PARAMS params = {0};
  SECItem paramsItem = {siBuffer, (unsigned char*)&params, sizeof(params)};

  params.bExtract = CK_TRUE;
  params.bExpand = CK_FALSE;
  params.prfHashMechanism = CKM_SHA256;
  params.ulSaltType = aSalt ? CKF_HKDF_SALT_DATA : CKF_HKDF_SALT_NULL;
  params.pSalt = aSalt ? (CK_BYTE_PTR)aSalt->data : nullptr;
  params.ulSaltLen = aSalt ? aSalt->len : 0;

  UniquePK11SymKey prk(PK11_Derive(aIkm, CKM_HKDF_DERIVE, &paramsItem,
                                   CKM_HKDF_DERIVE, CKA_DERIVE, 0));
  if (!prk) {
    return SECFailure;
  }

  aOutKey.swap(prk);
  return SECSuccess;
}

static SECStatus HKDFExpand(PK11SymKey* aPrk, const SECItem* aInfo, int aLen,
                            bool aKey, UniquePK11SymKey& aOutKey) {
  CK_HKDF_PARAMS params = {0};
  SECItem paramsItem = {siBuffer, (unsigned char*)&params, sizeof(params)};

  params.bExtract = CK_FALSE;
  params.bExpand = CK_TRUE;
  params.prfHashMechanism = CKM_SHA256;
  params.ulSaltType = CKF_HKDF_SALT_NULL;
  params.pInfo = (CK_BYTE_PTR)aInfo->data;
  params.ulInfoLen = aInfo->len;
  CK_MECHANISM_TYPE deriveMech = CKM_HKDF_DERIVE;
  CK_MECHANISM_TYPE keyMech = aKey ? CKM_AES_GCM : CKM_HKDF_DERIVE;

  UniquePK11SymKey derivedKey(
      PK11_Derive(aPrk, deriveMech, &paramsItem, keyMech, CKA_DERIVE, aLen));
  if (!derivedKey) {
    return SECFailure;
  }

  aOutKey.swap(derivedKey);
  return SECSuccess;
}

/*
 * def decrypt_response_body(context, Q_plain, R_encrypted, response_nonce):
 *   aead_key, aead_nonce = derive_secrets(context, Q_plain, response_nonce)
 *   aad = 0x02 || len(response_nonce) || response_nonce
 *   R_plain, error = Open(key, nonce, aad, R_encrypted)
 *   return R_plain, error
 */
bool ODoHDNSPacket::DecryptDNSResponse() {
  ObliviousDoHMessage message;
  if (!CreateObliviousDoHMessage(mResponse, mBodySize, message)) {
    LOG(("ODoHDNSPacket::DecryptDNSResponse invalid response"));
    return false;
  }

  if (message.mType != ODOH_RESPONSE) {
    return false;
  }

  const unsigned int kResponseNonceLen = 16;
  // KeyId is actually response_nonce
  if (message.mKeyId.Length() != kResponseNonceLen) {
    return false;
  }

  // def derive_secrets(context, Q_plain, response_nonce):
  //    secret = context.Export("odoh response", Nk)
  //    salt = Q_plain || len(response_nonce) || response_nonce
  //    prk = Extract(salt, secret)
  //    key = Expand(odoh_prk, "odoh key", Nk)
  //    nonce = Expand(odoh_prk, "odoh nonce", Nn)
  //    return key, nonce
  const SECItem kODoHResponsetInfoItem = {
      siBuffer, (unsigned char*)kODoHResponse,
      static_cast<unsigned int>(strlen(kODoHResponse))};
  const unsigned int kAes128GcmKeyLen = 16;
  const unsigned int kAes128GcmNonceLen = 12;
  PK11SymKey* tmp = nullptr;
  SECStatus rv = PK11_HPKE_ExportSecret(mContext, &kODoHResponsetInfoItem,
                                        kAes128GcmKeyLen, &tmp);
  if (rv != SECSuccess) {
    LOG(("ODoHDNSPacket::DecryptDNSResponse export secret failed"));
    return false;
  }
  UniquePK11SymKey odohSecret(tmp);

  SECItem* salt(::SECITEM_AllocItem(nullptr, nullptr,
                                    mPlainQuery->len + 2 + kResponseNonceLen));
  memcpy(salt->data, mPlainQuery->data, mPlainQuery->len);
  NetworkEndian::writeUint16(&salt->data[mPlainQuery->len], kResponseNonceLen);
  memcpy(salt->data + mPlainQuery->len + 2, message.mKeyId.Elements(),
         kResponseNonceLen);
  UniqueSECItem st(salt);
  UniquePK11SymKey odohPrk;
  rv = HKDFExtract(salt, odohSecret.get(), odohPrk);
  if (rv != SECSuccess) {
    LOG(("ODoHDNSPacket::DecryptDNSResponse extract failed"));
    return false;
  }

  SECItem keyInfoItem = {siBuffer, (unsigned char*)&kODoHKey[0],
                         static_cast<unsigned int>(strlen(kODoHKey))};
  UniquePK11SymKey key;
  rv = HKDFExpand(odohPrk.get(), &keyInfoItem, kAes128GcmKeyLen, true, key);
  if (rv != SECSuccess) {
    LOG(("ODoHDNSPacket::DecryptDNSResponse expand key failed"));
    return false;
  }

  SECItem nonceInfoItem = {siBuffer, (unsigned char*)&kODoHNonce[0],
                           static_cast<unsigned int>(strlen(kODoHNonce))};
  UniquePK11SymKey nonce;
  rv = HKDFExpand(odohPrk.get(), &nonceInfoItem, kAes128GcmNonceLen, false,
                  nonce);
  if (rv != SECSuccess) {
    LOG(("ODoHDNSPacket::DecryptDNSResponse expand nonce failed"));
    return false;
  }

  rv = PK11_ExtractKeyValue(nonce.get());
  if (rv != SECSuccess) {
    return false;
  }

  SECItem* derivedItem = PK11_GetKeyData(nonce.get());
  if (!derivedItem) {
    return false;
  }

  // aad = 0x02 || len(response_nonce) || response_nonce
  SECItem* aadItem(
      ::SECITEM_AllocItem(nullptr, nullptr, 1 + 2 + kResponseNonceLen));
  aadItem->data[0] = ODOH_RESPONSE;
  NetworkEndian::writeUint16(&aadItem->data[1], kResponseNonceLen);
  memcpy(&aadItem->data[3], message.mKeyId.Elements(), kResponseNonceLen);
  UniqueSECItem aad(aadItem);

  SECItem paramItem;
  CK_GCM_PARAMS param;
  param.pIv = derivedItem->data;
  param.ulIvLen = derivedItem->len;
  param.ulIvBits = param.ulIvLen * 8;
  param.ulTagBits = 16 * 8;
  param.pAAD = (CK_BYTE_PTR)aad->data;
  param.ulAADLen = aad->len;

  paramItem.type = siBuffer;
  paramItem.data = (unsigned char*)(&param);
  paramItem.len = sizeof(CK_GCM_PARAMS);

  memset(mResponse, 0, mBodySize);
  rv = PK11_Decrypt(key.get(), CKM_AES_GCM, &paramItem, mResponse, &mBodySize,
                    MAX_SIZE, message.mEncryptedMessage.Elements(),
                    message.mEncryptedMessage.Length());
  if (rv != SECSuccess) {
    LOG(("ODoHDNSPacket::DecryptDNSResponse decrypt failed %d",
         PORT_GetError()));
    return false;
  }

  return true;
}

}  // namespace net
}  // namespace mozilla
