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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "prtime.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

nsresult
rdf_MakeRelativeRef(const nsCSubstring& aBaseURI, nsCString& aURI)
{
    // This implementation is extremely simple: e.g., it can't compute
    // relative paths, or anything fancy like that. If the context URI
    // is not a prefix of the URI in question, we'll just bail.
    PRUint32 prefixLen = aBaseURI.Length();
    if (prefixLen != 0 && StringBeginsWith(aURI, aBaseURI)) {
        if (prefixLen < aURI.Length() && aURI.CharAt(prefixLen) == '/')
            ++prefixLen; // chop the leading slash so it's not `absolute'

        aURI.Cut(0, prefixLen);
    }

    return NS_OK;
}

static PRBool
rdf_RequiresAbsoluteURI(const nsString& uri)
{
    // cheap shot at figuring out if this requires an absolute url translation
    return !(StringBeginsWith(uri, NS_LITERAL_STRING("urn:")) ||
             StringBeginsWith(uri, NS_LITERAL_STRING("chrome:")) ||
             StringBeginsWith(uri, NS_LITERAL_STRING("nc:"),
                              nsCaseInsensitiveStringComparator()));
}

nsresult
rdf_MakeAbsoluteURI(const nsString& aBaseURI, nsString& aURI)
{
    nsresult rv;
    nsAutoString result;

    if (!rdf_RequiresAbsoluteURI(aURI))
        return NS_OK;
    
    nsCOMPtr<nsIURI> base;
    rv = NS_NewURI(getter_AddRefs(base), aBaseURI);
    if (NS_FAILED(rv)) return rv;

    rv = NS_MakeAbsoluteURI(result, aURI, base);

    if (NS_SUCCEEDED(rv)) {
        aURI = result;
    }
    else {
        // There are some ugly URIs (e.g., "NC:Foo") that netlib can't
        // parse. If NS_MakeAbsoluteURL fails, then just punt and
        // assume that aURI was already absolute.
    }

    return NS_OK;
}


nsresult
rdf_MakeAbsoluteURI(nsIURI* aBase, nsString& aURI)
{
    nsresult rv;

    if (!rdf_RequiresAbsoluteURI(aURI))
        return NS_OK;

    nsAutoString result;

    rv = NS_MakeAbsoluteURI(result, aURI, aBase);

    if (NS_SUCCEEDED(rv)) {
        aURI = result;
    }
    else {
        // There are some ugly URIs (e.g., "NC:Foo") that netlib can't
        // parse. If NS_MakeAbsoluteURL fails, then just punt and
        // assume that aURI was already absolute.
    }

    return NS_OK;
}

nsresult
rdf_MakeAbsoluteURI(nsIURI* aBase, nsCString& aURI)
{
    nsresult rv;
    nsXPIDLCString result;

    rv = NS_MakeAbsoluteURI(getter_Copies(result), aURI.get(), aBase);

    if (NS_SUCCEEDED(rv)) {
        aURI.Assign(result);
    }
    else {
        // There are some ugly URIs (e.g., "NC:Foo") that netlib can't
        // parse. If NS_MakeAbsoluteURL fails, then just punt and
        // assume that aURI was already absolute.
    }

    return NS_OK;
}

void
rdf_FormatDate(PRTime aTime, nsACString &aResult)
{
    // Outputs Unixish date in GMT plus usecs; e.g.,
    //   Wed Jan  9 19:15:13 GMT 2002 +002441
    //
    PRExplodedTime t;
    PR_ExplodeTime(aTime, PR_LocalTimeParameters, &t);

    char buf[256];
    PR_FormatTimeUSEnglish(buf, sizeof buf, "%a %b %d %H:%M:%S %Z %Y", &t);
    aResult.Append(buf);

    // usecs
    aResult.Append(" +");
    PRInt32 usec = t.tm_usec;
    for (PRInt32 digit = 100000; digit > 1; digit /= 10) {
        aResult.Append(char('0' + (usec / digit)));
        usec %= digit;
    }
    aResult.Append(char('0' + usec));
}

PRTime
rdf_ParseDate(const nsACString &aTime)
{
    PRTime t;
    PR_ParseTimeString(PromiseFlatCString(aTime).get(), PR_TRUE, &t);

    PRInt32 usec = 0;

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

        PRTime temp;
        LL_I2L(temp, usec);
        LL_ADD(t, t, temp);
    }

    return t;
}
