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
 */

#ifndef _nsdragservice_h
#define _nsdragservice_h

// Drag service.  Manages drag & drop events, converts between OS/2 and
//                mozilla-style databuffers.
//
// This exists as a singleton in the nsModule; it's created at creation of the
// primaeval appshell and destroyed at DLL unload-time.
//

#include "nsBaseDragService.h"

class nsFileSpec;

// This implements nsIDragSession and nsIDragService.

class nsDragService : public nsBaseDragService
{
 public:
   nsDragService();
   virtual ~nsDragService();

   // nsIDragService
   NS_IMETHOD InvokeDragSession( nsISupportsArray *anArrayTransferables,
                                 nsIRegion *aRegion, PRUint32 aActionType);

   // nsIDragSession
   NS_IMETHOD GetData( nsITransferable *aTransferable, PRUint32 aItemIndex);
   NS_IMETHOD GetNumDropItems( PRUint32 *aNumItems);
   NS_IMETHOD IsDataFlavorSupported( nsString *aDataFlavour);

   // platform methods, called from nsWindow
   void    InitDragOver( PDRAGINFO aDragInfo);
   MRESULT TermDragOver();

   void InitDragExit( PDRAGINFO aDragInfo);
   void TermDragExit();

   void InitDrop( PDRAGINFO aDragInfo);
   void TermDrop();

 protected:
   // Natives
   void     CreateDragItems( PULONG pCount, PDRAGITEM *ppItems,
                             nsITransferable *aTransferable);
   void     FillDragItem( PDRAGITEM aItem, nsITransferable *aTransferable);
   void     FillDragItem( PDRAGITEM aItem, nsFileSpec *aFilespec);
   nsresult InvokeDrag( PDRAGITEM aItems, ULONG aCItems, PRUint32 aActionType);
   MRESULT  HandleMessage( ULONG msg, MPARAM mp1, MPARAM mp2);
   void     DoPushedOS2FILE( PDRAGITEM pItem, const char *szRf,
                             void **pData, PRUint32 *cData);
   void     DoMozillaXfer( PDRAGITEM pItem, char *szFlavour,
                           void **ppData, PRUint32 *cData);

   HWND      mDragWnd;
   HPOINTER  mIcon;

   // State; allocated draginfo & outstanding items
   PDRAGINFO mDragInfo;
   PFNWP     mWndProc;
   ULONG     mDragItems;

   friend MRESULT EXPENTRY fnwpDragSource(HWND,ULONG,MPARAM,MPARAM);
};

nsresult NS_GetDragService( nsISupports **aDragService);

#endif
