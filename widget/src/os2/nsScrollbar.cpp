/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsScrollbar.h"
#include "nsGUIEvent.h"
#include "nsUnitConversion.h"

NS_IMPL_ADDREF(nsScrollbar)
NS_IMPL_RELEASE(nsScrollbar)

nsresult nsScrollbar::QueryInterface( const nsIID &aIID, void **aInstancePtr)
{
   nsresult result = nsWindow::QueryInterface( aIID, aInstancePtr);

   if( result == NS_NOINTERFACE && aIID.Equals( NS_GET_IID(nsIScrollbar)))
   {
      *aInstancePtr = (void*) ((nsIScrollbar*)this);
      NS_ADDREF_THIS();
      result = NS_OK;
   }

   return result;
}

// Init; window creation
nsScrollbar::nsScrollbar( PRBool aIsVertical) : nsWindow()
{
   mOrientation = aIsVertical ? SBS_VERT : SBS_HORZ;
   mScaleFactor   = 1.0f;
   mLineIncrement = 0;
   mThumbSize = 0;
   mMaxRange = 0;
}

// nsWindow overrides to create the right kind of window
PCSZ nsScrollbar::WindowClass()
{
   return WC_SCROLLBAR;
}

ULONG nsScrollbar::WindowStyle()
{
   return mOrientation | SBS_THUMBSIZE | SBS_AUTOTRACK | BASE_CONTROL_STYLE;
}

//---------------------------------------------------------------------------
// Metrics set/get ----------------------------------------------------------

// Note carefully that the widget concepts of "thumb size" & "range" are not
// the same as the corresponding PM ones; they're much more like (?identical)
// Java's.  Basically the thumb size is not just cosmetic.
//
// This means that changing the thumb size or the max range means the other
// must be changed too.  This code may need some work.

#define ROUND_D(u) NSToIntRound( (u) / mScaleFactor)
#define ROUND_U(u) (PRUint32) NSToIntRound( (u) * mScaleFactor)

void nsScrollbar::SetThumbRange( PRUint32 aEndRange, PRUint32 aSize)
{
   // get current position

   PRUint32 pos;
   GetPosition( pos);

   // set scale factor dependent on max range
   if( aEndRange > 32000)
      mScaleFactor = aEndRange / 32000.0f;

   // set new sizes
   mMaxRange = aEndRange;
   mThumbSize = aSize;

   // set range & pos
   mOS2Toolkit->SendMsg( mWnd, SBM_SETSCROLLBAR,
                         MPFROMSHORT( ROUND_D(pos)),
                         MPFROM2SHORT( 0, ROUND_D(mMaxRange-mThumbSize)));

   // set thumb
   mOS2Toolkit->SendMsg( mWnd, SBM_SETTHUMBSIZE,
                         MPFROM2SHORT( ROUND_D(mThumbSize),
                                       ROUND_D(mMaxRange))); 
}

// Range - 0 to max range
nsresult nsScrollbar::SetMaxRange( PRUint32 aEndRange)
{
   SetThumbRange( aEndRange, mThumbSize);
   return NS_OK;
}

// Thumb size
nsresult nsScrollbar::SetThumbSize( PRUint32 aSize)
{
   SetThumbRange( mMaxRange, aSize);
   return NS_OK;
}

// Thumb position
nsresult nsScrollbar::SetPosition( PRUint32 aPos)
{
   mOS2Toolkit->SendMsg( mWnd, SBM_SETPOS, MPFROMSHORT(ROUND_D( aPos)));
   return NS_OK;
}

NS_IMETHODIMP nsScrollbar::GetPosition( PRUint32 &pos)
{
   MRESULT rc = mOS2Toolkit->SendMsg( mWnd, SBM_QUERYPOS);
   pos = ROUND_U( SHORT1FROMMR(rc));
   return NS_OK;
}

// All the above
nsresult nsScrollbar::SetParameters( PRUint32 aMaxRange, PRUint32 aThumbSize,
                                     PRUint32 aPosition, PRUint32 aLineIncrement)
{
   SetThumbRange( aMaxRange, aThumbSize);
   SetPosition( aPosition);
   SetLineIncrement( aLineIncrement);
   return NS_OK;
}

//-------------------------------------------------------------------------
// Deal with scrollbar messages -------------------------------------------

PRBool nsScrollbar::OnScroll( MPARAM mp1, MPARAM mp2)
{
   PRBool   rc = PR_TRUE;

   USHORT   usCmd = SHORT2FROMMP(mp2);
   int      msg = 0;

   BOOL hasEvent = mEventListener != nsnull || mEventCallback != nsnull;

   PRUint32 newpos, li, ts;

   GetPosition( newpos);
   GetLineIncrement( li);
   GetThumbSize( ts);

   switch( usCmd)
   {
      case SB_LINERIGHT: // == SB_LINEDOWN
      {
         newpos += li;
         msg = NS_SCROLLBAR_LINE_NEXT;
         break;
      }

      case SB_LINELEFT:  // == SB_LINEUP
      {
         if( newpos > li) newpos -= li;
         else newpos = 0;
         msg = NS_SCROLLBAR_LINE_PREV;
         break;
      }

      case SB_PAGERIGHT: // == SB_PAGEDOWN
      {
         newpos += (ts - li);
         msg = NS_SCROLLBAR_PAGE_NEXT;
         break;
      }

      case SB_PAGELEFT:  // == SB_PAGEUP
      {
         PRUint32 delta;
         delta = ts - li;
         if( newpos > delta) newpos -= delta;
         else newpos = 0;
         msg = NS_SCROLLBAR_PAGE_PREV;
         break;
      }

      case SB_SLIDERTRACK:
         newpos = ROUND_U( SHORT1FROMMP(mp2));
         msg = NS_SCROLLBAR_POS;
         break;
   }

   if( msg != 0)
   {
      // Ensure newpos is sensible
      if( newpos > (mMaxRange - mThumbSize))
         newpos = mMaxRange - mThumbSize;

      // if there are listeners, give them a chance to alter newpos
      if( mEventListener != nsnull || mEventCallback != nsnull)
      {
         nsScrollbarEvent event;
         InitEvent( event, msg);
         event.eventStructType = NS_SCROLLBAR_EVENT;
         event.position = newpos;
         DispatchWindowEvent( &event);

         // Ensure position is still sensible
         newpos = event.position;
         if( newpos > (mMaxRange - mThumbSize))
            newpos = mMaxRange - mThumbSize;
      }

      // Now move the scrollbar
      SetPosition( newpos);
   }

   return rc;
}
