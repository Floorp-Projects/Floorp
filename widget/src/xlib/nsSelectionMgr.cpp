/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsSelectionMgr.h"

// XXX BWEEP BWEEP This is ONLY TEMPORARY until the service manager
// has a way of registering instances
// (see http://bugzilla.mozilla.org/show_bug.cgi?id=3509 ).
nsISelectionMgr* theSelectionMgr = 0;

extern "C" NS_EXPORT nsISelectionMgr*
GetSelectionMgr()
{
  if (!theSelectionMgr)
  {
    nsresult result = NS_NewSelectionMgr(&theSelectionMgr);
    NS_ASSERTION(NS_SUCCEEDED(result),"Could not allocate the selection manager");
  }
  return theSelectionMgr;
}
// BWEEP BWEEP

/**
 * Selection Manager for Windows.
 * Owns the copied text, listens for selection request events.
 */

NS_IMPL_ADDREF(nsSelectionMgr)
NS_IMPL_RELEASE(nsSelectionMgr)

nsSelectionMgr::nsSelectionMgr()
{
  NS_INIT_REFCNT();

  // BWEEP see above
  theSelectionMgr = this;
}

nsSelectionMgr::~nsSelectionMgr()
{
}

nsresult nsSelectionMgr::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
  NS_PRECONDITION(aInstancePtrResult, "null pointer");
  if (!aInstancePtrResult) 
  {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsISupports::GetIID())) 
  {
    *aInstancePtrResult = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsISelectionMgr::GetIID())) 
  {
    *aInstancePtrResult = (void*)(nsISelectionMgr*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return !NS_OK;
}

nsresult nsSelectionMgr::GetCopyOStream(ostream** aStream)
{
  return NS_OK;
}


static void PlaceHTMLOnClipboard(PRUint32 aFormat, char* aData, int aLength)
{

}

nsresult nsSelectionMgr::CopyToClipboard()
{
  return NS_OK;
}

nsresult nsSelectionMgr::PasteTextBlocking(nsString* aPastedText)
{
  return NS_OK;
}

nsresult NS_NewSelectionMgr(nsISelectionMgr** aInstancePtrResult)
{
  nsSelectionMgr* sm = new nsSelectionMgr();
  return sm->QueryInterface(nsISelectionMgr::GetIID(),
                            (void**) aInstancePtrResult);
}


