/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  Implementations for a bunch of useful RDF utility routines.  Many of
  these will eventually be exported outside of RDF.DLL via the
  nsIRDFService interface.

  TO DO

  1) Make this so that it doesn't permanently leak the RDF service
     object.

  2) Make container functions thread-safe. They currently don't ensure
     that the RDF:nextVal property is maintained safely.

 */

#include "nsCOMPtr.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsUnicharUtils.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

nsresult
rdf_MakeRelativeRef(const nsCSubstring& aBaseURI, nsCString& aURI)
{
    // This implementation is extremely simple: e.g., it can't compute
    // relative paths, or anything fancy like that. If the context URI
    // is not a prefix of the URI in question, we'll just bail.
    uint32_t prefixLen = aBaseURI.Length();
    if (prefixLen != 0 && StringBeginsWith(aURI, aBaseURI)) {
        if (prefixLen < aURI.Length() && aURI.CharAt(prefixLen) == '/')
            ++prefixLen; // chop the leading slash so it's not `absolute'

        aURI.Cut(0, prefixLen);
    }

    return NS_OK;
}

void
rdf_FormatDate(PRTime aTime, nsACString &aResult)
{
    // Outputs Unixish date in GMT plus usecs; e.g.,
    //   Wed Jan  9 19:15:13 2002 +002441
    //
    PRExplodedTime t;
    PR_ExplodeTime(aTime, PR_GMTParameters, &t);

    char buf[256];
    PR_FormatTimeUSEnglish(buf, sizeof buf, "%a %b %d %H:%M:%S %Y", &t);
    aResult.Append(buf);

    // usecs
    aResult.Append(" +");
    int32_t usec = t.tm_usec;
    for (int32_t digit = 100000; digit > 1; digit /= 10) {
        aResult.Append(char('0' + (usec / digit)));
        usec %= digit;
    }
    aResult.Append(char('0' + usec));
}

PRTime
rdf_ParseDate(const nsACString &aTime)
{
    PRTime t;
    PR_ParseTimeString(PromiseFlatCString(aTime).get(), true, &t);

    int32_t usec = 0;

    nsACString::const_iterator begin, digit, end;
    aTime.BeginReading(begin);
    aTime.EndReading(end);

    // Walk backwards until we find a `+', run out of string, or a
    // non-numeric character.
    digit = end;
    while (--digit != begin && *digit != '+') {
        if (*digit < '0' || *digit > '9')
            break;
    }

    if (digit != begin && *digit == '+') {
        // There's a usec field specified (or, at least, something
        // that looks close enough. Parse it, and add it to the time.
        while (++digit != end) {
            usec *= 10;
            usec += *digit - '0';
        }

        t += usec;
    }

    return t;
}
