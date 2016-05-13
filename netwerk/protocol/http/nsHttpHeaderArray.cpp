/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttpHeaderArray.h"
#include "nsURLHelper.h"
#include "nsIHttpHeaderVisitor.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// nsHttpHeaderArray <public>
//-----------------------------------------------------------------------------
nsresult
nsHttpHeaderArray::SetHeader(nsHttpAtom header,
                             const nsACString &value,
                             bool merge,
                             nsHttpHeaderArray::HeaderVariety variety)
{
    MOZ_ASSERT((variety == eVarietyResponse) ||
               (variety == eVarietyRequestDefault) ||
               (variety == eVarietyRequestOverride),
               "Net original headers can only be set using SetHeader_internal().");

    nsEntry *entry = nullptr;
    int32_t index;

    index = LookupEntry(header, &entry);

    // If an empty value is passed in, then delete the header entry...
    // unless we are merging, in which case this function becomes a NOP.
    if (value.IsEmpty()) {
        if (!merge && entry) {
            if (entry->variety == eVarietyResponseNetOriginalAndResponse) {
                MOZ_ASSERT(variety == eVarietyResponse);
                entry->variety = eVarietyResponseNetOriginal;
            } else {
                mHeaders.RemoveElementAt(index);
            }
        }
        return NS_OK;
    }

    MOZ_ASSERT(!entry || variety != eVarietyRequestDefault,
               "Cannot set default entry which overrides existing entry!");
    if (!entry) {
        return SetHeader_internal(header, value, variety);
    } else if (merge && !IsSingletonHeader(header)) {
        return MergeHeader(header, entry, value, variety);
    } else {
        // Replace the existing string with the new value
        if (entry->variety == eVarietyResponseNetOriginalAndResponse) {
            MOZ_ASSERT(variety == eVarietyResponse);
            entry->variety = eVarietyResponseNetOriginal;
            return SetHeader_internal(header, value, variety);
        } else {
            entry->value = value;
            entry->variety = variety;
        }
    }

    return NS_OK;
}

nsresult
nsHttpHeaderArray::SetHeader_internal(nsHttpAtom header,
                                      const nsACString &value,
                                      nsHttpHeaderArray::HeaderVariety variety)
{
    nsEntry *entry =  mHeaders.AppendElement();
    if (!entry) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    entry->header = header;
    entry->value = value;
    entry->variety = variety;
    return NS_OK;
}

nsresult
nsHttpHeaderArray::SetEmptyHeader(nsHttpAtom header, HeaderVariety variety)
{
    MOZ_ASSERT((variety == eVarietyResponse) ||
               (variety == eVarietyRequestDefault) ||
               (variety == eVarietyRequestOverride),
               "Original headers can only be set using SetHeader_internal().");
    nsEntry *entry = nullptr;

    LookupEntry(header, &entry);

    if (entry &&
        entry->variety != eVarietyResponseNetOriginalAndResponse) {
        entry->value.Truncate();
        return NS_OK;
    } else if (entry) {
        MOZ_ASSERT(variety == eVarietyResponse);
        entry->variety = eVarietyResponseNetOriginal;
    }

    return SetHeader_internal(header, EmptyCString(), variety);
}

nsresult
nsHttpHeaderArray::SetHeaderFromNet(nsHttpAtom header,
                                    const nsACString &value,
                                    bool response)
{
    // mHeader holds the consolidated (merged or updated) headers.
    // mHeader for response header will keep the original heades as well.
    nsEntry *entry = nullptr;

    LookupEntry(header, &entry);

    if (!entry) {
        if (value.IsEmpty()) {
            if (!gHttpHandler->KeepEmptyResponseHeadersAsEmtpyString() &&
                !TrackEmptyHeader(header)) {
                LOG(("Ignoring Empty Header: %s\n", header.get()));
                if (response) {
                    // Set header as original but not as response header.
                    return SetHeader_internal(header, value,
                                              eVarietyResponseNetOriginal);
                }
                return NS_OK; // ignore empty headers by default
            }
        }
        HeaderVariety variety = eVarietyRequestOverride;
        if (response) {
            variety = eVarietyResponseNetOriginalAndResponse;
        }
        return SetHeader_internal(header, value, variety);

    } else if (!IsSingletonHeader(header)) {
        HeaderVariety variety = eVarietyRequestOverride;
        if (response) {
            variety = eVarietyResponse;
        }
        nsresult rv = MergeHeader(header, entry, value, variety);
        if (NS_FAILED(rv)) {
            return rv;
        }
        if (response) {
            rv = SetHeader_internal(header, value,
                                    eVarietyResponseNetOriginal);
        }
        return rv;
    } else {
        // Multiple instances of non-mergeable header received from network
        // - ignore if same value
        if (!entry->value.Equals(value)) {
            if (IsSuspectDuplicateHeader(header)) {
                // reply may be corrupt/hacked (ex: CLRF injection attacks)
                return NS_ERROR_CORRUPTED_CONTENT;
            } // else silently drop value: keep value from 1st header seen
            LOG(("Header %s silently dropped as non mergeable header\n",
                 header.get()));

        }
        if (response) {
            return SetHeader_internal(header, value,
                                      eVarietyResponseNetOriginal);
        }
    }

    return NS_OK;
}

void
nsHttpHeaderArray::ClearHeader(nsHttpAtom header)
{
    nsEntry *entry = nullptr;
    int32_t index = LookupEntry(header, &entry);
    if (entry) {
        if (entry->variety == eVarietyResponseNetOriginalAndResponse) {
            entry->variety = eVarietyResponseNetOriginal;
        } else {
            mHeaders.RemoveElementAt(index);
        }
    }
}

const char *
nsHttpHeaderArray::PeekHeader(nsHttpAtom header) const
{
    const nsEntry *entry = nullptr;
    LookupEntry(header, &entry);
    return entry ? entry->value.get() : nullptr;
}

nsresult
nsHttpHeaderArray::GetHeader(nsHttpAtom header, nsACString &result) const
{
    const nsEntry *entry = nullptr;
    LookupEntry(header, &entry);
    if (!entry)
        return NS_ERROR_NOT_AVAILABLE;
    result = entry->value;
    return NS_OK;
}

nsresult
nsHttpHeaderArray::GetOriginalHeader(nsHttpAtom aHeader,
                                     nsIHttpHeaderVisitor *aVisitor)
{
    NS_ENSURE_ARG_POINTER(aVisitor);
    uint32_t index = 0;
    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    while (true) {
        index = mHeaders.IndexOf(aHeader, index, nsEntry::MatchHeader());
        if (index != UINT32_MAX) {
            const nsEntry &entry = mHeaders[index];

            MOZ_ASSERT((entry.variety == eVarietyResponseNetOriginalAndResponse) ||
                       (entry.variety == eVarietyResponseNetOriginal) ||
                       (entry.variety == eVarietyResponse),
                       "This must be a response header.");
            index++;
            if (entry.variety == eVarietyResponse) {
                continue;
            }
            rv = NS_OK;
            if (NS_FAILED(aVisitor->VisitHeader(nsDependentCString(entry.header),
                                                entry.value))) {
                break;
            }
        } else {
            // if there is no such a header, it will return
            // NS_ERROR_NOT_AVAILABLE or NS_OK otherwise.
            return rv;
        }
    }
    return NS_OK;
}

bool
nsHttpHeaderArray::HasHeader(nsHttpAtom header) const
{
    const nsEntry *entry = nullptr;
    LookupEntry(header, &entry);
    return entry;
}

nsresult
nsHttpHeaderArray::VisitHeaders(nsIHttpHeaderVisitor *visitor, nsHttpHeaderArray::VisitorFilter filter)
{
    NS_ENSURE_ARG_POINTER(visitor);
    uint32_t i, count = mHeaders.Length();
    for (i = 0; i < count; ++i) {
        const nsEntry &entry = mHeaders[i];
        if (filter == eFilterSkipDefault && entry.variety == eVarietyRequestDefault) {
            continue;
        } else if (filter == eFilterResponse && entry.variety == eVarietyResponseNetOriginal) {
            continue;
        } else if (filter == eFilterResponseOriginal && entry.variety == eVarietyResponse) {
            continue;
        }
        if (NS_FAILED(visitor->VisitHeader(nsDependentCString(entry.header),
                                           entry.value))) {
            break;
        }
    }
    return NS_OK;
}

/*static*/ nsresult
nsHttpHeaderArray::ParseHeaderLine(const char *line,
                                   nsHttpAtom *hdr,
                                   char **val)
{
    //
    // BNF from section 4.2 of RFC 2616:
    //
    //   message-header = field-name ":" [ field-value ]
    //   field-name     = token
    //   field-value    = *( field-content | LWS )
    //   field-content  = <the OCTETs making up the field-value
    //                     and consisting of either *TEXT or combinations
    //                     of token, separators, and quoted-string>
    //

    // We skip over mal-formed headers in the hope that we'll still be able to
    // do something useful with the response.
    char *p = (char *) strchr(line, ':');
    if (!p) {
        LOG(("malformed header [%s]: no colon\n", line));
        return NS_ERROR_FAILURE;
    }

    // make sure we have a valid token for the field-name
    if (!nsHttp::IsValidToken(line, p)) {
        LOG(("malformed header [%s]: field-name not a token\n", line));
        return NS_ERROR_FAILURE;
    }

    *p = 0; // null terminate field-name

    nsHttpAtom atom = nsHttp::ResolveAtom(line);
    if (!atom) {
        LOG(("failed to resolve atom [%s]\n", line));
        return NS_ERROR_FAILURE;
    }

    // skip over whitespace
    p = net_FindCharNotInSet(++p, HTTP_LWS);

    // trim trailing whitespace - bug 86608
    char *p2 = net_RFindCharNotInSet(p, HTTP_LWS);

    *++p2 = 0; // null terminate header value; if all chars starting at |p|
               // consisted of LWS, then p2 would have pointed at |p-1|, so
               // the prefix increment is always valid.

    // assign return values
    if (hdr) *hdr = atom;
    if (val) *val = p;

    return NS_OK;
}

void
nsHttpHeaderArray::Flatten(nsACString &buf, bool pruneProxyHeaders,
                           bool pruneTransients)
{
    uint32_t i, count = mHeaders.Length();
    for (i = 0; i < count; ++i) {
        const nsEntry &entry = mHeaders[i];
        // Skip original header.
        if (entry.variety == eVarietyResponseNetOriginal) {
            continue;
        }
        // prune proxy headers if requested
        if (pruneProxyHeaders &&
            ((entry.header == nsHttp::Proxy_Authorization) ||
             (entry.header == nsHttp::Proxy_Connection))) {
            continue;
        }
        if (pruneTransients &&
            (entry.value.IsEmpty() ||
             entry.header == nsHttp::Connection ||
             entry.header == nsHttp::Proxy_Connection ||
             entry.header == nsHttp::Keep_Alive ||
             entry.header == nsHttp::WWW_Authenticate ||
             entry.header == nsHttp::Proxy_Authenticate ||
             entry.header == nsHttp::Trailer ||
             entry.header == nsHttp::Transfer_Encoding ||
             entry.header == nsHttp::Upgrade ||
             // XXX this will cause problems when we start honoring
             // Cache-Control: no-cache="set-cookie", what to do?
             entry.header == nsHttp::Set_Cookie)) {
            continue;
        }

        buf.Append(entry.header);
        buf.AppendLiteral(": ");
        buf.Append(entry.value);
        buf.AppendLiteral("\r\n");
    }
}

void
nsHttpHeaderArray::FlattenOriginalHeader(nsACString &buf)
{
    uint32_t i, count = mHeaders.Length();
    for (i = 0; i < count; ++i) {
        const nsEntry &entry = mHeaders[i];
        // Skip changed header.
        if (entry.variety == eVarietyResponse) {
            continue;
        }

        buf.Append(entry.header);
        buf.AppendLiteral(": ");
        buf.Append(entry.value);
        buf.AppendLiteral("\r\n");
    }
}

const char *
nsHttpHeaderArray::PeekHeaderAt(uint32_t index, nsHttpAtom &header) const
{
    const nsEntry &entry = mHeaders[index];

    header = entry.header;
    return entry.value.get();
}

void
nsHttpHeaderArray::Clear()
{
    mHeaders.Clear();
}

} // namespace net
} // namespace mozilla
