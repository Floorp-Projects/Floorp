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

#include "nsXPFCDialog.h"
#include "nsxpfcCIID.h"
#include "nspr.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCXPFCDialogCID, NS_XPFC_DIALOG_CID);
static NS_DEFINE_IID(kCIXPFCDialogIID, NS_IXPFC_DIALOG_IID);

#define DEFAULT_WIDTH  300
#define DEFAULT_HEIGHT 300

nsXPFCDialog :: nsXPFCDialog(nsISupports* outer) : nsXPFCCanvas(outer)
{
  NS_INIT_REFCNT();
}

nsXPFCDialog :: ~nsXPFCDialog()
{
}

nsresult nsXPFCDialog::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCXPFCDialogCID);                         
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsXPFCDialog *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kCIXPFCDialogIID)) {                                          
    *aInstancePtr = (void*) (nsIXPFCDialog *)this;                                        
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

NS_IMPL_ADDREF(nsXPFCDialog)
NS_IMPL_RELEASE(nsXPFCDialog)

nsresult nsXPFCDialog :: Init()
{
  nsresult res = nsXPFCCanvas::Init();    

  return res;
}

nsresult nsXPFCDialog :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsXPFCCanvas::SetParameter(aKey, aValue));
}

nsresult nsXPFCDialog :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}

