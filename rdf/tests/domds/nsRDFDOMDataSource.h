/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

#ifndef __nsRDFDOMDataSource_h
#define __nsRDFDOMDataSource_h

#include "nsIRDFDataSource.h"

#include "nsIDOMDataSource.h"
#include "nsIRDFService.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"

#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"

/* {c7cf77e8-245a-11d3-80f0-006008948010} */
#define NS_RDF_DOMDATASOURCE_CID \
  {0xc7cf77e8, 0x245a, 0x11d3, \
    {0x80, 0xf0, 0x00, 0x60, 0x08, 0x94, 0x80, 0x10}}

class nsRDFDOMDataSource : public nsIRDFDataSource,
                           public nsIDOMDataSource
{
 public:
  nsRDFDOMDataSource();
  virtual ~nsRDFDOMDataSource();
  
  NS_DECL_ISUPPORTS

  NS_DECL_NSIRDFDATASOURCE

  NS_DECL_NSIDOMDATASOURCE
    
  static NS_METHOD  Create(nsISupports* aOuter,
                       const nsIID& iid,
                       void **result);

 protected:
    char *mURI;

	nsIRDFService *getRDFService();
	static PRBool assertEnumFunc(void *aElement, void *aData);
	static PRBool unassertEnumFunc(void *aElement, void *aData);
	nsresult  NotifyObservers(nsIRDFResource *subject, nsIRDFResource *property,
								nsIRDFNode *object, PRBool assert);

 private:

    PRBool init;
    nsIRDFService *mRDFService;
    PRInt32 mMode;
    nsVoidArray *mObservers;

    nsCOMPtr<nsIDOMDocument> mDocument;

    nsIRDFResource* kNC_Name;
    nsIRDFResource* kNC_Value;
    nsIRDFResource* kNC_Type;
    nsIRDFResource* kNC_Child;
    nsIRDFResource* kNC_DOMRoot;

};

#endif
