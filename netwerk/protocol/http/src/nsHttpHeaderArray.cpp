/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#include "nsHttpHeaderArray.h"
#include "nsHttp.h"

//-----------------------------------------------------------------------------
// nsHttpHeaderArray <public>
//-----------------------------------------------------------------------------

nsresult
nsHttpHeaderArray::SetHeader(nsHttpAtom header, const char *value)
{
    nsEntry *entry = nsnull;
    PRInt32 index;

    // If a NULL value is passed in, then delete the header entry...
    index = LookupEntry(header, &entry);
    if (!value || !*value) {
        if (entry) {
            mHeaders.RemoveElementAt(index);
            delete entry;
        }
        return NS_OK;
    }

    // Create a new entry or...
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
    else if (CanAppendToHeader(header)) {
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

const char *
nsHttpHeaderArray::PeekHeader(nsHttpAtom header)
{
    nsEntry *entry = nsnull;
    LookupEntry(header, &entry);
    return entry ? entry->value.get() : nsnull;
}

nsresult
nsHttpHeaderArray::GetHeader(nsHttpAtom header, char **result)
{
    const char *val = PeekHeader(header);
    return val ? DupString(val, result) : NS_ERROR_NOT_AVAILABLE;
}

nsresult
nsHttpHeaderArray::VisitHeaders(nsIHttpHeaderVisitor *visitor)
{
    NS_ENSURE_ARG_POINTER(visitor);
    PRInt32 i, count = mHeaders.Count();
    for (i=0; i<count; ++i) {
        nsEntry *entry = (nsEntry *) mHeaders[i];
        if (NS_FAILED(visitor->VisitHeader(entry->header, entry->value.get())))
            break;
    }
    return NS_OK;
}

void
nsHttpHeaderArray::Flatten(nsACString &buf)
{
    PRInt32 i, count = mHeaders.Count();
    for (i=0; i<count; ++i) {
        nsEntry *entry = (nsEntry *) mHeaders[i];
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
    return header == nsHttp::Accept_Charset ||
           header == nsHttp::Content_Type ||
           header == nsHttp::User_Agent ||
           header == nsHttp::Referer ||
           header == nsHttp::Host ||
           header == nsHttp::Authorization ||
           header == nsHttp::Proxy_Authorization ||
           header == nsHttp::If_Modified_Since ||
           header == nsHttp::If_Unmodified_Since ||
           header == nsHttp::From ||
           header == nsHttp::Location ||
           header == nsHttp::Max_Forwards
           ?
           PR_FALSE : PR_TRUE;
}
