/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*

  A content model builder interface. An object that implements this
  interface is associated with an nsIRDFDocument object to construct
  an NGLayout content model.

 */

#ifndef nsIRDFContentModelBuilder_h__
#define nsIRDFContentModelBuilder_h__

#include "nsISupports.h"

class nsIAtom;
class nsIContent;
class nsIRDFCompositeDataSource;
class nsIRDFDocument;
class nsIRDFNode;
class nsIRDFResource;

// {541AFCB0-A9A3-11d2-8EC5-00805F29F370}
#define NS_IRDFCONTENTMODELBUILDER_IID \
{ 0x541afcb0, 0xa9a3, 0x11d2, { 0x8e, 0xc5, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

class nsIRDFContentModelBuilder : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IRDFCONTENTMODELBUILDER_IID; return iid; }

    /**
     * Point the content model builder to the document. The content model
     * builder must not reference count the document.
     */
    NS_IMETHOD SetDocument(nsIRDFDocument* aDocument) = 0;

    NS_IMETHOD SetDataBase(nsIRDFCompositeDataSource* aDataBase) = 0;
    NS_IMETHOD GetDataBase(nsIRDFCompositeDataSource** aDataBase) = 0;

    /**
     * Set the root element from which this content model will
     * operate.
     */
    NS_IMETHOD CreateRootContent(nsIRDFResource* aResource) = 0;
    NS_IMETHOD SetRootContent(nsIContent* aElement) = 0;

    /**
     * Construct the contents for a container element.
     */
    NS_IMETHOD CreateContents(nsIContent* aElement) = 0;

    /** 
     * Construct an element. This is exposed as a public method,
     * because the document implementation may need to call it to
     * support the DOM's document.createElement() method.
     */
    NS_IMETHOD CreateElement(PRInt32 aNameSpaceID,
                             nsIAtom* aTag,
                             nsIRDFResource* aResource,
                             nsIContent** aResult) = 0;
};

extern nsresult NS_NewXULTemplateBuilder(nsIRDFContentModelBuilder** aResult);
extern nsresult NS_NewRDFXULBuilder(nsIRDFContentModelBuilder** aResult);

#endif // nsIRDFContentModelBuilder_h__
