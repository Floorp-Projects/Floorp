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

#include "prtypes.h"

class nsACString;
class nsCString;
class nsString;
class nsIURI;

nsresult
rdf_MakeRelativeRef(const nsCSubstring& aBaseURI, nsCString& aURI);

nsresult
rdf_MakeAbsoluteURI(const nsString& aBaseURI, nsString& aURI);

nsresult
rdf_MakeAbsoluteURI(nsIURI* aBaseURL, nsString& aURI);

nsresult
rdf_MakeAbsoluteURI(nsIURI* aBaseURL, nsCString& aURI);

void
rdf_FormatDate(PRTime aTime, nsACString &aResult);

PRTime
rdf_ParseDate(const nsACString &aTime);

#endif // rdfutil_h__


