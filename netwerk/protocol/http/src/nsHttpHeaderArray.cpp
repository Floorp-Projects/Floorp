/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsHttpHeaderArray.h"
#include "nsHttp.h"

//-----------------------------------------------------------------------------
// nsHttpHeaderArray <public>
//-----------------------------------------------------------------------------

nsresult
nsHttpHeaderArray::SetHeader(nsHttpAtom header,
                             const nsACString &value,
                             PRBool merge)
{
    nsEntry *entry = nsnull;
    PRInt32 index;

    index = LookupEntry(header, &entry);

    // If an empty value is passed in, then delete the header entry...
    // unless we are merging, in which case this function becomes a NOP.
    if (value.IsEmpty()) {
        if (!merge && entry) {
            mHeaders.RemoveElementAt(index);
            delete entry;
        }
        return NS_OK;
    }

    // Create a new entry, or...
    if (!entry) {
        entry = new nsEntry(header, value);
        if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;
        if (!mHeaders.AppendElement(entry)) {
            NS_WARNING("AppendElement failed");
            delete entry;
        }
    }
    // Append the new value to the existing value iff...
    else if (merge && CanAppendToHeader(header)) {
        if (header == nsHttp::Set_Cookie ||
            header == nsHttp::WWW_Authenticate ||
            header == nsHttp::Proxy_Authenticate)
            // Special case these headers and use a newline delimiter to
            // delimit the values from one another as commas may appear
            // in the values of these headers contrary to what the spec says.
            entry->value.Append('\n');
        else
            // Delimit each value from the others using a comma (per HTTP spec)
            entry->value.Append(", ");
        entry->value.Append(value);
    }
    // Replace the existing string with the new value
    else
        entry->value = value;
    return NS_OK;
}

void
nsHttpHeaderArray::ClearHeader(nsHttpAtom header)
{
    nsEntry *entry = nsnull;
    PRInt32 index;

    index = LookupEntry(header, &entry);
    if (entry) {
        mHeaders.RemoveElementAt(index);
        delete entry;
    }
}

const char *
nsHttpHeaderArray::PeekHeader(nsHttpAtom header)
{
    nsEntry *entry = nsnull;
    LookupEntry(header, &entry);
    return entry ? entry->value.get() : nsnull;
}

nsresult
nsHttpHeaderArray::GetHeader(nsHttpAtom header, nsACString &result)
{
    nsEntry *entry = nsnull;
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
    PRInt32 i, count = mHeaders.Count();
    for (i=0; i<count; ++i) {
        nsEntry *entry = (nsEntry *) mHeaders[i];
        if (NS_FAILED(visitor->VisitHeader(nsDependentCString(entry->header), entry->value)))
            break;
    }
    return NS_OK;
}

void
nsHttpHeaderArray::ParseHeaderLine(char *line, nsHttpAtom *hdr, char **val)
{
    //
    // Augmented BNF (from section 4.2 of RFC 2616 w/ modifications):
    //
    //   message-header = field-name field-sep [ field-value ]
    //   field-name     = token
    //   field-sep      = LWS ( ":" | "=" | SP | HT )
    //   field-value    = *( field-content | LWS )
    //   field-content  = <the OCTETs making up the field-value
    //                     and consisting of either *TEXT or combinations
    //                     of token, separators, and quoted-string>
    //
    // Here, we allow a greater set of possible header value separators
    // for compatibility with the vast number of broken web servers (mostly
    // lame CGI scripts).  NN4 and IE are similarly tolerant.
    //
    //
    // Examples:
    //  
    //   Header: Value
    //   Header :Value
    //   Header Value
    //   Header=Value
    //

    char *p = (char *) strchr(line, ':');
    if (!p)
        p = net_FindCharInSet(line, " \t=");

    if (p) {
        // ignore whitespace between header name and colon
        char *p2 = net_FindCharInSet(line, p, HTTP_LWS);
        *p2 = 0; // null terminate header name

        nsHttpAtom atom = nsHttp::ResolveAtom(line);
        if (atom) {
            // skip over whitespace
            p = net_FindCharNotInSet(++p, HTTP_LWS);

            // trim trailing whitespace - bug 86608
            p2 = net_RFindCharNotInSet(p, HTTP_LWS);
            *++p2 = 0; // null terminate header value; if all chars
                       // starting at |p| consisted of LWS, then p2
                       // would have pointed at |p-1|, so the prefix
                       // increment is always valid.

            // assign return values
            if (hdr) *hdr = atom;
            if (val) *val = p;

            // assign response header
            SetHeader(atom, nsDependentCString(p, p2 - p), PR_TRUE);
        }
        else
            LOG(("unknown header; skipping\n"));
    }
    else
        LOG(("malformed header\n"));

    // We ignore mal-formed headers in the hope that we'll still be able
    // to do something useful with the response.
}

void
nsHttpHeaderArray::Flatten(nsACString &buf, PRBool pruneProxyHeaders)
{
    PRInt32 i, count = mHeaders.Count();
    for (i=0; i<count; ++i) {
        nsEntry *entry = (nsEntry *) mHeaders[i];
        // prune proxy headers if requested
        if (pruneProxyHeaders && ((entry->header == nsHttp::Proxy_Authorization) || 
                                  (entry->header == nsHttp::Proxy_Connection)))
            continue;
        buf.Append(entry->header);
        buf.Append(": ");
        buf.Append(entry->value);
        buf.Append("\r\n");
    }
}

const char *
nsHttpHeaderArray::PeekHeaderAt(PRUint32 index, nsHttpAtom &header)
{
    nsEntry *entry = (nsEntry *) mHeaders[index]; 
    if (!entry)
        return nsnull;

    header = entry->header;
    return entry->value.get();
}

void
nsHttpHeaderArray::Clear()
{
    PRInt32 i, count = mHeaders.Count();
    for (i=0; i<count; ++i)
        delete (nsEntry *) mHeaders[i];
    mHeaders.Clear();
}

//-----------------------------------------------------------------------------
// nsHttpHeaderArray <private>
//-----------------------------------------------------------------------------

PRInt32
nsHttpHeaderArray::LookupEntry(nsHttpAtom header, nsEntry **entry)
{
    PRInt32 i, count = mHeaders.Count();
    for (i=0; i<count; ++i) {
        *entry = (nsEntry *) mHeaders[i];
        if ((*entry)->header == header)
            return i;
    }
    *entry = nsnull;
    return -1;
}

PRBool
nsHttpHeaderArray::CanAppendToHeader(nsHttpAtom header)
{
    return header != nsHttp::Content_Type        &&
           header != nsHttp::Content_Length      &&
           header != nsHttp::User_Agent          &&
           header != nsHttp::Referer             &&
           header != nsHttp::Host                &&
           header != nsHttp::Authorization       &&
           header != nsHttp::Proxy_Authorization &&
           header != nsHttp::If_Modified_Since   &&
           header != nsHttp::If_Unmodified_Since &&
           header != nsHttp::From                &&
           header != nsHttp::Location            &&
           header != nsHttp::Max_Forwards;
}
