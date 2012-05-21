/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __gen_nsRDFDOMDataSource_h__
#define __gen_nsRDFDOMDataSource_h__

#include "nsRDFDOMDataSource.h"

/* starting interface:    nsRDFDOMDataSource */

/* {0F78DA58-8321-11d2-8EAC-00805F29F370} */
#define NS_IRDFDATASOURCE_IID_STR "0F78DA58-8321-11d2-8EAC-00805F29F370"
#define NS_IRDFDATASOURCE_IID \
  {0x0F78DA58, 0x8321, 0x11d2, \
    { 0x8E, 0xAC, 0x00, 0x80, 0x5F, 0x29, 0xF3, 0x70 }}

class nsRDFDOMDataSource : public nsISupports {
 public: 
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IRDFDATASOURCE_IID)

  /* void Init (in string uri); */
  NS_IMETHOD Init(const char *uri) = 0;

  /* readonly attribute string URI; */
  NS_IMETHOD GetURI(char * *aURI) = 0;

  /* nsIRDFResource GetSource (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, bool aTruthValue, nsIRDFResource **_retval) = 0;

  /* nsISimpleEnumerator GetSources (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, bool aTruthValue, nsISimpleEnumerator **_retval) = 0;

  /* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
  NS_IMETHOD GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, bool aTruthValue, nsIRDFNode **_retval) = 0;

  /* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
  NS_IMETHOD GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, bool aTruthValue, nsISimpleEnumerator **_retval) = 0;

  /* void Assert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, bool aTruthValue) = 0;

  /* void Unassert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget); */
  NS_IMETHOD Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget) = 0;

  /* boolean HasAssertion (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, bool aTruthValue, bool *_retval) = 0;

  /* void AddObserver (in nsIRDFObserver aObserver); */
  NS_IMETHOD AddObserver(nsIRDFObserver *aObserver) = 0;

  /* void RemoveObserver (in nsIRDFObserver aObserver); */
  NS_IMETHOD RemoveObserver(nsIRDFObserver *aObserver) = 0;

  /* nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode); */
  NS_IMETHOD ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval) = 0;

  /* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
  NS_IMETHOD ArcLabelsOut(nsIRDFResource *aSource, nsISimpleEnumerator **_retval) = 0;

  /* nsISimpleEnumerator GetAllResources (); */
  NS_IMETHOD GetAllResources(nsISimpleEnumerator **_retval) = 0;

  /* void Flush (); */
  NS_IMETHOD Flush() = 0;

  /* nsISimpleEnumerator GetAllCmds (in nsIRDFResource aSource); */
  NS_IMETHOD GetAllCmds(nsIRDFResource *aSource, nsISimpleEnumerator **_retval) = 0;

  /* boolean IsCommandEnabled (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
  NS_IMETHOD IsCommandEnabled(nsISupportsArray * aSources, nsIRDFResource *aCommand, nsISupportsArray * aArguments, bool *_retval) = 0;

  /* void DoCommand (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
  NS_IMETHOD DoCommand(nsISupportsArray * aSources, nsIRDFResource *aCommand, nsISupportsArray * aArguments) = 0;

  /* void beginUpdateBatch (); */
  NS_IMETHOD BeginUpdateBatch() = 0;

  /* void endUpdateBatch (); */
  NS_IMETHOD EndUpdateBatch() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsRDFDOMDataSource, NS_IRDFDATASOURCE_IID)

#endif /* __gen_nsRDFDOMDataSource_h__ */
