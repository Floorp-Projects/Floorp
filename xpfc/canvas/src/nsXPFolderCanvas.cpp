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

#include "nsXPFolderCanvas.h"
#include "nsxpfcCIID.h"
#include "nsIDeviceContext.h"
#include "nsFont.h"
#include "nsIFontMetrics.h"
#include "nspr.h"
#include "nsxpfcstrings.h"
#include "nsListLayout.h"

static NS_DEFINE_IID(kCListLayoutCID,   NS_LISTLAYOUT_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kXPFolderCanvasCID, NS_XP_FOLDER_CANVAS_CID);

#define DEFAULT_WIDTH  50
#define DEFAULT_HEIGHT 50

nsXPFolderCanvas :: nsXPFolderCanvas(nsISupports* outer) : nsXPFCCanvas(outer)
{
  NS_INIT_REFCNT();
}

nsXPFolderCanvas :: ~nsXPFolderCanvas()
{
}

nsresult nsXPFolderCanvas::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kXPFolderCanvasCID);
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsXPFolderCanvas *)this;                                        
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

NS_IMPL_ADDREF(nsXPFolderCanvas)
NS_IMPL_RELEASE(nsXPFolderCanvas)

nsresult nsXPFolderCanvas :: Init()
{  
  return (nsXPFCCanvas::Init());
}

nsEventStatus nsXPFolderCanvas :: OnResize(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  return (nsXPFCCanvas::OnResize(aX, aY, aWidth, aHeight));
}

nsresult nsXPFolderCanvas :: SetBounds(const nsRect &aBounds)
{
  return (nsXPFCCanvas::SetBounds(aBounds));
}


nsEventStatus nsXPFolderCanvas :: OnPaint(nsIRenderingContext& aRenderingContext,
                                          const nsRect& aDirtyRect)

{
  return (nsXPFCCanvas::OnPaint(aRenderingContext,aDirtyRect));
}

nsEventStatus nsXPFolderCanvas :: HandleEvent(nsGUIEvent *aEvent)
{
  return (nsXPFCCanvas::HandleEvent(aEvent));
}

nsresult nsXPFolderCanvas :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}


nsresult nsXPFolderCanvas :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsXPFCCanvas::SetParameter(aKey,aValue));
}

nsresult nsXPFolderCanvas :: CreateDefaultLayout()
{
  nsresult res = NS_OK;

  nsListLayout * layout ;
  
  res = nsRepository::CreateInstance(kCListLayoutCID, 
                                     nsnull, 
                                     kCListLayoutCID, 
                                     (void **)&layout);

  if (NS_OK != res)
    return res ;

  SetLayout(layout);

  layout->Init(this);

  return res;
}
