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
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 *
 */

#include "nsTooltipManager.h"
#include "nswindow.h"

// Created 25-9-98 mjf, improvising furiously...

// --> Bank on TTData->cRcls being smallish.
//
// This is probably a bad bet, seeing as the main use will be on a child
// window rendering content with many separate areas of interest (eg. 1
// per image with alt text).
//
// So it may turn out to be a good idea to make this data structure more
// sophisticated -- hash on the coordinates.

// Hashtable defs; key is HWND, element is TTData*
class HWNDHashKey : public nsHashKey
{
   HWND hwnd;

 public:
   HWNDHashKey( HWND h) : hwnd(h) {}

   virtual PRUint32 HashValue() const { return hwnd; }
   virtual PRBool Equals( const nsHashKey *aKey) const
   { return (((HWNDHashKey*)aKey)->hwnd == hwnd) ? PR_TRUE : PR_FALSE; }
   virtual nsHashKey *Clone() const
   { return new HWNDHashKey( hwnd); }
};

#define ID_TT_TIMER (TID_USERMAX-1)

struct TTData
{
   HWND   hwnd;
   RECTL *aRcls;  // could use a region to hold rects, but need to know
   ULONG  cRcls;  // index of rectangle for notifications.
                  // [see large comment above]


   LONG   lIndex; // Index of last rectangle hit. -1 is none.
   LONG   lIndexShowing;
   PFNWP  oldFnWp;
   BOOL   bTimerSet;
   POINTL ptStart;

   TTData( HWND h) : hwnd(h), aRcls(0), cRcls(0), lIndex(-1),
                     lIndexShowing(-1), oldFnWp(0), bTimerSet(0)
   { ptStart.x = 0; ptStart.y = 0; }

  ~TTData()
   { delete [] aRcls; }

   // if cRects is 0, same number as we already have.
   void UpdateTooltips( nsWindow *w, nsRect *rects[], PRUint32 cRects = 0)
   {
      if( cRects > 0)
      {
         delete [] aRcls;
         aRcls = new RECTL [cRects];
         cRcls = cRects;
      }

      // Get bounds once..
      nsRect bounds;
      w->GetBounds( bounds);

      for( ULONG i = 0; i < cRects; i++)
      {
         // convert the ith nsrect to the ith in-ex rectl
         aRcls[i].yTop = bounds.height - rects[i]->y;
         aRcls[i].xLeft = rects[i]->x;
         aRcls[i].xRight = aRcls[i].xLeft + rects[i]->width;
         aRcls[i].yBottom = aRcls[i].yTop - rects[i]->height;
      }
   }

   void StopTimer()
   {
      if( bTimerSet)
      {
         WinStopTimer( 0/*hab*/, hwnd, ID_TT_TIMER);
         bTimerSet = FALSE;
      }
   }

   void StartTimer()
   {
      if( !bTimerSet)
      {
         WinStartTimer( 0/*hab*/, hwnd, ID_TT_TIMER, 1000); // 1 second?
         bTimerSet = TRUE;
      }
   }

   long Test( PPOINTL pt)
   {
      lIndex = -1;
      for( ULONG i = 0; lIndex == -1 && i < cRcls; i++)
         if( WinPtInRect( 0/*hab*/, aRcls + i, pt))
            lIndex = i;
      return lIndex;
   }

   void ShowTooltip()
   {
      NS_ASSERTION( lIndex >= 0, "Invalid tt index");
      if( lIndex != lIndexShowing)
      {
         WinSendMsg( hwnd, WMU_SHOW_TOOLTIP, MPFROMLONG(lIndex), 0);
         lIndexShowing = lIndex;
      }
   }

   void HideTooltip()
   {
      if( lIndexShowing != -1)
      {
         WinSendMsg( hwnd, WMU_HIDE_TOOLTIP, 0, 0);
         lIndexShowing = -1;
      }
   }
};

// subclass to watch for mouse messages
MRESULT EXPENTRY HandleMsg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   return nsTooltipManager::GetManager()->HandleMsg( hwnd, msg, mp1, mp2);
}

#define HOVER_DIST_SQR 100

// Message handler ---------------------------------------------------------
MRESULT nsTooltipManager::HandleMsg( HWND hwnd, ULONG msg,
                                     MPARAM mp1, MPARAM mp2)
{
   MRESULT mRC = 0;

   // find data area
   TTData *pData = GetEntry( hwnd);

   if( pData)
   {
      BOOL bSet = FALSE;

      switch( msg)
      {
         case WMU_MOUSEENTER:
         case WM_MOUSEMOVE:
         {
            // Is point in rectl list?
            // Yes: Is tooltip mode on?
            //      Yes: Show tooltip
            //      No:  Is timer set?
            //           Yes: Is point < HOVER_DIST from startpoint ?
            //                Yes: run away
            //                No:  Stop timer
            //           No:  nop
            //           Record startpoint
            //           Start timer
            // No:  No tooltip here: Stop timer, run away
            pData->StopTimer();
            POINTL pt = { SHORT1FROMMP(mp1), SHORT2FROMMP(mp1) };
            if( pData->Test( &pt) >= 0)
            {
               if( mTTMode == PR_TRUE)
               {
                  pData->ShowTooltip();
               }
               else
               {
                  if( pData->bTimerSet)
                  {
                     // ..approx..
                     long lX = pt.x - pData->ptStart.x;
                     long lY = pt.y - pData->ptStart.y;

                     if( lX * lX + lY * lY < HOVER_DIST_SQR)
                     {
                        break;
                     }
                     else
                     {
                        pData->StopTimer();
                     }
                  }
                  memcpy( &pData->ptStart, &pt, sizeof pt);
                  pData->StartTimer();
               }
            }
            else
            {
               pData->StopTimer();
            }
            break;
         }

         case WM_TIMER:
            // Is it our timer?
            // Yes: Set tooltip mode; cancel timer; show tooltip
            // No:  Run away
            if( SHORT1FROMMP( mp1) == ID_TT_TIMER)
            {
               mTTMode = PR_TRUE;
               pData->StopTimer();
               pData->ShowTooltip();
               bSet = TRUE;
            }
            break;

         case WMU_MOUSELEAVE:
            // Is tooltip mode on?
            // Yes: hide tooltip; Is next window one of ours?
            //                    Yes: do nothing
            //                    No:  unset tooltip mode
            // No:  stop timer
            if( mTTMode)
            {
               pData->HideTooltip();
               if( 0 == GetEntry( HWNDFROMMP(mp2)))
               {
                  mTTMode = PR_FALSE;
               }
            }
            else
            {
               pData->StopTimer();
            }
            break;

         case WM_BUTTON1DOWN:
         case WM_BUTTON2DOWN:
         case WM_BUTTON3DOWN:
            // Is tooltip mode on?
            // Yes: hide tooltip; unset tooltip mode
            // No:  Stop timer
            if( mTTMode)
            {
               pData->HideTooltip();
               mTTMode = PR_FALSE;
            }
            else
            {
               pData->StopTimer();
            }
            break;
      }

      if( !bSet && pData->oldFnWp)
         mRC = (*pData->oldFnWp)( hwnd, msg, mp1, mp2);
   }
   else // ..oops..
      mRC = WinDefWindowProc( hwnd, msg, mp1, mp2);
   return mRC;
}

// Manager methods ---------------------------------------------------------
TTData *nsTooltipManager::GetEntry( nsWindow *w)
{
   HWND hwnd = (HWND) w->GetNativeData( NS_NATIVE_WINDOW);
   return GetEntry( hwnd);
}

TTData *nsTooltipManager::GetEntry( HWND hwnd)
{
   HWNDHashKey key( hwnd);
   return (TTData*) mManaged.Get( &key);
}

void nsTooltipManager::SetTooltips( nsWindow *window,
                                    PRUint32 cTips, nsRect *rects[])
{
   TTData *pData = GetEntry( window);

   if( !pData)
   {
      HWND hwnd = (HWND) window->GetNativeData( NS_NATIVE_WINDOW);
      pData = new TTData( hwnd);
      pData->oldFnWp = WinSubclassWindow( hwnd, ::HandleMsg);
      HWNDHashKey key(hwnd);
      mManaged.Put( &key, pData);
      mElements++;
   }
   pData->UpdateTooltips( window, rects, cTips);
}

void nsTooltipManager::UpdateTooltips( nsWindow *window, nsRect *rects[])
{
   TTData *pData = GetEntry( window);

   NS_ASSERTION( pData, "Update tt for bad window");

   pData->UpdateTooltips( window, rects);
}

void nsTooltipManager::RemoveTooltips( nsWindow *window)
{
   HWND hwnd = (HWND) window->GetNativeData( NS_NATIVE_WINDOW);

   HWNDHashKey  key( hwnd);
   TTData      *pData = (TTData*) mManaged.Remove( &key);

   if( pData)
   {
      pData->StopTimer(); // just for safety
      WinSubclassWindow( hwnd, pData->oldFnWp);
   
      delete pData;
      mElements--;
   }
   
   if( 0 == mElements) // no clients, go away
   {
      delete this;
      nsTooltipManager::sMgr = 0;
   }
}

// Ctor/dtor stuff ---------------------------------------------------------
nsTooltipManager::nsTooltipManager() : mElements(0), mTTMode( PR_FALSE)
{}

PRBool PR_CALLBACK emptyHashtableFunc( nsHashKey *aKey, void *aData, void *pClosure)
{
   delete (TTData*) aData;
   return PR_TRUE; // keep going
}

nsTooltipManager::~nsTooltipManager()
{
   // shouldn't be any elements...
   // NS_ASSERTION( mElements == 0, "Non-deregistered tts");
   mManaged.Enumerate( emptyHashtableFunc);
}

nsTooltipManager *nsTooltipManager::GetManager()
{
   if( !sMgr)
      sMgr = new nsTooltipManager;

   return sMgr;
}

nsTooltipManager *nsTooltipManager::sMgr;
