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

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsINameSpaceManager.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIContent.h"

#include "nsMenuBar.h"
#include "nsDynamicMDEF.h"

#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentObserver.h"

#include "nsWidgetAtoms.h"

#include "nsIDOMXULDocument.h"

#include <Menus.h>
#include <TextUtils.h>
#include <Balloons.h>
#include <Traps.h>
#include <Resources.h>
#include <Appearance.h>
#include "nsMacResources.h"

#include "DefProcFakery.h"


Handle       gMDEF = nsnull;
nsWeakPtr    gMacMenubar;
nsWeakPtr    gOriginalMenuBar;
bool         gFirstMenuBar = true; 

// The four Golden Hierarchical Child Menus
MenuHandle gLevel2HierMenu = nsnull;
MenuHandle gLevel3HierMenu = nsnull;
MenuHandle gLevel4HierMenu = nsnull;
MenuHandle gLevel5HierMenu = nsnull;

extern nsMenuStack          gPreviousMenuStack;
extern PRInt16 gCurrentMenuDepth;

// #if APPLE_MENU_HACK
#include "nsMenu.h"				// need to get APPLE_MENU_HACK macro
// #endif

// CIDs
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kMenuBarCID, NS_MENUBAR_CID);
static NS_DEFINE_CID(kMenuCID, NS_MENU_CID);
static NS_DEFINE_CID(kMenuItemCID, NS_MENUITEM_CID);

void InstallDefProc( short dpPath, ResType dpType, short dpID, Ptr dpAddr);


PRInt32   gMenuBarCounter = 0;


NS_IMPL_ISUPPORTS5(nsMenuBar, nsIMenuBar, nsIMenuListener, nsIDocumentObserver, nsIChangeManager, nsISupportsWeakReference)

//
// nsMenuBar constructor
//
nsMenuBar::nsMenuBar()
{  
  gCurrentMenuDepth = 1;
  
  nsPreviousMenuStackUnwind(nsnull, nsnull);

  NS_INIT_REFCNT();
  mNumMenus       = 0;
  mParent         = nsnull;
  mIsMenuBarAdded = PR_FALSE;
  mUnicodeTextRunConverter = nsnull;
    
  mOriginalMacMBarHandle = nsnull;
  mMacMBarHandle = nsnull;
  mDocument = nsnull;
  
  mOriginalMacMBarHandle = ::GetMenuBar();
  Handle tmp = ::GetMenuBar();
  ::SetMenuBar(tmp);
  this->SetNativeData((void*)tmp);
  
  ::ClearMenuBar(); 
  mRefCnt = 1;      // NS_GetWeakReference does an addref then a release, so this +1 is needed
  gMacMenubar = getter_AddRefs(NS_GetWeakReference((nsIMenuBar *)this));
  mRefCnt = 0;
  // copy from nsMenu.cpp
  ScriptCode ps[1];
  ps[0] = ::GetScriptManagerVariable(smSysScript);
  
  OSErr err = ::CreateUnicodeToTextRunInfoByScriptCode(0x80000000, ps, &mUnicodeTextRunConverter);
  NS_ASSERTION(err==noErr,"nsMenu::nsMenu: CreateUnicodeToTextRunInfoByScriptCode failed.");

  ++gMenuBarCounter;
}


//
// nsMenuBar destructor
//
nsMenuBar::~nsMenuBar()
{
  mMenusArray.Clear();    // release all menus
  
  OSErr err = ::DisposeUnicodeToTextRunInfo(&mUnicodeTextRunConverter);
  NS_ASSERTION(err==noErr,"nsMenu::~nsMenu: DisposeUnicodeToTextRunInfo failed.");	 

  // make sure we unregister ourselves as a document observer
  if ( mDocument ) {
    nsCOMPtr<nsIDocumentObserver> observer ( do_QueryInterface(NS_STATIC_CAST(nsIMenuBar*,this)) );
    mDocument->RemoveObserver(observer);
  }

  --gMenuBarCounter;
  if(gMenuBarCounter == 1) {
    nsCOMPtr<nsIMenuBar> menubar = do_QueryReferent(gOriginalMenuBar);
    if(menubar)
      menubar->Paint();
  } 
  
  ::DisposeHandle(mOriginalMacMBarHandle);   
  ::DisposeHandle(mMacMBarHandle);
    
}


nsEventStatus 
nsMenuBar::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  // Dispatch menu event
  nsEventStatus eventStatus = nsEventStatus_eIgnore;

  PRUint32  numItems;
  mMenusArray.Count(&numItems);
  
  for (PRUint32 i = numItems; i > 0; --i)
  {
    nsCOMPtr<nsISupports>     menuSupports = getter_AddRefs(mMenusArray.ElementAt(i - 1));
    nsCOMPtr<nsIMenuListener> menuListener = do_QueryInterface(menuSupports);
    if(menuListener)
    {
      eventStatus = menuListener->MenuItemSelected(aMenuEvent);
      if(nsEventStatus_eIgnore != eventStatus)
        return eventStatus;
    }
  }
  return eventStatus;
}


nsEventStatus 
nsMenuBar::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  // Dispatch event
  nsEventStatus eventStatus = nsEventStatus_eIgnore;

  nsCOMPtr<nsIMenuListener> menuListener;
  nsCOMPtr<nsIMenu> theMenu;
  gPreviousMenuStack.GetMenuAt(gPreviousMenuStack.Count() - 1, getter_AddRefs(theMenu));
  menuListener = do_QueryInterface(theMenu);
  if (menuListener) {
    //TODO: MenuSelected is the right thing to call...
    //eventStatus = menuListener->MenuSelected(aMenuEvent);
    eventStatus = menuListener->MenuItemSelected(aMenuEvent);
    if (nsEventStatus_eIgnore != eventStatus)
      return eventStatus;
  } else {
    // If it's the help menu, gPreviousMenuStack won't be accurate so we need to get the listener a different way 
    // We'll do it the old fashioned way of looping through and finding it
    PRUint32  numItems;
    mMenusArray.Count(&numItems);
    for (PRUint32 i = numItems; i > 0; --i)
    {
      nsCOMPtr<nsISupports>     menuSupports = getter_AddRefs(mMenusArray.ElementAt(i - 1));
      nsCOMPtr<nsIMenuListener> thisListener = do_QueryInterface(menuSupports);
	    if (thisListener)
	    {
        //TODO: MenuSelected is the right thing to call...
  	    //eventStatus = menuListener->MenuSelected(aMenuEvent);
  	    eventStatus = thisListener->MenuItemSelected(aMenuEvent);
  	    if(nsEventStatus_eIgnore != eventStatus)
  	      return eventStatus;
      }
    }
  }
  return eventStatus;
}


nsEventStatus 
nsMenuBar::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus 
nsMenuBar::CheckRebuild(PRBool & aNeedsRebuild)
{
  aNeedsRebuild = PR_TRUE;
  return nsEventStatus_eIgnore;
}

nsEventStatus
nsMenuBar::SetRebuild(PRBool aNeedsRebuild)
{
  return nsEventStatus_eIgnore;
}

void
nsMenuBar :: GetDocument ( nsIWebShell* inWebShell, nsIDocument** outDocument )
{
  *outDocument = nsnull;
  
  nsCOMPtr<nsIDocShell> docShell ( do_QueryInterface(inWebShell) );
  nsCOMPtr<nsIContentViewer> cv;
  if ( docShell ) {
    docShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
      // get the document
      nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
      if (!docv)
        return;
      docv->GetDocument(*outDocument);    // addrefs
    }
  }
}


//
// RegisterAsDocumentObserver
//
// Name says it all.
//
void
nsMenuBar :: RegisterAsDocumentObserver ( nsIWebShell* inWebShell )
{
  nsCOMPtr<nsIDocument> doc;
  GetDocument(inWebShell, getter_AddRefs(doc));
  if (!doc)
    return;

  // register ourselves
  nsCOMPtr<nsIDocumentObserver> observer ( do_QueryInterface(NS_STATIC_CAST(nsIMenuBar*,this)) );
  doc->AddObserver(observer);
  // also get pointer to doc, just in case webshell goes away
  // we can still remove ourself as doc observer directly from doc
  mDocument = doc;

} // RegisterAsDocumentObesrver


nsEventStatus
nsMenuBar::MenuConstruct( const nsMenuEvent & aMenuEvent, nsIWidget* aParentWindow, 
                            void * menubarNode, void * aWebShell )
{
  mWebShellWeakRef = getter_AddRefs(NS_GetWeakReference(NS_STATIC_CAST(nsIWebShell*, aWebShell)));
  nsIDOMNode* aDOMNode  = NS_STATIC_CAST(nsIDOMNode*, menubarNode);
  mMenuBarContent = do_QueryInterface(aDOMNode);           // strong ref
  
  if(gFirstMenuBar) {
      gOriginalMenuBar = getter_AddRefs(NS_GetWeakReference((nsIMenuBar *)this));
      
	  gFirstMenuBar = false;
	  // Add the 4 Golden Hierarchical Menus to the MenuList        
				
	  MenuHandle macMenuHandle = ::NewMenu(2, "\psubmenu");
	  if(macMenuHandle) { 

      // get our fake MDEF ready to be stashed into every menu that comes our way.
      // if we fail creating it, we're probably so doomed there's no point in going
      // on.
      if ( !gMDEF ) {
        MenuDefUPP mdef = NewMenuDefProc( nsDynamicMDEFMain );
        Boolean success = DefProcFakery::CreateDefProc( mdef, (**macMenuHandle).menuProc, &gMDEF );
        if ( !success )
          ::ExitToShell();
      }

	    gLevel2HierMenu = macMenuHandle;
			(**macMenuHandle).menuProc = gMDEF;
			(**macMenuHandle).menuWidth = -1;
			(**macMenuHandle).menuHeight = -1;
			::InsertMenu(macMenuHandle, hierMenu);
    }
			
    macMenuHandle = ::NewMenu(3, "\psubmenu");
	  if(macMenuHandle) { 
	    gLevel3HierMenu = macMenuHandle;
		  (**macMenuHandle).menuProc = gMDEF;
		  (**macMenuHandle).menuWidth = -1;
		  (**macMenuHandle).menuHeight = -1;
		  ::InsertMenu(macMenuHandle, hierMenu);
    }
			
    macMenuHandle = ::NewMenu(4, "\psubmenu");
	  if(macMenuHandle) { 
	    gLevel4HierMenu = macMenuHandle;
		  (**macMenuHandle).menuProc = gMDEF;
		  (**macMenuHandle).menuWidth = -1;
		  (**macMenuHandle).menuHeight = -1;
		  ::InsertMenu(macMenuHandle, hierMenu);
    }
			
    macMenuHandle = ::NewMenu(5, "\psubmenu");
	  if(macMenuHandle) { 
	    gLevel5HierMenu = macMenuHandle; 
			(**macMenuHandle).menuProc = gMDEF;
			(**macMenuHandle).menuWidth = -1;
			(**macMenuHandle).menuHeight = -1;
		  ::InsertMenu(macMenuHandle, hierMenu);
		}
	} else {
	  ::InsertMenu(gLevel2HierMenu, hierMenu);
		::InsertMenu(gLevel3HierMenu, hierMenu);
		::InsertMenu(gLevel4HierMenu, hierMenu);
		::InsertMenu(gLevel5HierMenu, hierMenu);
	}
    
  Create(aParentWindow);

  nsCOMPtr<nsIWebShell> webShell = do_QueryReferent(mWebShellWeakRef);
  if (webShell)
    RegisterAsDocumentObserver(webShell);
      
  // set this as a nsMenuListener on aParentWindow
  aParentWindow->AddMenuListener((nsIMenuListener *)this);
  
  PRInt32 count;
  mMenuBarContent->ChildCount(count);
  for ( int i = 0; i < count; ++i ) { 
    nsCOMPtr<nsIContent> menu;
    mMenuBarContent->ChildAt ( i, *getter_AddRefs(menu) );
    if ( menu ) {
      nsCOMPtr<nsIAtom> tag;
      menu->GetTag ( *getter_AddRefs(tag) );
      if (tag == nsWidgetAtoms::menu) {
        nsAutoString menuName;
        nsAutoString menuAccessKey(NS_LITERAL_STRING(" "));
        menu->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, menuName);
        menu->GetAttr(kNameSpaceID_None, nsWidgetAtoms::accesskey, menuAccessKey);
			  
        // Don't create the whole menu yet, just add in the top level names
              
        // Create nsMenu, the menubar will own it
        nsCOMPtr<nsIMenu> pnsMenu ( do_CreateInstance(kMenuCID) );
        if ( pnsMenu ) {
          pnsMenu->Create(NS_STATIC_CAST(nsIMenuBar*, this), menuName, menuAccessKey, 
                          NS_STATIC_CAST(nsIChangeManager *, this), 
                          NS_REINTERPRET_CAST(nsIWebShell*, aWebShell), menu);

          // Make nsMenu a child of nsMenuBar. nsMenuBar takes ownership
          AddMenu(pnsMenu); 
                  
          nsAutoString menuIDstring;
          menu->GetAttr(kNameSpaceID_None, nsWidgetAtoms::id, menuIDstring);
          if ( menuIDstring == NS_LITERAL_STRING("menu_Help") ) {
            nsMenuEvent event;
            MenuHandle handle = nsnull;
            ::HMGetHelpMenuHandle(&handle);
            event.mCommand = (unsigned int) handle;
            nsCOMPtr<nsIMenuListener> listener(do_QueryInterface(pnsMenu));
            listener->MenuSelected(event);
          }          
        }
      } 
    }
  } // for each menu
        
  // Give the aParentWindow this nsMenuBar to hold onto.
  // The parent takes ownership
  aParentWindow->SetMenuBar(this);
		
  Handle tempMenuBar = ::GetMenuBar(); // Get a copy of the menu list
	SetNativeData((void*)tempMenuBar);
		
  return nsEventStatus_eIgnore;
}


nsEventStatus 
nsMenuBar::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}


//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::Create(nsIWidget *aParent)
{
  SetParent(aParent);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetParent(nsIWidget *&aParent)
{
  NS_IF_ADDREF(aParent = mParent);
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::SetParent(nsIWidget *aParent)
{
  mParent = aParent;    // weak ref  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::AddMenu(nsIMenu * aMenu)
{
  // XXX add to internal data structure
  nsCOMPtr<nsISupports> supports = do_QueryInterface(aMenu);
  if(supports)
    mMenusArray.AppendElement(supports);    // owner

#ifdef APPLE_MENU_HACK
  if (mNumMenus == 0)
  {
    Str32 menuStr = { 1, 0x14 };
    MenuHandle appleMenu = ::NewMenu(kAppleMenuID, menuStr);

    if (appleMenu)
    {
      // this code reads the "label" attribute from the <menuitem/> with
      // id="aboutName" and puts its label in the Apple Menu
      nsAutoString label;
      nsCOMPtr<nsIContent> menu;
      aMenu->GetMenuContent(getter_AddRefs(menu));
      if (menu) {
        nsCOMPtr<nsIDocument> doc;
        menu->GetDocument(*getter_AddRefs(doc));
        if (doc) {
          nsCOMPtr<nsIDOMDocument> domdoc ( do_QueryInterface(doc) );
          if ( domdoc ) {
            nsCOMPtr<nsIDOMElement> aboutMenuItem;
            domdoc->GetElementById(NS_LITERAL_STRING("aboutName"), getter_AddRefs(aboutMenuItem));
            if (aboutMenuItem)
              aboutMenuItem->GetAttribute(NS_LITERAL_STRING("label"), label);
          }
        }
      }

      ::AppendMenu(appleMenu, "\pa");
      MenuHelpers::SetMenuItemText(appleMenu, 1, label, mUnicodeTextRunConverter);
      ::AppendMenu(appleMenu, "\p-");
      ::AppendResMenu(appleMenu, 'DRVR');
      ::InsertMenu(appleMenu, 0);
    }
  }
#endif
  
  MenuHandle menuHandle = nsnull;
  aMenu->GetNativeData((void**)&menuHandle);
  
  mNumMenus++;
  PRBool helpMenu;
  aMenu->IsHelpMenu(&helpMenu);
  if(!helpMenu) {
    nsCOMPtr<nsIContent> menu;
    aMenu->GetMenuContent(getter_AddRefs(menu));
    nsAutoString menuHidden;
    menu->GetAttr(kNameSpaceID_None, nsWidgetAtoms::hidden, menuHidden);
    if( menuHidden != NS_LITERAL_STRING("true"))
      ::InsertMenu(menuHandle, 0);
  }
  
  return NS_OK;
}

                                
//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuCount(PRUint32 &aCount)
{
  aCount = mNumMenus;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{ 
  aMenu = NULL;
  nsCOMPtr<nsISupports> supports = getter_AddRefs(mMenusArray.ElementAt(aCount));
  if (!supports) return NS_OK;
  
  return CallQueryInterface(supports, &aMenu); // addref
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::InsertMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::RemoveMenu(const PRUint32 aCount)
{
  mMenusArray.RemoveElementAt(aCount);
  ::DrawMenuBar();
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::RemoveAll()
{
  NS_ASSERTION(0, "Not implemented!");
  // mMenusArray.Clear();    // maybe?
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetNativeData(void *& aData)
{
  aData = (void *) mMacMBarHandle;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::SetNativeData(void* aData)
{
  Handle  menubarHandle = (Handle)aData;
  
  if(mMacMBarHandle && mMacMBarHandle != menubarHandle)
     ::DisposeHandle(mMacMBarHandle);
  
  mMacMBarHandle = menubarHandle;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::Paint()
{
  gMacMenubar = getter_AddRefs(NS_GetWeakReference((nsIMenuBar *)this));
  
  ::SetMenuBar(mMacMBarHandle);
  // Now we have blown away the merged Help menu, so we have to rebuild it
  PRUint32  numItems;
  mMenusArray.Count(&numItems);
  
  for (PRInt32 i = numItems - 1; i >= 0; --i)
  {
    nsCOMPtr<nsISupports> thisItem = getter_AddRefs(mMenusArray.ElementAt(i));
    nsCOMPtr<nsIMenu>     menu = do_QueryInterface(thisItem);
    PRBool isHelpMenu = PR_FALSE;
    if (menu)
      menu->IsHelpMenu(&isHelpMenu);
    if (isHelpMenu)
    {
      MenuHandle helpMenuHandle = nil;
      ::HMGetHelpMenuHandle(&helpMenuHandle);
      menu->SetNativeData((void*)helpMenuHandle);

      nsMenuEvent event;
      event.mCommand = (unsigned int) helpMenuHandle;
      nsCOMPtr<nsIMenuListener> listener = do_QueryInterface(menu);
      listener->MenuSelected(event);
    }
  }
  ::DrawMenuBar();
  return NS_OK;
}


#pragma mark -

//
// nsIDocumentObserver
// this is needed for menubar changes
//


NS_IMETHODIMP
nsMenuBar::BeginUpdate( nsIDocument * aDocument )
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::EndUpdate( nsIDocument * aDocument )
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::BeginLoad( nsIDocument * aDocument )
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::EndLoad( nsIDocument * aDocument )
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::BeginReflow(  nsIDocument * aDocument, nsIPresShell * aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::EndReflow( nsIDocument * aDocument, nsIPresShell * aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::ContentChanged( nsIDocument * aDocument, nsIContent * aContent, nsISupports * aSubContent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::ContentStatesChanged( nsIDocument * aDocument, nsIContent  * aContent1, nsIContent  * aContent2)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::ContentAppended( nsIDocument * aDocument, nsIContent  * aContainer,
                              PRInt32 aNewIndexInContainer)
{
  if ( aContainer == mMenuBarContent ) {
    //Register(aContainer, );
    //InsertMenu ( aNewIndexInContainer );
  }
  else {
    nsCOMPtr<nsIChangeObserver> obs;
    Lookup ( aContainer, getter_AddRefs(obs) );
    if ( obs )
      obs->ContentInserted ( aDocument, aContainer, aNewIndexInContainer );
    else {
      nsCOMPtr<nsIContent> parent;
      aContainer->GetParent(*getter_AddRefs(parent));
      if(parent) {
        Lookup ( parent, getter_AddRefs(obs) );
        if ( obs )
          obs->ContentInserted ( aDocument, aContainer, aNewIndexInContainer );
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::ContentReplaced( nsIDocument * aDocument, nsIContent * aContainer, nsIContent * aOldChild,
                          nsIContent * aNewChild, PRInt32 aIndexInContainer)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::StyleSheetAdded( nsIDocument * aDocument, nsIStyleSheet * aStyleSheet)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::StyleSheetRemoved(nsIDocument * aDocument, nsIStyleSheet * aStyleSheet)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::StyleSheetDisabledStateChanged(nsIDocument * aDocument, nsIStyleSheet * aStyleSheet,
                                            PRBool aDisabled)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::StyleRuleChanged( nsIDocument * aDocument, nsIStyleSheet * aStyleSheet,
                              nsIStyleRule * aStyleRule, PRInt32 aHint)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::StyleRuleAdded( nsIDocument * aDocument, nsIStyleSheet * aStyleSheet,
                            nsIStyleRule * aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::StyleRuleRemoved(nsIDocument * aDocument, nsIStyleSheet * aStyleSheet,
                              nsIStyleRule  * aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::DocumentWillBeDestroyed( nsIDocument * aDocument )
{
  mDocument = nsnull; // just for yucks
  return NS_OK;
}


NS_IMETHODIMP
nsMenuBar::AttributeChanged( nsIDocument * aDocument, nsIContent * aContent, PRInt32 aNameSpaceID,
                              nsIAtom * aAttribute, PRInt32 aModType, PRInt32 aHint)
{
  // lookup and dispatch to registered thang.
  nsCOMPtr<nsIChangeObserver> obs;
  Lookup ( aContent, getter_AddRefs(obs) );
  if ( obs )
    obs->AttributeChanged ( aDocument, aNameSpaceID, aAttribute, aHint );

  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::ContentRemoved( nsIDocument * aDocument, nsIContent * aContainer,
                            nsIContent * aChild, PRInt32 aIndexInContainer )
{  
  if ( aContainer == mMenuBarContent ) {
    Unregister(aChild);
    RemoveMenu ( aIndexInContainer );
  }
  else {
    nsCOMPtr<nsIChangeObserver> obs;
    Lookup ( aContainer, getter_AddRefs(obs) );
    if ( obs )
      obs->ContentRemoved ( aDocument, aChild, aIndexInContainer );
    else {
      nsCOMPtr<nsIContent> parent;
      aContainer->GetParent(*getter_AddRefs(parent));
      if(parent) {
        Lookup ( parent, getter_AddRefs(obs) );
        if ( obs )
          obs->ContentRemoved ( aDocument, aChild, aIndexInContainer );
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::ContentInserted( nsIDocument * aDocument, nsIContent * aContainer,
                            nsIContent * aChild, PRInt32 aIndexInContainer )
{  
  if ( aContainer == mMenuBarContent ) {
    //Register(aChild, );
    //InsertMenu ( aIndexInContainer );
  }
  else {
    nsCOMPtr<nsIChangeObserver> obs;
    Lookup ( aContainer, getter_AddRefs(obs) );
    if ( obs )
      obs->ContentInserted ( aDocument, aChild, aIndexInContainer );
    else {
      nsCOMPtr<nsIContent> parent;
      aContainer->GetParent(*getter_AddRefs(parent));
      if(parent) {
        Lookup ( parent, getter_AddRefs(obs) );
        if ( obs )
          obs->ContentInserted ( aDocument, aChild, aIndexInContainer );
      }
    }
  }
  return NS_OK;
}

#pragma mark - 

//
// nsIChangeManager
//
// We don't use a |nsSupportsHashtable| because we know that the lifetime of all these items
// is bouded by the lifetime of the menubar. No need to add any more strong refs to the
// picture because the containment hierarchy already uses strong refs.
//

NS_IMETHODIMP 
nsMenuBar :: Register ( nsIContent *aContent, nsIChangeObserver *aMenuObject )
{
  nsVoidKey key ( aContent );
  mObserverTable.Put ( &key, aMenuObject );
  
  return NS_OK;
}


NS_IMETHODIMP 
nsMenuBar :: Unregister ( nsIContent *aContent )
{
  nsVoidKey key ( aContent );
  mObserverTable.Remove ( &key );
  
  return NS_OK;
}


NS_IMETHODIMP 
nsMenuBar :: Lookup ( nsIContent *aContent, nsIChangeObserver **_retval )
{
  *_retval = nsnull;
  
  nsVoidKey key ( aContent );
  *_retval = NS_REINTERPRET_CAST(nsIChangeObserver*, mObserverTable.Get(&key));
  NS_IF_ADDREF ( *_retval );
  
  return NS_OK;
}


#pragma mark -


//
// SetMenuItemText
//
// A utility routine for handling unicode->OS text conversions for setting the item
// text in a menu.
//
void 
MenuHelpers :: SetMenuItemText ( MenuHandle inMacMenuHandle, short inMenuItem, const nsString& inMenuString,
                                   const UnicodeToTextRunInfo inConverter )
{
  // ::TruncString() doesn't take the number of characters to truncate to, it takes a pixel with
  // to fit the string in. Ugh. I talked it over with sfraser and we couldn't come up with an 
  // easy way to compute what this should be given the system font, etc, so we're just going
  // to hard code it to something reasonable and bigger fonts will just have to deal.
  const short kMaxItemPixelWidth = 300;
  
	short themeFontID;
	SInt16 themeFontSize;
	Style themeFontStyle;
  char* scriptRunText = ConvertToScriptRun ( inMenuString, inConverter, &themeFontID,
                                               &themeFontSize, &themeFontStyle );
  if ( scriptRunText ) {    
    // convert it to a pascal string
    Str255 menuTitle;
    short scriptRunTextLength = strlen(scriptRunText);
    if (scriptRunTextLength > 255)
      scriptRunTextLength = 255;
    BlockMoveData(scriptRunText, &menuTitle[1], scriptRunTextLength);
    menuTitle[0] = scriptRunTextLength;
    
    // if the item text is too long, truncate it.
    ::TruncString ( kMaxItemPixelWidth, menuTitle, truncMiddle );
	  ::SetMenuItemText(inMacMenuHandle, inMenuItem, menuTitle);
	  OSErr err = ::SetMenuItemFontID(inMacMenuHandle, inMenuItem, themeFontID);	

  	nsMemory::Free(scriptRunText);
  }
  		
} // SetMenuItemText


//
// ConvertToScriptRun
//
// Converts unicode to a single script run and extract the relevant font information. The
// caller is responsible for deleting the memory allocated by this call with |nsMemory::Free()|.
// Returns |nsnull| if an error occurred.
//
char*
MenuHelpers :: ConvertToScriptRun ( const nsString & inStr, const UnicodeToTextRunInfo inConverter,
                                    short* outFontID, SInt16* outFontSize, Style* outFontStyle )
{
  //
  // extract the Unicode text from the nsString and convert it into a single script run
  //
  const PRUnichar* unicodeText = inStr.get();
  size_t unicodeTextLengthInBytes = inStr.Length() * sizeof(PRUnichar);
  size_t scriptRunTextSizeInBytes = unicodeTextLengthInBytes * 2;
  char* scriptRunText = NS_REINTERPRET_CAST(char*, nsMemory::Alloc(scriptRunTextSizeInBytes + sizeof(char)));
  if ( !scriptRunText )
    return nsnull;
    
  ScriptCodeRun convertedTextScript;
  size_t unicdeTextReadInBytes, scriptRunTextLengthInBytes, scriptCodeRunListLength;
  OSErr err = ::ConvertFromUnicodeToScriptCodeRun(inConverter, unicodeTextLengthInBytes,
                  NS_REINTERPRET_CAST(const PRUint16*, unicodeText),
                  0, /* no flags*/
                  0,NULL,NULL,NULL, /* no offset arrays */
                  scriptRunTextSizeInBytes,&unicdeTextReadInBytes,&scriptRunTextLengthInBytes,
                  scriptRunText,
                  1 /* count of script runs*/,&scriptCodeRunListLength,&convertedTextScript);
  NS_ASSERTION(err==noErr,"nsMenu::NSStringSetMenuItemText: ConvertFromUnicodeToScriptCodeRun failed.");
  if ( err ) { nsMemory::Free(scriptRunText); return nsnull; }
  scriptRunText[scriptRunTextLengthInBytes] = '\0';	// null terminate
	
  //
  // get a font from the script code
  //
  Str255 themeFontName;
  err = ::GetThemeFont(kThemeSystemFont, convertedTextScript.script, themeFontName, outFontSize, outFontStyle);
  NS_ASSERTION(err==noErr,"nsMenu::NSStringSetMenuItemText: GetThemeFont failed.");
  if ( err ) { nsMemory::Free(scriptRunText); return nsnull; }
  ::GetFNum(themeFontName, outFontID);

  return scriptRunText;
                              
} // ConvertToScriptRun


//
// WebShellToPresContext
//
// Helper to dig out a pres context from a webshell. A common thing to do before
// sending an event into the dom.
//
nsresult
MenuHelpers :: WebShellToPresContext (nsIWebShell* inWebShell, nsIPresContext** outContext )
{
  NS_ENSURE_ARG_POINTER(outContext);
  *outContext = nsnull;
  if (!inWebShell)
    return NS_ERROR_INVALID_ARG;
  
  nsresult retval = NS_OK;
  
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(inWebShell));

  nsCOMPtr<nsIContentViewer> contentViewer;
  docShell->GetContentViewer(getter_AddRefs(contentViewer));
  if ( contentViewer ) {
    nsCOMPtr<nsIDocumentViewer> docViewer ( do_QueryInterface(contentViewer) );
    if ( docViewer )
      docViewer->GetPresContext(*outContext);     // AddRefs for us
    else
      retval = NS_ERROR_FAILURE;
  }
  else
    retval = NS_ERROR_FAILURE;
  
  return retval;
  
} // WebShellToPresContext



