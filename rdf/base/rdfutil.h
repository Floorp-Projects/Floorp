/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/*

  A bunch of useful RDF utility routines.  Many of these will
  eventually be exported outside of RDF.DLL via the nsIRDFService
  interface.

  TO DO

  1) Move the anonymous resource stuff to nsIRDFService?

  2) All that's left is rdf_PossiblyMakeRelative() and
     -Absolute(). Maybe those go on nsIRDFService, too.

 */

#ifndef rdfutil_h__
#define rdfutil_h__


class nsACString;
class nsCString;

nsresult
rdf_MakeRelativeRef(const nsACString& aBaseURI, nsCString& aURI);

void
rdf_FormatDate(PRTime aTime, nsACString &aResult);

PRTime
rdf_ParseDate(const nsACString &aTime);

#endif // rdfutil_h__


