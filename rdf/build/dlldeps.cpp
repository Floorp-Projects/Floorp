/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsRDFParserUtils.h"
#include "nsRDFResource.h"
#include "nsString.h"
#include "rdfutil.h"

void XXXNeverCalled()
{
    static nsAutoString s;

    // nsRDFParserUtils
    nsRDFParserUtils::EntityToUnicode("");
    nsRDFParserUtils::StripAndConvert(s);
    nsRDFParserUtils::GetQuotedAttributeValue(s, s, s);
    nsRDFParserUtils::IsJavaScriptLanguage(s);

    // rdfutils
    rdf_IsOrdinalProperty(nsnull);
    rdf_OrdinalResourceToIndex(nsnull, nsnull);
    rdf_IndexToOrdinalResource(0, nsnull);
    rdf_IsContainer(nsnull, nsnull);
    rdf_IsBag(nsnull, nsnull);
    rdf_IsSeq(nsnull, nsnull);
    rdf_IsAlt(nsnull, nsnull);
    rdf_CreateAnonymousResource(s, nsnull);
    rdf_IsAnonymousResource(s, nsnull);
    rdf_PossiblyMakeRelative(s, s);
    rdf_MakeBag(nsnull, nsnull);
    rdf_MakeSeq(nsnull, nsnull);
    rdf_MakeAlt(nsnull, nsnull);
    rdf_ContainerAppendElement(nsnull, nsnull, nsnull);
    rdf_ContainerRemoveElement(nsnull, nsnull, nsnull, PR_FALSE);
    rdf_ContainerInsertElementAt(nsnull, nsnull, nsnull, 0, PR_FALSE);
    rdf_ContainerIndexOf(nsnull, nsnull, nsnull, nsnull);
    NS_NewContainerEnumerator(nsnull, nsnull, nsnull);
    NS_NewEmptyEnumerator(nsnull);

    // nsRDFResource
    nsRDFResource r();
}




