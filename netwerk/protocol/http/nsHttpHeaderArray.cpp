/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHttpHeaderArray.h"
#include "nsHttp.h"

//-----------------------------------------------------------------------------
// nsHttpHeaderArray <public>
//-----------------------------------------------------------------------------
nsresult
nsHttpHeaderArray::SetHeader(nsHttpAtom header,
                             const nsACString &value,
                             bool merge)
{
    nsEntry *entry = nullptr;
    PRInt32 index;

    index = LookupEntry(header, &entry);

    // If an empty value is passed in, then delete the header entry...
    // unless we are merging, in which case this function becomes a NOP.
    if (value.IsEmpty()) {
        if (!merge && entry)
            mHeaders.RemoveElementAt(index);
        return NS_OK;
    }

    if (!entry) {
        entry = mHeaders.AppendElement(); // new nsEntry()
        if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;
        entry->header = header;
        entry->value = value;
    } else if (merge && !IsSingletonHeader(header)) {
        MergeHeader(header, entry, value);
    } else {
        // Replace the existing string with the new value
        entry->value = value;
    }

    return NS_OK;
}

nsresult
nsHttpHeaderArray::SetHeaderFromNet(nsHttpAtom header, const nsACString &value)
{
    nsEntry *entry = nullptr;
    PRInt32 index;

    index = LookupEntry(header, &entry);

    if (!entry) {
        if (value.IsEmpty()) {
            if (!TrackEmptyHeader(header)) {
                LOG(("Ignoring Empty Header: %s\n", header.get()));
                return NS_OK; // ignore empty headers by default
            }
        }
        entry = mHeaders.AppendElement(); //new nsEntry(header, value);
        if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;
        entry->header = header;
        entry->value = value;
    } else if (!IsSingletonHeader(header)) {
        MergeHeader(header, entry, value);
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
    }

    return NS_OK;
}

void
nsHttpHeaderArray::ClearHeader(nsHttpAtom header)
{
    mHeaders.RemoveElement(header, nsEntry::MatchHeader());
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
nsHttpHeaderArray::VisitHeaders(nsIHttpHeaderVisitor *visitor)
{
    NS_ENSURE_ARG_POINTER(visitor);
    PRUint32 i, count = mHeaders.Length();
    for (i = 0; i < count; ++i) {
        const nsEntry &entry = mHeaders[i];
        if (NS_FAILED(visitor->VisitHeader(nsDependentCString(entry.header),
                                           entry.value)))
            break;
    }
    return NS_OK;
}

nsresult
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
        return NS_OK;
    }

    // make sure we have a valid token for the field-name
    if (!nsHttp::IsValidToken(line, p)) {
        LOG(("malformed header [%s]: field-name not a token\n", line));
        return NS_OK;
    }
    
    *p = 0; // null terminate field-name

    nsHttpAtom atom = nsHttp::ResolveAtom(line);
    if (!atom) {
        LOG(("failed to resolve atom [%s]\n", line));
        return NS_OK;
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

    // assign response header
    return SetHeaderFromNet(atom, nsDependentCString(p, p2 - p));
}

void
nsHttpHeaderArray::Flatten(nsACString &buf, bool pruneProxyHeaders)
{
    PRUint32 i, count = mHeaders.Length();
    for (i = 0; i < count; ++i) {
        const nsEntry &entry = mHeaders[i];
        // prune proxy headers if requested
        if (pruneProxyHeaders && ((entry.header == nsHttp::Proxy_Authorization) || 
                                  (entry.header == nsHttp::Proxy_Connection)))
            continue;
        buf.Append(entry.header);
        buf.AppendLiteral(": ");
        buf.Append(entry.value);
        buf.AppendLiteral("\r\n");
    }
}

const char *
nsHttpHeaderArray::PeekHeaderAt(PRUint32 index, nsHttpAtom &header) const
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
