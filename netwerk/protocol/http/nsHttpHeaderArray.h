/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpHeaderArray_h__
#define nsHttpHeaderArray_h__

#include "nsHttp.h"
#include "nsTArray.h"
#include "nsIHttpChannel.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsCOMPtr.h"
#include "nsString.h"

namespace mozilla { namespace net {

// A nsCString that aborts if it fails to successfully copy its input during
// copy construction and/or assignment. This is useful for building classes
// that are safely copy-constructable and safely assignable using the compiler-
// generated copy constructor and assignment operator.
class InfallableCopyCString : public nsCString
{
public:
    InfallableCopyCString() { }
    InfallableCopyCString(const nsACString & other)
        : nsCString(other)
    {
        if (Length() != other.Length())
            NS_RUNTIMEABORT("malloc");
    }

    InfallableCopyCString & operator=(const nsACString & other)
    {
        nsCString::operator=(other);

        if (Length() != other.Length())
            NS_RUNTIMEABORT("malloc");

        return *this;
    }
};

} } // namespace mozilla::net

class nsHttpHeaderArray
{
public:
    const char *PeekHeader(nsHttpAtom header) const;

    // Used by internal setters: to set header from network use SetHeaderFromNet
    nsresult SetHeader(nsHttpAtom header, const nsACString &value,
                       bool merge = false);

    // Merges supported headers. For other duplicate values, determines if error
    // needs to be thrown or 1st value kept.
    nsresult SetHeaderFromNet(nsHttpAtom header, const nsACString &value);

    nsresult GetHeader(nsHttpAtom header, nsACString &value) const;
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

    nsresult VisitHeaders(nsIHttpHeaderVisitor *visitor);

    // parse a header line, return the header atom and a pointer to the
    // header value (the substring of the header line -- do not free).
    nsresult ParseHeaderLine(const char *line,
                             nsHttpAtom *header=nullptr,
                             char **value=nullptr);

    void Flatten(nsACString &, bool pruneProxyHeaders=false);

    uint32_t Count() const { return mHeaders.Length(); }

    const char *PeekHeaderAt(uint32_t i, nsHttpAtom &header) const;

    void Clear();

    // Must be copy-constructable and assignable
    struct nsEntry
    {
        nsHttpAtom header;
        mozilla::net::InfallableCopyCString value;

        struct MatchHeader {
          bool Equals(const nsEntry &entry, const nsHttpAtom &header) const {
            return entry.header == header;
          }
        };
    };

private:
    int32_t LookupEntry(nsHttpAtom header, const nsEntry **) const;
    int32_t LookupEntry(nsHttpAtom header, nsEntry **);
    void MergeHeader(nsHttpAtom header, nsEntry *entry, const nsACString &value);

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
    uint32_t index = mHeaders.IndexOf(header, 0, nsEntry::MatchHeader());
    if (index != UINT32_MAX)
        *entry = &mHeaders[index];
    return index;
}

inline int32_t
nsHttpHeaderArray::LookupEntry(nsHttpAtom header, nsEntry **entry)
{
    uint32_t index = mHeaders.IndexOf(header, 0, nsEntry::MatchHeader());
    if (index != UINT32_MAX)
        *entry = &mHeaders[index];
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

inline void
nsHttpHeaderArray::MergeHeader(nsHttpAtom header,
                               nsEntry *entry,
                               const nsACString &value)
{
    if (value.IsEmpty())
        return;   // merge of empty header = no-op

    // Append the new value to the existing value
    if (header == nsHttp::Set_Cookie ||
        header == nsHttp::WWW_Authenticate ||
        header == nsHttp::Proxy_Authenticate)
    {
        // Special case these headers and use a newline delimiter to
        // delimit the values from one another as commas may appear
        // in the values of these headers contrary to what the spec says.
        entry->value.Append('\n');
    } else {
        // Delimit each value from the others using a comma (per HTTP spec)
        entry->value.AppendLiteral(", ");
    }
    entry->value.Append(value);
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

#endif
