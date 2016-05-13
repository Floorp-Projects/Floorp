/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
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
    template <typename> struct ParamTraits;
} // namespace IPC

namespace mozilla { namespace net {

class nsHttpHeaderArray
{
public:
    const char *PeekHeader(nsHttpAtom header) const;

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
    enum HeaderVariety
    {
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
    nsresult SetHeader(nsHttpAtom header, const nsACString &value,
                       bool merge, HeaderVariety variety);

    // Used by internal setters to set an empty header
    nsresult SetEmptyHeader(nsHttpAtom header, HeaderVariety variety);

    // Merges supported headers. For other duplicate values, determines if error
    // needs to be thrown or 1st value kept.
    // For the response header we keep the original headers as well.
    nsresult SetHeaderFromNet(nsHttpAtom header, const nsACString &value,
                              bool response);

    nsresult GetHeader(nsHttpAtom header, nsACString &value) const;
    nsresult GetOriginalHeader(nsHttpAtom aHeader,
                               nsIHttpHeaderVisitor *aVisitor);
    void     ClearHeader(nsHttpAtom h);

    // Find the location of the given header value, or null if none exists.
    const char *FindHeaderValue(nsHttpAtom header, const char *value) const
    {
        return nsHttp::FindToken(PeekHeader(header), value,
                                 HTTP_HEADER_VALUE_SEPS);
    }

    // Determine if the given header value exists.
    bool HasHeaderValue(nsHttpAtom header, const char *value) const
    {
        return FindHeaderValue(header, value) != nullptr;
    }

    bool HasHeader(nsHttpAtom header) const;

    enum VisitorFilter
    {
        eFilterAll,
        eFilterSkipDefault,
        eFilterResponse,
        eFilterResponseOriginal
    };

    nsresult VisitHeaders(nsIHttpHeaderVisitor *visitor, VisitorFilter filter = eFilterAll);

    // parse a header line, return the header atom and a pointer to the
    // header value (the substring of the header line -- do not free).
    static nsresult ParseHeaderLine(const char *line,
                                    nsHttpAtom *header=nullptr,
                                    char **value=nullptr);

    void Flatten(nsACString &, bool pruneProxyHeaders, bool pruneTransients);
    void FlattenOriginalHeader(nsACString &);

    uint32_t Count() const { return mHeaders.Length(); }

    const char *PeekHeaderAt(uint32_t i, nsHttpAtom &header) const;

    void Clear();

    // Must be copy-constructable and assignable
    struct nsEntry
    {
        nsHttpAtom header;
        nsCString value;
        HeaderVariety variety = eVarietyUnknown;

        struct MatchHeader {
          bool Equals(const nsEntry &entry, const nsHttpAtom &header) const {
            return entry.header == header;
          }
        };

        bool operator==(const nsEntry& aOther) const
        {
            return header == aOther.header && value == aOther.value;
        }
    };

    bool operator==(const nsHttpHeaderArray& aOther) const
    {
        return mHeaders == aOther.mHeaders;
    }

private:
    // LookupEntry function will never return eVarietyResponseNetOriginal.
    // It will ignore original headers from the network.
    int32_t LookupEntry(nsHttpAtom header, const nsEntry **) const;
    int32_t LookupEntry(nsHttpAtom header, nsEntry **);
    nsresult MergeHeader(nsHttpAtom header, nsEntry *entry,
                         const nsACString &value, HeaderVariety variety);
    nsresult SetHeader_internal(nsHttpAtom header, const nsACString &value,
                                HeaderVariety variety);

    // Header cannot be merged: only one value possible
    bool    IsSingletonHeader(nsHttpAtom header);
    // For some headers we want to track empty values to prevent them being
    // combined with non-empty ones as a CRLF attack vector
    bool    TrackEmptyHeader(nsHttpAtom header);

    // Subset of singleton headers: should never see multiple, different
    // instances of these, else something fishy may be going on (like CLRF
    // injection)
    bool    IsSuspectDuplicateHeader(nsHttpAtom header);

    // All members must be copy-constructable and assignable
    nsTArray<nsEntry> mHeaders;

    friend struct IPC::ParamTraits<nsHttpHeaderArray>;
};


//-----------------------------------------------------------------------------
// nsHttpHeaderArray <private>: inline functions
//-----------------------------------------------------------------------------

inline int32_t
nsHttpHeaderArray::LookupEntry(nsHttpAtom header, const nsEntry **entry) const
{
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

inline int32_t
nsHttpHeaderArray::LookupEntry(nsHttpAtom header, nsEntry **entry)
{
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

inline bool
nsHttpHeaderArray::IsSingletonHeader(nsHttpAtom header)
{
    return header == nsHttp::Content_Type        ||
           header == nsHttp::Content_Disposition ||
           header == nsHttp::Content_Length      ||
           header == nsHttp::User_Agent          ||
           header == nsHttp::Referer             ||
           header == nsHttp::Host                ||
           header == nsHttp::Authorization       ||
           header == nsHttp::Proxy_Authorization ||
           header == nsHttp::If_Modified_Since   ||
           header == nsHttp::If_Unmodified_Since ||
           header == nsHttp::From                ||
           header == nsHttp::Location            ||
           header == nsHttp::Max_Forwards;
}

inline bool
nsHttpHeaderArray::TrackEmptyHeader(nsHttpAtom header)
{
    return header == nsHttp::Content_Length ||
           header == nsHttp::Location;
}

inline nsresult
nsHttpHeaderArray::MergeHeader(nsHttpAtom header,
                               nsEntry *entry,
                               const nsACString &value,
                               nsHttpHeaderArray::HeaderVariety variety)
{
    if (value.IsEmpty())
        return NS_OK;   // merge of empty header = no-op

    nsCString newValue = entry->value;
    if (!newValue.IsEmpty()) {
        // Append the new value to the existing value
        if (header == nsHttp::Set_Cookie ||
            header == nsHttp::WWW_Authenticate ||
            header == nsHttp::Proxy_Authenticate)
        {
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
        nsresult rv = SetHeader_internal(header, newValue, eVarietyResponse);
        if (NS_FAILED(rv)) {
            return rv;
        }
    } else {
        entry->value = newValue;
        entry->variety = variety;
    }
    return NS_OK;
}

inline bool
nsHttpHeaderArray::IsSuspectDuplicateHeader(nsHttpAtom header)
{
    bool retval =  header == nsHttp::Content_Length         ||
                     header == nsHttp::Content_Disposition    ||
                     header == nsHttp::Location;

    MOZ_ASSERT(!retval || IsSingletonHeader(header),
               "Only non-mergeable headers should be in this list\n");

    return retval;
}

} // namespace net
} // namespace mozilla

#endif
