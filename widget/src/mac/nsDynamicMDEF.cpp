/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#include "nsDynamicMDEF.h"

#include "prinrval.h"

#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsMenuBar.h"
#include "nsIMenuBar.h"
#include "DefProcFakery.h"
#include "nsGUIEvent.h"

#include <Resources.h>
#include <MixedMode.h>
  
extern nsWeakPtr    gMacMenubar;


// needed for CallMenuDefProc() to work correctly
#pragma options align=mac68k


// Caching the Mac menu
nsVoidArray     gPreviousMenuHandleStack; // hold MenuHandles
nsMenuStack     gPreviousMenuStack;   // weak refs
nsWeakPtr       gPreviousMenuBar;     // weak ref

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


nsMenuStack::nsMenuStack()
{
}

nsMenuStack::~nsMenuStack()
{
}

nsresult
nsMenuStack::GetMenuAt(PRInt32 aIndex, nsIMenu **outMenu)
{
  nsCOMPtr<nsISupports> elementPtr = getter_AddRefs(mMenuArray.ElementAt(aIndex));
  if (!elementPtr)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIWeakReference> weakRef = do_QueryInterface(elementPtr);
  return weakRef->QueryReferent(NS_GET_IID(nsIMenu), NS_REINTERPRET_CAST(void**, outMenu));
}

PRBool
nsMenuStack::HaveMenuAt(PRInt32 aIndex)
{
  nsCOMPtr<nsISupports> elementPtr = getter_AddRefs(mMenuArray.ElementAt(aIndex));
  if (!elementPtr)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIWeakReference> weakRef = do_QueryInterface(elementPtr);
  nsCOMPtr<nsIMenu> theMenu = do_QueryReferent(weakRef);
  return (theMenu.get() != nsnull);
}

PRBool nsMenuStack::RemoveMenuAt(PRInt32 aIndex)
{
  return mMenuArray.RemoveElementAt(aIndex);
}

PRBool nsMenuStack::InsertMenuAt(nsIMenu* inMenuItem, PRInt32 aIndex)
{
  nsCOMPtr<nsISupportsWeakReference> weakRefFactory = do_QueryInterface(inMenuItem);
  if (!weakRefFactory) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISupports> observerRef = getter_AddRefs(NS_STATIC_CAST(nsISupports*, NS_GetWeakReference(weakRefFactory)));
  if (!observerRef) return NS_ERROR_NULL_POINTER;
  return mMenuArray.InsertElementAt(observerRef, aIndex); 
}

#pragma mark -


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
      
      // Need to make sure that we rebuild the menu every time...
      if (gPreviousMenuHandleStack.Count())
      {
          nsCOMPtr<nsIMenu> menu;
          gPreviousMenuStack.GetMenuAt(gPreviousMenuStack.Count() - 1, getter_AddRefs(menu));
          // nsIMenu * menu = (nsIMenu *) gPreviousMenuStack[gPreviousMenuStack.Count() - 1];
          MenuHandle menuHandle = (MenuHandle) gPreviousMenuHandleStack[gPreviousMenuHandleStack.Count() - 1];
          
          //printf("  gPreviousMenuStack.Count() = %d \n", gPreviousMenuStack.Count());
          //printf("  gPreviousMenuHandleStack.Count() = %d \n", gPreviousMenuHandleStack.Count()); 
                
          if( menu && menuHandle ) {
            
            if( menuHandle == theMenu ) {
                 
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
              
      		    //gPreviousMenuStack.RemoveElementAt(gPreviousMenuStack.Count() - 1);
      		    gPreviousMenuStack.RemoveMenuAt(gPreviousMenuStack.Count() - 1);
      		    //NS_IF_RELEASE(menu);      		    
              
              //printf("%d items now on gPreviousMenuStack \n", gPreviousMenuStack.Count());
              gPreviousMenuHandleStack.RemoveElementAt(gPreviousMenuHandleStack.Count() - 1);
              
            }
          }
        }
      } 
          
      nsDynamicSizeTheMenu(theMenu);
      break;
  }
  nsCallSystemMDEF(message, theMenu, menuRect, hitPt, whichItem);
  
  // Force a size message next time we draw this menu
  if(message == kMenuDrawMsg) {
#if !TARGET_CARBON
    (**theMenu).menuWidth = -1;
    (**theMenu).menuHeight = -1;
#endif
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
  
  if (gPreviousMenuStack.Count() > 0)
  {
    if (gPreviousMenuStack.HaveMenuAt(gPreviousMenuStack.Count() - 1)) {
      if(nsIsHierChild(theMenu)) {
        isChild = PR_TRUE;
      }  
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
  nsCOMPtr<nsIMenuBar> menubar = do_QueryReferent(gMacMenubar);
  if (!menubar || !theMenu) {
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
      nsCOMPtr<nsIMenu> menu;
      menubar->GetMenuAt(i, *getter_AddRefs(menu));
      if(menu) {
          nsCOMPtr<nsIMenuListener> listener(do_QueryInterface(menu));
          if(listener) {
            // Reset menu depth count
            gMenuDepth = 0;
            nsEventStatus status = listener->MenuSelected(mevent);
            if(status != nsEventStatus_eIgnore)
            {             
              nsPostBuild(menu, theMenu, isChild);
              gPreviousTopLevelMenuHandle = theMenu;
              gPreviousTopLevelMenu = menu;
              
              gPreviousMenuBar = getter_AddRefs(NS_GetWeakReference(menubar));
	            //printf("exit BuildMenu \n");
	            return;   
	          }
	        }
	    } 
	  }
  } else {
      // Not top level, so we can't use recursive MenuSelect <sigh>
      // We must use the previously chosen menu item in combination
      // with the current menu to determine what menu needs to be constructed
      //printf("gCurrentMenuItem = %d \n", gCurrentMenuItem);
        if (gCurrentMenuItem){
        	if (gPreviousMenuStack.Count() > 0)
        	{
        	    nsCOMPtr<nsIMenu> prevMenu;        	  
        	    gPreviousMenuStack.GetMenuAt(gPreviousMenuStack.Count() - 1, getter_AddRefs(prevMenu));
        	  
	            //nsIMenu * prevMenu = (nsIMenu *) gPreviousMenuStack[gPreviousMenuStack.Count() - 1];
	            //printf("gPreviousMenuStack.Count() = %d \n", gPreviousMenuStack.Count() );
	            
          if(prevMenu)
          {
            nsCOMPtr<nsISupports> supports;
            prevMenu->GetItemAt(gCurrentMenuItem - 1, *getter_AddRefs(supports));
            nsCOMPtr<nsIMenu> menu = do_QueryInterface(supports);
            if (menu)
            {
              nsCOMPtr<nsIMenuListener> menulistener = do_QueryInterface(menu);
              menulistener->MenuSelected(mevent);
              nsPostBuild(menu, theMenu, isChild);
            }
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
  if(isChild || (gPreviousMenuHandleStack[gPreviousMenuHandleStack.Count() - 1] != theMenu))
  {
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
  // extract the real system mdef out of the fake one we've stored in the menu
  Handle fakedMDEF = (**theMenu).menuProc;
  Handle systemDefProc = DefProcFakery::GetSystemDefProc ( fakedMDEF );

  SInt8 state = ::HGetState(systemDefProc);
  ::HLock(systemDefProc);

  // can't use NewMenuDefProc() here because the routine descriptor has to use kM68kISA
  CallMenuDefProc((RoutineDescriptorPtr)*systemDefProc, message, theMenu, menuRect, hitPt, whichItem);
  
  ::HSetState(systemDefProc, state);
  return;
}

//------------------------------------------------------------------------------
void nsPushMenu(nsIMenu *aMenu) 
{  
  gPreviousMenuStack.InsertMenuAt(aMenu, gPreviousMenuStack.Count());
}

//------------------------------------------------------------------------------
void nsPopMenu(nsIMenu ** aMenu)
{
  if(gPreviousMenuStack.Count() > 0)
  {
    // *aMenu = (nsIMenu *) gPreviousMenuStack[gPreviousMenuStack.Count() - 1];    
    gPreviousMenuStack.GetMenuAt(gPreviousMenuStack.Count() - 1, aMenu);    
    gPreviousMenuStack.RemoveMenuAt(gPreviousMenuStack.Count() - 1);
  } else
    *aMenu = nsnull;
  }

//------------------------------------------------------------------------------
void nsPreviousMenuStackUnwind(nsIMenu * aMenuJustBuilt, MenuHandle aMenuHandleJustBuilt)
{
  //PRBool shouldReleaseMenubar = PR_FALSE;
  
  //printf("PreviousMenuStackUnwind called \n");
  //printf("%d items on gPreviousMenuStack \n", gPreviousMenuStack.Count());
  while (gPreviousMenuHandleStack.Count())
  {
    nsCOMPtr<nsIMenu> menu; // = (nsIMenu *) gPreviousMenuStack[gPreviousMenuStack.Count() - 1];
    gPreviousMenuStack.GetMenuAt(gPreviousMenuStack.Count() - 1, getter_AddRefs(menu));
 
    MenuHandle menuHandle = (MenuHandle) gPreviousMenuHandleStack[gPreviousMenuHandleStack.Count() - 1];
    
    if (menu)
    {  
      
      //printf("  gPreviousMenuStack.Count() = %d \n", gPreviousMenuStack.Count());
      //printf("  gPreviousMenuHandleStack.Count() = %d \n", gPreviousMenuHandleStack.Count()); 
            
      if( menuHandle ) {
        
        if( menuHandle != aMenuHandleJustBuilt ) {           
          nsCOMPtr<nsIMenuListener> listener(do_QueryInterface(menu));
          if(listener) {
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
            
            //gPreviousMenuStack.RemoveElementAt(gPreviousMenuStack.Count() - 1);
            gPreviousMenuStack.RemoveMenuAt(gPreviousMenuStack.Count() - 1);
            // NS_IF_RELEASE(menu);
            //shouldReleaseMenubar = PR_TRUE;
            
            //printf("%d items now on gPreviousMenuStack \n", gPreviousMenuStack.Count());
            gPreviousMenuHandleStack.RemoveElementAt(gPreviousMenuHandleStack.Count() - 1);          
          }
        } 
        else {
            //printf("  gPreviousMenuStack.Count() = %d \n", gPreviousMenuStack.Count());
            //printf("  gPreviousMenuHandleStack.Count() = %d \n", gPreviousMenuHandleStack.Count()); 
            
          // we are the aMenuHandleJustBuilt
          return;
        }
      }
    }
    else
    {
      // remove the weak ref
      gPreviousMenuStack.RemoveMenuAt(gPreviousMenuStack.Count() - 1);
      NS_ASSERTION(menuHandle != aMenuHandleJustBuilt, "Got the menu handle just built");
      if( menuHandle )
        gPreviousMenuHandleStack.RemoveElementAt(gPreviousMenuHandleStack.Count() - 1);          
    }
  }
  
  // relinquish hold of the menubar _after_ releasing the menu so it can finish
  // unregistering itself.
  //if ( shouldReleaseMenubar )
  //  gPreviousMenuBar = nsnull;
  
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

#pragma options align=reset
