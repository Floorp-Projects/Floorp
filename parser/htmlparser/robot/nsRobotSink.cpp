/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
#include "nsIRobotSink.h"
#include "nsIRobotSinkObserver.h"
#include "nsIParserNode.h"
#include "nsParserTypes.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsVoidArray.h"
class nsIDocument;

// TODO
// - add in base tag support
// - get links from other sources:
//      - LINK tag
//      - STYLE SRC
//      - IMG SRC
//      - LAYER SRC

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTMLCONTENTSINK_IID);
static NS_DEFINE_IID(kIRobotSinkIID, NS_IROBOTSINK_IID);

class RobotSink : public nsIRobotSink {
public:
  RobotSink();
  ~RobotSink();

  void* operator new(size_t size) {
    void* rv = ::operator new(size);
    nsCRT::zero(rv, size);
    return (void*) rv;
  }

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIHTMLContentSink
  virtual PRInt32 SetTitle(const nsString& aValue);
  virtual PRInt32 OpenHTML(const nsIParserNode& aNode);
  virtual PRInt32 CloseHTML(const nsIParserNode& aNode);
  virtual PRInt32 OpenHead(const nsIParserNode& aNode);
  virtual PRInt32 CloseHead(const nsIParserNode& aNode);
  virtual PRInt32 OpenBody(const nsIParserNode& aNode);
  virtual PRInt32 CloseBody(const nsIParserNode& aNode);
  virtual PRInt32 OpenForm(const nsIParserNode& aNode);
  virtual PRInt32 CloseForm(const nsIParserNode& aNode);
  virtual PRInt32 OpenFrameset(const nsIParserNode& aNode);
  virtual PRInt32 CloseFrameset(const nsIParserNode& aNode);
  virtual PRInt32 OpenContainer(const nsIParserNode& aNode);
  virtual PRInt32 CloseContainer(const nsIParserNode& aNode);
  virtual PRInt32 CloseTopmostContainer();
  virtual PRInt32 AddLeaf(const nsIParserNode& aNode);
  virtual void WillBuildModel(void) { }
  virtual void DidBuildModel(PRInt32 aQualityLevel) { }
  virtual void WillInterrupt(void) { }
  virtual void WillResume(void) { }

  // nsIRobotSink
  NS_IMETHOD Init(nsIURL* aDocumentURL);
  NS_IMETHOD AddObserver(nsIRobotSinkObserver* aObserver);
  NS_IMETHOD RemoveObserver(nsIRobotSinkObserver* aObserver);

  void ProcessLink(const nsString& aLink);

protected:
  nsIURL* mDocumentURL;
  nsVoidArray mObservers;
};

nsresult NS_NewRobotSink(nsIRobotSink** aInstancePtrResult)
{
  RobotSink* it = new RobotSink();
  return it->QueryInterface(kIRobotSinkIID, (void**) aInstancePtrResult);
}

RobotSink::RobotSink()
{
}

RobotSink::~RobotSink()
{
  NS_IF_RELEASE(mDocumentURL);
  PRInt32 i, n = mObservers.Count();
  for (i = 0; i < n; i++) {
    nsIRobotSinkObserver* cop = (nsIRobotSinkObserver*)mObservers.ElementAt(i);
    NS_RELEASE(cop);
  }
}  

NS_IMPL_ADDREF(RobotSink);

NS_IMPL_RELEASE(RobotSink);

NS_IMETHODIMP RobotSink::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIRobotSinkIID)) {
    *aInstancePtr = (void*) this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIHTMLContentSinkIID)) {
    *aInstancePtr = (void*) this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

PRInt32 RobotSink::SetTitle(const nsString& aValue)
{
  return kNoError;
}

PRInt32 RobotSink::OpenHTML(const nsIParserNode& aNode)
{
  return kNoError;
}

PRInt32 RobotSink::CloseHTML(const nsIParserNode& aNode)
{
  return kNoError;
}

PRInt32 RobotSink::OpenHead(const nsIParserNode& aNode)
{
  return kNoError;
}

PRInt32 RobotSink::CloseHead(const nsIParserNode& aNode)
{
  return kNoError;
}

PRInt32 RobotSink::OpenBody(const nsIParserNode& aNode)
{
  return kNoError;
}

PRInt32 RobotSink::CloseBody(const nsIParserNode& aNode)
{
  return kNoError;
}

PRInt32 RobotSink::OpenForm(const nsIParserNode& aNode)
{
  return kNoError;
}

PRInt32 RobotSink::CloseForm(const nsIParserNode& aNode)
{
  return kNoError;
}

PRInt32 RobotSink::OpenFrameset(const nsIParserNode& aNode)
{
  return kNoError;
}

PRInt32 RobotSink::CloseFrameset(const nsIParserNode& aNode)
{
  return kNoError;
}

PRInt32 RobotSink::OpenContainer(const nsIParserNode& aNode)
{
  nsAutoString tmp(aNode.GetText());
  tmp.ToUpperCase();
  if (tmp.Equals("A")) {
    nsAutoString k, v;
    PRInt32 ac = aNode.GetAttributeCount();
    for (PRInt32 i = 0; i < ac; i++) {
      // Get upper-cased key
      const nsString& key = aNode.GetKeyAt(i);
      k.Truncate();
      k.Append(key);
      k.ToUpperCase();
      if (k.Equals("HREF")) {
        // Get value and remove mandatory quotes
        v.Truncate();
        v.Append(aNode.GetValueAt(i));
        PRUnichar first = v.First();
        if ((first == '"') || (first == '\'')) {
          if (v.Last() == first) {
            v.Cut(0, 1);
            PRInt32 pos = v.Length() - 1;
            if (pos >= 0) {
              v.Cut(pos, 1);
            }
          } else {
            // Mismatched quotes - leave them in
          }
        }
        ProcessLink(v);
      }
    }
  }
  return kNoError;
}

PRInt32 RobotSink::CloseContainer(const nsIParserNode& aNode)
{
  return kNoError;
}

PRInt32 RobotSink::CloseTopmostContainer()
{
  return kNoError;
}

PRInt32 RobotSink::AddLeaf(const nsIParserNode& aNode)
{
  return kNoError;
}

NS_IMETHODIMP RobotSink::Init(nsIURL* aDocumentURL)
{
  NS_IF_RELEASE(mDocumentURL);
  mDocumentURL = aDocumentURL;
  NS_IF_ADDREF(aDocumentURL);
  return NS_OK;
}

NS_IMETHODIMP RobotSink::AddObserver(nsIRobotSinkObserver* aObserver)
{
  if (mObservers.AppendElement(aObserver)) {
    NS_ADDREF(aObserver);
    return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP RobotSink::RemoveObserver(nsIRobotSinkObserver* aObserver)
{
  if (mObservers.RemoveElement(aObserver)) {
    NS_RELEASE(aObserver);
    return NS_OK;
  }
  //XXX return NS_ERROR_NOT_FOUND;
  return NS_OK;
}

void RobotSink::ProcessLink(const nsString& aLink)
{
  nsAutoString absURLSpec(aLink);

  // Make link absolute
  // XXX base tag handling
  nsIURL* docURL = mDocumentURL;
  if (nsnull != docURL) {
    nsIURL* absurl;
    nsresult rv = NS_NewURL(&absurl, docURL, aLink);
    if (NS_OK == rv) {
      absURLSpec.Truncate();
      absurl->ToString(absURLSpec);
      NS_RELEASE(absurl);
    }
  }

  // Now give link to robot observers
  PRInt32 i, n = mObservers.Count();
  for (i = 0; i < n; i++) {
    nsIRobotSinkObserver* cop = (nsIRobotSinkObserver*)mObservers.ElementAt(i);
    cop->ProcessLink(absURLSpec);
  }
}

