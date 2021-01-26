/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DNSPacket.h"

namespace mozilla {
namespace net {

extern mozilla::LazyLogModule gHostResolverLog;
#undef LOG
#undef LOG_ENABLED
#define LOG(args) MOZ_LOG(gHostResolverLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() \
  MOZ_LOG_TEST(mozilla::net::gHostResolverLog, mozilla::LogLevel::Debug)

static uint16_t get16bit(const unsigned char* aData, unsigned int index) {
  return ((aData[index] << 8) | aData[index + 1]);
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
                                  SvcFieldValue& field, uint16_t length) {
  switch (key) {
    case SvcParamKeyMandatory: {
      if (length % 2 != 0) {
        // This key should encode a list of uint16_t
        return NS_ERROR_UNEXPECTED;
      }
      while (length > 0) {
        uint16_t mandatoryKey = get16bit(mResponse, svcbIndex);
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
        uint8_t alpnIdLength = mResponse[svcbIndex++];
        length -= 1;
        if (alpnIdLength > length) {
          return NS_ERROR_UNEXPECTED;
        }

        alpnArray.AppendElement(
            nsCString((const char*)&mResponse[svcbIndex], alpnIdLength));
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
          AsVariant(SvcParamPort{.mValue = get16bit(mResponse, svcbIndex)});
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
        addr.inet.ip = ntohl(get32bit(mResponse, svcbIndex));
        ipv4array.AppendElement(addr);
        length -= 4;
        svcbIndex += 4;
      }
      break;
    }
    case SvcParamKeyEchConfig: {
      field.mValue = AsVariant(SvcParamEchConfig{
          .mValue = nsCString((const char*)(&mResponse[svcbIndex]), length)});
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
          addr.inet6.ip.u8[i] = mResponse[svcbIndex];
        }
        ipv6array.AppendElement(addr);
        length -= 16;
        // no need to increase svcbIndex - we did it in the for above.
      }
      break;
    }
    case SvcParamKeyODoHConfig: {
      field.mValue = AsVariant(SvcParamODoHConfig{
          .mValue = nsCString((const char*)(&mResponse[svcbIndex]), length)});
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

nsresult DNSPacket::PassQName(unsigned int& index) {
  uint8_t length;
  do {
    if (mBodySize < (index + 1)) {
      LOG(("TRR: PassQName:%d fail at index %d\n", __LINE__, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    length = static_cast<uint8_t>(mResponse[index]);
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
nsresult DNSPacket::GetQname(nsACString& aQname, unsigned int& aIndex) {
  uint8_t clength = 0;
  unsigned int cindex = aIndex;
  unsigned int loop = 128;    // a valid DNS name can never loop this much
  unsigned int endindex = 0;  // index position after this data
  do {
    if (cindex >= mBodySize) {
      LOG(("TRR: bad Qname packet\n"));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    clength = static_cast<uint8_t>(mResponse[cindex]);
    if ((clength & 0xc0) == 0xc0) {
      // name pointer, get the new offset (14 bits)
      if ((cindex + 1) >= mBodySize) {
        return NS_ERROR_ILLEGAL_VALUE;
      }
      // extract the new index position for the next label
      uint16_t newpos = (clength & 0x3f) << 8 | mResponse[cindex + 1];
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
      aQname.Append((const char*)(&mResponse[cindex]), clength);
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

// static
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

  aBody += '\0';                    // ARCOUNT
  aBody += aDisableECS ? 1 : '\0';  // ARCOUNT low byte for EDNS(0)

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

  if (aDisableECS) {
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

    aBody += '\0';  // upper 8 bit RDLEN
    aBody += 8;  // RDLEN      | u_int16_t    | length of all RDATA          |

    // RDATA      | octet stream | {attribute,value} pairs      |
    // The RDATA is just the ECS option setting zero subnet prefix

    aBody += '\0';  // upper 8 bit OPTION-CODE ECS
    aBody += 8;     // OPTION-CODE, 2 octets, for ECS is 8

    aBody += '\0';  // upper 8 bit OPTION-LENGTH
    aBody += 4;  // OPTION-LENGTH, 2 octets, contains the length of the payload
                 // after OPTION-LENGTH
    aBody += '\0';  // upper 8 bit FAMILY. IANA Address Family Numbers registry,
                    // not the AF_* constants!
    aBody += 1;     // FAMILY (Ipv4), 2 octets

    aBody += '\0';  // SOURCE PREFIX-LENGTH      |     SCOPE PREFIX-LENGTH |
    aBody += '\0';

    // ADDRESS, minimum number of octets == nothing because zero bits
  }
  return NS_OK;
}

//
// DohDecode() collects the TTL and the IP addresses in the response
//
// static
nsresult DNSPacket::Decode(
    nsCString& aHost, enum TrrType aType, nsCString& aCname, bool aAllowRFC1918,
    nsHostRecord::TRRSkippedReason& aReason, DOHresp& aResp,
    TypeRecordResultType& aTypeResult,
    nsClassHashtable<nsCStringHashKey, DOHresp>& aAdditionalRecords,
    uint32_t& aTTL) {
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

  LOG(("doh decode %s %d bytes\n", aHost.get(), mBodySize));

  aCname.Truncate();

  if (mBodySize < 12 || mResponse[0] || mResponse[1]) {
    LOG(("TRR bad incoming DOH, eject!\n"));
    return NS_ERROR_ILLEGAL_VALUE;
  }
  uint8_t rcode = mResponse[3] & 0x0F;
  LOG(("TRR Decode %s RCODE %d\n", aHost.get(), rcode));
  if (rcode) {
    if (aReason == nsHostRecord::TRR_UNSET) {
      aReason = nsHostRecord::TRR_RCODE_FAIL;
    }
  }

  uint16_t questionRecords = get16bit(mResponse, 4);  // qdcount
  // iterate over the single(?) host name in question
  while (questionRecords) {
    do {
      if (mBodySize < (index + 1)) {
        LOG(("TRR Decode 1 index: %u size: %u", index, mBodySize));
        return NS_ERROR_ILLEGAL_VALUE;
      }
      length = static_cast<uint8_t>(mResponse[index]);
      if (length) {
        if (host.Length()) {
          host.Append(".");
        }
        if (mBodySize < (index + 1 + length)) {
          LOG(("TRR Decode 2 index: %u size: %u len: %u", index, mBodySize,
               length));
          return NS_ERROR_ILLEGAL_VALUE;
        }
        host.Append(((char*)mResponse) + index + 1, length);
      }
      index += 1 + length;  // skip length byte + label
    } while (length);
    if (mBodySize < (index + 4)) {
      LOG(("TRR Decode 3 index: %u size: %u", index, mBodySize));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    index += 4;  // skip question's type, class
    questionRecords--;
  }

  // Figure out the number of answer records from ANCOUNT
  uint16_t answerRecords = get16bit(mResponse, 6);

  LOG(("TRR Decode: %d answer records (%u bytes body) %s index=%u\n",
       answerRecords, mBodySize, host.get(), index));

  while (answerRecords) {
    nsAutoCString qname;
    rv = GetQname(qname, index);
    if (NS_FAILED(rv)) {
      return rv;
    }
    // 16 bit TYPE
    if (mBodySize < (index + 2)) {
      LOG(("TRR: Dohdecode:%d fail at index %d\n", __LINE__, index + 2));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t TYPE = get16bit(mResponse, index);

    if ((TYPE != TRRTYPE_CNAME) && (TYPE != TRRTYPE_HTTPSSVC) &&
        (TYPE != static_cast<uint16_t>(aType))) {
      // Not the same type as was asked for nor CNAME
      LOG(("TRR: Dohdecode:%d asked for type %d got %d\n", __LINE__, aType,
           TYPE));
      return NS_ERROR_UNEXPECTED;
    }
    index += 2;

    // 16 bit class
    if (mBodySize < (index + 2)) {
      LOG(("TRR: Dohdecode:%d fail at index %d\n", __LINE__, index + 2));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t CLASS = get16bit(mResponse, index);
    if (kDNS_CLASS_IN != CLASS) {
      LOG(("TRR bad CLASS (%u) at index %d\n", CLASS, index));
      return NS_ERROR_UNEXPECTED;
    }
    index += 2;

    // 32 bit TTL (seconds)
    if (mBodySize < (index + 4)) {
      LOG(("TRR: Dohdecode:%d fail at index %d\n", __LINE__, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint32_t TTL = get32bit(mResponse, index);
    index += 4;

    // 16 bit RDLENGTH
    if (mBodySize < (index + 2)) {
      LOG(("TRR: Dohdecode:%d fail at index %d\n", __LINE__, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t RDLENGTH = get16bit(mResponse, index);
    index += 2;

    if (mBodySize < (index + RDLENGTH)) {
      LOG(("TRR: Dohdecode:%d fail RDLENGTH=%d at index %d\n", __LINE__,
           RDLENGTH, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    // We check if the qname is a case-insensitive match for the host or the
    // FQDN version of the host
    bool responseMatchesQuestion =
        (qname.Length() == aHost.Length() ||
         (aHost.Length() == qname.Length() + 1 && aHost.Last() == '.')) &&
        qname.Compare(aHost.BeginReading(), true, qname.Length()) == 0;

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
          rv = aResp.Add(TTL, mResponse, index, RDLENGTH, aAllowRFC1918);
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
          rv = aResp.Add(TTL, mResponse, index, RDLENGTH, aAllowRFC1918);
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
            rv = GetQname(qname, qnameindex);
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
            uint8_t characterStringLen = mResponse[txtIndex++];
            available--;
            if (characterStringLen > available) {
              LOG(("DNSPacket::DohDecode MALFORMED TXT RECORD\n"));
              break;
            }
            txt.Append((const char*)(&mResponse[txtIndex]), characterStringLen);
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

          parsed.mSvcFieldPriority = get16bit(mResponse, svcbIndex);
          svcbIndex += 2;

          rv = GetQname(parsed.mSvcDomainName, svcbIndex);
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
            parsed.mSvcDomainName = qname;
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
            uint16_t key = get16bit(mResponse, svcbIndex);
            svcbIndex += 2;

            // 2.2 Clients MUST consider an RR malformed if SvcParamKeys are
            // not in strictly increasing numeric order.
            if (key <= lastSvcParamKey) {
              LOG(("SvcParamKeys not in increasing order"));
              return NS_ERROR_UNEXPECTED;
            }
            lastSvcParamKey = key;

            uint16_t len = get16bit(mResponse, svcbIndex);
            svcbIndex += 2;

            available -= 4 + len;
            if (!available.isValid()) {
              return NS_ERROR_UNEXPECTED;
            }

            rv = ParseSvcParam(svcbIndex, key, value, len);
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

          // Check for AliasForm
          if (aCname.IsEmpty() && parsed.mSvcFieldPriority == 0) {
            // Alias form SvcDomainName must not have the "." value (empty)
            if (parsed.mSvcDomainName.IsEmpty()) {
              return NS_ERROR_UNEXPECTED;
            }
            aCname = parsed.mSvcDomainName;
            ToLowerCase(aCname);
            LOG(("DNSPacket::DohDecode HTTPSSVC AliasForm host %s => %s\n",
                 host.get(), aCname.get()));
            break;
          }

          if (aType != TRRTYPE_HTTPSSVC) {
            // Ignore the entry that we just parsed if we didn't ask for it.
            break;
          }

          if (!aTypeResult.is<TypeRecordHTTPSSVC>()) {
            aTypeResult = mozilla::AsVariant(CopyableTArray<SVCB>());
          }
          {
            auto& results = aTypeResult.as<TypeRecordHTTPSSVC>();
            results.AppendElement(parsed);
          }
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
         index, mBodySize));
    answerRecords--;
  }

  // NSCOUNT
  uint16_t nsRecords = get16bit(mResponse, 8);
  LOG(("TRR Decode: %d ns records (%u bytes body)\n", nsRecords, mBodySize));
  while (nsRecords) {
    rv = PassQName(index);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (mBodySize < (index + 8)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    index += 2;  // type
    index += 2;  // class
    index += 4;  // ttl

    // 16 bit RDLENGTH
    if (mBodySize < (index + 2)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t RDLENGTH = get16bit(mResponse, index);
    index += 2;
    if (mBodySize < (index + RDLENGTH)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    index += RDLENGTH;
    LOG(("done with nsRecord now %u of %u\n", index, mBodySize));
    nsRecords--;
  }

  // additional resource records
  uint16_t arRecords = get16bit(mResponse, 10);
  LOG(("TRR Decode: %d additional resource records (%u bytes body)\n",
       arRecords, mBodySize));

  while (arRecords) {
    nsAutoCString qname;
    rv = GetQname(qname, index);
    if (NS_FAILED(rv)) {
      LOG(("Bad qname for additional record"));
      return rv;
    }

    if (mBodySize < (index + 8)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t type = get16bit(mResponse, index);
    index += 2;
    // The next two bytes encode class
    // (or udpPayloadSize when type is TRRTYPE_OPT)
    uint16_t cls = get16bit(mResponse, index);
    index += 2;
    // The next 4 bytes encode TTL
    // (or extRCode + ednsVersion + flags when type is TRRTYPE_OPT)
    uint32_t ttl = get32bit(mResponse, index);
    index += 4;
    // cls and ttl are unused when type is TRRTYPE_OPT

    // 16 bit RDLENGTH
    if (mBodySize < (index + 2)) {
      LOG(("Record too small"));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    uint16_t rdlength = get16bit(mResponse, index);
    index += 2;
    if (mBodySize < (index + rdlength)) {
      LOG(("rdlength too big"));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    auto parseRecord = [&]() {
      LOG(("Parsing additional record type: %u", type));
      auto& entry = aAdditionalRecords.GetOrInsert(qname);
      if (!entry) {
        entry.reset(new DOHresp());
      }

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
          rv = entry->Add(ttl, mResponse, index, rdlength, aAllowRFC1918);
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
          rv = entry->Add(ttl, mResponse, index, rdlength, aAllowRFC1918);
          if (NS_FAILED(rv)) {
            LOG(("TRR got unique/local IPv6 address!\n"));
            return;
          }
          break;
        case TRRTYPE_OPT: {  // OPT
          LOG(("Parsing opt rdlen: %u", rdlength));
          unsigned int offset = 0;
          while (offset + 2 <= rdlength) {
            uint16_t optCode = get16bit(mResponse, index + offset);
            LOG(("optCode: %u", optCode));
            offset += 2;
            if (offset + 2 > rdlength) {
              break;
            }
            uint16_t optLen = get16bit(mResponse, index + offset);
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
            extendedError = get16bit(mResponse, index + offset);

            LOG((
                "Extended error code: %u message: %s", extendedError,
                nsAutoCString((char*)mResponse + index + offset + 2, optLen - 2)
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
    LOG(("done with additional rr now %u of %u\n", index, mBodySize));
    arRecords--;
  }

  if (index != mBodySize) {
    LOG(("DohDecode failed to parse entire response body, %u out of %u bytes\n",
         index, mBodySize));
    // failed to parse 100%, do not continue
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if ((aType != TRRTYPE_NS) && aCname.IsEmpty() && aResp.mAddresses.IsEmpty() &&
      aTypeResult.is<TypeRecordEmpty>()) {
    // no entries were stored!
    LOG(("TRR: No entries were stored!\n"));
    if (extendedError != UINT16_MAX && hardFail(extendedError)) {
      return NS_ERROR_DEFINITIVE_UNKNOWN_HOST;
    }
    return NS_ERROR_FAILURE;
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

}  // namespace net
}  // namespace mozilla
