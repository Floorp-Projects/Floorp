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
    
  /* void Init (in string uri); */
  NS_IMETHOD Init(const char *uri);

  /* readonly attribute string URI; */
  NS_IMETHOD GetURI(char * *aURI);

  /* nsIRDFResource GetSource (in nsIRDFResource aProperty,
     in nsIRDFNode aTarget,
     in boolean aTruthValue); */
  NS_IMETHOD GetSource(nsIRDFResource *aProperty,
                       nsIRDFNode *aTarget,
                       PRBool aTruthValue,
                       nsIRDFResource **_retval);

  /* nsISimpleEnumerator GetSources (in nsIRDFResource aProperty,
     in nsIRDFNode aTarget,
     in boolean aTruthValue); */
  NS_IMETHOD GetSources(nsIRDFResource *aProperty,
                        nsIRDFNode *aTarget,
                        PRBool aTruthValue,
                        nsISimpleEnumerator **_retval);

  /* nsIRDFNode GetTarget (in nsIRDFResource aSource,
     in nsIRDFResource aProperty,
     in boolean aTruthValue); */
  NS_IMETHOD GetTarget(nsIRDFResource *aSource,
                       nsIRDFResource *aProperty,
                       PRBool aTruthValue,
                       nsIRDFNode **_retval);

  /* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource,
     in nsIRDFResource aProperty,
     in boolean aTruthValue); */
  NS_IMETHOD GetTargets(nsIRDFResource *aSource,
                        nsIRDFResource *aProperty,
                        PRBool aTruthValue,
                        nsISimpleEnumerator **_retval);

  /* void Assert (in nsIRDFResource aSource,
     in nsIRDFResource aProperty,
     in nsIRDFNode aTarget,
     in boolean aTruthValue); */
  NS_IMETHOD Assert(nsIRDFResource *aSource,
                    nsIRDFResource *aProperty,
                    nsIRDFNode *aTarget,
                    PRBool aTruthValue);

  /* void Unassert (in nsIRDFResource aSource,
     in nsIRDFResource aProperty,
     in nsIRDFNode aTarget); */
  NS_IMETHOD Unassert(nsIRDFResource *aSource,
                      nsIRDFResource *aProperty,
                      nsIRDFNode *aTarget);

  /* boolean HasAssertion (in nsIRDFResource aSource,
     in nsIRDFResource aProperty,
     in nsIRDFNode aTarget,
     in boolean aTruthValue); */
  NS_IMETHOD HasAssertion(nsIRDFResource *aSource,
                          nsIRDFResource *aProperty,
                          nsIRDFNode *aTarget,
                          PRBool aTruthValue,
                          PRBool *_retval);

  /* void AddObserver (in nsIRDFObserver aObserver); */
  NS_IMETHOD AddObserver(nsIRDFObserver *aObserver);

  /* void RemoveObserver (in nsIRDFObserver aObserver); */
  NS_IMETHOD RemoveObserver(nsIRDFObserver *aObserver);

  /* nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode); */
  NS_IMETHOD ArcLabelsIn(nsIRDFNode *aNode,
                         nsISimpleEnumerator **_retval);

  /* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
  NS_IMETHOD ArcLabelsOut(nsIRDFResource *aSource,
                          nsISimpleEnumerator **_retval);

  /* nsISimpleEnumerator GetAllResources (); */
  NS_IMETHOD GetAllResources(nsISimpleEnumerator **_retval);

  /* void Flush (); */
  NS_IMETHOD Flush();

  /* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
  NS_IMETHOD GetAllCommands(nsIRDFResource *aSource,
                            nsIEnumerator **_retval);

  /* nsISimpleEnumerator GetAllCmds (in nsIRDFResource aSource); */
  NS_IMETHOD GetAllCmds(nsIRDFResource *aSource,
                            nsISimpleEnumerator **_retval);

  /* boolean IsCommandEnabled (in nsISupportsArray aSources,
     in nsIRDFResource aCommand,
     in nsISupportsArray aArguments); */
  NS_IMETHOD IsCommandEnabled(nsISupportsArray *aSources,
                              nsIRDFResource *aCommand,
                              nsISupportsArray *aArguments,
                              PRBool *_retval);

  /* void DoCommand (in nsISupportsArray aSources,
     in nsIRDFResource aCommand,
     in nsISupportsArray aArguments); */
  NS_IMETHOD DoCommand(nsISupportsArray *aSources,
                       nsIRDFResource *aCommand,
                       nsISupportsArray *aArguments);

  NS_IMETHOD SetWindow(nsIDOMWindow *window);

  NS_IMETHOD Change(nsIRDFResource*, nsIRDFResource*,
                    nsIRDFNode*, nsIRDFNode*)
        {return NS_ERROR_NOT_IMPLEMENTED;}
    
  NS_IMETHOD Move(nsIRDFResource*, nsIRDFResource*,
                  nsIRDFResource*, nsIRDFNode*)
        {return NS_ERROR_NOT_IMPLEMENTED;}
    
 protected:
    char *mURI;

	nsIRDFService *getRDFService();
	static PRBool assertEnumFunc(void *aElement, void *aData);
	static PRBool unassertEnumFunc(void *aElement, void *aData);
	nsresult  NotifyObservers(nsIRDFResource *subject, nsIRDFResource *property,
								nsIRDFNode *object, PRBool assert);

 private:

    nsresult getNodeByURI(const char* uri, nsIDOMNode** aResult);
    nsresult getURIForNode(nsIDOMNode *node, char **uri);
    
    PRBool init;
    nsIRDFService *mRDFService;
    nsVoidArray *mObservers;

    nsCOMPtr<nsIDOMDocument> mDocument;

    nsIRDFResource* kNC_Name;
    nsIRDFResource* kNC_Value;
    nsIRDFResource* kNC_Type;
    nsIRDFResource* kNC_Child;
};

nsresult
NS_NewRDFDOMDataSource(nsISupports* aOuter,
                       const nsIID& iid,
                       void **result);

#endif
