/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsxpfcCIID.h"

#include "nsLayout.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kLayoutCID, NS_LAYOUT_CID);

nsLayout :: nsLayout()
{
  NS_INIT_REFCNT();
}

nsLayout :: ~nsLayout()
{
}


nsresult nsLayout::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kLayoutCID);                         
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return NS_NOINTERFACE;                                                 
}

NS_IMPL_ADDREF(nsLayout)
NS_IMPL_RELEASE(nsLayout)

nsresult nsLayout :: Init()
{
  return NS_OK ;
}

nsresult nsLayout :: Layout()
{
  return NS_OK ;
}


nsresult nsLayout :: PreferredSize(nsSize &aSize)
{
  return NS_OK ;
}
nsresult nsLayout :: MinimumSize(nsSize &aSize)
{
  return NS_OK ;
}
nsresult nsLayout :: MaximumSize(nsSize &aSize)
{
  return NS_OK ;
}
PRUint32 nsLayout :: GetHorizontalGap()
{
  return 0 ;
}
PRUint32 nsLayout :: GetVerticalGap()
{
  return 0 ;
}
void nsLayout :: SetHorizontalGap(PRUint32 aGapSize)
{
  return;
}
void nsLayout :: SetVerticalGap(PRUint32 aGapSize)
{
  return;
}

PRFloat64 nsLayout :: GetHorizontalFill()
{
  return 1.0 ;
}
PRFloat64 nsLayout :: GetVerticalFill()
{
  return 1.0 ;
}
void nsLayout :: SetHorizontalFill(PRFloat64 aFillSize)
{
  return;
}
void nsLayout :: SetVerticalFill(PRFloat64 aFillSize)
{
  return;
}

