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

/*

  This interface encapsulates information about an RDF/XML file,
  including the root resource, CSS style sheets, and named data
  sources.

 */

#ifndef nsIRDFXMLDocument_h__
#define nsIRDFXMLDocument_h__

#include "nsISupports.h"
class nsIOutputStream;
class nsIURL;

// {EB1A5D30-AB33-11d2-8EC6-00805F29F370}
#define NS_IRDFXMLDOCUMENTOBSERVER_IID \
{ 0xeb1a5d30, 0xab33, 0x11d2, { 0x8e, 0xc6, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

class nsIRDFXMLDocumentObserver : public nsISupports
{
public:
    NS_IMETHOD OnBeginLoad(void) = 0;
    NS_IMETHOD OnInterrupt(void) = 0;
    NS_IMETHOD OnResume(void) = 0;
    NS_IMETHOD OnEndLoad(void) = 0;

    NS_IMETHOD OnRootResourceFound(nsIRDFResource* aResource) = 0;
    NS_IMETHOD OnCSSStyleSheetAdded(nsIURL* aCSSStyleSheetURL) = 0;
    NS_IMETHOD OnNamedDataSourceAdded(const char* aNamedDataSourceURI) = 0;
};

// {EB1A5D31-AB33-11d2-8EC6-00805F29F370}
#define NS_IRDFXMLDOCUMENT_IID \
{ 0xeb1a5d31, 0xab33, 0x11d2, { 0x8e, 0xc6, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

class nsIRDFXMLDocument : public nsISupports
{
public:
    NS_IMETHOD BeginLoad(void) = 0;
    NS_IMETHOD Interrupt(void) = 0;
    NS_IMETHOD Resume(void) = 0;
    NS_IMETHOD EndLoad(void) = 0;

    NS_IMETHOD SetRootResource(nsIRDFResource* aResource) = 0;
    NS_IMETHOD GetRootResource(nsIRDFResource** aResource) = 0;
    NS_IMETHOD AddCSSStyleSheetURL(nsIURL* aStyleSheetURL) = 0;
    NS_IMETHOD GetCSSStyleSheetURLs(nsIURL*** aStyleSheetURLs, PRInt32* aCount) = 0;
    NS_IMETHOD AddNamedDataSourceURI(const char* aNamedDataSourceURI) = 0;
    NS_IMETHOD GetNamedDataSourceURIs(const char* const** aNamedDataSourceURIs, PRInt32* aCount) = 0;
    NS_IMETHOD AddDocumentObserver(nsIRDFXMLDocumentObserver* aObserver) = 0;
    NS_IMETHOD RemoveDocumentObserver(nsIRDFXMLDocumentObserver* aObserver) = 0;
};


#endif // nsIRDFXMLDocument_h__

