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

#include "nsBoxLayout.h"
#include "nsIXPFCCanvas.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kBoxLayoutCID, NS_BOXLAYOUT_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID,   NS_IXPFC_CANVAS_IID);

nsBoxLayout :: nsBoxLayout() : nsLayout()
{  
  NS_INIT_REFCNT();
  mContainer        = nsnull;
  mLayoutAlignment  = eLayoutAlignment_horizontal;
  mVerticalGap      = 0;
  mHorizontalGap    = 0;
  mVerticalFill     = 1.0;
  mHorizontalFill   = 1.0;
}

nsBoxLayout :: ~nsBoxLayout()
{
}


nsresult nsBoxLayout::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kBoxLayoutCID);                         
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

NS_IMPL_ADDREF(nsBoxLayout)
NS_IMPL_RELEASE(nsBoxLayout)

nsresult nsBoxLayout :: Init()
{
  return NS_OK ;
}

nsresult nsBoxLayout :: Init(nsIXPFCCanvas * aContainer)
{
  mContainer = aContainer;
  return NS_OK ;
}

nsresult nsBoxLayout :: Layout()
{
  if (!mContainer)
    return NS_OK;

  LayoutContainer();

  LayoutChildren();

  return NS_OK ;


}

nsresult nsBoxLayout :: LayoutContainer()
{
  nsIIterator * iterator ;
  nsIXPFCCanvas * canvas ;
  nsRect rect;
  PRUint32 width, height;
  PRUint32 wsize, hsize;
  PRUint32 count = 0;
  PRBool bFloater = PR_FALSE;
  PRInt32 spaceleft = 0;
  PRInt32 space_per_object = 0;
  PRUint32 totalfloaters = 0;
  PRUint32 start = 0;
  nsSize prefSize;
  nsSize minSize;
  nsSize maxSize;
  PRUint32 startX = 0, startY = 0;

  // Default case is everyone gets equal space  
  mContainer->GetBounds(rect);


  // Offset by the gaps
  rect.x += mVerticalGap;
  rect.width-=(2*mVerticalGap);
  rect.y += mHorizontalGap;
  rect.height-=(2*mHorizontalGap);


  wsize = width = rect.width;
  hsize = height = rect.height;
  
  // Iterate through the children
  CreateIterator(&iterator);

  iterator->Init();

  if (iterator->Count() == 0)
  {
    NS_RELEASE(iterator);
    return NS_OK;
  }

  if (GetLayoutAlignment() == eLayoutAlignment_horizontal) {
    wsize  /= iterator->Count();
    start = rect.x;
  } else {
    hsize  /=  iterator->Count();
    start = rect.y;
  }

  startY = rect.y;
  startX = rect.x;

  /*
   * First, we need to see if there is a floater so that preferred sizes
   * can be used
   */

  bFloater = CanLayoutUsingPreferredSizes(iterator);

  /*
   * If we have atleast one floater, all the non-floaters will get their preferred
   * sizes and the floaters will then *deal* with the remaining space. So, compute
   * the remaining space here.
   *
   * Then, we lay out all widgets with a preferred size using that size.  The
   * remaining widgets will get the average size which is the remaining space
   * divided by the number of floating objects
   */

  if (bFloater == PR_TRUE) {
  
    spaceleft = ComputeFreeFloatingSpace(iterator);

    totalfloaters = QueryNumberFloatingWidgets(iterator);

    /*
     * XXX Need to deal with case where space left is not enough.
     *     See ComputeFreeFloatingSpace at bottom for comment.
     *
     * for now, just fallback on equal layout for all widgets
     */

    /*
     * If spaceleft is negative, then no floaters are visible and we should
     * give each canvas its preferred size. Some may not be visible.
     */

    if (spaceleft < 0) {
      bFloater = PR_FALSE;
    } else {
      space_per_object = spaceleft / totalfloaters;
    }
  }


  iterator->First();

  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();

    if (GetLayoutAlignment() == eLayoutAlignment_horizontal) {

      if (bFloater == PR_FALSE)
      {

         if (canvas->HasPreferredSize() == PR_TRUE) {
      
            canvas->GetPreferredSize(prefSize);

            rect.x = start;
            rect.width = prefSize.width;

            start += rect.width;

         } else {

          rect.x = start;
          rect.width = 0;

         }

//        rect.x = wsize * count;
//        rect.width = wsize;

      } else {

        /*
         * In this case, if we have a preferred size, use it.
         * If no preferred size, use the weighted size if it
         * falls within min/max range, else use min/max
         */

         if (canvas->HasPreferredSize() == PR_TRUE) {
      
            canvas->GetPreferredSize(prefSize);

            rect.x = start;
            rect.width = prefSize.width;

            start += rect.width;

         } 
         else {

            /*
             * check to see that space_per_object is between min and max
             * and if so, use it else use the min or max
             */

            PRBool bSetToMinMax = PR_FALSE;

            if (canvas->HasMinimumSize() == PR_TRUE) {
  
              canvas->GetMinimumSize(minSize);

              if (space_per_object < minSize.width) {

                rect.x = start;
                rect.width = minSize.width;

                start += rect.width;

                bSetToMinMax = PR_TRUE;

              }

            }

            if (bSetToMinMax == PR_FALSE && canvas->HasMaximumSize() == PR_TRUE) {
  
              canvas->GetMaximumSize(maxSize);

              if (space_per_object > maxSize.width) {

                rect.x = start;
                rect.width = maxSize.width;

                start += rect.width;

              }

            }

            /*
             * Yeah, this object can fill space_per_object
             */

            if (bSetToMinMax == PR_FALSE) {

              rect.x = start;
              rect.width = space_per_object;

              start += rect.width;

            }

         }

      }

      rect.height = (PRInt32) (hsize * mHorizontalFill) ;
      rect.y = (((PRUint32)(height - rect.height)) >> 2) + startY ;
      
      canvas->SetBounds(rect);

    } else  {

      if (bFloater == PR_FALSE)
      {
         if (canvas->HasPreferredSize() == PR_TRUE) {
      
            canvas->GetPreferredSize(prefSize);

            rect.y = start;
            rect.height = prefSize.height;

            start += rect.height;

         } else {

          rect.y = start;
          rect.height = 0;

         }

//        rect.y = hsize * count;
//        rect.height = hsize;

      } else {

        /*
         * In this case, if we have a preferred size, use it.
         * If no preferred size, use the weighted size if it
         * falls within min/max range, else use min/max
         */

         if (canvas->HasPreferredSize() == PR_TRUE) {
      
            canvas->GetPreferredSize(prefSize);

            rect.y = start;
            rect.height = prefSize.height;

            start += rect.height;

         } 
         else {

            /*
             * check to see that space_per_object is between min and max
             * and if so, use it else use the min or max
             */

            PRBool bSetToMinMax = PR_FALSE;

            if (canvas->HasMinimumSize() == PR_TRUE) {
  
              canvas->GetMinimumSize(minSize);

              if (space_per_object < minSize.height) {

                rect.y = start;
                rect.height = minSize.height;

                start += rect.height;

                bSetToMinMax = PR_TRUE;

              }

            }

            if (bSetToMinMax == PR_FALSE && canvas->HasMaximumSize() == PR_TRUE) {
  
              canvas->GetMaximumSize(maxSize);

              if (space_per_object > maxSize.height) {

                rect.y = start;
                rect.height = maxSize.height;

                start += rect.height;

              }

            }

            /*
             * Yeah, this object can fill space_per_object
             */

            if (bSetToMinMax == PR_FALSE) {

              rect.y = start;
              rect.height = space_per_object;

              start += rect.height;

            }

         }

      }
      
      rect.width = (PRInt32) (wsize * mVerticalFill) ;
      rect.x = (((PRUint32)(width - rect.width)) >> 2) + startX ;

      canvas->SetBounds(rect);
    } 

    count++;
    iterator->Next();
  }

  NS_RELEASE(iterator);

  return NS_OK ;

}

nsresult nsBoxLayout :: LayoutChildren()
{
  nsIIterator * iterator ;
  nsIXPFCCanvas * canvas;
  nsresult res ;

  // Iterate through the children
  CreateIterator(&iterator);

  iterator->Init();


  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();

    canvas->Layout();

    iterator->Next();
  }

  NS_RELEASE(iterator);

  return NS_OK ;

}

PRBool nsBoxLayout :: CanLayoutUsingPreferredSizes(nsIIterator * aIterator)
{

  nsresult res;
  nsIXPFCCanvas * canvas;

  aIterator->First();

  while(!(aIterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) aIterator->CurrentItem();
    
    if (canvas->HasPreferredSize() == PR_FALSE)
      return PR_TRUE;

    aIterator->Next();
  }

  return PR_FALSE;
}

PRUint32 nsBoxLayout :: QueryNumberFloatingWidgets(nsIIterator * aIterator)
{

  nsresult res;
  nsIXPFCCanvas * canvas;
  PRUint32 count = 0;

  aIterator->First();

  while(!(aIterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) aIterator->CurrentItem();

    if (canvas->HasPreferredSize() == PR_FALSE) {

      count ++;

    }

    aIterator->Next();
  }

  return count;
}

/*
 * Compute free floating space.  Start with full area and then subtract 
 * the preferred space of every absolutely positioned widget.
 */

PRInt32 nsBoxLayout :: ComputeFreeFloatingSpace(nsIIterator * aIterator)
{

  nsresult res;
  nsIXPFCCanvas * canvas;
  PRInt32 space = 0;
  PRInt32 space_per_object = 0;
  nsRect rect;
  nsSize prefSize;
  nsSize minSize;
  nsSize maxSize;
  PRUint32 totalfloaters = 0;

  /*
   * compute overall layout size
   */

  mContainer->GetBounds(rect);
  if (GetLayoutAlignment() == eLayoutAlignment_horizontal) {
    space  = rect.width;
  } else {
    space  = rect.height;
  }
  
  /*
   * subtract off all the preferred sizes
   */

  aIterator->First();

  while(!(aIterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) aIterator->CurrentItem();

    if (canvas->HasPreferredSize() == PR_TRUE) 
    {  
      canvas->GetPreferredSize(prefSize);

      if (GetLayoutAlignment() == eLayoutAlignment_horizontal) 
      {
        space  -= prefSize.width;
      } 
      else 
      {
        space  -= prefSize.height;
      }
        
    }

    aIterator->Next();
  }


  /*
   * Now compute the temporary space per object
   */

  totalfloaters = QueryNumberFloatingWidgets(aIterator);

  space_per_object = space / totalfloaters;

  /*
   * Ok, so now we know how to lay out preferred size widgets.
   * Normally, the floating widgets would use 'spaceleft' space,
   * but that amount *might* fall outside their preferred ranges.
   * When it does fall outside, we can set that widget to the 
   * specified min/max, but the space remaining will be altered,
   * so compute that now
   */


  aIterator->First();

  while(!(aIterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) aIterator->CurrentItem();
    
      if (canvas->HasPreferredSize() == PR_FALSE) {

      /*
       * Check to see if the space assigned for this widget
       * falls within it's min/max range
       */

      if (canvas->HasMinimumSize() == PR_TRUE) {
        
        canvas->GetMinimumSize(minSize);

        if (GetLayoutAlignment() == eLayoutAlignment_horizontal) {

          if (space_per_object < minSize.width)
            space  -= (minSize.width - space_per_object);

        } else {

          if (space_per_object < minSize.height)
            space  -= (minSize.height - space_per_object);

        }
        
      }

      if (canvas->HasMaximumSize() == PR_TRUE) {

        canvas->GetMaximumSize(maxSize);

        if (GetLayoutAlignment() == eLayoutAlignment_horizontal) {

          if (space_per_object > maxSize.width)
            space  += (space_per_object - maxSize.width);

        } else {

          if (space_per_object < maxSize.height)
            space  += (space_per_object - maxSize.height);

        }

      }
           
    }


    aIterator->Next();
  }


  /*
   * At this point, if space is positive, we are ready to lay out.
   * If space is negative, we need to shrink the floaters which have 
   * a maximum specified by the amount we are negative by the total 
   * number of floaters (it's getting ugly, maybe we can clean up later)
   *
   * We'll leave that logic for the actual layout
   */


  return space;
}


nsresult nsBoxLayout :: CreateIterator(nsIIterator ** aIterator)
{
  return (mContainer->CreateIterator(aIterator));
}


nsresult nsBoxLayout :: PreferredSize(nsSize &aSize)
{
  return NS_OK ;
}
nsresult nsBoxLayout :: MinimumSize(nsSize &aSize)
{
  return NS_OK ;
}
nsresult nsBoxLayout :: MaximumSize(nsSize &aSize)
{
  return NS_OK ;
}

PRFloat64 nsBoxLayout :: GetHorizontalFill()
{
  return mHorizontalFill ;
}
PRFloat64 nsBoxLayout :: GetVerticalFill()
{
  return mVerticalFill ;
}
void nsBoxLayout :: SetHorizontalFill(PRFloat64 aFillSize)
{
  mHorizontalFill = aFillSize;
}
void nsBoxLayout :: SetVerticalFill(PRFloat64 aFillSize)
{
  mVerticalFill = aFillSize;
}

PRUint32 nsBoxLayout :: GetHorizontalGap()
{
  return mHorizontalGap ;
}
PRUint32 nsBoxLayout :: GetVerticalGap()
{
  return mVerticalGap ;
}
void nsBoxLayout :: SetHorizontalGap(PRUint32 aGapSize)
{
  mHorizontalGap = aGapSize;
}
void nsBoxLayout :: SetVerticalGap(PRUint32 aGapSize)
{
  mVerticalGap = aGapSize;
}

void nsBoxLayout :: SetLayoutAlignment(nsLayoutAlignment aLayoutAlignment)
{
  mLayoutAlignment = aLayoutAlignment;
}

nsLayoutAlignment nsBoxLayout :: GetLayoutAlignment()
{
  return mLayoutAlignment;
}
