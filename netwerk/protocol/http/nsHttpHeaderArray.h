/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpHeaderArray_h__
#define nsHttpHeaderArray_h__

#include "nsHttp.h"
#include "nsTArray.h"
#include "nsString.h"

class nsIHttpHeaderVisitor;

// This needs to be forward declared here so we can include only this header
// without also including PHttpChannelParams.h
namespace IPC {
template <typename>
struct ParamTraits;
}  // namespace IPC

namespace mozilla {
namespace net {

class nsHttpHeaderArray {
 public:
  const char* PeekHeader(const nsHttpAtom& header) const;

  // For nsHttpResponseHead nsHttpHeaderArray will keep track of the original
  // headers as they come from the network and the parse headers used in
  // firefox.
  // If the original and the firefox header are the same, we will keep just
  // one copy and marked it as eVarietyResponseNetOriginalAndResponse.
  // If firefox header representation changes a header coming from the
  // network (e.g. merged it) or a eVarietyResponseNetOriginalAndResponse
  // header has been changed by SetHeader method, we will keep the original
  // header as eVarietyResponseNetOriginal and make a copy for the new header
  // and mark it as eVarietyResponse.
  enum HeaderVariety {
    eVarietyUnknown,
    // Used only for request header.
    eVarietyRequestOverride,
    eVarietyRequestDefault,
    // Used only for response header.
    eVarietyResponseNetOriginalAndResponse,
    eVarietyResponseNetOriginal,
    eVarietyResponse
  };

  // Used by internal setters: to set header from network use SetHeaderFromNet
  [[nodiscard]] nsresult SetHeader(const nsACString& headerName,
                                   const nsACString& value, bool merge,
                                   HeaderVariety variety);
  [[nodiscard]] nsresult SetHeader(const nsHttpAtom& header,
                                   const nsACString& value, bool merge,
                                   HeaderVariety variety);
  [[nodiscard]] nsresult SetHeader(const nsHttpAtom& header,
                                   const nsACString& headerName,
                                   const nsACString& value, bool merge,
                                   HeaderVariety variety);

  // Used by internal setters to set an empty header
  [[nodiscard]] nsresult SetEmptyHeader(const nsACString& headerName,
                                        HeaderVariety variety);

  // Merges supported headers. For other duplicate values, determines if error
  // needs to be thrown or 1st value kept.
  // For the response header we keep the original headers as well.
  [[nodiscard]] nsresult SetHeaderFromNet(const nsHttpAtom& header,
                                          const nsACString& headerNameOriginal,
                                          const nsACString& value,
                                          bool response);

  [[nodiscard]] nsresult SetResponseHeaderFromCache(
      const nsHttpAtom& header, const nsACString& headerNameOriginal,
      const nsACString& value, HeaderVariety variety);

  [[nodiscard]] nsresult GetHeader(const nsHttpAtom& header,
                                   nsACString& result) const;
  [[nodiscard]] nsresult GetOriginalHeader(const nsHttpAtom& aHeader,
                                           nsIHttpHeaderVisitor* aVisitor);
  void ClearHeader(const nsHttpAtom& h);

  // Find the location of the given header value, or null if none exists.
  const char* FindHeaderValue(const nsHttpAtom& header,
                              const char* value) const {
    return nsHttp::FindToken(PeekHeader(header), value, HTTP_HEADER_VALUE_SEPS);
  }

  // Determine if the given header value exists.
  bool HasHeaderValue(const nsHttpAtom& header, const char* value) const {
    return FindHeaderValue(header, value) != nullptr;
  }

  bool HasHeader(const nsHttpAtom& header) const;

  enum VisitorFilter {
    eFilterAll,
    eFilterSkipDefault,
    eFilterResponse,
    eFilterResponseOriginal
  };

  [[nodiscard]] nsresult VisitHeaders(nsIHttpHeaderVisitor* visitor,
                                      VisitorFilter filter = eFilterAll);

  // parse a header line, return the header atom, the header name, and the
  // header value
  [[nodiscard]] static nsresult ParseHeaderLine(
      const nsACString& line, nsHttpAtom* hdr = nullptr,
      nsACString* headerNameOriginal = nullptr, nsACString* value = nullptr);

  void Flatten(nsACString&, bool pruneProxyHeaders, bool pruneTransients);
  void FlattenOriginalHeader(nsACString&);

  uint32_t Count() const { return mHeaders.Length(); }

  const char* PeekHeaderAt(uint32_t i, nsHttpAtom& header,
                           nsACString& headerNameOriginal) const;

  void Clear();

  // Must be copy-constructable and assignable
  struct nsEntry {
    nsHttpAtom header;
    nsCString headerNameOriginal;
    nsCString value;
    HeaderVariety variety = eVarietyUnknown;

    struct MatchHeader {
      bool Equals(const nsEntry& aEntry, const nsHttpAtom& aHeader) const {
        return aEntry.header == aHeader;
      }
    };

    bool operator==(const nsEntry& aOther) const {
      return header == aOther.header && value == aOther.value;
    }
  };

  bool operator==(const nsHttpHeaderArray& aOther) const {
    return mHeaders == aOther.mHeaders;
  }

 private:
  // LookupEntry function will never return eVarietyResponseNetOriginal.
  // It will ignore original headers from the network.
  int32_t LookupEntry(const nsHttpAtom& header, const nsEntry**) const;
  int32_t LookupEntry(const nsHttpAtom& header, nsEntry**);
  [[nodiscard]] nsresult MergeHeader(const nsHttpAtom& header, nsEntry* entry,
                                     const nsACString& value,
                                     HeaderVariety variety);
  [[nodiscard]] nsresult SetHeader_internal(const nsHttpAtom& header,
                                            const nsACString& headerName,
                                            const nsACString& value,
                                            HeaderVariety variety);

  // Header cannot be merged: only one value possible
  bool IsSingletonHeader(const nsHttpAtom& header);
  // Header cannot be merged, and subsequent values should be ignored
  bool IsIgnoreMultipleHeader(const nsHttpAtom& header);

  // Subset of singleton headers: should never see multiple, different
  // instances of these, else something fishy may be going on (like CLRF
  // injection)
  bool IsSuspectDuplicateHeader(const nsHttpAtom& header);

  // Removes duplicate header values entries
  // Will return unmodified header value if the header values contains
  // non-duplicate entries
  void RemoveDuplicateHeaderValues(const nsACString& aHeaderValue,
                                   nsACString& aResult);

  // All members must be copy-constructable and assignable
  CopyableTArray<nsEntry> mHeaders;

  friend struct IPC::ParamTraits<nsHttpHeaderArray>;
  friend class nsHttpRequestHead;
};

//-----------------------------------------------------------------------------
// nsHttpHeaderArray <private>: inline functions
//-----------------------------------------------------------------------------

inline int32_t nsHttpHeaderArray::LookupEntry(const nsHttpAtom& header,
                                              const nsEntry** entry) const {
  uint32_t index = 0;
  while (index != UINT32_MAX) {
    index = mHeaders.IndexOf(header, index, nsEntry::MatchHeader());
    if (index != UINT32_MAX) {
      if ((&mHeaders[index])->variety != eVarietyResponseNetOriginal) {
        *entry = &mHeaders[index];
        return index;
      }
      index++;
    }
  }

  return index;
}

inline int32_t nsHttpHeaderArray::LookupEntry(const nsHttpAtom& header,
                                              nsEntry** entry) {
  uint32_t index = 0;
  while (index != UINT32_MAX) {
    index = mHeaders.IndexOf(header, index, nsEntry::MatchHeader());
    if (index != UINT32_MAX) {
      if ((&mHeaders[index])->variety != eVarietyResponseNetOriginal) {
        *entry = &mHeaders[index];
        return index;
      }
      index++;
    }
  }
  return index;
}

inline bool nsHttpHeaderArray::IsSingletonHeader(const nsHttpAtom& header) {
  return header == nsHttp::Content_Type ||
         header == nsHttp::Content_Disposition ||
         header == nsHttp::Content_Length || header == nsHttp::User_Agent ||
         header == nsHttp::Referer || header == nsHttp::Host ||
         header == nsHttp::Authorization ||
         header == nsHttp::Proxy_Authorization ||
         header == nsHttp::If_Modified_Since ||
         header == nsHttp::If_Unmodified_Since || header == nsHttp::From ||
         header == nsHttp::Location || header == nsHttp::Max_Forwards ||
         header == nsHttp::GlobalPrivacyControl ||
         // Ignore-multiple-headers are singletons in the sense that they
         // shouldn't be merged.
         IsIgnoreMultipleHeader(header);
}

// These are headers for which, in the presence of multiple values, we only
// consider the first.
inline bool nsHttpHeaderArray::IsIgnoreMultipleHeader(
    const nsHttpAtom& header) {
  // https://tools.ietf.org/html/rfc6797#section-8:
  //
  //     If a UA receives more than one STS header field in an HTTP
  //     response message over secure transport, then the UA MUST process
  //     only the first such header field.
  return header == nsHttp::Strict_Transport_Security;
}

[[nodiscard]] inline nsresult nsHttpHeaderArray::MergeHeader(
    const nsHttpAtom& header, nsEntry* entry, const nsACString& value,
    nsHttpHeaderArray::HeaderVariety variety) {
  // merge of empty header = no-op
  if (value.IsEmpty() && header != nsHttp::X_Frame_Options) {
    return NS_OK;
  }

  // x-frame-options having an empty header value still has an effect so we make
  // sure that we retain encountering it
  nsCString newValue = entry->value;
  if (!newValue.IsEmpty() || header == nsHttp::X_Frame_Options) {
    // Append the new value to the existing value
    if (header == nsHttp::Set_Cookie || header == nsHttp::WWW_Authenticate ||
        header == nsHttp::Proxy_Authenticate) {
      // Special case these headers and use a newline delimiter to
      // delimit the values from one another as commas may appear
      // in the values of these headers contrary to what the spec says.
      newValue.Append('\n');
    } else {
      // Delimit each value from the others using a comma (per HTTP spec)
      newValue.AppendLiteral(", ");
    }
  }

  newValue.Append(value);
  if (entry->variety == eVarietyResponseNetOriginalAndResponse) {
    MOZ_ASSERT(variety == eVarietyResponse);
    entry->variety = eVarietyResponseNetOriginal;
    // Copy entry->headerNameOriginal because in SetHeader_internal we are going
    // to a new one and a realocation can happen.
    nsCString headerNameOriginal = entry->headerNameOriginal;
    nsresult rv = SetHeader_internal(header, headerNameOriginal, newValue,
                                     eVarietyResponse);
    if (NS_FAILED(rv)) {
      return rv;
    }
  } else {
    entry->value = newValue;
    entry->variety = variety;
  }
  return NS_OK;
}

inline bool nsHttpHeaderArray::IsSuspectDuplicateHeader(
    const nsHttpAtom& header) {
  bool retval = header == nsHttp::Content_Length ||
                header == nsHttp::Content_Disposition ||
                header == nsHttp::Location;

  MOZ_ASSERT(!retval || IsSingletonHeader(header),
             "Only non-mergeable headers should be in this list\n");

  return retval;
}

inline void nsHttpHeaderArray::RemoveDuplicateHeaderValues(
    const nsACString& aHeaderValue, nsACString& aResult) {
  mozilla::Maybe<nsAutoCString> result;
  for (const nsACString& token :
       nsCCharSeparatedTokenizer(aHeaderValue, ',').ToRange()) {
    if (result.isNothing()) {
      // assign the first value
      result.emplace(token);
      continue;
    }
    if (*result != token) {
      // non-identical header values. Do not change the header values
      result.reset();
      break;
    }
  }

  if (result.isSome()) {
    aResult = *result;
  } else {
    // either header values do not have multiple values or
    // has unequal multiple values
    // for both the cases restore the original header value
    aResult = aHeaderValue;
  }
}

}  // namespace net
}  // namespace mozilla

#endif
