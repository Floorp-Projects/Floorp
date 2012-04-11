/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsHttpHeaderArray_h__
#define nsHttpHeaderArray_h__

#include "nsHttp.h"
#include "nsTArray.h"
#include "nsIHttpChannel.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsHttpHeaderArray
{
public:
    nsHttpHeaderArray() {}
   ~nsHttpHeaderArray() { Clear(); }

    const char *PeekHeader(nsHttpAtom header);

    // Used by internal setters: to set header from network use SetHeaderFromNet
    nsresult SetHeader(nsHttpAtom header, const nsACString &value,
                       bool merge = false);

    // Merges supported headers. For other duplicate values, determines if error
    // needs to be thrown or 1st value kept.
    nsresult SetHeaderFromNet(nsHttpAtom header, const nsACString &value);

    nsresult GetHeader(nsHttpAtom header, nsACString &value);
    void     ClearHeader(nsHttpAtom h);

    // Find the location of the given header value, or null if none exists.
    const char *FindHeaderValue(nsHttpAtom header, const char *value) {
        return nsHttp::FindToken(PeekHeader(header), value,
                                 HTTP_HEADER_VALUE_SEPS);
    }

    // Determine if the given header value exists.
    bool HasHeaderValue(nsHttpAtom header, const char *value) {
        return FindHeaderValue(header, value) != nsnull;
    }

    nsresult VisitHeaders(nsIHttpHeaderVisitor *visitor);

    // parse a header line, return the header atom and a pointer to the 
    // header value (the substring of the header line -- do not free).
    nsresult ParseHeaderLine(const char *line,
                             nsHttpAtom *header=nsnull,
                             char **value=nsnull);

    void Flatten(nsACString &, bool pruneProxyHeaders=false);

    PRUint32 Count() { return mHeaders.Length(); }

    const char *PeekHeaderAt(PRUint32 i, nsHttpAtom &header);

    void Clear();

    struct nsEntry
    {
        nsEntry() {}

        nsHttpAtom header;
        nsCString  value;

        struct MatchHeader {
          bool Equals(const nsEntry &entry, const nsHttpAtom &header) const {
            return entry.header == header;
          }
        };
    };

private:
    PRInt32 LookupEntry(nsHttpAtom header, nsEntry **);
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

    nsTArray<nsEntry> mHeaders;

    friend struct IPC::ParamTraits<nsHttpHeaderArray>;
};


//-----------------------------------------------------------------------------
// nsHttpHeaderArray <private>: inline functions
//-----------------------------------------------------------------------------

inline PRInt32
nsHttpHeaderArray::LookupEntry(nsHttpAtom header, nsEntry **entry)
{
    PRUint32 index = mHeaders.IndexOf(header, 0, nsEntry::MatchHeader());
    if (index != PR_UINT32_MAX)
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

    NS_ASSERTION(!retval || IsSingletonHeader(header),
                 "Only non-mergeable headers should be in this list\n");

    return retval;
}

#endif
