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

#include "nsMenuX.h"
#include "nsMenuItemX.h"
#include "nsMenuUtilsX.h"
#include "nsMenuItemIconX.h"

#include "nsObjCExceptions.h"

#include "nsToolkit.h"
#include "nsCocoaUtils.h"
#include "nsCOMPtr.h"
#include "prinrval.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "plstr.h"
#include "nsWidgetAtoms.h"
#include "nsGUIEvent.h"
#include "nsCRT.h"

#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIComponentManager.h"
#include "nsIRollupListener.h"
#include "nsIDOMElement.h"
#include "nsIXBLService.h"
#include "nsIServiceManager.h"

#include "jsapi.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"

// externs defined in nsChildView.mm
extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;

static PRBool gConstructingMenu = PR_FALSE;
static PRBool gMenuMethodsSwizzled = PR_FALSE;

PRInt32 nsMenuX::sIndexingMenuLevel = 0;


nsMenuX::nsMenuX()
: mVisibleItemsCount(0), mParent(nsnull), mMenuBar(nsnull),
  mNativeMenu(nil), mNativeMenuItem(nil), mIsEnabled(PR_TRUE),
  mDestroyHandlerCalled(PR_FALSE), mNeedsRebuild(PR_TRUE),
  mConstructed(PR_FALSE), mVisible(PR_TRUE), mXBLAttached(PR_FALSE)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!gMenuMethodsSwizzled) {
    if (nsToolkit::OnLeopardOrLater()) {
      nsToolkit::SwizzleMethods([NSMenu class], @selector(_addItem:toTable:),
                                @selector(nsMenuX_NSMenu_addItem:toTable:), PR_TRUE);
      nsToolkit::SwizzleMethods([NSMenu class], @selector(_removeItem:fromTable:),
                                @selector(nsMenuX_NSMenu_removeItem:fromTable:), PR_TRUE);
      Class SCTGRLIndexClass = ::NSClassFromString(@"SCTGRLIndex");
      nsToolkit::SwizzleMethods(SCTGRLIndexClass, @selector(indexMenuBarDynamically),
                                @selector(nsMenuX_SCTGRLIndex_indexMenuBarDynamically));
    } else {
      nsToolkit::SwizzleMethods([NSMenu class], @selector(performKeyEquivalent:),
                                @selector(nsMenuX_NSMenu_performKeyEquivalent:));
    }
    gMenuMethodsSwizzled = PR_TRUE;
  }

  mMenuDelegate = [[MenuDelegate alloc] initWithGeckoMenu:this];
    
  if (!nsMenuBarX::sNativeEventTarget)
    nsMenuBarX::sNativeEventTarget = [[NativeMenuItemTarget alloc] init];

  MOZ_COUNT_CTOR(nsMenuX);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


nsMenuX::~nsMenuX()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  RemoveAll();

  [mNativeMenu setDelegate:nil];
  [mNativeMenu release];
  [mMenuDelegate release];
  [mNativeMenuItem release];

  // alert the change notifier we don't care no more
  if (mContent)
    mMenuBar->UnregisterForContentChanges(mContent);

  MOZ_COUNT_DTOR(nsMenuX);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


nsresult nsMenuX::Create(nsMenuObjectX* aParent, nsMenuBarX* aMenuBar, nsIContent* aNode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mContent = aNode;
  mContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, mLabel);
  mNativeMenu = CreateMenuWithGeckoString(mLabel);

  // register this menu to be notified when changes are made to our content object
  mMenuBar = aMenuBar; // weak ref
  NS_ASSERTION(mMenuBar, "No menu bar given, must have one");
  mMenuBar->RegisterForContentChanges(mContent, this);

  mParent = aParent;
  // our parent could be either a menu bar (if we're toplevel) or a menu (if we're a submenu)
  nsMenuObjectTypeX parentType = mParent->MenuObjectType();
  NS_ASSERTION((parentType == eMenuBarObjectType || parentType == eSubmenuObjectType),
               "Menu parent not a menu bar or menu!");

  if (nsMenuUtilsX::NodeIsHiddenOrCollapsed(mContent))
    mVisible = PR_FALSE;
  if (mContent->GetChildCount() == 0)
    mVisible = PR_FALSE;

  NSString *newCocoaLabelString = nsMenuUtilsX::CreateTruncatedCocoaLabel(mLabel);
  mNativeMenuItem = [[NSMenuItem alloc] initWithTitle:newCocoaLabelString action:nil keyEquivalent:@""];
  [newCocoaLabelString release];
  [mNativeMenuItem setSubmenu:mNativeMenu];

  SetEnabled(!mContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::disabled,
                                    nsWidgetAtoms::_true, eCaseMatters));

  // We call MenuConstruct here because keyboard commands are dependent upon
  // native menu items being created. If we only call MenuConstruct when a menu
  // is actually selected, then we can't access keyboard commands until the
  // menu gets selected, which is bad.
  MenuConstruct();

  mIcon = new nsMenuItemIconX(this, mContent, mNativeMenuItem);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


nsresult nsMenuX::AddMenuItem(nsMenuItemX* aMenuItem)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!aMenuItem)
    return NS_ERROR_INVALID_ARG;

  mMenuObjectsArray.AppendElement(aMenuItem);
  if (nsMenuUtilsX::NodeIsHiddenOrCollapsed(aMenuItem->Content()))
    return NS_OK;
  ++mVisibleItemsCount;

  NSMenuItem* newNativeMenuItem = (NSMenuItem*)aMenuItem->NativeData();

  // add the menu item to this menu
  [mNativeMenu addItem:newNativeMenuItem];

  // set up target/action
  [newNativeMenuItem setTarget:nsMenuBarX::sNativeEventTarget];
  [newNativeMenuItem setAction:@selector(menuItemHit:)];

  // set its command. we get the unique command id from the menubar
  [newNativeMenuItem setTag:mMenuBar->RegisterForCommand(aMenuItem)];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


nsresult nsMenuX::AddMenu(nsMenuX* aMenu)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Add a submenu
  if (!aMenu)
    return NS_ERROR_NULL_POINTER;

  mMenuObjectsArray.AppendElement(aMenu);
  if (nsMenuUtilsX::NodeIsHiddenOrCollapsed(aMenu->Content()))
    return NS_OK;
  ++mVisibleItemsCount;

  // We have to add a menu item and then associate the menu with it
  NSMenuItem* newNativeMenuItem = aMenu->NativeMenuItem();
  if (!newNativeMenuItem)
    return NS_ERROR_FAILURE;
  [mNativeMenu addItem:newNativeMenuItem];

  [newNativeMenuItem setSubmenu:(NSMenu*)aMenu->NativeData()];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


// Includes all items, including hidden/collapsed ones
PRUint32 nsMenuX::GetItemCount()
{
  return mMenuObjectsArray.Length();
}


// Includes all items, including hidden/collapsed ones
nsMenuObjectX* nsMenuX::GetItemAt(PRUint32 aPos)
{
  if (aPos >= (PRUint32)mMenuObjectsArray.Length())
    return NULL;

  return mMenuObjectsArray[aPos];
}


// Only includes visible items
nsresult nsMenuX::GetVisibleItemCount(PRUint32 &aCount)
{
  aCount = mVisibleItemsCount;
  return NS_OK;
}


// Only includes visible items. Note that this is provides O(N) access
// If you need to iterate or search, consider using GetItemAt and doing your own filtering
nsMenuObjectX* nsMenuX::GetVisibleItemAt(PRUint32 aPos)
{
  
  PRUint32 count = mMenuObjectsArray.Length();
  if (aPos >= mVisibleItemsCount || aPos >= count)
    return NULL;

  // If there are no invisible items, can provide direct access
  if (mVisibleItemsCount == count)
    return mMenuObjectsArray[aPos];

  // Otherwise, traverse the array until we find the the item we're looking for.
  nsMenuObjectX* item;
  PRUint32 visibleNodeIndex = 0;
  for (PRUint32 i = 0; i < count; i++) {
    item = mMenuObjectsArray[i];
    if (!nsMenuUtilsX::NodeIsHiddenOrCollapsed(item->Content())) {
      if (aPos == visibleNodeIndex) {
        // we found the visible node we're looking for, return it
        return item;
      }
      visibleNodeIndex++;
    }
  }

  return NULL;
}


nsresult nsMenuX::RemoveAll()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (mNativeMenu) {
    // clear command id's
    int itemCount = [mNativeMenu numberOfItems];
    for (int i = 0; i < itemCount; i++)
      mMenuBar->UnregisterCommand((PRUint32)[[mNativeMenu itemAtIndex:i] tag]);
    // get rid of Cocoa menu items
    for (int i = [mNativeMenu numberOfItems] - 1; i >= 0; i--)
      [mNativeMenu removeItemAtIndex:i];
  }

  mMenuObjectsArray.Clear();
  mVisibleItemsCount = 0;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


nsEventStatus nsMenuX::MenuOpened(const nsMenuEvent & aMenuEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // Determine if this is the correct menu to handle the event
  MenuRef selectedMenuHandle = (MenuRef)aMenuEvent.mCommand;

  // at this point, the carbon event handler was installed so there
  // must be a carbon MenuRef to be had
  if (_NSGetCarbonMenu(mNativeMenu) == selectedMenuHandle) {
    // Open the node.
    mContent->SetAttr(kNameSpaceID_None, nsWidgetAtoms::open, NS_LITERAL_STRING("true"), PR_TRUE);

    // Fire a handler. If we're told to stop, don't build the menu at all
    PRBool keepProcessing = OnOpen();

    if (!mNeedsRebuild || !keepProcessing)
      return nsEventStatus_eConsumeNoDefault;

    if (!mConstructed || mNeedsRebuild) {
      if (mNeedsRebuild)
        RemoveAll();

      MenuConstruct();
      mConstructed = true;
    }

    OnOpened();

    return nsEventStatus_eConsumeNoDefault;  
  }
  else {
    // Make sure none of our submenus are the ones that should be handling this
    PRUint32 count = mMenuObjectsArray.Length();
    for (PRUint32 i = 0; i < count; i++) {
      nsMenuObjectX* menuObject = mMenuObjectsArray[i];
      if (menuObject->MenuObjectType() == eSubmenuObjectType) {
        nsEventStatus status = static_cast<nsMenuX*>(menuObject)->MenuOpened(aMenuEvent);
        if (status != nsEventStatus_eIgnore)
          return status;
      }  
    }
  }

  return nsEventStatus_eIgnore;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(nsEventStatus_eIgnore);
}


void nsMenuX::MenuClosed(const nsMenuEvent & aMenuEvent)
{
  if (mConstructed) {
    // Don't close if a handler tells us to stop.
    if (!OnClose())
      return;

    if (mNeedsRebuild)
      mConstructed = false;

    mContent->UnsetAttr(kNameSpaceID_None, nsWidgetAtoms::open, PR_TRUE);

    OnClosed();

    mConstructed = false;
  }
}


void nsMenuX::MenuConstruct()
{
  mConstructed = false;
  gConstructingMenu = PR_TRUE;
  
  // reset destroy handler flag so that we'll know to fire it next time this menu goes away.
  mDestroyHandlerCalled = PR_FALSE;

  //printf("nsMenuX::MenuConstruct called for %s = %d \n", NS_LossyConvertUTF16toASCII(mLabel).get(), mNativeMenu);

  // Retrieve our menupopup.
  nsCOMPtr<nsIContent> menuPopup;
  GetMenuPopupContent(getter_AddRefs(menuPopup));
  if (!menuPopup) {
    gConstructingMenu = PR_FALSE;
    return;
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
      if (tag == nsWidgetAtoms::menuitem || tag == nsWidgetAtoms::menuseparator)
        LoadMenuItem(child);
      else if (tag == nsWidgetAtoms::menu)
        LoadSubMenu(child);
    }
  } // for each menu item

  gConstructingMenu = PR_FALSE;
  mNeedsRebuild = PR_FALSE;
  // printf("Done building, mMenuObjectsArray.Count() = %d \n", mMenuObjectsArray.Count());
}


void nsMenuX::SetRebuild(PRBool aNeedsRebuild)
{
  if (!gConstructingMenu)
    mNeedsRebuild = aNeedsRebuild;
}


nsresult nsMenuX::SetEnabled(PRBool aIsEnabled)
{
  if (aIsEnabled != mIsEnabled) {
    // we always want to rebuild when this changes
    mIsEnabled = aIsEnabled;
    [mNativeMenuItem setEnabled:(BOOL)mIsEnabled];
  }
  return NS_OK;
}


nsresult nsMenuX::GetEnabled(PRBool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  *aIsEnabled = mIsEnabled;
  return NS_OK;
}


GeckoNSMenu* nsMenuX::CreateMenuWithGeckoString(nsString& menuTitle)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSString* title = [NSString stringWithCharacters:(UniChar*)menuTitle.get() length:menuTitle.Length()];
  GeckoNSMenu* myMenu = [[GeckoNSMenu alloc] initWithTitle:title];
  [myMenu setDelegate:mMenuDelegate];

  // We don't want this menu to auto-enable menu items because then Cocoa
  // overrides our decisions and things get incorrectly enabled/disabled.
  [myMenu setAutoenablesItems:NO];

  // we used to install Carbon event handlers here, but since NSMenu* doesn't
  // create its underlying MenuRef until just before display, we delay until
  // that happens. Now we install the event handlers when Cocoa notifies
  // us that a menu is about to display - see the Cocoa MenuDelegate class.

  return myMenu;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}


void nsMenuX::LoadMenuItem(nsIContent* inMenuItemContent)
{
  if (!inMenuItemContent)
    return;

  nsAutoString menuitemName;
  inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, menuitemName);

  // printf("menuitem %s \n", NS_LossyConvertUTF16toASCII(menuitemName).get());

  EMenuItemType itemType = eRegularMenuItemType;
  if (inMenuItemContent->Tag() == nsWidgetAtoms::menuseparator) {
    itemType = eSeparatorMenuItemType;
  }
  else {
    static nsIContent::AttrValuesArray strings[] =
  {&nsWidgetAtoms::checkbox, &nsWidgetAtoms::radio, nsnull};
    switch (inMenuItemContent->FindAttrValueIn(kNameSpaceID_None, nsWidgetAtoms::type,
                                               strings, eCaseMatters)) {
      case 0: itemType = eCheckboxMenuItemType; break;
      case 1: itemType = eRadioMenuItemType; break;
    }
  }

  // Create the item.
  nsMenuItemX* menuItem = new nsMenuItemX();
  if (!menuItem)
    return;

  nsresult rv = menuItem->Create(this, menuitemName, itemType, mMenuBar, inMenuItemContent);
  if (NS_FAILED(rv)) {
    delete menuItem;
    return;
  }

  AddMenuItem(menuItem);

  // This needs to happen after the nsIMenuItem object is inserted into
  // our item array in AddMenuItem()
  menuItem->SetupIcon();
}


void nsMenuX::LoadSubMenu(nsIContent* inMenuContent)
{
  nsAutoPtr<nsMenuX> menu(new nsMenuX());
  if (!menu)
    return;

  nsresult rv = menu->Create(this, mMenuBar, inMenuContent);
  if (NS_FAILED(rv))
    return;

  AddMenu(menu);

  // This needs to happen after the nsIMenu object is inserted into
  // our item array in AddMenu()
  menu->SetupIcon();

  menu.forget();
}


// This menu is about to open. Returns TRUE if we should keep processing the event,
// FALSE if the handler wants to stop the opening of the menu.
PRBool nsMenuX::OnOpen()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_SHOWING, nsnull,
                     nsMouseEvent::eReal);
  
  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));
  
  nsresult rv = NS_OK;
  nsIContent* dispatchTo = popupContent ? popupContent : mContent;
  rv = dispatchTo->DispatchDOMEvent(&event, nsnull, nsnull, &status);
  if (NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault)
    return PR_FALSE;

  // If the open is going to succeed we need to walk our menu items, checking to
  // see if any of them have a command attribute. If so, several apptributes
  // must potentially be updated.

  // Get new popup content first since it might have changed as a result of the
  // NS_XUL_POPUP_SHOWING event above.
  GetMenuPopupContent(getter_AddRefs(popupContent));
  if (!popupContent)
    return PR_TRUE;

  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(popupContent->GetDocument()));
  if (!domDoc)
    return PR_TRUE;

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

  return PR_TRUE;
}


PRBool nsMenuX::OnOpened()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_SHOWN, nsnull, nsMouseEvent::eReal);
  
  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));

  nsresult rv = NS_OK;
  nsIContent* dispatchTo = popupContent ? popupContent : mContent;
  rv = dispatchTo->DispatchDOMEvent(&event, nsnull, nsnull, &status);
  if (NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault)
    return PR_FALSE;  
  
  return PR_TRUE;
}


// Returns TRUE if we should keep processing the event, FALSE if the handler
// wants to stop the closing of the menu.
PRBool nsMenuX::OnClose()
{
  if (mDestroyHandlerCalled)
    return PR_TRUE;

  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_HIDING, nsnull,
                     nsMouseEvent::eReal);

  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));

  nsresult rv = NS_OK;
  nsIContent* dispatchTo = popupContent ? popupContent : mContent;
  rv = dispatchTo->DispatchDOMEvent(&event, nsnull, nsnull, &status);
  
  mDestroyHandlerCalled = PR_TRUE;
  
  if (NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault)
    return PR_FALSE;
  
  return PR_TRUE;
}


PRBool nsMenuX::OnClosed()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_HIDDEN, nsnull,
                     nsMouseEvent::eReal);

  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));

  nsresult rv = NS_OK;
  nsIContent* dispatchTo = popupContent ? popupContent : mContent;
  rv = dispatchTo->DispatchDOMEvent(&event, nsnull, nsnull, &status);
  
  mDestroyHandlerCalled = PR_TRUE;
  
  if (NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault)
    return PR_FALSE;
  
  return PR_TRUE;
}


// Find the |menupopup| child in the |popup| representing this menu. It should be one
// of a very few children so we won't be iterating over a bazillion menu items to find
// it (so the strcmp won't kill us).
void nsMenuX::GetMenuPopupContent(nsIContent** aResult)
{
  if (!aResult)
    return;
  *aResult = nsnull;
  
  nsresult rv;
  nsCOMPtr<nsIXBLService> xblService = do_GetService("@mozilla.org/xbl;1", &rv);
  if (!xblService)
    return;
  
  PRUint32 count = mContent->GetChildCount();

  for (PRUint32 i = 0; i < count; i++) {
    PRInt32 dummy;
    nsIContent *child = mContent->GetChildAt(i);
    nsCOMPtr<nsIAtom> tag;
    xblService->ResolveTag(child, &dummy, getter_AddRefs(tag));
    if (tag == nsWidgetAtoms::menupopup) {
      *aResult = child;
      NS_ADDREF(*aResult);
      return;
    }
  }

}


NSMenuItem* nsMenuX::NativeMenuItem()
{
  return mNativeMenuItem;
}


//
// nsChangeObserver
//


void nsMenuX::ObserveAttributeChanged(nsIDocument *aDocument, nsIContent *aContent,
                                      nsIAtom *aAttribute)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // ignore the |open| attribute, which is by far the most common
  if (gConstructingMenu || (aAttribute == nsWidgetAtoms::open))
    return;

  nsMenuObjectTypeX parentType = mParent->MenuObjectType();

  if (aAttribute == nsWidgetAtoms::disabled) {
    SetEnabled(!mContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::disabled,
                                      nsWidgetAtoms::_true, eCaseMatters));
  }
  else if (aAttribute == nsWidgetAtoms::label) {
    mContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, mLabel);

    // invalidate my parent. If we're a submenu parent, we have to rebuild
    // the parent menu in order for the changes to be picked up. If we're
    // a regular menu, just change the title and redraw the menubar.
    if (parentType == eMenuBarObjectType) {
      // reuse the existing menu, to avoid rebuilding the root menu bar.
      NS_ASSERTION(mNativeMenu, "nsMenuX::AttributeChanged: invalid menu handle.");
      NSString *newCocoaLabelString = nsMenuUtilsX::CreateTruncatedCocoaLabel(mLabel);
      [mNativeMenu setTitle:newCocoaLabelString];
      [newCocoaLabelString release];
    }
    else {
      static_cast<nsMenuX*>(mParent)->SetRebuild(PR_TRUE);
    }    
  }
  else if (aAttribute == nsWidgetAtoms::hidden || aAttribute == nsWidgetAtoms::collapsed) {
    SetRebuild(PR_TRUE);

    PRBool contentIsHiddenOrCollapsed = nsMenuUtilsX::NodeIsHiddenOrCollapsed(mContent);

    // don't do anything if the state is correct already
    if (contentIsHiddenOrCollapsed != mVisible)
      return;

    if (contentIsHiddenOrCollapsed) {
      if (parentType == eMenuBarObjectType || parentType == eSubmenuObjectType) {
        NSMenu* parentMenu = (NSMenu*)mParent->NativeData();
        // An exception will get thrown if we try to remove an item that isn't
        // in the menu.
        if ([parentMenu indexOfItem:mNativeMenuItem] != -1)
          [parentMenu removeItem:mNativeMenuItem];
        mVisible = PR_FALSE;
      }
    }
    else {
      if (parentType == eMenuBarObjectType || parentType == eSubmenuObjectType) {
        int insertionIndex = nsMenuUtilsX::CalculateNativeInsertionPoint(mParent, this);
        if (parentType == eMenuBarObjectType) {
          // Before inserting we need to figure out if we should take the native
          // application menu into account.
          nsMenuBarX* mb = static_cast<nsMenuBarX*>(mParent);
          if (mb->MenuContainsAppMenu())
            insertionIndex++;
        }
        NSMenu* parentMenu = (NSMenu*)mParent->NativeData();
        [parentMenu insertItem:mNativeMenuItem atIndex:insertionIndex];
        [mNativeMenuItem setSubmenu:mNativeMenu];
        mVisible = PR_TRUE;
      }
    }
  }
  else if (aAttribute == nsWidgetAtoms::image) {
    SetupIcon();
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


void nsMenuX::ObserveContentRemoved(nsIDocument *aDocument, nsIContent *aChild,
                                    PRInt32 aIndexInContainer)
{
  if (gConstructingMenu)
    return;

  SetRebuild(PR_TRUE);
  mMenuBar->UnregisterForContentChanges(aChild);
}


void nsMenuX::ObserveContentInserted(nsIDocument *aDocument, nsIContent *aChild,
                                     PRInt32 aIndexInContainer)
{
  if (gConstructingMenu)
    return;

  SetRebuild(PR_TRUE);
}


nsresult nsMenuX::SetupIcon()
{
  // In addition to out-of-memory, menus that are children of the menu bar
  // will not have mIcon set.
  if (!mIcon)
    return NS_ERROR_OUT_OF_MEMORY;

  return mIcon->SetupIcon();
}


//
// Carbon event support
//


static pascal OSStatus MyMenuEventHandler(EventHandlerCallRef myHandler, EventRef event, void* userData)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // Don't do anything while the OS is (re)indexing our menus (on Leopard and
  // higher).  This stops the Help menu from being able to search in our
  // menus, but it also resolves many other problems -- including crashes and
  // long delays while opening the Help menu.  Once we know better which
  // operations are safe during (re)indexing, we can start allowing some
  // operations here while it's happening.  This change resolves bmo bugs
  // 426499 and 414699.
  if (nsMenuX::sIndexingMenuLevel > 0)
    return noErr;

  nsMenuX* targetMenu = static_cast<nsMenuX*>(userData);
  UInt32 kind = ::GetEventKind(event);
  if (kind == kEventMenuTargetItem) {
    // get the position of the menu item we want
    PRUint16 aPos;
    ::GetEventParameter(event, kEventParamMenuItemIndex, typeMenuItemIndex, NULL, sizeof(MenuItemIndex), NULL, &aPos);
    aPos--; // subtract 1 from aPos because Carbon menu positions start at 1 not 0
    
    // don't request a menu item that doesn't exist or we crash
    // this might happen just due to some random quirks in the event system
    PRUint32 itemCount;
    targetMenu->GetVisibleItemCount(itemCount);
    if (aPos >= itemCount)
      return eventNotHandledErr;

    // Send DOM event if we're over a menu item
    nsMenuObjectX* target = targetMenu->GetVisibleItemAt((PRUint32)aPos);
    if (target->MenuObjectType() == eMenuItemObjectType) {
      nsMenuItemX* targetMenuItem = static_cast<nsMenuItemX*>(target);
      PRBool handlerCalledPreventDefault; // but we don't actually care
      targetMenuItem->DispatchDOMEvent(NS_LITERAL_STRING("DOMMenuItemActive"), &handlerCalledPreventDefault);
      return noErr;
    }
  }
  else if (kind == kEventMenuOpening || kind == kEventMenuClosed) {
    if (kind == kEventMenuOpening && gRollupListener && gRollupWidget) {
      gRollupListener->Rollup(nsnull);
      return userCanceledErr;
    }
    MenuRef menuRef;
    ::GetEventParameter(event, kEventParamDirectObject, typeMenuRef, NULL, sizeof(menuRef), NULL, &menuRef);
    nsMenuEvent menuEvent(PR_TRUE, NS_MENU_SELECTED, nsnull);
    menuEvent.time = PR_IntervalNow();
    menuEvent.mCommand = (PRUint32)menuRef;
    if (kind == kEventMenuOpening)
      targetMenu->MenuOpened(menuEvent);
    else
      targetMenu->MenuClosed(menuEvent);
    return noErr;
  }
  return eventNotHandledErr;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(noErr);
}


static OSStatus InstallMyMenuEventHandler(MenuRef menuRef, void* userData, EventHandlerRef* outHandler)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

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

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(noErr);
}


//
// MenuDelegate Objective-C class, used to set up Carbon events
//


@implementation MenuDelegate


- (id)initWithGeckoMenu:(nsMenuX*)geckoMenu
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super init])) {
    mGeckoMenu = geckoMenu;
    mHaveInstalledCarbonEvents = FALSE;
  }
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}


- (void)dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  RemoveEventHandler(mEventHandler);
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


// You can get a MenuRef from an NSMenu*, but not until it has been made visible
// or added to the main menu bar. Basically, Cocoa is attempting lazy loading,
// and that doesn't work for us. We don't need any carbon events until after the
// first time the menu is shown, so when that happens we install the carbon
// event handler. This works because at this point we can get a MenuRef without
// much trouble.
- (void)menuNeedsUpdate:(NSMenu*)aMenu
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mHaveInstalledCarbonEvents) {
    MenuRef myMenuRef = _NSGetCarbonMenu(aMenu);
    if (myMenuRef) {
      InstallMyMenuEventHandler(myMenuRef, mGeckoMenu, &mEventHandler);
      mHaveInstalledCarbonEvents = TRUE;
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end


// OS X Leopard (at least as of 10.5.2) has an obscure bug triggered by some
// behavior that's present in Mozilla.org browsers but not (as best I can
// tell) in Apple products like Safari.  (It's not yet clear exactly what this
// behavior is.)
//
// The bug is that sometimes you crash on quit in nsMenuX::RemoveAll(), on a
// call to [NSMenu removeItemAtIndex:].  The crash is caused by trying to
// access a deleted NSMenuItem object (sometimes (perhaps always?) by trying
// to send it a _setChangedFlags: message).  Though this object was deleted
// some time ago, it remains registered as a potential target for a particular
// key equivalent.  So when [NSMenu removeItemAtIndex:] removes the current
// target for that same key equivalent, the OS tries to "activate" the
// previous target.
//
// The underlying reason appears to be that NSMenu's _addItem:toTable: and
// _removeItem:fromTable: methods (which are used to keep a hashtable of
// registered key equivalents) don't properly "retain" and "release"
// NSMenuItem objects as they are added to and removed from the hashtable.
//
// Our (hackish) workaround is to shadow the OS's hashtable with another
// hastable of our own (gShadowKeyEquivDB), and use it to "retain" and
// "release" NSMenuItem objects as needed.  This resolves bmo bugs 422287 and
// 423669.  When (if) Apple fixes this bug, we can remove this workaround.

static NSMutableDictionary *gShadowKeyEquivDB = nil;

// Class for values in gShadowKeyEquivDB.

@interface KeyEquivDBItem : NSObject
{
  NSMenuItem *mItem;
  NSMutableSet *mTables;
}

- (id)initWithItem:(NSMenuItem *)aItem table:(NSMapTable *)aTable;
- (BOOL)hasTable:(NSMapTable *)aTable;
- (int)addTable:(NSMapTable *)aTable;
- (int)removeTable:(NSMapTable *)aTable;

@end

@implementation KeyEquivDBItem

- (id)initWithItem:(NSMenuItem *)aItem table:(NSMapTable *)aTable
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  
  if (!gShadowKeyEquivDB)
    gShadowKeyEquivDB = [[NSMutableDictionary alloc] init];
  self = [super init];
  if (aItem && aTable) {
    mTables = [[NSMutableSet alloc] init];
    mItem = [aItem retain];
    [mTables addObject:[NSValue valueWithPointer:aTable]];
  } else {
    mTables = nil;
    mItem = nil;
  }
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mTables)
    [mTables release];
  if (mItem)
    [mItem release];
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL)hasTable:(NSMapTable *)aTable
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return [mTables member:[NSValue valueWithPointer:aTable]] ? YES : NO;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

// Does nothing if aTable (its index value) is already present in mTables.
- (int)addTable:(NSMapTable *)aTable
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (aTable)
    [mTables addObject:[NSValue valueWithPointer:aTable]];
  return [mTables count];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

- (int)removeTable:(NSMapTable *)aTable
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (aTable) {
    NSValue *objectToRemove =
      [mTables member:[NSValue valueWithPointer:aTable]];
    if (objectToRemove)
      [mTables removeObject:objectToRemove];
  }
  return [mTables count];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

@end


@interface NSMenu (MethodSwizzling)
+ (void)nsMenuX_NSMenu_addItem:(NSMenuItem *)aItem toTable:(NSMapTable *)aTable;
+ (void)nsMenuX_NSMenu_removeItem:(NSMenuItem *)aItem fromTable:(NSMapTable *)aTable;
- (BOOL)nsMenuX_NSMenu_performKeyEquivalent:(NSEvent *)theEvent;
@end

@implementation NSMenu (MethodSwizzling)

+ (void)nsMenuX_NSMenu_addItem:(NSMenuItem *)aItem toTable:(NSMapTable *)aTable
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (aItem && aTable) {
    NSValue *key = [NSValue valueWithPointer:aItem];
    KeyEquivDBItem *shadowItem = [gShadowKeyEquivDB objectForKey:key];
    if (shadowItem) {
      [shadowItem addTable:aTable];
    } else {
      shadowItem = [[KeyEquivDBItem alloc] initWithItem:aItem table:aTable];
      [gShadowKeyEquivDB setObject:shadowItem forKey:key];
      // Release after [NSMutableDictionary setObject:forKey:] retains it (so
      // that it will get dealloced when removeObjectForKey: is called).
      [shadowItem release];
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;

  [self nsMenuX_NSMenu_addItem:aItem toTable:aTable];
}

+ (void)nsMenuX_NSMenu_removeItem:(NSMenuItem *)aItem fromTable:(NSMapTable *)aTable
{
  [self nsMenuX_NSMenu_removeItem:aItem fromTable:aTable];

  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (aItem && aTable) {
    NSValue *key = [NSValue valueWithPointer:aItem];
    KeyEquivDBItem *shadowItem = [gShadowKeyEquivDB objectForKey:key];
    if (shadowItem && [shadowItem hasTable:aTable]) {
      if (![shadowItem removeTable:aTable])
        [gShadowKeyEquivDB removeObjectForKey:key];
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL)nsMenuX_NSMenu_performKeyEquivalent:(NSEvent *)theEvent
{
  // On OS X 10.4.X (Tiger), Objective-C exceptions can occur during calls to
  // [NSMenu performKeyEquivalent:] (from [GeckoNSMenu performKeyEquivalent:]
  // or otherwise) that shouldn't be fatal (see bmo bug 461381).  So on Tiger
  // we hook this system call to eat (and log) all Objective-C exceptions that
  // occur during its execution.  Since we don't call XPCOM code from here,
  // this will never cause XPCOM objects to be left on the stack without
  // cleanup.
  NS_OBJC_BEGIN_TRY_LOGONLY_BLOCK_RETURN;
  return [self nsMenuX_NSMenu_performKeyEquivalent:theEvent];
  NS_OBJC_END_TRY_LOGONLY_BLOCK_RETURN(NO);
}

@end

// This class is needed to keep track of when the OS is (re)indexing all of
// our menus.  This appears to only happen on Leopard and higher, and can
// be triggered by opening the Help menu.  Some operations are unsafe while
// this is happening -- notably the calls to [[NSImage alloc]
// initWithSize:imageRect.size] and [newImage lockFocus] in nsMenuItemIconX::
// OnStopFrame().  But we don't yet have a complete list, and Apple doesn't
// yet have any documentation on this subject.  (Apple also doesn't yet have
// any documented way to find the information we seek here.)  The "original"
// of this class (the one whose indexMenuBarDynamically method we hook) is
// defined in the Shortcut framework in /System/Library/PrivateFrameworks.
@interface NSObject (SCTGRLIndexMethodSwizzling)
- (void)nsMenuX_SCTGRLIndex_indexMenuBarDynamically;
@end

@implementation NSObject (SCTGRLIndexMethodSwizzling)

- (void)nsMenuX_SCTGRLIndex_indexMenuBarDynamically
{
  // This method appears to be called (once) whenever the OS (re)indexes our
  // menus.  sIndexingMenuLevel is a PRInt32 just in case it might be
  // reentered.  As it's running, it spawns calls to two undocumented
  // HIToolbox methods (_SimulateMenuOpening() and _SimulateMenuClosed()),
  // which "simulate" the opening and closing of our menus without actually
  // displaying them.
  ++nsMenuX::sIndexingMenuLevel;
  [self nsMenuX_SCTGRLIndex_indexMenuBarDynamically];
  --nsMenuX::sIndexingMenuLevel;
}

@end
