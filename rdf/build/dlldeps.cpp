/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIRDFContainer.h"
#include "nsRDFParserUtils.h"
#include "nsRDFResource.h"
#include "nsString.h"
#include "rdfutil.h"

void XXXNeverCalled()
{
    nsAutoString s;
    nsCAutoString cs;
	nsCString cStr;
    int v;

    // nsRDFParserUtils
    nsRDFParserUtils::EntityToUnicode("");
    nsRDFParserUtils::StripAndConvert(s);
    nsRDFParserUtils::GetQuotedAttributeValue(s, s, s);
    nsRDFParserUtils::IsJavaScriptLanguage(s, &v);

    // rdfutils
    rdf_MakeRelativeRef(s, s);
    rdf_MakeRelativeName(s, s);
    rdf_MakeAbsoluteURI(s, s);
    rdf_MakeAbsoluteURI(s, cStr);

    NS_NewContainerEnumerator(nsnull, nsnull, nsnull);
    NS_NewEmptyEnumerator(nsnull);

    // nsRDFResource
    nsRDFResource r();
}




