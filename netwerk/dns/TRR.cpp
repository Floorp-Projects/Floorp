/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DNS.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsHostResolver.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIIOService.h"
#include "nsIInputStream.h"
#include "nsISupportsBase.h"
#include "nsISupportsUtils.h"
#include "nsIUploadChannel2.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"
#include "nsThreadUtils.h"
#include "nsURLHelper.h"
#include "TRR.h"
#include "TRRService.h"

#include "mozilla/Base64.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Tokenizer.h"

namespace mozilla {
namespace net {

#undef LOG
extern mozilla::LazyLogModule gHostResolverLog;
#define LOG(args) MOZ_LOG(gHostResolverLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(mozilla::net::gHostResolverLog, mozilla::LogLevel::Debug)

NS_IMPL_ISUPPORTS(TRR, nsIHttpPushListener, nsIInterfaceRequestor, nsIStreamListener, nsIRunnable)

const uint8_t kDNS_CLASS_IN = 1;

NS_IMETHODIMP
TRR::Notify(nsITimer *aTimer)
{
  if (aTimer == mTimeout) {
    mTimeout = nullptr;
    Cancel();
  } else {
    MOZ_CRASH("Unknown timer");
  }

  return NS_OK;
}

// convert a given host request to a DOH 'body'
//
nsresult
TRR::DohEncode(nsCString &aBody)
{
  aBody.Truncate();
  // Header
  aBody += '\0';
  aBody += '\0'; // 16 bit id
  aBody += 0x01; // |QR|   Opcode  |AA|TC|RD| Set the RD bit
  aBody += '\0'; // |RA|   Z    |   RCODE   |
  aBody += '\0';
  aBody += 1;    // QDCOUNT (number of entries in the question section)
  aBody += '\0';
  aBody += '\0'; // ANCOUNT
  aBody += '\0';
  aBody += '\0'; // NSCOUNT
  aBody += '\0';
  aBody += '\0'; // ARCOUNT

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
    index = mHost.FindChar('.', offset);
    if (kNotFound != index) {
      dotFound = true;
      labelLength = index - offset;
    } else {
      labelLength = mHost.Length() - offset;
    }
    if (labelLength > 63) {
      // too long label!
      return NS_ERROR_ILLEGAL_VALUE;
    }
    aBody += static_cast<unsigned char>(labelLength);
    nsDependentCSubstring label = Substring(mHost, offset, labelLength);
    aBody.Append(label);
    if(!dotFound) {
      aBody += '\0'; // terminate with a final zero
      break;
    }
    offset += labelLength + 1; // move over label and dot
  } while(true);

  aBody += '\0'; // upper 8 bit TYPE
  aBody += static_cast<uint8_t>(mType);
  aBody += '\0'; // upper 8 bit CLASS
  aBody += kDNS_CLASS_IN;  // IN - "the Internet"

  return NS_OK;
}

NS_IMETHODIMP
TRR::Run()
{
  MOZ_ASSERT(NS_IsMainThread());
  if ((gTRRService == nullptr) || NS_FAILED(SendHTTPRequest())) {
    FailData(NS_ERROR_FAILURE);
    // The dtor will now be run
  }
  return NS_OK;
}

nsresult
TRR::SendHTTPRequest()
{
  // This is essentially the "run" method - created from nsHostResolver
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");

  if ((mType != TRRTYPE_A) && (mType != TRRTYPE_AAAA) && (mType != TRRTYPE_NS)) {
    // limit the calling interface because nsHostResolver has explicit slots for
    // these types
    return NS_ERROR_FAILURE;
  }

  if ((mType == TRRTYPE_A) || (mType == TRRTYPE_AAAA)) {
    // let NS resolves skip the blacklist check
    if (gTRRService->IsTRRBlacklisted(mHost, mPB, true)) {
      if (mType == TRRTYPE_A) {
        // count only blacklist for A records to avoid double counts
        Telemetry::Accumulate(Telemetry::DNS_TRR_BLACKLISTED, true);
      }
      // not really an error but no TRR is issued
      return NS_ERROR_UNKNOWN_HOST;
    } else {
      if (mType == TRRTYPE_A) {
        Telemetry::Accumulate(Telemetry::DNS_TRR_BLACKLISTED, false);
      }
    }
  }

  nsresult rv;
  nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
  NS_ENSURE_SUCCESS(rv, rv);

  bool useGet = gTRRService->UseGET();
  nsAutoCString body;
  nsCOMPtr<nsIURI> dnsURI;

  LOG(("TRR::SendHTTPRequest resolve %s type %u\n", mHost.get(), mType));

  if (useGet) {
    nsAutoCString tmp;
    rv = DohEncode(tmp);
    NS_ENSURE_SUCCESS(rv, rv);

    /* For GET requests, the outgoing packet needs to be Base64url-encoded and
       then appended to the end of the URI. */
    rv = Base64URLEncode(tmp.Length(), reinterpret_cast<const unsigned char *>(tmp.get()),
                         Base64URLEncodePaddingPolicy::Omit, body);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString uri;
    gTRRService->GetURI(uri);
    uri.Append(NS_LITERAL_CSTRING("?ct&dns="));
    uri.Append(body);
    rv = NS_NewURI(getter_AddRefs(dnsURI), uri);
  } else {
    rv = DohEncode(body);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString uri;
    gTRRService->GetURI(uri);
    rv = NS_NewURI(getter_AddRefs(dnsURI), uri);
  }
  if (NS_FAILED(rv)) {
    LOG(("TRR:SendHTTPRequest: NewURI failed!\n"));
    return rv;
  }

  rv = NS_NewChannel(getter_AddRefs(mChannel),
                     dnsURI,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER,
                     nullptr,   // PerformanceStorage
                     nullptr, // aLoadGroup
                     this,
                     nsIRequest::LOAD_ANONYMOUS |
                     (mPB ? nsIRequest::INHIBIT_CACHING: 0), ios);
  if (NS_FAILED(rv)) {
    LOG(("TRR:SendHTTPRequest: NewChannel failed!\n"));
    return rv;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  if (!httpChannel) {
    return NS_ERROR_UNEXPECTED;
  }

  rv = httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                     NS_LITERAL_CSTRING("application/dns-udpwireformat"),
                                     false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString cred;
  gTRRService->GetCredentials(cred);
  if (!cred.IsEmpty()){
    rv = httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Authorization"), cred, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIHttpChannelInternal> internalChannel = do_QueryInterface(mChannel);
  if (!internalChannel) {
    return NS_ERROR_UNEXPECTED;
  }

  // setting a small stream window means the h2 stack won't pipeline a window update
  // with each HEADERS or reply to a DATA with a WINDOW UPDATE
  rv = internalChannel->SetInitialRwin(127 * 1024);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = internalChannel->SetTrr(true);
  NS_ENSURE_SUCCESS(rv, rv);

  mAllowRFC1918 = gTRRService->AllowRFC1918();

  if (useGet) {
    rv = httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("GET"));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Cache-Control"),
                                       NS_LITERAL_CSTRING("no-store"), false);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(httpChannel);
    if (!uploadChannel) {
      return NS_ERROR_UNEXPECTED;
    }
    uint32_t streamLength = body.Length();
    nsCOMPtr<nsIInputStream> uploadStream;
    rv = NS_NewCStringInputStream(getter_AddRefs(uploadStream), std::move(body));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = uploadChannel->ExplicitSetUploadStream(uploadStream,
                                                NS_LITERAL_CSTRING("application/dns-udpwireformat"),
                                                streamLength,
                                                NS_LITERAL_CSTRING("POST"), false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // set the *default* response content type
  if (NS_FAILED(httpChannel->SetContentType(NS_LITERAL_CSTRING("application/dns-udpwireformat")))) {
    LOG(("TRR::SendHTTPRequest: couldn't set content-type!\n"));
  }
  if (NS_SUCCEEDED(httpChannel->AsyncOpen2(this))) {
    NS_NewTimerWithCallback(getter_AddRefs(mTimeout),
                            this, gTRRService->GetRequestTimeout(),
                            nsITimer::TYPE_ONE_SHOT);
    return NS_OK;
  }
  mChannel = nullptr;
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
TRR::GetInterface(const nsIID &iid, void **result)
{
  if (!iid.Equals(NS_GET_IID(nsIHttpPushListener))) {
    return NS_ERROR_NO_INTERFACE;
  }

  nsCOMPtr<nsIHttpPushListener> copy(this);
  *result = copy.forget().take();
  return NS_OK;
}

nsresult
TRR::DohDecodeQuery(const nsCString &query, nsCString &host, enum TrrType &type)
{
  FallibleTArray<uint8_t> binary;
  bool found_dns = false;
  LOG(("TRR::DohDecodeQuery %s!\n", query.get()));

  // extract "dns=" from the query string
  nsCCharSeparatedTokenizer tokenizer(query, '&');
  nsAutoCString data;
  while (tokenizer.hasMoreTokens()) {
    const nsACString& token = tokenizer.nextToken();
    nsDependentCSubstring dns = Substring(token, 0, 4);
    nsAutoCString check(dns);
    if (check.Equals("dns=")) {
      nsDependentCSubstring q = Substring(token, 4, -1);
      data = q;
      found_dns = true;
      break;
    }
  }
  if (!found_dns) {
    LOG(("TRR::DohDecodeQuery no dns= in pushed URI query string\n"));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsresult rv = Base64URLDecode(data,
                                Base64URLDecodePaddingPolicy::Ignore, binary);
  NS_ENSURE_SUCCESS(rv, rv);
  uint32_t avail = binary.Length();
  if (avail < 12) {
    return NS_ERROR_FAILURE;
  }
  // check the query bit and the opcode
  if ((binary[2] & 0xf8) != 0) {
    return NS_ERROR_FAILURE;
  }
  uint32_t qdcount = (binary[4] << 8) + binary[5];
  if (!qdcount) {
    return NS_ERROR_FAILURE;
  }

  uint32_t index = 12;
  uint32_t length = 0;
  host.Truncate();
  do {
    if (avail < (index + 1)) {
      return NS_ERROR_UNEXPECTED;
    }

    length = binary[index];
    if (length) {
      if (host.Length()) {
        host.Append(".");
      }
      if (avail < (index + 1 + length)) {
        return NS_ERROR_UNEXPECTED;
      }
      host.Append((const char *)(&binary[0]) + index + 1, length);
    }
    index += 1 + length; // skip length byte + label
  } while (length);

  LOG(("TRR::DohDecodeQuery host %s\n", host.get()));

  if (avail < (index + 2)) {
    return NS_ERROR_UNEXPECTED;
  }
  uint16_t i16 = 0;
  i16 += binary[index] << 8;
  i16 += binary[index + 1];
  index += 4; // skip question's type, class
  type = (enum TrrType)i16;

  LOG(("TRR::DohDecodeQuery type %d\n", (int)type));

  return NS_OK;
}

nsresult
TRR::ReceivePush(nsIHttpChannel *pushed, nsHostRecord *pushedRec)
{
  if (!mHostResolver) {
    return NS_ERROR_UNEXPECTED;
  }

  LOG(("TRR::ReceivePush: PUSH incoming!\n"));

  nsCOMPtr<nsIURI> uri;
  pushed->GetURI(getter_AddRefs(uri));
  nsAutoCString query;
  if (uri) {
    uri->GetQuery(query);
  }

  PRNetAddr tempAddr;
  if (NS_FAILED(DohDecodeQuery(query, mHost, mType)) ||
      (PR_StringToNetAddr(mHost.get(), &tempAddr) == PR_SUCCESS)) { // literal
    LOG(("TRR::ReceivePush failed to decode %s\n", mHost.get()));
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<nsHostRecord> hostRecord;
  nsresult rv;
  rv = mHostResolver->GetHostRecord(mHost,
                                    pushedRec->flags, pushedRec->af,
                                    pushedRec->pb,
                                    pushedRec->originSuffix,
                                    getter_AddRefs(hostRecord));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = mHostResolver->TrrLookup_unlocked(hostRecord, this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = pushed->AsyncOpen2(this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // OK!
  mChannel = pushed;
  mRec.swap(hostRecord);

  return NS_OK;
}

NS_IMETHODIMP
TRR::OnPush(nsIHttpChannel *associated, nsIHttpChannel *pushed)
{
  LOG(("TRR::OnPush entry\n"));
  MOZ_ASSERT(associated == mChannel);
  if (!mRec) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<TRR> trr = new TRR(mHostResolver, mPB);
  return trr->ReceivePush(pushed, mRec);
}

NS_IMETHODIMP
TRR::OnStartRequest(nsIRequest *aRequest,
                    nsISupports *aContext)
{
  LOG(("TRR::OnStartRequest %p %s %d\n", this, mHost.get(), mType));
  mStartTime = TimeStamp::Now();
  return NS_OK;
}

static uint16_t get16bit(unsigned char *aData, int index)
{
  return ((aData[index] << 8) | aData[index + 1]);
}

static uint32_t get32bit(unsigned char *aData, int index)
{
  return (aData[index] << 24) | (aData[index+1] << 16) |
    (aData[index+2] << 8) | aData[index+3];
}

nsresult
TRR::PassQName(unsigned int &index)
{
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

nsresult
TRR::GetQname(nsAutoCString &aQname, unsigned int &aIndex)
{
  uint8_t clength = 0;
  unsigned int cindex = aIndex;
  unsigned int loop = 128; // a valid DNS name can never loop this much
  unsigned int endindex = 0; // index position after this data
  do {
    if (cindex >= mBodySize) {
      LOG(("TRR: bad cname packet\n"));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    clength = static_cast<uint8_t>(mResponse[cindex]);
    if ((clength & 0xc0) == 0xc0) {
      // name pointer, get the new offset (14 bits)
      if ((cindex +1) >= mBodySize) {
        return NS_ERROR_ILLEGAL_VALUE;
      }
      // extract the new index position for the next label
      uint16_t newpos = (clength & 0x3f) << 8 | mResponse[cindex+1];
      if (!endindex) {
        // only update on the first "jump"
        endindex = cindex + 2;
      }
      cindex = newpos;
      continue;
    } else if (clength & 0xc0) {
      // any of those bits set individually is an error
      LOG(("TRR: bad cname packet\n"));
      return NS_ERROR_ILLEGAL_VALUE;
    } else {
      cindex++;
    }
    if (clength) {
      if (!aQname.IsEmpty()) {
        aQname.Append(".");
      }
      if ((cindex + clength) > mBodySize) {
        return NS_ERROR_ILLEGAL_VALUE;
      }
      aQname.Append((const char *)(&mResponse[cindex]), clength);
      cindex += clength; // skip label
    }
  } while (clength && --loop);

  if (!loop) {
    LOG(("TRR::DohDecode pointer loop error\n"));
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (!endindex) {
    // there was no "jump"
    endindex = cindex;
  }
  aIndex = endindex;
  return NS_OK;
}

//
// DohDecode() collects the TTL and the IP addresses in the response
//
nsresult
TRR::DohDecode(nsCString &aHost)
{
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

  LOG(("doh decode %s %d bytes\n", aHost.get(), mBodySize));

  mCname.Truncate();

  if (mBodySize < 12 || mResponse[0] || mResponse[1]) {
    LOG(("TRR bad incoming DOH, eject!\n"));
    return NS_ERROR_ILLEGAL_VALUE;
  }
  uint8_t rcode = mResponse[3] & 0x0F;
  if (rcode) {
    LOG(("TRR Decode %s RCODE %d\n", aHost.get(), rcode));
    return NS_ERROR_FAILURE;
  }

  uint16_t questionRecords = get16bit(mResponse, 4); // qdcount
  // iterate over the single(?) host name in question
  while (questionRecords) {
    do {
      if (mBodySize < (index + 1)) {
        return NS_ERROR_ILLEGAL_VALUE;
      }
      length = static_cast<uint8_t>(mResponse[index]);
      if (length) {
        if (host.Length()) {
          host.Append(".");
        }
        if (mBodySize < (index + 1 + length)) {
          return NS_ERROR_ILLEGAL_VALUE;
        }
        host.Append(((char *)mResponse) + index + 1, length);
      }
      index += 1 + length; // skip length byte + label
    } while (length);
    if (mBodySize < (index + 4)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    index += 4; // skip question's type, class
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

    if ((TYPE != TRRTYPE_CNAME) &&
        (TYPE != static_cast<uint16_t>(mType))) {
      // Not the same type as was asked for nor CNAME
      LOG(("TRR: Dohdecode:%d asked for type %d got %d\n", __LINE__,
           mType, TYPE));
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

    if (qname.Equals(aHost)) {

      // RDATA
      // - A (TYPE 1):  4 bytes
      // - AAAA (TYPE 28): 16 bytes
      // - NS (TYPE 2): N bytes

      switch(TYPE) {
      case TRRTYPE_A:
        if (RDLENGTH != 4) {
          LOG(("TRR bad length for A (%u)\n", RDLENGTH));
          return NS_ERROR_UNEXPECTED;
        }
        rv = mDNS.Add(TTL, mResponse, index, RDLENGTH,
                      mAllowRFC1918);
        if (NS_FAILED(rv)) {
          LOG(("TRR:DohDecode failed: local IP addresses or unknown IP family\n"));
          return rv;
        }
        break;
      case TRRTYPE_AAAA:
        if (RDLENGTH != 16) {
          LOG(("TRR bad length for AAAA (%u)\n", RDLENGTH));
          return NS_ERROR_UNEXPECTED;
        }
        rv = mDNS.Add(TTL, mResponse, index, RDLENGTH,
                      mAllowRFC1918);
        if (NS_FAILED(rv)) {
          LOG(("TRR got unique/local IPv6 address!\n"));
          return rv;
        }
        break;

      case TRRTYPE_NS:
        break;
      case TRRTYPE_CNAME:
        if (mCname.IsEmpty()) {
          nsAutoCString qname;
          unsigned int qnameindex = index;
          rv = GetQname(qname, qnameindex);
          if (NS_FAILED(rv)) {
            return rv;
          }
          if(!qname.IsEmpty()) {
            mCname = qname;
            LOG(("TRR::DohDecode CNAME host %s => %s\n",
                 host.get(), mCname.get()));
          } else {
            LOG(("TRR::DohDecode empty CNAME for host %s!\n",
                 host.get()));
          }
        }
        else {
          LOG(("TRR::DohDecode CNAME - ignoring another entry\n"));
        }
        break;
      default:
        // skip unknown record types
        LOG(("TRR unsupported TYPE (%u) RDLENGTH %u\n", TYPE, RDLENGTH));
        break;
      }
    }
    else {
      LOG(("TRR asked for %s data but got %s\n", aHost.get(), qname.get()));
    }

    index += RDLENGTH;
    LOG(("done with record type %u len %u index now %u of %u\n",
         TYPE, RDLENGTH, index, mBodySize));
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
    index += 2; // type
    index += 2; // class
    index += 4; // ttl

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
    rv = PassQName(index);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (mBodySize < (index + 8)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    index += 2; // type
    index += 2; // class
    index += 4; // ttl

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
    LOG(("done with additional rr now %u of %u\n", index, mBodySize));
    arRecords--;
  }

  if (index != mBodySize) {
    LOG(("DohDecode failed to parse entire response body, %u out of %u bytes\n",
         index, mBodySize));
    // failed to parse 100%, do not continue
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if ((mType != TRRTYPE_NS) && mCname.IsEmpty() &&
      !mDNS.mAddresses.getFirst()) {
    // no entries were stored!
    LOG(("TRR: No entries were stored!\n"));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
TRR::ReturnData()
{
  // create and populate an AddrInfo instance to pass on
  nsAutoPtr<AddrInfo> ai(new AddrInfo(mHost, mType));
  DOHaddr *item;
  uint32_t ttl = AddrInfo::NO_TTL_DATA;
  while ((item = static_cast<DOHaddr*>(mDNS.mAddresses.popFirst()))) {
    PRNetAddr prAddr;
    NetAddrToPRNetAddr(&item->mNet, &prAddr);
    auto *addrElement = new NetAddrElement(&prAddr);
    ai->AddAddress(addrElement);
    if (item->mTtl < ttl) {
      // While the DNS packet might return individual TTLs for each address,
      // we can only return one value in the AddrInfo class so pick the
      // lowest number.
      ttl = item->mTtl;
    }
  }
  ai->ttl = ttl;
  if (!mHostResolver) {
    return NS_ERROR_FAILURE;
  }
  (void)mHostResolver->CompleteLookup(mRec, NS_OK, ai.forget(), mPB);
  mHostResolver = nullptr;
  mRec = nullptr;
  return NS_OK;
}

nsresult
TRR::FailData(nsresult error)
{
  if (!mHostResolver) {
    return NS_ERROR_FAILURE;
  }
  // create and populate an TRR AddrInfo instance to pass on to signal that
  // this comes from TRR
  AddrInfo *ai = new AddrInfo(mHost, mType);

  (void)mHostResolver->CompleteLookup(mRec, error, ai, mPB);
  mHostResolver = nullptr;
  mRec = nullptr;
  return NS_OK;
}

nsresult
TRR::On200Response()
{
  // decode body and create an AddrInfo struct for the response
  nsresult rv = DohDecode(mHost);

  if (NS_SUCCEEDED(rv)) {
    if (!mDNS.mAddresses.getFirst() && !mCname.IsEmpty()) {
      nsCString cname = mCname;
      LOG(("TRR: check for CNAME record for %s within previous response\n",
           cname.get()));
      rv = DohDecode(cname);
      if (NS_SUCCEEDED(rv) && mDNS.mAddresses.getFirst()) {
        LOG(("TRR: Got the CNAME record without asking for it\n"));
        ReturnData();
        return NS_OK;
      }
      // restore mCname as DohDecode() change it
      mCname = cname;
      if (!--mCnameLoop) {
        LOG(("TRR::On200Response CNAME loop, eject!\n"));
      } else  {
        LOG(("TRR::On200Response CNAME %s => %s (%u)\n", mHost.get(), mCname.get(),
             mCnameLoop));
        RefPtr<TRR> trr = new TRR(mHostResolver, mRec, mCname,
                                  mType, mCnameLoop, mPB);
        rv = NS_DispatchToMainThread(trr);
        if (NS_SUCCEEDED(rv)) {
          return rv;
        }
      }
    } else {
      // pass back the response data
      ReturnData();
      return NS_OK;
    }
  } else {
    LOG(("TRR::On200Response DohDecode %x\n", (int)rv));
  }
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
TRR::OnStopRequest(nsIRequest *aRequest,
                   nsISupports *aContext,
                   nsresult aStatusCode)
{
  // The dtor will be run after the function returns
  LOG(("TRR:OnStopRequest %p %s %d failed=%d code=%X\n",
       this, mHost.get(), mType, mFailed, (unsigned int)aStatusCode));
  nsCOMPtr<nsIChannel> channel;
  channel.swap(mChannel);

  // if status was "fine", parse the response and pass on the answer
  if (!mFailed && NS_SUCCEEDED(aStatusCode)) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
    if (!httpChannel) {
      return NS_ERROR_UNEXPECTED;
    }
    nsresult rv = NS_OK;
    nsAutoCString contentType;
    httpChannel->GetContentType(contentType);
    if (contentType.Length() &&
        !contentType.LowerCaseEqualsLiteral("application/dns-udpwireformat")) {
      // try and parse missing content-types, but otherwise require udpwireformat
      LOG(("TRR:OnStopRequest %p %s %d should fail due to content type %s\n",
           this, mHost.get(), mType, contentType.get()));
      FailData(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

    uint32_t httpStatus;
    rv = httpChannel->GetResponseStatus(&httpStatus);
    if (NS_SUCCEEDED(rv) && httpStatus == 200) {
      rv = On200Response();
      if (NS_SUCCEEDED(rv)) {
        return rv;
      }
    } else {
      LOG(("TRR:OnStopRequest:%d %p rv %x httpStatus %d\n", __LINE__,
           this, (int)rv, httpStatus));
    }
  }

  LOG(("TRR:OnStopRequest %p status %x mFailed %d\n",
       this, (int)aStatusCode, mFailed));
  FailData(NS_ERROR_UNKNOWN_HOST);
  return NS_OK;
}

NS_IMETHODIMP
TRR::OnDataAvailable(nsIRequest *aRequest,
                     nsISupports *aContext,
                     nsIInputStream *aInputStream,
                     uint64_t aOffset,
                     const uint32_t aCount)
{
  LOG(("TRR:OnDataAvailable %p %s %d failed=%d aCount=%u\n",
       this, mHost.get(), mType, mFailed, (unsigned int)aCount));
  // receive DNS response into the local buffer
  if (mFailed) {
    return NS_ERROR_FAILURE;
  }

  if (aCount + mBodySize > kMaxSize) {
    LOG(("TRR::OnDataAvailable:%d fail\n",  __LINE__));
    mFailed = true;
    return NS_ERROR_FAILURE;
  }

  uint32_t count;
  nsresult rv = aInputStream->Read((char *)mResponse + mBodySize, aCount, &count);
  if (NS_FAILED(rv)) {
    LOG(("TRR::OnDataAvailable:%d fail\n",  __LINE__));
    mFailed = true;
    return rv;
  }
  MOZ_ASSERT(count == aCount);
  mBodySize += aCount;
  return NS_OK;
}

nsresult
DOHresp::Add(uint32_t TTL, unsigned char *dns, int index, uint16_t len,
             bool aLocalAllowed)
{
  nsAutoPtr<DOHaddr> doh(new DOHaddr);
  NetAddr *addr = &doh->mNet;
  if (4 == len) {
    // IPv4
    addr->inet.family = AF_INET;
    addr->inet.port = 0; // unknown
    addr->inet.ip = ntohl(get32bit(dns, index));
  } else if (16 == len) {
    // IPv6
    addr->inet6.family = AF_INET6;
    addr->inet6.port = 0;     // unknown
    addr->inet6.flowinfo = 0; // unknown
    addr->inet6.scope_id = 0; // unknown
    for(int i = 0; i < 16; i++, index++) {
      addr->inet6.ip.u8[i] = dns[index];
    }
  } else {
    return NS_ERROR_UNEXPECTED;
  }

  if (IsIPAddrLocal(addr) && !aLocalAllowed) {
    return NS_ERROR_FAILURE;
  }
  doh->mTtl = TTL;

  if (LOG_ENABLED()) {
    char buf[128];
    NetAddrToString(addr, buf, sizeof(buf));
    LOG(("DOHresp:Add %s\n", buf));
  }
  mAddresses.insertBack(doh.forget());
  return NS_OK;
}

class ProxyCancel : public Runnable
{
public:
  explicit ProxyCancel(TRR *aTRR)
    : Runnable("proxyTrrCancel")
    , mTRR(aTRR)
  { }

  NS_IMETHOD Run() override
  {
    mTRR->Cancel();
    mTRR = nullptr;
    return NS_OK;
  }

private:
  RefPtr<TRR> mTRR;
};

void
TRR::Cancel()
{
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(new ProxyCancel(this));
    return;
  }
  if (mChannel) {
    LOG(("TRR: %p canceling Channel %p %s %d\n", this,
         mChannel.get(), mHost.get(), mType));
    mChannel->Cancel(NS_ERROR_ABORT);
  }
}

#undef LOG

// namespace
}
}
