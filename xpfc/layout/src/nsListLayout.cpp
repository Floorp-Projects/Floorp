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

/*
 * Note that throughout this code we assume (from a performance perspective) 
 * that the number of widgets being laid out is very small (< 50).  If this 
 * assumption does not hold, the massive while loops found in this logic 
 * should be optimized to be one for loop.
 *
 * This code is recommended as a clean way to layout small numbers of
 * containers holding objects, in an extensible way.  For large lists,
 * use widgets geared towards them.
 */

#include "nsxpfcCIID.h"

#include "nsListLayout.h"
#include "nsIXPFCCanvas.h"

#define DEFAULT_CELL_HEIGHT 50

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kListLayoutCID, NS_LISTLAYOUT_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID,   NS_IXPFC_CANVAS_IID);

nsListLayout :: nsListLayout() : nsLayout()
{  
  NS_INIT_REFCNT();
  mContainer        = nsnull;

  mCellHeight = DEFAULT_CELL_HEIGHT;
}

nsListLayout :: ~nsListLayout()
{
  NS_IF_RELEASE(mContainer);
}


nsresult nsListLayout::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kListLayoutCID);                         
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
  return (nsLayout::QueryInterface(aIID,aInstancePtr)); 
}

NS_IMPL_ADDREF(nsListLayout)
NS_IMPL_RELEASE(nsListLayout)

nsresult nsListLayout :: Init()
{
  return NS_OK ;
}

nsresult nsListLayout :: Init(nsIXPFCCanvas * aContainer)
{
  NS_IF_RELEASE(mContainer);
  mContainer = aContainer;
  NS_ADDREF(mContainer);
  return NS_OK ;
}

nsresult nsListLayout :: Layout()
{
  if (!mContainer)
    return NS_OK;

  LayoutContainer();

  return NS_OK ;


}

nsresult nsListLayout :: LayoutContainer()
{
  /*
   * The list container will arrange icons in a specified size
   * depending on whether we are using small or large images.
   *
   * In the future, we should stretch blit the icons to fit the 
   * desired size.
   */

  nsIIterator * iterator ;
  nscoord startx, starty;
  nsRect rect, canvas_rect;
  nsIXPFCCanvas * canvas ;
  PRUint32 count = 0;

  mContainer->GetBounds(rect);

  starty = rect.y;
  startx = rect.x;

  CreateIterator(&iterator);

  iterator->Init();

  if (iterator->Count() == 0)
  {
    NS_RELEASE(iterator);
    return NS_OK;
  }

  iterator->First();

  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();

    canvas_rect.x = startx;
    canvas_rect.width = rect.width;
    canvas_rect.height = mCellHeight;
    canvas_rect.y = starty + mCellHeight * count;

    canvas->SetBounds(canvas_rect);

    count++;

    iterator->Next();
  }

  NS_RELEASE(iterator);

  return NS_OK ;
}

nsresult nsListLayout :: CreateIterator(nsIIterator ** aIterator)
{
  return (mContainer->CreateIterator(aIterator));
}


