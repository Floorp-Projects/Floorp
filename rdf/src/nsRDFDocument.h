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

#ifndef nsRDFDocument_h___
#define nsRDFDocument_h___

#include "nsMarkupDocument.h"
#include "nsIRDFDocument.h"

class nsIParser;

class nsRDFDocument : public nsMarkupDocument,
                      public nsIRDFDocument
{
public:
  nsRDFDocument();
  virtual ~nsRDFDocument();

  // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  // Override nsDocument's implementation
  NS_IMETHOD StartDocumentLoad(nsIURL *aUrl,
                               nsIContentViewerContainer* aContainer,
                               nsIStreamListener **aDocListener,
                               const char* aCommand);

  NS_IMETHOD EndLoad();


  // nsIXMLDocument interface
  NS_IMETHOD RegisterNameSpace(nsIAtom* aPrefix, const nsString& aURI, 
			       PRInt32& aNameSpaceId);

  NS_IMETHOD GetNameSpaceURI(PRInt32 aNameSpaceId, nsString& aURI);
  NS_IMETHOD GetNameSpacePrefix(PRInt32 aNameSpaceId, nsIAtom*& aPrefix);

  NS_IMETHOD PrologElementAt(PRInt32 aOffset, nsIContent** aContent);
  NS_IMETHOD PrologCount(PRInt32* aCount);
  NS_IMETHOD AppendToProlog(nsIContent* aContent);

  NS_IMETHOD EpilogElementAt(PRInt32 aOffset, nsIContent** aContent);
  NS_IMETHOD EpilogCount(PRInt32* aCount);
  NS_IMETHOD AppendToEpilog(nsIContent* aContent);

  // nsIRDFDocument interface
  NS_IMETHOD GetDataBase(nsIRDFDataBase*& result);
  NS_IMETHOD SetDataSource(nsIRDFDataSource* ds);

protected:
  nsVoidArray*    mNameSpaces;
  nsIParser*      mParser;
  nsIRDFDataBase* mDB;
};


#endif // nsRDFDocument_h___
