/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Josh Aas <josh@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentObserver.h"
#include "nsIComponentManager.h"
#include "nsIDocShell.h"
#include "prinrval.h"
#include "nsIRollupListener.h"

#include "nsMenuX.h"
#include "nsMenuItemIconX.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "nsIMenuCommandDispatcher.h"
#include "nsToolkit.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "plstr.h"

#include "nsINameSpaceManager.h"
#include "nsWidgetAtoms.h"
#include "nsIXBLService.h"
#include "nsIServiceManager.h"

#include "nsGUIEvent.h"

#include "nsCRT.h"

#include "jsapi.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"

// externs defined in nsChildView.mm
extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;

static PRBool gConstructingMenu = PR_FALSE;

// CIDs
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kMenuCID,     NS_MENU_CID);
static NS_DEFINE_CID(kMenuItemCID, NS_MENUITEM_CID);

// Refcounted class for dummy menu items like separators
class nsDummyMenuItemX : public nsISupports {
public:
    NS_DECL_ISUPPORTS

    nsDummyMenuItemX()
    {
    }
};

NS_IMETHODIMP_(nsrefcnt) nsDummyMenuItemX::AddRef() { return ++mRefCnt; }
NS_IMETHODIMP nsDummyMenuItemX::Release() { return --mRefCnt; }
NS_IMPL_QUERY_INTERFACE0(nsDummyMenuItemX)
static nsDummyMenuItemX gDummyMenuItemX;


NS_IMPL_ISUPPORTS4(nsMenuX, nsIMenu, nsIMenuListener, nsIChangeObserver, nsISupportsWeakReference)


nsMenuX::nsMenuX()
: mParent(nsnull), mManager(nsnull), mMacMenuID(0), mMacMenu(nil),
  mIsEnabled(PR_TRUE), mDestroyHandlerCalled(PR_FALSE),
  mNeedsRebuild(PR_TRUE), mConstructed(PR_FALSE), mVisible(PR_TRUE),
  mXBLAttached(PR_FALSE)
{
  mMenuDelegate = [[MenuDelegate alloc] initWithGeckoMenu:this];
    
  if (!nsMenuBarX::sNativeEventTarget)
    nsMenuBarX::sNativeEventTarget = [[NativeMenuItemTarget alloc] init];
}


nsMenuX::~nsMenuX()
{
  RemoveAll();

  [mMacMenu release];
  [mMenuDelegate release];
  
  // alert the change notifier we don't care no more
  mManager->Unregister(mMenuContent);
}


NS_IMETHODIMP 
nsMenuX::Create(nsISupports * aParent, const nsAString &aLabel, const nsAString &aAccessKey, 
                nsIChangeManager* aManager, nsIDocShell* aShell, nsIContent* aNode)
{
  mDocShellWeakRef = do_GetWeakReference(aShell);
  mMenuContent = aNode;

  // register this menu to be notified when changes are made to our content object
  mManager = aManager; // weak ref
  nsCOMPtr<nsIChangeObserver> changeObs(do_QueryInterface(NS_STATIC_CAST(nsIChangeObserver*, this)));
  mManager->Register(mMenuContent, changeObs);

  NS_ASSERTION(mMenuContent, "Menu not given a dom node at creation time");
  NS_ASSERTION(mManager, "No change manager given, can't tell content model updates");

  mParent = aParent;
  // our parent could be either a menu bar (if we're toplevel) or a menu (if we're a submenu)
  nsCOMPtr<nsIMenuBar> menubar = do_QueryInterface(aParent);
  nsCOMPtr<nsIMenu> menu = do_QueryInterface(aParent);
  NS_ASSERTION(menubar || menu, "Menu parent not a menu bar or menu!");

  // SetLabel will create the native menu if it has not been created yet
  SetLabel(aLabel);

  if (NodeIsHiddenOrCollapsed(mMenuContent))
    mVisible = PR_FALSE;

  if (menubar && mMenuContent->GetChildCount() == 0)
    mVisible = PR_FALSE;

  // We call MenuConstruct here because keyboard commands are dependent upon
  // native menu items being created. If we only call MenuConstruct when a menu
  // is actually selected, then we can't access keyboard commands until the
  // menu gets selected, which is bad.
  nsMenuEvent fake(PR_TRUE, 0, nsnull);
  MenuConstruct(fake, nsnull, nsnull, nsnull);
  
  if (menu)
    mIcon = new nsMenuItemIconX(NS_STATIC_CAST(nsIMenu*, this), menu, mMenuContent);

  return NS_OK;
}


NS_IMETHODIMP nsMenuX::GetParent(nsISupports*& aParent)
{
  aParent = mParent;
  NS_IF_ADDREF(aParent);
  return NS_OK;
}


NS_IMETHODIMP nsMenuX::GetLabel(nsString &aText)
{
  aText = mLabel;
  return NS_OK;
}


NS_IMETHODIMP nsMenuX::SetLabel(const nsAString &aText)
{
  mLabel = aText;
  // create an actual NSMenu if this is the first time
  if (mMacMenu == nil)
    mMacMenu = CreateMenuWithGeckoString(mLabel);
  return NS_OK;
}


NS_IMETHODIMP nsMenuX::GetAccessKey(nsString &aText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMenuX::SetAccessKey(const nsAString &aText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// This should only be used internally by our menu implementation. In all other
// cases menus and their items should be added and modified via the DOM.
NS_IMETHODIMP nsMenuX::AddItem(nsISupports* aItem)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (!aItem)
    return NS_ERROR_INVALID_ARG;

  // Figure out what we're adding
  nsCOMPtr<nsIMenuItem> menuItem(do_QueryInterface(aItem, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = AddMenuItem(menuItem);
  }
  else {
    nsCOMPtr<nsIMenu> menu(do_QueryInterface(aItem, &rv));
    if (NS_SUCCEEDED(rv))
      rv = AddMenu(menu);
  }

  return rv;
}


nsresult nsMenuX::AddMenuItem(nsIMenuItem * aMenuItem)
{
  if (!aMenuItem)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIContent> menuItemContent;
  aMenuItem->GetMenuItemContent(getter_AddRefs(menuItemContent));
  if (menuItemContent && NodeIsHiddenOrCollapsed(menuItemContent)) {
    mHiddenMenuItemsArray.AppendObject(aMenuItem); // owning ref
    return NS_OK;
  }
  else {
    mMenuItemsArray.AppendObject(aMenuItem); // owning ref
  }

  // add the menu item to this menu
  NSMenuItem* newNativeMenuItem;
  aMenuItem->GetNativeData((void*&)newNativeMenuItem);
  if (!newNativeMenuItem)
    return NS_ERROR_FAILURE;
  [mMacMenu addItem:newNativeMenuItem];

  // set up target/action
  [newNativeMenuItem setTarget:nsMenuBarX::sNativeEventTarget];
  [newNativeMenuItem setAction:@selector(menuItemHit:)];
  
  // set its command. we get the unique command id from the menubar
  nsCOMPtr<nsIMenuCommandDispatcher> dispatcher(do_QueryInterface(mManager));
  if (dispatcher) {
    PRUint32 commandID = 0L;
    dispatcher->Register(aMenuItem, &commandID);
    if (commandID)
      [newNativeMenuItem setTag:commandID];
  }
  
  return NS_OK;
}


nsresult nsMenuX::AddMenu(nsIMenu * aMenu)
{
  // Add a submenu
  if (!aMenu)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIContent> menuContent;
  aMenu->GetMenuContent(getter_AddRefs(menuContent));
  if (menuContent && NodeIsHiddenOrCollapsed(menuContent)) {
    mHiddenMenuItemsArray.AppendObject(aMenu); // owning ref
    return NS_OK;
  }
  else {
    mMenuItemsArray.AppendObject(aMenu); // owning ref
  }

  // We have to add a menu item and then associate the menu with it
  nsAutoString label;
  aMenu->GetLabel(label);
  PRBool enabled;
  aMenu->GetEnabled(&enabled);
  NSString *newCocoaLabelString = MenuHelpersX::CreateTruncatedCocoaLabel(label);
  NSMenuItem* newNativeMenuItem = [[NSMenuItem alloc] initWithTitle:newCocoaLabelString action:nil keyEquivalent:@""];
  [newNativeMenuItem setEnabled:enabled];
  [mMacMenu addItem:newNativeMenuItem];
  [newCocoaLabelString release];
  [newNativeMenuItem release];
  
  NSMenu* childMenu;
  if (aMenu->GetNativeData((void**)&childMenu) == NS_OK)
    [newNativeMenuItem setSubmenu:childMenu];

  return NS_OK;
}


NS_IMETHODIMP nsMenuX::AddSeparator()
{
  // We're not really appending an nsMenuItem but a placeholder needs to be
  // here to make sure that event dispatching isn't off by one.
  mMenuItemsArray.AppendObject(&gDummyMenuItemX);  // owning ref
  [mMacMenu addItem:[NSMenuItem separatorItem]];
  return NS_OK;
}


NS_IMETHODIMP nsMenuX::GetItemCount(PRUint32 &aCount)
{
  aCount = mMenuItemsArray.Count();
  return NS_OK;
}


NS_IMETHODIMP nsMenuX::GetItemAt(const PRUint32 aPos, nsISupports *& aMenuItem)
{
  aMenuItem = mMenuItemsArray.ObjectAt(aPos);
  NS_IF_ADDREF(aMenuItem);
  return NS_OK;
}


NS_IMETHODIMP nsMenuX::InsertItemAt(const PRUint32 aPos, nsISupports * aMenuItem)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMenuX::RemoveItem(const PRUint32 aPos)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMenuX::RemoveAll()
{
  if (mMacMenu != nil) {
    // clear command id's
    nsCOMPtr<nsIMenuCommandDispatcher> dispatcher(do_QueryInterface(mManager));
    if (dispatcher) {
      for (int i = 0; i < [mMacMenu numberOfItems]; i++)
        dispatcher->Unregister((PRUint32)[[mMacMenu itemAtIndex:i] tag]);
    }
    // get rid of Cocoa menu items
    for (int i = [mMacMenu numberOfItems] - 1; i >= 0; i--)
      [mMacMenu removeItemAtIndex:i];
  }
  // get rid of Gecko menu items
  mMenuItemsArray.Clear();
  mHiddenMenuItemsArray.Clear();
  
  return NS_OK;
}


NS_IMETHODIMP nsMenuX::GetNativeData(void ** aData)
{
  *aData = mMacMenu;
  return NS_OK;
}


NS_IMETHODIMP nsMenuX::SetNativeData(void * aData)
{
  [mMacMenu release];
  mMacMenu = [(NSMenu*)aData retain];
  return NS_OK;
}


NS_IMETHODIMP nsMenuX::AddMenuListener(nsIMenuListener * aMenuListener)
{
  mListener = aMenuListener; // strong ref
  return NS_OK;
}


NS_IMETHODIMP nsMenuX::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  if (aMenuListener == mListener)
    mListener = nsnull;
  return NS_OK;
}


//
// nsIMenuListener interface
//


nsEventStatus nsMenuX::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  // all this is now handled by Carbon Events.
  return nsEventStatus_eConsumeNoDefault;
}


nsEventStatus nsMenuX::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  // printf("JOSH: MenuSelected called for %s \n", NS_LossyConvertUTF16toASCII(mLabel).get());
  nsEventStatus eventStatus = nsEventStatus_eIgnore;

  // Determine if this is the correct menu to handle the event
  MenuRef selectedMenuHandle = (MenuRef)aMenuEvent.mCommand;

  // at this point, the carbon event handler was installed so there
  // must be a carbon MenuRef to be had
  if (_NSGetCarbonMenu(mMacMenu) == selectedMenuHandle) {
    // Open the node.
    mMenuContent->SetAttr(kNameSpaceID_None, nsWidgetAtoms::open, NS_LITERAL_STRING("true"), PR_TRUE);

    // Fire our oncreate handler. If we're told to stop, don't build the menu at all
    PRBool keepProcessing = OnCreate();

    if (!mNeedsRebuild || !keepProcessing)
      return nsEventStatus_eConsumeNoDefault;

    if (!mConstructed || mNeedsRebuild) {
      if (mNeedsRebuild)
        RemoveAll();

      nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocShellWeakRef);
      if (!docShell) {
        NS_ERROR("No doc shell");
        return nsEventStatus_eConsumeNoDefault;
      }

      MenuConstruct(aMenuEvent, nsnull, nsnull, docShell);
      mConstructed = true;
    }

    OnCreated();  // Now that it's built, fire the popupShown event.

    eventStatus = nsEventStatus_eConsumeNoDefault;  
  }
  else {
    // Make sure none of our submenus are the ones that should be handling this
    for (PRUint32 i = mMenuItemsArray.Count() - 1; i >= 0; i--) {
      nsISupports*              menuSupports = mMenuItemsArray.ObjectAt(i);
      nsCOMPtr<nsIMenuListener> menuListener = do_QueryInterface(menuSupports);
      if (menuListener) {
        eventStatus = menuListener->MenuSelected(aMenuEvent);
        if (nsEventStatus_eIgnore != eventStatus)
          return eventStatus;
      }  
    }
  }

  return eventStatus;
}


nsEventStatus nsMenuX::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  // Destroy the menu
  if (mConstructed) {
    MenuDestruct(aMenuEvent);
    mConstructed = false;
  }
  return nsEventStatus_eIgnore;
}


nsEventStatus nsMenuX::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * /* menuNode */,
    void              * aDocShell)
{
  mConstructed = false;
  gConstructingMenu = PR_TRUE;
  
  // reset destroy handler flag so that we'll know to fire it next time this menu goes away.
  mDestroyHandlerCalled = PR_FALSE;
  
  //printf("nsMenuX::MenuConstruct called for %s = %d \n", NS_LossyConvertUTF16toASCII(mLabel).get(), mMacMenu);
  
  // Retrieve our menupopup.
  nsCOMPtr<nsIContent> menuPopup;
  GetMenuPopupContent(getter_AddRefs(menuPopup));
  if (!menuPopup) {
    gConstructingMenu = PR_FALSE;
    return nsEventStatus_eIgnore;
  }

  // bug 365405: Manually wrap the menupopup node to make sure it's bounded
  if (!mXBLAttached) {
    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpconnect =
      do_GetService(nsIXPConnect::GetCID(), &rv);
    if (NS_SUCCEEDED(rv)) {
      nsIDocument* ownerDoc = menuPopup->GetOwnerDoc();
      nsIScriptGlobalObject* sgo;
      if (ownerDoc && (sgo = ownerDoc->GetScriptGlobalObject())) {
        nsCOMPtr<nsIScriptContext> scriptContext = sgo->GetContext();
        JSObject* global = sgo->GetGlobalJSObject();
        if (scriptContext && global) {
          JSContext* cx = (JSContext*)scriptContext->GetNativeContext();
          if (cx) {
            nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
            xpconnect->WrapNative(cx, global,
                                  menuPopup, NS_GET_IID(nsISupports),
                                  getter_AddRefs(wrapper));
            mXBLAttached = PR_TRUE;
          }
        }
      } 
    }
  }

  // Iterate over the kids
  PRUint32 count = menuPopup->GetChildCount();
  for (PRUint32 i = 0; i < count; i++) {
    nsIContent *child = menuPopup->GetChildAt(i);
    if (child) {
      // depending on the type, create a menu item, separator, or submenu
      nsIAtom *tag = child->Tag();
      if (tag == nsWidgetAtoms::menuitem)
        LoadMenuItem(child);
      else if (tag == nsWidgetAtoms::menuseparator)
        LoadSeparator(child);
      else if (tag == nsWidgetAtoms::menu)
        LoadSubMenu(child);
    }
  } // for each menu item

  gConstructingMenu = PR_FALSE;
  mNeedsRebuild = PR_FALSE;
  // printf("Done building, mMenuItemVoidArray.Count() = %d \n", mMenuItemVoidArray.Count());
  
  return nsEventStatus_eIgnore;
}


nsEventStatus nsMenuX::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  // printf("nsMenuX::MenuDestruct() called for %s \n", NS_LossyConvertUTF16toASCII(mLabel).get());
  
  // Fire our ondestroy handler. If we're told to stop, don't destroy the menu
  PRBool keepProcessing = OnDestroy();
  if (keepProcessing) {
    if (mNeedsRebuild)
        mConstructed = false;
    // Close the node.
    mMenuContent->UnsetAttr(kNameSpaceID_None, nsWidgetAtoms::open, PR_TRUE);
    OnDestroyed();
  }
  return nsEventStatus_eIgnore;
}


nsEventStatus nsMenuX::CheckRebuild(PRBool & aNeedsRebuild)
{
  aNeedsRebuild = PR_TRUE;
  return nsEventStatus_eIgnore;
}


nsEventStatus nsMenuX::SetRebuild(PRBool aNeedsRebuild)
{
  if (!gConstructingMenu)
    mNeedsRebuild = aNeedsRebuild;
  
  return nsEventStatus_eIgnore;
}


NS_IMETHODIMP nsMenuX::SetEnabled(PRBool aIsEnabled)
{
  if (aIsEnabled != mIsEnabled) {
    // we always want to rebuild when this changes
    SetRebuild(PR_TRUE); 
    mIsEnabled = aIsEnabled;
  }
  return NS_OK;
}


NS_IMETHODIMP nsMenuX::GetEnabled(PRBool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  *aIsEnabled = mIsEnabled;
  return NS_OK;
}


NS_IMETHODIMP nsMenuX::GetMenuContent(nsIContent ** aMenuContent)
{
  NS_ENSURE_ARG_POINTER(aMenuContent);
  NS_IF_ADDREF(*aMenuContent = mMenuContent);
  return NS_OK;
}


NSMenu* nsMenuX::CreateMenuWithGeckoString(nsString& menuTitle)
{
  NSString* title = [NSString stringWithCharacters:(UniChar*)menuTitle.get() length:menuTitle.Length()];
  NSMenu* myMenu = [[NSMenu alloc] initWithTitle:title];
  [myMenu setDelegate:mMenuDelegate];
  
  // We don't want this menu to auto-enable menu items because then Cocoa
  // overrides our decisions and things get incorrectly enabled/disabled.
  [myMenu setAutoenablesItems:NO];
  
  // we used to install Carbon event handlers here, but since NSMenu* doesn't
  // create its underlying MenuRef until just before display, we delay until
  // that happens. Now we install the event handlers when Cocoa notifies
  // us that a menu is about to display - see the Cocoa MenuDelegate class.
  
  return myMenu;
}


void nsMenuX::LoadMenuItem(nsIContent* inMenuItemContent)
{
  if (!inMenuItemContent)
    return;

  // create nsMenuItem
  nsCOMPtr<nsIMenuItem> pnsMenuItem = do_CreateInstance(kMenuItemCID);
  if (!pnsMenuItem)
    return;

  nsAutoString menuitemName;
  inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, menuitemName);

  // printf("menuitem %s \n", NS_LossyConvertUTF16toASCII(menuitemName).get());

  static nsIContent::AttrValuesArray strings[] =
  {&nsWidgetAtoms::checkbox, &nsWidgetAtoms::radio, nsnull};
  nsIMenuItem::EMenuItemType itemType = nsIMenuItem::eRegular;
  switch (inMenuItemContent->FindAttrValueIn(kNameSpaceID_None, nsWidgetAtoms::type,
                                             strings, eCaseMatters)) {
    case 0: itemType = nsIMenuItem::eCheckbox; break;
    case 1: itemType = nsIMenuItem::eRadio; break;
  }

  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocShellWeakRef);
  if (!docShell)
    return;

  // Create the item.
  pnsMenuItem->Create(this, menuitemName, PR_FALSE, itemType, mManager,
                      docShell, inMenuItemContent);

  // Set key shortcut and modifiers

  nsAutoString keyValue;
  inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::key, keyValue);

  // Try to find the key node. Get the document so we can do |GetElementByID|
  nsCOMPtr<nsIDOMDocument> domDocument =
    do_QueryInterface(inMenuItemContent->GetDocument());
  if (!domDocument)
    return;

  nsCOMPtr<nsIDOMElement> keyElement;
  if (!keyValue.IsEmpty())
    domDocument->GetElementById(keyValue, getter_AddRefs(keyElement));
  if (keyElement) {
    nsCOMPtr<nsIContent> keyContent (do_QueryInterface(keyElement));
    nsAutoString keyChar(NS_LITERAL_STRING(" "));
    keyContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::key, keyChar);
    if (!keyChar.EqualsLiteral(" ")) 
      pnsMenuItem->SetShortcutChar(keyChar);
    
    nsAutoString modifiersStr;
    keyContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::modifiers, modifiersStr);
    char* str = ToNewCString(modifiersStr);
    PRUint8 modifiers = MenuHelpersX::GeckoModifiersForNodeAttribute(str);
    nsMemory::Free(str);
    
    pnsMenuItem->SetModifiers(modifiers);
  }

  if (inMenuItemContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::checked,
                                     nsWidgetAtoms::_true, eCaseMatters))
    pnsMenuItem->SetChecked(PR_TRUE);
  else
    pnsMenuItem->SetChecked(PR_FALSE);

  AddMenuItem(pnsMenuItem);

  // This needs to happen after the nsIMenuItem object is inserted into
  // our item array in AddMenuItem()
  pnsMenuItem->SetupIcon();
}


void nsMenuX::LoadSubMenu(nsIContent* inMenuContent)
{
  nsAutoString menuName; 
  inMenuContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, menuName);
  //printf("Creating Menu [%s] \n", NS_LossyConvertUTF16toASCII(menuName).get());

  // Create nsMenu
  nsCOMPtr<nsIMenu> pnsMenu(do_CreateInstance(kMenuCID));
  if (!pnsMenu)
    return;

  // Call Create
  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocShellWeakRef);
  if (!docShell)
    return;
  pnsMenu->Create(NS_REINTERPRET_CAST(nsISupports*, this), menuName, EmptyString(), mManager, docShell, inMenuContent);

  // set if it's enabled or disabled
  if (inMenuContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::disabled,
                                 nsWidgetAtoms::_true, eCaseMatters))
    pnsMenu->SetEnabled(PR_FALSE);
  else
    pnsMenu->SetEnabled(PR_TRUE);

  AddMenu(pnsMenu);

  // This needs to happen after the nsIMenu object is inserted into
  // our item array in AddMenu()
  pnsMenu->SetupIcon();
}


void nsMenuX::LoadSeparator(nsIContent* inSeparatorContent) 
{
  // Currently we don't create nsIMenuItem objects for separators so we can't
  // track changes in their hidden/collapsed attributes. If it is hidden now it
  // is hidden forever.
  if (!NodeIsHiddenOrCollapsed(inSeparatorContent))
    AddSeparator();
}


//
// OnCreate
//
// Fire our oncreate handler. Returns TRUE if we should keep processing the event,
// FALSE if the handler wants to stop the creation of the menu
//
PRBool nsMenuX::OnCreate()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_SHOWING, nsnull,
                     nsMouseEvent::eReal);
  
  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));

  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocShellWeakRef);
  if (!docShell) {
    NS_ERROR("No doc shell");
    return PR_FALSE;
  }
  
  nsresult rv = NS_OK;
  nsIContent* dispatchTo = popupContent ? popupContent : mMenuContent;
  rv = dispatchTo->DispatchDOMEvent(&event, nsnull, nsnull, &status);
  if (NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault)
    return PR_FALSE;

  // the menu is going to show and the oncreate handler has executed. We
  // now need to walk our menu items, checking to see if any of them have
  // a command attribute. If so, several apptributes must potentially
  // be updated.
  if (popupContent) {
    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(popupContent->GetDocument()));

    PRUint32 count = popupContent->GetChildCount();
    for (PRUint32 i = 0; i < count; i++) {
      nsIContent *grandChild = popupContent->GetChildAt(i);
      if (grandChild->Tag() == nsWidgetAtoms::menuitem) {
        // See if we have a command attribute.
        nsAutoString command;
        grandChild->GetAttr(kNameSpaceID_None, nsWidgetAtoms::command, command);
        if (!command.IsEmpty()) {
          // We do! Look it up in our document
          nsCOMPtr<nsIDOMElement> commandElt;
          domDoc->GetElementById(command, getter_AddRefs(commandElt));
          nsCOMPtr<nsIContent> commandContent(do_QueryInterface(commandElt));

          if (commandContent) {
            nsAutoString commandDisabled, menuDisabled;
            commandContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, commandDisabled);
            grandChild->GetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, menuDisabled);
            if (!commandDisabled.Equals(menuDisabled)) {
              // The menu's disabled state needs to be updated to match the command.
              if (commandDisabled.IsEmpty()) 
                grandChild->UnsetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, PR_TRUE);
              else
                grandChild->SetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, commandDisabled, PR_TRUE);
            }

            // The menu's value and checked states need to be updated to match the command.
            // Note that (unlike the disabled state) if the command has *no* value for either, we
            // assume the menu is supplying its own.
            nsAutoString commandChecked, menuChecked;
            commandContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::checked, commandChecked);
            grandChild->GetAttr(kNameSpaceID_None, nsWidgetAtoms::checked, menuChecked);
            if (!commandChecked.Equals(menuChecked)) {
              if (!commandChecked.IsEmpty()) 
                grandChild->SetAttr(kNameSpaceID_None, nsWidgetAtoms::checked, commandChecked, PR_TRUE);
            }

            nsAutoString commandValue, menuValue;
            commandContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, commandValue);
            grandChild->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, menuValue);
            if (!commandValue.Equals(menuValue)) {
              if (!commandValue.IsEmpty()) 
                grandChild->SetAttr(kNameSpaceID_None, nsWidgetAtoms::label, commandValue, PR_TRUE);
            }
          }
        }
      }
    }
  }
  
  return PR_TRUE;
}


PRBool nsMenuX::OnCreated()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_SHOWN, nsnull, nsMouseEvent::eReal);
  
  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));

  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocShellWeakRef);
  if (!docShell) {
    NS_ERROR("No doc shell");
    return PR_FALSE;
  }

  nsresult rv = NS_OK;
  nsIContent* dispatchTo = popupContent ? popupContent : mMenuContent;
  rv = dispatchTo->DispatchDOMEvent(&event, nsnull, nsnull, &status);
  if (NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault)
    return PR_FALSE;  
  
  return PR_TRUE;
}


//
// OnDestroy
//
// Fire our ondestroy handler. Returns TRUE if we should keep processing the event,
// FALSE if the handler wants to stop the destruction of the menu
//
PRBool nsMenuX::OnDestroy()
{
  if (mDestroyHandlerCalled)
    return PR_TRUE;

  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_HIDING, nsnull,
                     nsMouseEvent::eReal);
  
  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocShellWeakRef);
  if (!docShell) {
    NS_WARNING("No doc shell so can't run the OnDestroy");
    return PR_FALSE;
  }

  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));

  nsresult rv = NS_OK;
  nsIContent* dispatchTo = popupContent ? popupContent : mMenuContent;
  rv = dispatchTo->DispatchDOMEvent(&event, nsnull, nsnull, &status);
  
  mDestroyHandlerCalled = PR_TRUE;
  
  if (NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault)
    return PR_FALSE;
  
  return PR_TRUE;
}


PRBool nsMenuX::OnDestroyed()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_HIDDEN, nsnull,
                     nsMouseEvent::eReal);
  
  nsCOMPtr<nsIDocShell>  docShell = do_QueryReferent(mDocShellWeakRef);
  if (!docShell) {
    NS_WARNING("No doc shell so can't run the OnDestroy");
    return PR_FALSE;
  }

  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));

  nsresult rv = NS_OK;
  nsIContent* dispatchTo = popupContent ? popupContent : mMenuContent;
  rv = dispatchTo->DispatchDOMEvent(&event, nsnull, nsnull, &status);
  
  mDestroyHandlerCalled = PR_TRUE;
  
  if (NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault)
    return PR_FALSE;
  
  return PR_TRUE;
}


//
// GetMenuPopupContent
//
// Find the |menupopup| child in the |popup| representing this menu. It should be one
// of a very few children so we won't be iterating over a bazillion menu items to find
// it (so the strcmp won't kill us).
//
void nsMenuX::GetMenuPopupContent(nsIContent** aResult)
{
  if (!aResult)
    return;
  *aResult = nsnull;
  
  nsresult rv;
  nsCOMPtr<nsIXBLService> xblService = do_GetService("@mozilla.org/xbl;1", &rv);
  if (!xblService)
    return;
  
  PRUint32 count = mMenuContent->GetChildCount();

  for (PRUint32 i = 0; i < count; i++) {
    PRInt32 dummy;
    nsIContent *child = mMenuContent->GetChildAt(i);
    nsCOMPtr<nsIAtom> tag;
    xblService->ResolveTag(child, &dummy, getter_AddRefs(tag));
    if (tag == nsWidgetAtoms::menupopup) {
      *aResult = child;
      NS_ADDREF(*aResult);
      return;
    }
  }

} // GetMenuPopupContent


//
// CountVisibleBefore
//
// Determines how many menus are visible among the siblings that are before me.
// It doesn't matter if I am visible. Note that this will always count the
// Application menu, since we always put it in there.
//
nsresult nsMenuX::CountVisibleBefore(PRUint32* outVisibleBefore)
{
  NS_ASSERTION(outVisibleBefore, "bad index param in nsMenuX::CountVisibleBefore");
  
  nsCOMPtr<nsIMenuBar> menubarParent = do_QueryInterface(mParent);
  if (!menubarParent)
    return NS_ERROR_FAILURE;

  PRUint32 numMenus = 0;
  menubarParent->GetMenuCount(numMenus);
  
  // Find this menu among the children of my parent menubar
  PRBool gotThisMenu = PR_FALSE;
  *outVisibleBefore = 1; // start at 1, the Application menu will always be there
  for (PRUint32 i = 0; i < numMenus; i++) {
    nsCOMPtr<nsIMenu> currMenu;
    menubarParent->GetMenuAt(i, *getter_AddRefs(currMenu));
    if (currMenu == NS_STATIC_CAST(nsIMenu*, this)) {
      // we found ourselves, break out
      gotThisMenu = PR_TRUE;
      break;
    }

    if (currMenu) {
      nsCOMPtr<nsIContent> menuContent;
      currMenu->GetMenuContent(getter_AddRefs(menuContent));
      if (menuContent &&
          menuContent->GetChildCount() > 0 &&
          !NodeIsHiddenOrCollapsed(menuContent)) {
        ++(*outVisibleBefore);
      }
    }
  }

  return gotThisMenu ? NS_OK : NS_ERROR_FAILURE;
} // CountVisibleBefore


NS_IMETHODIMP
nsMenuX::ChangeNativeEnabledStatusForMenuItem(nsIMenuItem* aMenuItem,
                                              PRBool aEnabled)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsMenuX::GetMenuRefAndItemIndexForMenuItem(nsISupports* aMenuItem,
                                           void**       aMenuRef,
                                           PRUint16*    aMenuItemIndex)
{
  if (!mMacMenu)
    return NS_ERROR_FAILURE;
  
  // look for the menu item given
  PRUint32 menuItemCount = mMenuItemsArray.Count();
  for (PRUint32 i = 0; i < menuItemCount; i++) {
    nsCOMPtr<nsISupports> currItem = mMenuItemsArray.ObjectAt(i);
    if (currItem == aMenuItem) {   
      *aMenuRef = _NSGetCarbonMenu(mMacMenu);
      *aMenuItemIndex = i + 1;
      return NS_OK;
    }
  }
  
  return NS_ERROR_FAILURE;
}


//
// nsIChangeObserver
//


NS_IMETHODIMP nsMenuX::AttributeChanged(nsIDocument *aDocument, PRInt32 aNameSpaceID,
                                        nsIContent *aContent, nsIAtom *aAttribute)
{
  // ignore the |open| attribute, which is by far the most common
  if (gConstructingMenu || (aAttribute == nsWidgetAtoms::open))
    return NS_OK;
    
  nsCOMPtr<nsIMenuBar> menubarParent = do_QueryInterface(mParent);

  if (aAttribute == nsWidgetAtoms::disabled) {
    SetRebuild(PR_TRUE);
   
    if (mMenuContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::disabled,
                                  nsWidgetAtoms::_true, eCaseMatters))
      SetEnabled(PR_FALSE);
    else
      SetEnabled(PR_TRUE);
  }
  else if (aAttribute == nsWidgetAtoms::label) {
    SetRebuild(PR_TRUE);
    
    mMenuContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, mLabel);

    // invalidate my parent. If we're a submenu parent, we have to rebuild
    // the parent menu in order for the changes to be picked up. If we're
    // a regular menu, just change the title and redraw the menubar.
    if (!menubarParent) {
      nsCOMPtr<nsIMenuListener> parentListener(do_QueryInterface(mParent));
      parentListener->SetRebuild(PR_TRUE);
    }
    else {
      // reuse the existing menu, to avoid rebuilding the root menu bar.
      NS_ASSERTION(mMacMenu != nil, "nsMenuX::AttributeChanged: invalid menu handle.");
      RemoveAll();
      NSString *newCocoaLabelString = MenuHelpersX::CreateTruncatedCocoaLabel(mLabel);
      [mMacMenu setTitle:newCocoaLabelString];
      [newCocoaLabelString release];
    }
  }
  else if (aAttribute == nsWidgetAtoms::hidden || aAttribute == nsWidgetAtoms::collapsed) {
    SetRebuild(PR_TRUE);

    if (NodeIsHiddenOrCollapsed(mMenuContent)) {
      if (mVisible) {
        if (menubarParent) {
          PRUint32 indexToRemove = 0;
          if (NS_SUCCEEDED(CountVisibleBefore(&indexToRemove))) {
            void *clientData = nsnull;
            menubarParent->GetNativeData(clientData);
            if (clientData) {
              NSMenu* menubar = reinterpret_cast<NSMenu*>(clientData);
              [menubar removeItemAtIndex:indexToRemove];
              mVisible = PR_FALSE;
            }
          }
        } // if on the menubar
        else {
          // hide this submenu
          NS_ASSERTION(PR_FALSE, "nsMenuX::AttributeChanged: WRITE HIDE CODE FOR SUBMENU.");
        }
      } // if visible
      else
        NS_WARNING("You're hiding the menu twice, please stop");
    } // if told to hide menu
    else {
      if (!mVisible) {
        if (menubarParent) {
          PRUint32 insertAfter = 0;
          if (NS_SUCCEEDED(CountVisibleBefore(&insertAfter))) {
            void *clientData = nsnull;
            menubarParent->GetNativeData(clientData);
            if (clientData) {
              NSMenu* menubar = reinterpret_cast<NSMenu*>(clientData);
              // Shove this menu into its rightful place in the menubar. It doesn't matter
              // what title we pass to InsertMenuItem() because when we stuff the actual menu
              // handle in, the correct title goes with it.
              [menubar insertItemWithTitle:@"placeholder" action:nil keyEquivalent:@"" atIndex:insertAfter];
              [[menubar itemAtIndex:insertAfter] setSubmenu:mMacMenu];
              mVisible = PR_TRUE;
            }
          }
        } // if on menubar
        else {
          // show this submenu
          NS_ASSERTION(PR_FALSE, "nsMenuX::AttributeChanged: WRITE SHOW CODE FOR SUBMENU.");
        }
      } // if not visible
    } // if told to show menu
  }
  else if (aAttribute == nsWidgetAtoms::image) {
    SetupIcon();
  }  

  return NS_OK;
} // AttributeChanged


NS_IMETHODIMP nsMenuX::ContentRemoved(nsIDocument *aDocument, nsIContent *aChild,
                                      PRInt32 aIndexInContainer)
{  
  if (gConstructingMenu)
    return NS_OK;

  SetRebuild(PR_TRUE);

  RemoveItem(aIndexInContainer);
  mManager->Unregister(aChild);

  return NS_OK;
} // ContentRemoved


NS_IMETHODIMP nsMenuX::ContentInserted(nsIDocument *aDocument, nsIContent *aChild,
                                       PRInt32 aIndexInContainer)
{  
  if (gConstructingMenu)
    return NS_OK;

  SetRebuild(PR_TRUE);
  
  return NS_OK;
} // ContentInserted


NS_IMETHODIMP
nsMenuX::SetupIcon()
{
  // In addition to out-of-memory, menus that are children of the menu bar
  // will not have mIcon set.
  if (!mIcon) return NS_ERROR_OUT_OF_MEMORY;
  return mIcon->SetupIcon();
}


//
// Carbon event support
//


static pascal OSStatus MyMenuEventHandler(EventHandlerCallRef myHandler, EventRef event, void* userData)
{
  UInt32 kind = ::GetEventKind(event);
  if (kind == kEventMenuTargetItem) {
    // get the position of the menu item we want
    PRUint16 aPos;
    ::GetEventParameter(event, kEventParamMenuItemIndex, typeMenuItemIndex, NULL, sizeof(MenuItemIndex), NULL, &aPos);
    aPos--; // subtract 1 from aPos because Carbon menu positions start at 1 not 0
    
    // don't request a menu item that doesn't exist or we crash
    // this might happen just due to some random quirks in the event system
    PRUint32 itemCount;
    nsIMenu* targetMenu = NS_REINTERPRET_CAST(nsIMenu*, userData);
    targetMenu->GetItemCount(itemCount);
    if (aPos >= itemCount)
      return eventNotHandledErr;
    
    nsISupports* aTargetMenuItem;
    targetMenu->GetItemAt((PRUint32)aPos, aTargetMenuItem);
    
    // Send DOM event
    // If the QI fails, we're over a submenu and we shouldn't send the event
    nsCOMPtr<nsIMenuItem> bTargetMenuItem(do_QueryInterface(aTargetMenuItem));
    if (bTargetMenuItem) {
      PRBool handlerCalledPreventDefault; // but we don't actually care
      bTargetMenuItem->DispatchDOMEvent(NS_LITERAL_STRING("DOMMenuItemActive"), &handlerCalledPreventDefault);
      return noErr;
    }
  }
  else if (kind == kEventMenuOpening || kind == kEventMenuClosed) {
    if (kind == kEventMenuOpening && gRollupListener != nsnull && gRollupWidget != nsnull) {
      gRollupListener->Rollup();
      // returning userCanceledErr crashes on Panther. See bug 351230.
      if (nsToolkit::OnTigerOrLater())
        return userCanceledErr;
    }
    
    nsISupports* supports = reinterpret_cast<nsISupports*>(userData);
    nsCOMPtr<nsIMenuListener> listener(do_QueryInterface(supports));
    if (listener) {
      MenuRef menuRef;
      ::GetEventParameter(event, kEventParamDirectObject, typeMenuRef, NULL, sizeof(menuRef), NULL, &menuRef);
      nsMenuEvent menuEvent(PR_TRUE, NS_MENU_SELECTED, nsnull);
      menuEvent.time = PR_IntervalNow();
      menuEvent.mCommand = (PRUint32)menuRef;
      if (kind == kEventMenuOpening)
        listener->MenuSelected(menuEvent);
      else
        listener->MenuDeselected(menuEvent);
      return noErr;
    }
  }
  return eventNotHandledErr;
}


static OSStatus InstallMyMenuEventHandler(MenuRef menuRef, void* userData, EventHandlerRef* outHandler)
{
  static EventTypeSpec eventList[] = {
    {kEventClassMenu, kEventMenuOpening},
    {kEventClassMenu, kEventMenuClosed},
    {kEventClassMenu, kEventMenuTargetItem}
  };
  
  static EventHandlerUPP gMyMenuEventHandlerUPP = NewEventHandlerUPP(&MyMenuEventHandler);
  OSStatus status = ::InstallMenuEventHandler(menuRef, gMyMenuEventHandlerUPP,
                                   sizeof(eventList) / sizeof(EventTypeSpec), eventList,
                                   userData, outHandler);
  NS_ASSERTION(status == noErr,"Installing carbon menu events failed.");
  return status;
}


// MenuDelegate Objective-C class, used to set up Carbon events


@implementation MenuDelegate

- (id)initWithGeckoMenu:(nsMenuX*)geckoMenu
{
  if ((self = [super init])) {
    mGeckoMenu = geckoMenu;
    mHaveInstalledCarbonEvents = FALSE;
  }
  return self;
}

- (void)dealloc
{
  RemoveEventHandler(mEventHandler);
  [super dealloc];
}

// You can get a MenuRef from an NSMenu*, but not until it has been made visible
// or added to the main menu bar. Basically, Cocoa is attempting lazy loading,
// and that doesn't work for us. We don't need any carbon events until after the
// first time the menu is shown, so when that happens we install the carbon
// event handler. This works because at this point we can get a MenuRef without
// much trouble. This call is 10.3+, so this stuff won't work on 10.2.x or less.
- (void)menuNeedsUpdate:(NSMenu*)aMenu
{
  if (!mHaveInstalledCarbonEvents) {
    MenuRef myMenuRef = _NSGetCarbonMenu(aMenu);
    if (myMenuRef) {
      InstallMyMenuEventHandler(myMenuRef, mGeckoMenu, &mEventHandler);
      mHaveInstalledCarbonEvents = TRUE;
    }
  }
}

@end
