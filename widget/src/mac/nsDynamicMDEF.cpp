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
 
#include "nsDynamicMDEF.h"

#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsMenuBar.h"
#include "nsIMenuBar.h"

#include <Resources.h>
#include <MixedMode.h>
#include <A4Stuff.h>
  
extern nsIMenuBar * gMacMenubar;
extern Handle gSystemMDEFHandle;  

#pragma options align=mac68k
RoutineDescriptorPtr gOriginaldesc;
RoutineDescriptorPtr gmdefUPP;





// Caching the Mac menu
nsVoidArray gPreviousMenuHandleStack;
nsVoidArray gPreviousMenuStack;

nsIMenuBar * gCachedMacMenubar;

MenuHandle gSizedMenu = nsnull;


extern Handle gMDEF;
bool gTested = false;

MenuHandle gCachedMenuHandle = nsnull;
PRUint16 gCachedMenuID = 0;

extern PRInt16 gMenuDepth;  // volital running total
extern PRInt16 gCurrentMenuDepth; // The menu depth that is currently shown
extern MenuHandle gLevel2HierMenu;
extern MenuHandle gLevel3HierMenu;
extern MenuHandle gLevel4HierMenu;
extern MenuHandle gLevel5HierMenu;

PRUint32 gCurrentMenuItem = 0; // 1 based ala MacOS
PRUint32 gCurrentTopLevelMenuIndex = 0;
MenuHandle gPreviousTopLevelMenuHandle = nsnull;
nsIMenu * gPreviousTopLevelMenu = nsnull;
nsIMenu * gPreviousMenu = nsnull;
MenuHandle gPreviousMenuHandle = nsnull;

//------------------------------------------------------------------------------
// Internal functions
  
void nsDynamicChooseItem(
  MenuHandle theMenu, 
  Rect * menuRect, 
  Point hitPt, 
  short * whichItem);
  
void nsDynamicSizeTheMenu(
  MenuHandle theMenu);  

void nsBuildMenu(
  MenuHandle theMenu, 
  PRBool isChild);

void nsPushMenu(
  nsIMenu * aMenu);
  
void nsPopMenu(
  nsIMenu ** aMenu);
  
void nsPushMenuHandle(
  MenuHandle aMenu);
  
void nsPopMenuHandle(
  MenuHandle * aMenu);

void nsCallSystemMDEF(
  short message,
  MenuHandle theMenu, 
  Rect * menuRect, 
  Point hitPt, 
  short * whichItem);
  
void nsDoMagic(
  MenuHandle theMenu);

void nsPostBuild(
  nsIMenu * menu, 
  MenuHandle theMenu, 
  PRBool isChild);

bool nsIsHierChild(
  MenuHandle theMenu);

void nsPreviousMenuStackUnwind(
  nsIMenu * aMenuJustBuilt, 
  MenuHandle aMenuHandleJustBuilt);
  
void nsCheckDestroy(
  MenuHandle theMenu, 
  short * whichItem);

//void TestPreviousMenuStackUnwind(nsIMenu * aMenuJustBuilt, MenuHandle aMenuHandleJustBuilt);
//void TestCheckDestroy(MenuHandle theMenu, short * whichItem);
//void TestPostBuild(nsIMenu * menu, MenuHandle theMenu, PRBool isChild);

//------------------------------------------------------------------------------
pascal void nsDynamicMDEFMain(
  short message,
  MenuHandle theMenu, 
  Rect * menuRect, 
  Point hitPt, 
  short * whichItem) 
{		
  switch (message) {
    case kMenuDrawMsg: 
      //printf("  Draw passed in menu is = %d \n", *theMenu);
      //printf("  t= %d l= %d b= %d r= %d\n", (*menuRect).top, (*menuRect).left, (*menuRect).bottom, (*menuRect).right);
      //printf("  Point.v = %d  Point.h = %d\n", hitPt.v, hitPt.h);
      //printf("  whichItem = %d \n", *whichItem);
      //printf("  theMenu.menuID = %d \n", (**theMenu).menuID);
      
      nsCheckDestroy(theMenu, whichItem);
      break;

    case kMenuChooseMsg: 
      // Keep track of currently chosen item
      nsDynamicChooseItem(theMenu, menuRect, hitPt, whichItem);
      return; // Yes, intentional return as CallSystemMDEF has happened already
      break;
      
    case kMenuSizeMsg:
      //printf("Size passed in menu is = %d \n", *theMenu); 
      //printf("  t= %d l= %d b= %d r= %d \n", (*menuRect).top, (*menuRect).left, (*menuRect).bottom ,(*menuRect).right);
      //printf("  Point.v = %d  Point.h = %d \n", hitPt.v, hitPt.h);
      //printf("  whichItem = %d \n", *whichItem);
      //printf("  theMenu.menuID = %d \n", (**theMenu).menuID);
    
      nsCheckDestroy(theMenu, whichItem);
      nsDynamicSizeTheMenu(theMenu);
      break;
  }
  nsCallSystemMDEF(message, theMenu, menuRect, hitPt, whichItem);
  
  // Force a size message next time we draw this menu
  if(message == kMenuDrawMsg) {
    SInt8 state = ::HGetState((Handle)theMenu);
    HLock((Handle)theMenu);
    (**theMenu).menuWidth = -1;
	(**theMenu).menuHeight = -1;
    HSetState((Handle)theMenu, state);  
  }
}  

//------------------------------------------------------------------------------
void nsCheckDestroy(MenuHandle theMenu, short * whichItem)
{
  // Determine if we have unselected the previous popup. If so we need to 
  // destroy it now.
  bool changeOccured = true;
  bool isChild = false;

  if(gCurrentMenuItem == *whichItem)
    changeOccured = false;

  if(!gPreviousMenuHandleStack[gPreviousMenuHandleStack.Count() - 1])
    changeOccured = false;
  
  if(nsIsHierChild(theMenu))
    isChild = true;
  
  if(changeOccured && !isChild) {
    
    nsPreviousMenuStackUnwind(nsnull, theMenu);

    if(gCurrentMenuDepth > 1)
      gCurrentMenuDepth--;
  }
}

//------------------------------------------------------------------------------
void nsDynamicChooseItem(
  MenuHandle theMenu, 
  Rect * menuRect, 
  Point hitPt, 
  short * whichItem)
{
  //printf("enter DynamicChooseItem \n");
  nsCallSystemMDEF(kMenuChooseMsg, theMenu, menuRect, hitPt, whichItem);
   
  nsCheckDestroy(theMenu, whichItem);
  
  gCurrentMenuItem = *whichItem;
  
  //printf("exit DynamicChooseItem \n");
}

//------------------------------------------------------------------------------
void nsDynamicSizeTheMenu(
  MenuHandle theMenu)
{
  //printf("enter DynamicSizeTheMenu \n");
  
  nsDoMagic(theMenu);
  //printf("exit DynamicSizeTheMenu \n");
}

//------------------------------------------------------------------------------
bool nsIsHierChild(MenuHandle theMenu)
{
  if(theMenu == gLevel2HierMenu) {
    if(gCurrentMenuDepth <= 2) return true;
  } else if(theMenu == gLevel3HierMenu) {
    if(gCurrentMenuDepth <= 3) return true;
  } else if(theMenu == gLevel4HierMenu) {
    if(gCurrentMenuDepth <= 4) return true;
  } else if(theMenu == gLevel5HierMenu) {
    if(gCurrentMenuDepth <= 5) return true;
  }
  return false;
}

//------------------------------------------------------------------------------
void nsDoMagic(MenuHandle theMenu)
{
  //printf("DoMagic \n");
  // ask if this is a child of the previous menu
  PRBool isChild = PR_FALSE;
  
  if(gPreviousMenuStack[gPreviousMenuStack.Count() - 1]) {
    if(nsIsHierChild(theMenu)) {
      isChild = PR_TRUE;
    } 
  } 
  
  if(theMenu == gLevel2HierMenu) {
    gCurrentMenuDepth = 2;
  } else if(theMenu == gLevel3HierMenu) {
    gCurrentMenuDepth = 3;
  } else if(theMenu == gLevel4HierMenu) {
    gCurrentMenuDepth = 4;
  } else if(theMenu == gLevel5HierMenu) {
    gCurrentMenuDepth = 5;
  } else {
    gCurrentMenuDepth = 1;
  }
		  
  nsBuildMenu(theMenu, isChild);
}

//------------------------------------------------------------------------------
void nsBuildMenu(MenuHandle theMenu, PRBool isChild)
{ 
 // printf("enter BuildMenu \n");
  nsIMenuBar * menubar = gMacMenubar; // Global for current menubar 
  
  if(!menubar || !theMenu) {
    return;
  }
   
  nsMenuEvent mevent;
  mevent.message = NS_MENU_SELECTED;
  mevent.eventStructType = NS_MENU_EVENT;
  mevent.point.x = 0;
  mevent.point.y = 0;
  mevent.widget = nsnull;
  mevent.time = PR_IntervalNow();
  mevent.mCommand = (PRUint32) theMenu;
        
  // If toplevel
  if( gCurrentMenuDepth < 2 ) {
	  PRUint32 numMenus = 0;
	  menubar->GetMenuCount(numMenus);
	  numMenus--;
	  for(PRInt32 i = numMenus; i >= 0; i--) {
	    nsIMenu * menu = nsnull;
	    menubar->GetMenuAt(i, menu);
	    if(menu) {
	        nsCOMPtr<nsIMenuListener> listener(do_QueryInterface(menu));
	        if(listener) {
	          // Reset menu depth count
	          gMenuDepth = 0;
	          nsEventStatus status = listener->MenuSelected(mevent);
	          if(status != nsEventStatus_eIgnore) {
	            
                nsPostBuild(menu, theMenu, isChild);
	            gPreviousTopLevelMenuHandle = theMenu;
	            gPreviousTopLevelMenu = menu;
	  
	            NS_RELEASE(menu);
	            //printf("exit BuildMenu \n");
	            return;   
	          }
	        }
	      NS_RELEASE(menu);
	    } 
	  }
  } else {
      // Not top level, so we can't use recursive MenuSelect <sigh>
      // We must use the previously chosen menu item in combination
      // with the current menu to determine what menu needs to be constructed
        nsISupports * supports = nsnull;
        //printf("gCurrentMenuItem = %d \n", gCurrentMenuItem);
        if(gCurrentMenuItem) {
            nsIMenu * prevMenu = (nsIMenu *) gPreviousMenuStack[gPreviousMenuStack.Count() - 1];
            //printf("gPreviousMenuStack.Count() = %d \n", gPreviousMenuStack.Count() );
            
            if(prevMenu) {
		        prevMenu->GetItemAt(gCurrentMenuItem - 1, supports);
		        nsCOMPtr<nsIMenu> menu(do_QueryInterface(supports));
		        if(menu) {
		          nsCOMPtr<nsIMenuListener> menulistener(do_QueryInterface(supports));
		          menulistener->MenuSelected(mevent);
		          nsPostBuild(menu, theMenu, isChild);
		        }
	        } 
	    }
  }
  //printf("exit BuildMenu \n");
}

//------------------------------------------------------------------------------
void nsPostBuild(nsIMenu * menu, MenuHandle theMenu, PRBool isChild)
{
	// it is built now
	if(isChild || (gPreviousMenuHandleStack[gPreviousMenuHandleStack.Count() - 1] != theMenu)) {
	  nsPushMenu(menu);
	  nsPushMenuHandle(theMenu);
	  
	  //printf("Push: %d items in gMenuHandleStack \n", gMenuHandleStack.Count());
	} 
}

//------------------------------------------------------------------------------
void nsCallSystemMDEF(
  short message,
  MenuHandle theMenu, 
  Rect * menuRect, 
  Point hitPt, 
  short * whichItem) 
{
	SInt8 state = ::HGetState(gSystemMDEFHandle);
	::HLock(gSystemMDEFHandle);

	gmdefUPP = ::NewRoutineDescriptor( (ProcPtr)*gSystemMDEFHandle, uppMenuDefProcInfo, kM68kISA);

	CallMenuDefProc(
		gmdefUPP, 
		message, 
		theMenu, 
		menuRect, 
		hitPt, 
		whichItem);

	::DisposeRoutineDescriptor(gmdefUPP);
	::HSetState(gSystemMDEFHandle, state);

	return;
}

//------------------------------------------------------------------------------
void nsPushMenu(nsIMenu * aMenu) 
{  
  gPreviousMenuStack.InsertElementAt(aMenu, gPreviousMenuStack.Count());
}

//------------------------------------------------------------------------------
void nsPopMenu(nsIMenu ** aMenu)
{
  *aMenu = (nsIMenu *) gPreviousMenuStack[gPreviousMenuStack.Count() - 1];
  gPreviousMenuStack.RemoveElementAt(gPreviousMenuStack.Count() - 1);
}

//------------------------------------------------------------------------------
void nsPreviousMenuStackUnwind(nsIMenu * aMenuJustBuilt, MenuHandle aMenuHandleJustBuilt)
{
  //printf("PreviousMenuStackUnwind called \n");
  //printf("%d items on gPreviousMenuStack \n", gPreviousMenuStack.Count());
  while (gPreviousMenuHandleStack.Count()) {
    nsIMenu * menu = (nsIMenu *) gPreviousMenuStack[gPreviousMenuStack.Count() - 1];
    MenuHandle menuHandle = (MenuHandle) gPreviousMenuHandleStack[gPreviousMenuHandleStack.Count() - 1];
    
    //printf("  gPreviousMenuStack.Count() = %d \n", gPreviousMenuStack.Count());
    //printf("  gPreviousMenuHandleStack.Count() = %d \n", gPreviousMenuHandleStack.Count()); 
          
    if( menu && menuHandle ) {
      
      if( menuHandle != aMenuHandleJustBuilt ) {
           
	      nsCOMPtr<nsIMenuListener> listener(do_QueryInterface(menu));
		  if(listener) {
		    //printf("MenuPop \n");
		    
		    nsMenuEvent mevent;
		    mevent.message = NS_MENU_SELECTED;
		    mevent.eventStructType = NS_MENU_EVENT;
		    mevent.point.x = 0;
		    mevent.point.y = 0;
		    mevent.widget = nsnull;
		    mevent.time = PR_IntervalNow();
		    mevent.mCommand = (PRUint32) nsnull;
		    
		    // UNDO
		    listener->MenuDeselected(mevent);
		    
		    gPreviousMenuStack.RemoveElementAt(gPreviousMenuStack.Count() - 1);
	        //printf("%d items now on gPreviousMenuStack \n", gPreviousMenuStack.Count());
	        gPreviousMenuHandleStack.RemoveElementAt(gPreviousMenuHandleStack.Count() - 1);
	      
		  }
	  } else {
        //printf("  gPreviousMenuStack.Count() = %d \n", gPreviousMenuStack.Count());
        //printf("  gPreviousMenuHandleStack.Count() = %d \n", gPreviousMenuHandleStack.Count()); 
	    return;
	  }
	}
  } 
  //printf("  gPreviousMenuStack.Count() = %d \n", gPreviousMenuStack.Count());
  //printf("  gPreviousMenuHandleStack.Count() = %d \n", gPreviousMenuHandleStack.Count()); 
}

//------------------------------------------------------------------------------
void nsPushMenuHandle(MenuHandle aMenu) 
{
  gPreviousMenuHandleStack.AppendElement(aMenu);
}

//------------------------------------------------------------------------------
void nsPopMenuHandle(MenuHandle * aMenu)
{
  *aMenu = (MenuHandle) gPreviousMenuHandleStack[gPreviousMenuHandleStack.Count() - 1];
  gPreviousMenuHandleStack.RemoveElementAt(gPreviousMenuHandleStack.Count() - 1);
}

/*
//------------------------------------------------------------------------------
void TestPreviousMenuStackUnwind(nsIMenu * aMenuJustBuilt, MenuHandle aMenuHandleJustBuilt)
{
  //printf("PreviousMenuStackUnwind called \n");
  //printf("%d items on gPreviousMenuStack \n", gPreviousMenuStack.Count());
  while (gPreviousMenuHandleStack.Count()) {
    MenuHandle menuHandle = (MenuHandle) gPreviousMenuHandleStack[gPreviousMenuHandleStack.Count() - 1];
    
    //printf("  gPreviousMenuHandleStack.Count() = %d \n", gPreviousMenuHandleStack.Count()); 
          
    if( menuHandle != aMenuHandleJustBuilt) {
            ::DeleteMenuItem(menuHandle, 3);
            ::DeleteMenuItem(menuHandle, 2);
            ::DeleteMenuItem(menuHandle, 1);
            
	        //printf("%d items now on gPreviousMenuStack \n", gPreviousMenuStack.Count());
	        gPreviousMenuHandleStack.RemoveElementAt(gPreviousMenuHandleStack.Count() - 1);
	      

	  } else {
        //printf("  gPreviousMenuHandleStack.Count() = %d \n", gPreviousMenuHandleStack.Count()); 
	    return;
	  }
	}
    //printf("  gPreviousMenuHandleStack.Count() = %d \n", gPreviousMenuHandleStack.Count()); 
}

//------------------------------------------------------------------------------
void TestPostBuild(nsIMenu * menu, MenuHandle theMenu, PRBool isChild)
{
	// it is built now
	if(isChild || (gPreviousMenuHandleStack[gPreviousMenuHandleStack.Count() - 1] != theMenu)) {
	  PushMenuHandle(theMenu);
	  
	  //printf("Push: %d items in gMenuHandleStack \n", gMenuHandleStack.Count());
	} 
}

//------------------------------------------------------------------------------
void TestBuild(MenuHandle theMenu, PRBool isChild)
{ 
  // Add item
  ::InsertMenuItem(theMenu, "\pFooItem", 1);
  
  // Add item with sub menu
  ::InsertMenuItem(theMenu, "\pSubmenu", 2);

  if(theMenu == gLevel2HierMenu) {
    gCurrentMenuDepth = 3;
  } else if(theMenu == gLevel3HierMenu) {
    gCurrentMenuDepth = 4;
  } else if(theMenu == gLevel4HierMenu) {
    gCurrentMenuDepth = 5;
  } else if(theMenu == gLevel5HierMenu) {
    gCurrentMenuDepth = 6;
  } else {
    gCurrentMenuDepth = 2;
  }
  
  ::SetMenuItemHierarchicalID(theMenu, 2, gCurrentMenuDepth);
  // Add fake item to sub menu so we get the size message
  ::DeleteMenuItem(::GetMenuHandle(gCurrentMenuDepth), 1);
  ::InsertMenuItem(::GetMenuHandle(gCurrentMenuDepth), "\pFakeItem", 1);
  
  gCurrentMenuDepth--;
  
  TestPostBuild(nsnull, theMenu, isChild);
}
*/

#pragma options align=reset