/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsIRDFContent_h___
#define nsIRDFContent_h___

#include "nsISupports.h"
#include "nsIXMLContent.h"

class nsIRDFContent;
class nsIRDFDocument;
class nsIRDFResource;

// {954F0810-81DC-11d2-B52A-000000000000}
#define NS_IRDFCONTENT_IID \
{ 0x954f0810, 0x81dc, 0x11d2, { 0xb5, 0x2a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }

/**
 * RDF content extensions to nsIXMLContent
 */
class nsIRDFContent : public nsIXMLContent {
public:
  NS_IMETHOD Init(nsIRDFDocument* doc,
                  const nsString& tag,
                  nsIRDFResource* resource,
                  PRBool childrenMustBeGenerated) = 0;

  NS_IMETHOD SetResource(const nsString& aURI) = 0;
  NS_IMETHOD GetResource(nsString& rURI) const = 0;

  NS_IMETHOD SetResource(nsIRDFResource* aResource) = 0;
  NS_IMETHOD GetResource(nsIRDFResource*& aResource) = 0;

  NS_IMETHOD SetProperty(const nsString& aPropertyURI, const nsString& aValue) = 0;
  NS_IMETHOD GetProperty(const nsString& aPropertyURI, nsString& rValue) const = 0;
};


extern nsresult
NS_NewRDFElement(nsIRDFContent** aResult);

#endif // nsIRDFContent_h___
