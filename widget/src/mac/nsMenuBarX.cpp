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

#include "nsMenuBarX.h"
#include "nsMenuX.h"
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

nsWeakPtr    gMacMenubarX;

#if !TARGET_CARBON
extern nsMenuStack          gPreviousMenuStack;
#endif

// CIDs
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kMenuBarCID, NS_MENUBAR_CID);
static NS_DEFINE_CID(kMenuCID, NS_MENU_CID);
static NS_DEFINE_CID(kMenuItemCID, NS_MENUITEM_CID);

PRInt32   gMenuBarCounterX = 0;


NS_IMPL_ISUPPORTS5(nsMenuBarX, nsIMenuBar, nsIMenuListener, nsIDocumentObserver, nsIChangeManager, nsISupportsWeakReference)

//
// nsMenuBarX constructor
//
nsMenuBarX::nsMenuBarX()
{
    NS_INIT_REFCNT();
    mNumMenus       = 0;
    mParent         = nsnull;
    mIsMenuBarAdded = PR_FALSE;
    mMacMBarHandle = nsnull;
    ++gMenuBarCounterX;

    // if there's already a menu bar, save off its contents, so we have a clean slate.
    if (gMacMenubarX) {
        nsCOMPtr<nsIMenuBar> menuBar = do_QueryReferent(gMacMenubarX);
        if (menuBar) menuBar->SetNativeData(::GetMenuBar());
        gMacMenubarX = nsnull;
    }
    ::ClearMenuBar();
}

//
// nsMenuBarX destructor
//
nsMenuBarX::~nsMenuBarX()
{
    mMenusArray.Clear();    // release all menus

    // make sure we unregister ourselves as a document observer
    nsCOMPtr<nsIWebShell> webShell ( do_QueryReferent(mWebShellWeakRef) );
    nsCOMPtr<nsIDocument> doc;
    GetDocument(webShell, getter_AddRefs(doc));
    if ( doc ) {
        nsCOMPtr<nsIDocumentObserver> observer ( do_QueryInterface(NS_STATIC_CAST(nsIMenuBar*,this)) );
        doc->RemoveObserver(observer);
    }

    --gMenuBarCounterX;
    if (gMenuBarCounterX == 0)
        ::ClearMenuBar();
    
    if (mMacMBarHandle)
        ::DisposeHandle(mMacMBarHandle);
}

nsEventStatus 
nsMenuBarX::MenuItemSelected(const nsMenuEvent & aMenuEvent)
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
nsMenuBarX::MenuSelected(const nsMenuEvent & aMenuEvent)
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
nsMenuBarX::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus 
nsMenuBarX::CheckRebuild(PRBool & aNeedsRebuild)
{
  aNeedsRebuild = PR_TRUE;
  return nsEventStatus_eIgnore;
}

nsEventStatus
nsMenuBarX::SetRebuild(PRBool aNeedsRebuild)
{
  return nsEventStatus_eIgnore;
}

void
nsMenuBarX :: GetDocument ( nsIWebShell* inWebShell, nsIDocument** outDocument )
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
nsMenuBarX :: RegisterAsDocumentObserver ( nsIWebShell* inWebShell )
{
    nsCOMPtr<nsIDocument> doc;
    GetDocument(inWebShell, getter_AddRefs(doc));
    if (!doc)
    return;

    // register ourselves
    nsCOMPtr<nsIDocumentObserver> observer ( do_QueryInterface(NS_STATIC_CAST(nsIMenuBar*,this)) );
    doc->AddObserver(observer);
} // RegisterAsDocumentObesrver


nsEventStatus
nsMenuBarX::MenuConstruct( const nsMenuEvent & aMenuEvent, nsIWidget* aParentWindow, 
                            void * menubarNode, void * aWebShell )
{
    gMacMenubarX = getter_AddRefs(NS_GetWeakReference((nsIMenuBar*)this));

    mWebShellWeakRef = getter_AddRefs(NS_GetWeakReference(NS_STATIC_CAST(nsIWebShell*, aWebShell)));
    mDOMNode  = NS_STATIC_CAST(nsIDOMNode*, menubarNode);   // strong ref

    Create(aParentWindow);

    nsCOMPtr<nsIWebShell> webShell = do_QueryReferent(mWebShellWeakRef);
    if (webShell) RegisterAsDocumentObserver(webShell);

    // set this as a nsMenuListener on aParentWindow
    aParentWindow->AddMenuListener((nsIMenuListener *)this);

    nsCOMPtr<nsIDOMNode> menuNode;
    mDOMNode->GetFirstChild(getter_AddRefs(menuNode));
    while (menuNode) {
        nsCOMPtr<nsIDOMElement> menuElement(do_QueryInterface(menuNode));
        if (menuElement) {
            nsAutoString menuNodeType;
            nsAutoString menuName;
            nsAutoString menuAccessKey; menuAccessKey.AssignWithConversion(" ");

            menuElement->GetNodeName(menuNodeType);
            if (menuNodeType == NS_LITERAL_STRING("menu")) {
                menuElement->GetAttribute(NS_LITERAL_STRING("value"), menuName);
                menuElement->GetAttribute(NS_LITERAL_STRING("accesskey"), menuAccessKey);

                // Don't create the whole menu yet, just add in the top level names

                // Create nsMenu, the menubar will own it
                nsCOMPtr<nsIMenu> pnsMenu ( do_CreateInstance(kMenuCID) );
                if ( pnsMenu ) {
                    pnsMenu->Create(NS_STATIC_CAST(nsIMenuBar*, this), menuName, menuAccessKey, 
                    NS_STATIC_CAST(nsIChangeManager *, this), 
                    NS_REINTERPRET_CAST(nsIWebShell*, aWebShell), menuNode);

                    // Make nsMenu a child of nsMenuBarX. nsMenuBarX takes ownership
                    AddMenu(pnsMenu); 

                    nsAutoString menuIDstring;
                    menuElement->GetAttribute(NS_LITERAL_STRING("id"), menuIDstring);
                    if (menuIDstring == NS_LITERAL_STRING("menu_Help")) {
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

    // Give the aParentWindow this nsMenuBarX to hold onto.
    // The parent takes ownership
    aParentWindow->SetMenuBar(this);

    // Get a copy of the menu list
    SetNativeData(::GetMenuBar());

    return nsEventStatus_eIgnore;
}


nsEventStatus 
nsMenuBarX::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}


//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::Create(nsIWidget *aParent)
{
  SetParent(aParent);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::GetParent(nsIWidget *&aParent)
{
  NS_IF_ADDREF(aParent = mParent);
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::SetParent(nsIWidget *aParent)
{
  mParent = aParent;    // weak ref  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::AddMenu(nsIMenu * aMenu)
{
  // XXX add to internal data structure
  nsCOMPtr<nsISupports> supports = do_QueryInterface(aMenu);
  if(supports)
    mMenusArray.AppendElement(supports);    // owner

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
    domElement->GetAttribute(NS_LITERAL_STRING("hidden"), menuHidden);
    if( menuHidden != NS_LITERAL_STRING("true"))
      ::InsertMenu(menuHandle, 0);
  }
  
  return NS_OK;
}

                                
//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::GetMenuCount(PRUint32 &aCount)
{
  aCount = mNumMenus;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::GetMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{ 
  aMenu = NULL;
  nsCOMPtr<nsISupports> supports = getter_AddRefs(mMenusArray.ElementAt(aCount));
  if (!supports) return NS_OK;
  
  return CallQueryInterface(supports, &aMenu); // addref
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::InsertMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::RemoveMenu(const PRUint32 aCount)
{
  mMenusArray.RemoveElementAt(aCount);
  ::DrawMenuBar();
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::RemoveAll()
{
  NS_ASSERTION(0, "Not implemented!");
  // mMenusArray.Clear();    // maybe?
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::GetNativeData(void *& aData)
{
  aData = (void *) mMacMBarHandle;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::SetNativeData(void* aData)
{
  Handle  menubarHandle = (Handle)aData;
  
  if(mMacMBarHandle && mMacMBarHandle != menubarHandle)
     ::DisposeHandle(mMacMBarHandle);
  
  mMacMBarHandle = menubarHandle;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::Paint()
{
    // hack to correctly swap menu bars.
    nsCOMPtr<nsIMenuBar> menuBar = do_QueryReferent(gMacMenubarX);
    if (menuBar.get() != (nsIMenuBar*)this) {
        if (menuBar)
            menuBar->SetNativeData(::GetMenuBar());
        gMacMenubarX = getter_AddRefs(NS_GetWeakReference((nsIMenuBar*)this));
        if (mMacMBarHandle)
            ::SetMenuBar(mMacMBarHandle);
    }

#if !TARGET_CARBON
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
            ::HMGetHelpMenuHandle(&helpMenuHandle);
            menu->SetNativeData((void*)helpMenuHandle);
            nsMenuEvent event;
            event.mCommand = (unsigned int) helpMenuHandle;
            nsCOMPtr<nsIMenuListener> listener = do_QueryInterface(menu);
            listener->MenuSelected(event);
        }
    }
#endif

    ::DrawMenuBar();
    return NS_OK;
}

#pragma mark -

//
// nsIDocumentObserver
// this is needed for menubar changes
//


NS_IMETHODIMP
nsMenuBarX::BeginUpdate( nsIDocument * aDocument )
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::EndUpdate( nsIDocument * aDocument )
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::BeginLoad( nsIDocument * aDocument )
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::EndLoad( nsIDocument * aDocument )
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::BeginReflow(  nsIDocument * aDocument, nsIPresShell * aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::EndReflow( nsIDocument * aDocument, nsIPresShell * aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::ContentChanged( nsIDocument * aDocument, nsIContent * aContent, nsISupports * aSubContent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::ContentStatesChanged( nsIDocument * aDocument, nsIContent  * aContent1, nsIContent  * aContent2)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::ContentAppended( nsIDocument * aDocument, nsIContent  * aContainer,
                              PRInt32 aNewIndexInContainer)
{
 nsCOMPtr<nsIContent> me ( do_QueryInterface(mDOMNode) );
  if ( aContainer == me.get() ) {
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
nsMenuBarX::ContentReplaced( nsIDocument * aDocument, nsIContent * aContainer, nsIContent * aOldChild,
                          nsIContent * aNewChild, PRInt32 aIndexInContainer)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::StyleSheetAdded( nsIDocument * aDocument, nsIStyleSheet * aStyleSheet)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::StyleSheetRemoved(nsIDocument * aDocument, nsIStyleSheet * aStyleSheet)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::StyleSheetDisabledStateChanged(nsIDocument * aDocument, nsIStyleSheet * aStyleSheet,
                                            PRBool aDisabled)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::StyleRuleChanged( nsIDocument * aDocument, nsIStyleSheet * aStyleSheet,
                              nsIStyleRule * aStyleRule, PRInt32 aHint)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::StyleRuleAdded( nsIDocument * aDocument, nsIStyleSheet * aStyleSheet,
                            nsIStyleRule * aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::StyleRuleRemoved(nsIDocument * aDocument, nsIStyleSheet * aStyleSheet,
                              nsIStyleRule  * aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarX::DocumentWillBeDestroyed( nsIDocument * aDocument )
{
  return NS_OK;
}


NS_IMETHODIMP
nsMenuBarX::AttributeChanged( nsIDocument * aDocument, nsIContent * aContent, PRInt32 aNameSpaceID,
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
nsMenuBarX::ContentRemoved( nsIDocument * aDocument, nsIContent * aContainer,
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
nsMenuBarX::ContentInserted( nsIDocument * aDocument, nsIContent * aContainer,
                            nsIContent * aChild, PRInt32 aIndexInContainer )
{  
  nsCOMPtr<nsIContent> me ( do_QueryInterface(mDOMNode) );
  if ( aContainer == me.get() ) {
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
nsMenuBarX :: Register ( nsIContent *aContent, nsIChangeObserver *aMenuObject )
{
  nsVoidKey key ( aContent );
  mObserverTable.Put ( &key, aMenuObject );
  
  return NS_OK;
}


NS_IMETHODIMP 
nsMenuBarX :: Unregister ( nsIContent *aContent )
{
  nsVoidKey key ( aContent );
  mObserverTable.Remove ( &key );
  
  return NS_OK;
}


NS_IMETHODIMP 
nsMenuBarX :: Lookup ( nsIContent *aContent, nsIChangeObserver **_retval )
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
MenuHelpersX::SetMenuItemText ( MenuHandle inMacMenuHandle, short inMenuItem, const nsString& inMenuString,
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
MenuHelpersX::ConvertToScriptRun ( const nsString & inStr, const UnicodeToTextRunInfo inConverter,
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
MenuHelpersX::WebShellToPresContext (nsIWebShell* inWebShell, nsIPresContext** outContext )
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



