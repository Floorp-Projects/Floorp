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

#ifndef nsHttpHeaderArray_h__
#define nsHttpHeaderArray_h__

#include "nsVoidArray.h"
#include "nsIHttpChannel.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsHttp.h"

class nsHttpHeaderArray
{
public:
    nsHttpHeaderArray() {}
   ~nsHttpHeaderArray() { Clear(); }

    const char *PeekHeader(nsHttpAtom header);

    nsresult SetHeader(nsHttpAtom header, const char *value);
    nsresult GetHeader(nsHttpAtom header, char **value);

    nsresult VisitHeaders(nsIHttpHeaderVisitor *visitor);

    // parse a header line, return the header atom and a pointer to the 
    // header value (the substring of the header line -- do not free).
    void ParseHeaderLine(char *line, nsHttpAtom *header=nsnull, char **value=nsnull);

    void Flatten(nsACString &);

    PRUint32 Count() { return (PRUint32) mHeaders.Count(); }

    const char *PeekHeaderAt(PRUint32 i, nsHttpAtom &header);

    void Clear();

private:
    struct nsEntry
    {
        nsEntry(nsHttpAtom h, const char *v)
            : header(h) { value = v; }

        nsHttpAtom header;
        nsCString  value;
    };

    PRInt32 LookupEntry(nsHttpAtom header, nsEntry **);
    PRBool  CanAppendToHeader(nsHttpAtom header);

private:
    nsAutoVoidArray mHeaders;
};

#endif
