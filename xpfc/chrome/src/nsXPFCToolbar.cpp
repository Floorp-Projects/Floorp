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

#include "nsXPFCToolbar.h"
#include "nsxpfcCIID.h"
#include "nspr.h"
#include "nsBoxLayout.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCXPFCToolbarCID, NS_XPFC_TOOLBAR_CID);
static NS_DEFINE_IID(kCIXPFCToolbarIID, NS_IXPFC_TOOLBAR_IID);

#define DEFAULT_WIDTH  50
#define DEFAULT_HEIGHT 50

#define DEFAULT_GAP 2

nsXPFCToolbar :: nsXPFCToolbar(nsISupports* outer) : nsXPFCCanvas(outer)
{
  NS_INIT_REFCNT();
}

nsXPFCToolbar :: ~nsXPFCToolbar()
{
}

nsresult nsXPFCToolbar::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCXPFCToolbarCID);                         
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsXPFCToolbar *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kCIXPFCToolbarIID)) {                                          
    *aInstancePtr = (void*) (nsIXPFCToolbar *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return (nsXPFCCanvas::QueryInterface(aIID, aInstancePtr));
}

NS_IMPL_ADDREF(nsXPFCToolbar)
NS_IMPL_RELEASE(nsXPFCToolbar)

nsresult nsXPFCToolbar :: Init()
{
  nsresult res = nsXPFCCanvas::Init();

  ((nsBoxLayout *)GetLayout())->SetHorizontalGap(DEFAULT_GAP);
  ((nsBoxLayout *)GetLayout())->SetVerticalGap(DEFAULT_GAP);

  return (res);    
}

nsresult nsXPFCToolbar :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsXPFCCanvas::SetParameter(aKey, aValue));
}

nsresult nsXPFCToolbar :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}

