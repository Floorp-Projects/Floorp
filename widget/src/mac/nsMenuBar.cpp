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

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIContent.h"

#include "nsMenuBar.h"
#include "nsDynamicMDEF.h"

#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include "nsIStringBundle.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentObserver.h"

#include "nsIDOMXULDocument.h"

#include <Menus.h>
#include <TextUtils.h>
#include <Balloons.h>
#include <Traps.h>
#include <Resources.h>
#include <Appearance.h>
#include "nsMacResources.h"


static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);


#pragma options align=mac68k
typedef struct {
	short 	jmpInstr;
	Ptr 	jmpAddr;
} JmpRecord, *JmpPtr, **JmpHandle;
#pragma options align=reset

Handle       gMDEF = nsnull;
Handle       gSystemMDEFHandle = nsnull;
nsWeakPtr    gMacMenubar;
bool         gFirstMenuBar = true; 

// The four Golden Hierarchical Child Menus
MenuHandle gLevel2HierMenu = nsnull;
MenuHandle gLevel3HierMenu = nsnull;
MenuHandle gLevel4HierMenu = nsnull;
MenuHandle gLevel5HierMenu = nsnull;

#if !TARGET_CARBON
extern nsMenuStack          gPreviousMenuStack;
#endif
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


#if DEBUG
nsInstanceCounter   gMenuBarCounter("nsMenuBar");
#endif


NS_IMPL_ISUPPORTS5(nsMenuBar, nsIMenuBar, nsIMenuListener, nsIDocumentObserver, nsIChangeManager, nsISupportsWeakReference)

//
// nsMenuBar constructor
//
nsMenuBar::nsMenuBar()
{
  gCurrentMenuDepth = 1;
  
#if !TARGET_CARBON
  nsPreviousMenuStackUnwind(nsnull, nsnull);
#endif
  NS_INIT_REFCNT();
  mNumMenus       = 0;
  mParent         = nsnull;
  mIsMenuBarAdded = PR_FALSE;
  mUnicodeTextRunConverter = nsnull;
  
#if !TARGET_CARBON
  MenuDefUPP mdef = NewMenuDefProc( nsDynamicMDEFMain );
  InstallDefProc((short)nsMacResources::GetLocalResourceFile(), (ResType)'MDEF', (short)128, (Ptr) mdef );
#endif

  mOriginalMacMBarHandle = nsnull;
  mMacMBarHandle = nsnull;
  
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

#if DEBUG
  ++gMenuBarCounter;
#endif
}


//
// nsMenuBar destructor
//
nsMenuBar::~nsMenuBar()
{
  mMenusArray.Clear();    // release all menus
  
  OSErr err = ::DisposeUnicodeToTextRunInfo(&mUnicodeTextRunConverter);
  NS_ASSERTION(err==noErr,"nsMenu::~nsMenu: DisposeUnicodeToTextRunInfo failed.");	 

  ::DisposeHandle(mMacMBarHandle);
  ::DisposeHandle(mOriginalMacMBarHandle);

#if DEBUG
  --gMenuBarCounter;
#endif
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
  //((nsISupports*)mMenuVoidArray[i-1])->QueryInterface(NS_GET_IID(nsIMenuListener), (void**)&menuListener);
  //printf("gPreviousMenuStack.Count() = %d \n", gPreviousMenuStack.Count());
#if !TARGET_CARBON
  nsCOMPtr<nsIMenu> theMenu;
  gPreviousMenuStack.GetMenuAt(gPreviousMenuStack.Count() - 1, getter_AddRefs(theMenu));
  menuListener = do_QueryInterface(theMenu);
#endif
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


//
// RegisterAsDocumentObserver
//
// Name says it all.
//
void
nsMenuBar :: RegisterAsDocumentObserver ( nsIWebShell* inWebShell )
{
  nsCOMPtr<nsIDocShell> docShell ( do_QueryInterface(inWebShell) );
  nsCOMPtr<nsIContentViewer> cv;
  docShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
   
    // get the document
    nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
    if (!docv)
      return;
    nsCOMPtr<nsIDocument> doc;
    docv->GetDocument(*getter_AddRefs(doc));
    if (!doc)
      return;

    // register ourselves
    nsCOMPtr<nsIDocumentObserver> observer ( do_QueryInterface(NS_STATIC_CAST(nsIMenuBar*,this)) );
    doc->AddObserver(observer);
  }

} // RegisterAsDocumentObesrver


nsEventStatus
nsMenuBar::MenuConstruct( const nsMenuEvent & aMenuEvent, nsIWidget* aParentWindow, 
                            void * menubarNode, void * aWebShell )
{
  mWebShellWeakRef = getter_AddRefs(NS_GetWeakReference(NS_STATIC_CAST(nsIWebShell*, aWebShell)));
  mDOMNode  = NS_STATIC_CAST(nsIDOMNode*, menubarNode);   // strong ref
      
  if(gFirstMenuBar) {
	  gFirstMenuBar = false;
	  // Add the 4 Golden Hierarchical Menus to the MenuList        
				
	  MenuHandle macMenuHandle = ::NewMenu(2, "\psubmenu");
	  if(macMenuHandle) { 
	    gLevel2HierMenu = macMenuHandle;
#if !TARGET_CARBON
	    SInt8 state = ::HGetState((Handle)macMenuHandle);
	    ::HLock((Handle)macMenuHandle);
			gSystemMDEFHandle = (**macMenuHandle).menuProc;
			(**macMenuHandle).menuProc = gMDEF;
			(**macMenuHandle).menuWidth = -1;
			(**macMenuHandle).menuHeight = -1;
			::HSetState((Handle)macMenuHandle, state);
#endif
			::InsertMenu(macMenuHandle, hierMenu);
    }
			
    macMenuHandle = ::NewMenu(3, "\psubmenu");
	  if(macMenuHandle) { 
	    gLevel3HierMenu = macMenuHandle;
#if !TARGET_CARBON
	    SInt8 state = ::HGetState((Handle)macMenuHandle);
	    ::HLock((Handle)macMenuHandle);
		  gSystemMDEFHandle = (**macMenuHandle).menuProc;
		  (**macMenuHandle).menuProc = gMDEF;
		  (**macMenuHandle).menuWidth = -1;
		  (**macMenuHandle).menuHeight = -1;
		  ::HSetState((Handle)macMenuHandle, state);
#endif
		  ::InsertMenu(macMenuHandle, hierMenu);
    }
			
    macMenuHandle = ::NewMenu(4, "\psubmenu");
	  if(macMenuHandle) { 
	    gLevel4HierMenu = macMenuHandle;
#if !TARGET_CARBON
	    SInt8 state = ::HGetState((Handle)macMenuHandle);
	    ::HLock((Handle)macMenuHandle);
		  gSystemMDEFHandle = (**macMenuHandle).menuProc;
		  (**macMenuHandle).menuProc = gMDEF;
		  (**macMenuHandle).menuWidth = -1;
		  (**macMenuHandle).menuHeight = -1;
		  ::HSetState((Handle)macMenuHandle, state);
#endif
		  ::InsertMenu(macMenuHandle, hierMenu);
    }
			
    macMenuHandle = ::NewMenu(5, "\psubmenu");
	  if(macMenuHandle) { 
	    gLevel5HierMenu = macMenuHandle; 
#if !TARGET_CARBON
	    SInt8 state = ::HGetState((Handle)macMenuHandle);
	    ::HLock((Handle)macMenuHandle);
			gSystemMDEFHandle = (**macMenuHandle).menuProc;
			(**macMenuHandle).menuProc = gMDEF;
			(**macMenuHandle).menuWidth = -1;
			(**macMenuHandle).menuHeight = -1;
		  ::HSetState((Handle)macMenuHandle, state);
#endif
		  ::InsertMenu(macMenuHandle, hierMenu);
		}
	} else {
	  ::InsertMenu(gLevel2HierMenu, hierMenu);
		::InsertMenu(gLevel3HierMenu, hierMenu);
		::InsertMenu(gLevel4HierMenu, hierMenu);
		::InsertMenu(gLevel5HierMenu, hierMenu);
	}
    
  Create(aParentWindow);

  nsCOMPtr<nsIWebShell>    webShell = do_QueryReferent(mWebShellWeakRef);
  if (webShell)
    RegisterAsDocumentObserver(webShell);
      
  // set this as a nsMenuListener on aParentWindow
  aParentWindow->AddMenuListener((nsIMenuListener *)this);
         
  nsCOMPtr<nsIDOMNode> menuNode;
  mDOMNode->GetFirstChild(getter_AddRefs(menuNode));
  while (menuNode)
  {
    nsCOMPtr<nsIDOMElement> menuElement(do_QueryInterface(menuNode));
    if (menuElement)
    {
      nsAutoString menuNodeType;
      nsAutoString menuName;
			nsAutoString menuAccessKey; menuAccessKey.AssignWithConversion(" ");
			
      menuElement->GetNodeName(menuNodeType);
      if (menuNodeType.EqualsWithConversion("menu")) {
        menuElement->GetAttribute(NS_ConvertASCIItoUCS2("value"), menuName);
			  menuElement->GetAttribute(NS_ConvertASCIItoUCS2("accesskey"), menuAccessKey);
			  
        // Don't create the whole menu yet, just add in the top level names
              
        // Create nsMenu, the menubar will own it
        nsCOMPtr<nsIMenu> pnsMenu ( do_CreateInstance(kMenuCID) );
        if ( pnsMenu )
        {
          pnsMenu->Create(NS_STATIC_CAST(nsIMenuBar*, this), menuName, menuAccessKey, 
                          NS_STATIC_CAST(nsIChangeManager *, this), 
                          NS_REINTERPRET_CAST(nsIWebShell*, aWebShell), menuNode);

          // Make nsMenu a child of nsMenuBar. nsMenuBar takes ownership
          AddMenu(pnsMenu); 
                  
          nsAutoString menuIDstring;
          menuElement->GetAttribute(NS_ConvertASCIItoUCS2("id"), menuIDstring);
          if(menuIDstring.EqualsWithConversion("menu_Help")) {
            nsMenuEvent event;
            MenuHandle handle = nsnull;
#if !(defined(RHAPSODY) || defined(TARGET_CARBON))
            ::HMGetHelpMenuHandle(&handle);
#endif
            event.mCommand = (unsigned int) handle;
            nsCOMPtr<nsIMenuListener> listener(do_QueryInterface(pnsMenu));
            listener->MenuSelected(event);
          }          
        }
      } 
    }
    nsCOMPtr<nsIDOMNode> tempNode = menuNode;  
    tempNode->GetNextSibling(getter_AddRefs(menuNode));
  } // end while (nsnull != menuNode)
        
  // Give the aParentWindow this nsMenuBar to hold onto.
  // The parent takes ownership
  aParentWindow->SetMenuBar(this);
		
#ifdef XP_MAC
  Handle tempMenuBar = ::GetMenuBar(); // Get a copy of the menu list
	SetNativeData((void*)tempMenuBar);
#endif
		
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
	  nsresult ret;
	    nsCOMPtr<nsIStringBundleService> pStringService = do_GetService(kStringBundleServiceCID, &ret);
      if (NS_FAILED(ret)) {
        NS_WARNING("cannot get string service\n");
        return ret;
      }
      
      //XXX "chrome://global/locale/brand.properties" should be less hardcoded
      nsCOMPtr<nsIStringBundle> bundle;
      ret = pStringService->CreateBundle("chrome://global/locale/brand.properties", nsnull, getter_AddRefs(bundle));
      if (NS_FAILED(ret)) {
        NS_WARNING("cannot create instance\n");
        return ret;
      }      
      
      //XXX "aboutStrName" should be less hardcoded
      PRUnichar *ptrv = nsnull;
      bundle->GetStringFromName(NS_ConvertASCIItoUCS2("aboutStrName").GetUnicode(), &ptrv);
      nsAutoString label(ptrv);
		  nsCRT::free(ptrv);

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
    nsCOMPtr<nsIDOMNode> domNode;
    aMenu->GetDOMNode(getter_AddRefs(domNode));
    nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(domNode);
    nsAutoString menuHidden;
    domElement->GetAttribute(NS_ConvertASCIItoUCS2("hidden"), menuHidden);
    if(! menuHidden.EqualsWithConversion("true"))
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
      MenuHandle helpMenuHandle;
#if !TARGET_CARBON
      ::HMGetHelpMenuHandle(&helpMenuHandle);
      menu->SetNativeData((void*)helpMenuHandle);
#endif 
      nsMenuEvent event;
      event.mCommand = (unsigned int) helpMenuHandle;
      nsCOMPtr<nsIMenuListener> listener = do_QueryInterface(menu);
      listener->MenuSelected(event);
    }
  }
  ::DrawMenuBar();
  return NS_OK;
}


//-------------------------------------------------------------------------
void InstallDefProc(
  short dpPath, 
  ResType dpType, 
  short dpID, 
  Ptr dpAddr)
{
	JmpHandle 	jH;
	short 	savePath;

	savePath = CurResFile();
	UseResFile(dpPath);

	jH = (JmpHandle)GetResource(dpType, dpID);
	DetachResource((Handle)jH);
	gMDEF = (Handle) jH;
	UseResFile(savePath);

	if (!jH)				/* is there no defproc resource? */\
	{
#if DEBUG
			DebugStr("\pStub Defproc Not Found!");
#endif
      ExitToShell();    // bail
  }
  
	HNoPurge((Handle)jH);	/* make this resource nonpurgeable */
	(**jH).jmpAddr = dpAddr;
	(**jH).jmpInstr = 0x4EF9;
	//FlushCache();

	HLockHi((Handle)jH);
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
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBar::ContentInserted( nsIDocument * aDocument, nsIContent  * aContainer,
                          nsIContent  * aChild, PRInt32 aIndexInContainer)
{
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
  return NS_OK;
}


NS_IMETHODIMP
nsMenuBar::AttributeChanged( nsIDocument * aDocument, nsIContent * aContent, PRInt32 aNameSpaceID,
                              nsIAtom * aAttribute, PRInt32 aHint)
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
  nsCOMPtr<nsIContent> me ( do_QueryInterface(mDOMNode) );
  if ( aContainer == me.get() ) {
    Unregister(aChild);
    RemoveMenu ( aIndexInContainer );
  }
  else {
    nsCOMPtr<nsIChangeObserver> obs;
    Lookup ( aContainer, getter_AddRefs(obs) );
    if ( obs )
      obs->ContentRemoved ( aDocument, aChild, aIndexInContainer );
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
  const PRUnichar* unicodeText = inStr.GetUnicode();
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



