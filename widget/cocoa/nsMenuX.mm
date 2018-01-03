/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>

#include "nsMenuX.h"
#include "nsMenuItemX.h"
#include "nsMenuUtilsX.h"
#include "nsMenuItemIconX.h"
#include "nsStandaloneNativeMenu.h"

#include "nsObjCExceptions.h"

#include "nsToolkit.h"
#include "nsCocoaUtils.h"
#include "nsCOMPtr.h"
#include "prinrval.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "plstr.h"
#include "nsGkAtoms.h"
#include "nsCRT.h"
#include "nsBaseWidget.h"

#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIComponentManager.h"
#include "nsIRollupListener.h"
#include "nsIDOMElement.h"
#include "nsBindingManager.h"
#include "nsIServiceManager.h"
#include "nsXULPopupManager.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/EventDispatcher.h"

#include "jsapi.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"

#include "mozilla/MouseEvents.h"

using namespace mozilla;

static bool gConstructingMenu = false;
static bool gMenuMethodsSwizzled = false;

int32_t nsMenuX::sIndexingMenuLevel = 0;


//
// Objective-C class used for representedObject
//

@implementation MenuItemInfo

- (id) initWithMenuGroupOwner:(nsMenuGroupOwnerX *)aMenuGroupOwner
{
  if ((self = [super init]) != nil) {
    [self setMenuGroupOwner:aMenuGroupOwner];
  }
  return self;
}

- (void) dealloc
{
  [self setMenuGroupOwner:nullptr];
  [super dealloc];
}

- (nsMenuGroupOwnerX *) menuGroupOwner
{
  return mMenuGroupOwner;
}

- (void) setMenuGroupOwner:(nsMenuGroupOwnerX *)aMenuGroupOwner
{
  // weak reference as the nsMenuGroupOwnerX owns all of its sub-objects
  mMenuGroupOwner = aMenuGroupOwner;
  if (aMenuGroupOwner) {
    aMenuGroupOwner->AddMenuItemInfoToSet(self);
  }
}

@end


//
// nsMenuX
//

nsMenuX::nsMenuX()
: mVisibleItemsCount(0), mParent(nullptr), mMenuGroupOwner(nullptr),
  mNativeMenu(nil), mNativeMenuItem(nil), mIsEnabled(true),
  mDestroyHandlerCalled(false), mNeedsRebuild(true),
  mConstructed(false), mVisible(true), mXBLAttached(false)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!gMenuMethodsSwizzled) {
    nsToolkit::SwizzleMethods([NSMenu class], @selector(_addItem:toTable:),
                              @selector(nsMenuX_NSMenu_addItem:toTable:), true);
    nsToolkit::SwizzleMethods([NSMenu class], @selector(_removeItem:fromTable:),
                              @selector(nsMenuX_NSMenu_removeItem:fromTable:), true);
    // On SnowLeopard the Shortcut framework (which contains the
    // SCTGRLIndex class) is loaded on demand, whenever the user first opens
    // a menu (which normally hasn't happened yet).  So we need to load it
    // here explicitly.
    dlopen("/System/Library/PrivateFrameworks/Shortcut.framework/Shortcut", RTLD_LAZY);
    Class SCTGRLIndexClass = ::NSClassFromString(@"SCTGRLIndex");
    nsToolkit::SwizzleMethods(SCTGRLIndexClass, @selector(indexMenuBarDynamically),
                              @selector(nsMenuX_SCTGRLIndex_indexMenuBarDynamically));

    gMenuMethodsSwizzled = true;
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

  // Prevent the icon object from outliving us.
  if (mIcon)
    mIcon->Destroy();

  RemoveAll();

  [mNativeMenu setDelegate:nil];
  [mNativeMenu release];
  [mMenuDelegate release];
  // autorelease the native menu item so that anything else happening to this
  // object happens before the native menu item actually dies
  [mNativeMenuItem autorelease];

  // alert the change notifier we don't care no more
  if (mContent)
    mMenuGroupOwner->UnregisterForContentChanges(mContent);

  MOZ_COUNT_DTOR(nsMenuX);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsresult nsMenuX::Create(nsMenuObjectX* aParent, nsMenuGroupOwnerX* aMenuGroupOwner, nsIContent* aContent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mContent = aContent;
  if (mContent->IsElement()) {
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label, mLabel);
  }
  mNativeMenu = CreateMenuWithGeckoString(mLabel);

  // register this menu to be notified when changes are made to our content object
  mMenuGroupOwner = aMenuGroupOwner; // weak ref
  NS_ASSERTION(mMenuGroupOwner, "No menu owner given, must have one");
  mMenuGroupOwner->RegisterForContentChanges(mContent, this);

  mParent = aParent;
  // our parent could be either a menu bar (if we're toplevel) or a menu (if we're a submenu)

#ifdef DEBUG
  nsMenuObjectTypeX parentType =
#endif
    mParent->MenuObjectType();
  NS_ASSERTION((parentType == eMenuBarObjectType || parentType == eSubmenuObjectType || parentType == eStandaloneNativeMenuObjectType),
               "Menu parent not a menu bar, menu, or native menu!");

  if (nsMenuUtilsX::NodeIsHiddenOrCollapsed(mContent))
    mVisible = false;
  if (mContent->GetChildCount() == 0)
    mVisible = false;

  NSString *newCocoaLabelString = nsMenuUtilsX::GetTruncatedCocoaLabel(mLabel);
  mNativeMenuItem = [[NSMenuItem alloc] initWithTitle:newCocoaLabelString action:nil keyEquivalent:@""];
  [mNativeMenuItem setSubmenu:mNativeMenu];

  SetEnabled(!mContent->IsElement() ||
             !mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                                 nsGkAtoms::_true, eCaseMatters));

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
  [newNativeMenuItem setTag:mMenuGroupOwner->RegisterForCommand(aMenuItem)];
  MenuItemInfo * info = [[MenuItemInfo alloc] initWithMenuGroupOwner:mMenuGroupOwner];
  [newNativeMenuItem setRepresentedObject:info];
  [info release];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsMenuX* nsMenuX::AddMenu(UniquePtr<nsMenuX> aMenu)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // aMenu transfers ownership to mMenuObjectsArray and becomes nullptr, so
  // we need to keep a raw pointer to access it conveniently.
  nsMenuX* menu = aMenu.get();
  mMenuObjectsArray.AppendElement(Move(aMenu));

  if (nsMenuUtilsX::NodeIsHiddenOrCollapsed(menu->Content())) {
    return menu;
  }

  ++mVisibleItemsCount;

  // We have to add a menu item and then associate the menu with it
  NSMenuItem* newNativeMenuItem = menu->NativeMenuItem();
  if (newNativeMenuItem) {
    [mNativeMenu addItem:newNativeMenuItem];
    [newNativeMenuItem setSubmenu:(NSMenu*)menu->NativeData()];
  }

  return menu;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(nullptr);
}

// Includes all items, including hidden/collapsed ones
uint32_t nsMenuX::GetItemCount()
{
  return mMenuObjectsArray.Length();
}

// Includes all items, including hidden/collapsed ones
nsMenuObjectX* nsMenuX::GetItemAt(uint32_t aPos)
{
  if (aPos >= (uint32_t)mMenuObjectsArray.Length())
    return NULL;

  return mMenuObjectsArray[aPos].get();
}

// Only includes visible items
nsresult nsMenuX::GetVisibleItemCount(uint32_t &aCount)
{
  aCount = mVisibleItemsCount;
  return NS_OK;
}

// Only includes visible items. Note that this is provides O(N) access
// If you need to iterate or search, consider using GetItemAt and doing your own filtering
nsMenuObjectX* nsMenuX::GetVisibleItemAt(uint32_t aPos)
{

  uint32_t count = mMenuObjectsArray.Length();
  if (aPos >= mVisibleItemsCount || aPos >= count)
    return NULL;

  // If there are no invisible items, can provide direct access
  if (mVisibleItemsCount == count)
    return mMenuObjectsArray[aPos].get();

  // Otherwise, traverse the array until we find the the item we're looking for.
  nsMenuObjectX* item;
  uint32_t visibleNodeIndex = 0;
  for (uint32_t i = 0; i < count; i++) {
    item = mMenuObjectsArray[i].get();
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
      mMenuGroupOwner->UnregisterCommand((uint32_t)[[mNativeMenu itemAtIndex:i] tag]);
    // get rid of Cocoa menu items
    for (int i = [mNativeMenu numberOfItems] - 1; i >= 0; i--)
      [mNativeMenu removeItemAtIndex:i];
  }

  mMenuObjectsArray.Clear();
  mVisibleItemsCount = 0;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsEventStatus nsMenuX::MenuOpened()
{
  // Open the node.
  if (mContent->IsElement()) {
    mContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::open,
                                   NS_LITERAL_STRING("true"), true);
  }

  // Fire a handler. If we're told to stop, don't build the menu at all
  bool keepProcessing = OnOpen();

  if (!mNeedsRebuild || !keepProcessing)
    return nsEventStatus_eConsumeNoDefault;

  if (!mConstructed || mNeedsRebuild) {
    if (mNeedsRebuild)
      RemoveAll();

    MenuConstruct();
    mConstructed = true;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetMouseEvent event(true, eXULPopupShown, nullptr,
                         WidgetMouseEvent::eReal);

  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));
  nsIContent* dispatchTo = popupContent ? popupContent : mContent;
  EventDispatcher::Dispatch(dispatchTo, nullptr, &event, nullptr, &status);

  return nsEventStatus_eConsumeNoDefault;
}

void nsMenuX::MenuClosed()
{
  if (mConstructed) {
    // Don't close if a handler tells us to stop.
    if (!OnClose())
      return;

    if (mNeedsRebuild)
      mConstructed = false;

    if (mContent->IsElement()) {
      mContent->AsElement()->UnsetAttr(kNameSpaceID_None, nsGkAtoms::open, true);
    }

    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetMouseEvent event(true, eXULPopupHidden, nullptr,
                           WidgetMouseEvent::eReal);

    nsCOMPtr<nsIContent> popupContent;
    GetMenuPopupContent(getter_AddRefs(popupContent));
    nsIContent* dispatchTo = popupContent ? popupContent : mContent;
    EventDispatcher::Dispatch(dispatchTo, nullptr, &event, nullptr, &status);

    mDestroyHandlerCalled = true;
    mConstructed = false;
  }
}

void nsMenuX::MenuConstruct()
{
  mConstructed = false;
  gConstructingMenu = true;

  // reset destroy handler flag so that we'll know to fire it next time this menu goes away.
  mDestroyHandlerCalled = false;

  //printf("nsMenuX::MenuConstruct called for %s = %d \n", NS_LossyConvertUTF16toASCII(mLabel).get(), mNativeMenu);

  // Retrieve our menupopup.
  nsCOMPtr<nsIContent> menuPopup;
  GetMenuPopupContent(getter_AddRefs(menuPopup));
  if (!menuPopup) {
    gConstructingMenu = false;
    return;
  }

  // bug 365405: Manually wrap the menupopup node to make sure it's bounded
  if (!mXBLAttached) {
    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpconnect =
      do_GetService(nsIXPConnect::GetCID(), &rv);
    if (NS_SUCCEEDED(rv)) {
      nsIDocument* ownerDoc = menuPopup->OwnerDoc();
      dom::AutoJSAPI jsapi;
      if (ownerDoc && jsapi.Init(ownerDoc->GetInnerWindow())) {
        JSContext* cx = jsapi.cx();
        JS::RootedObject ignoredObj(cx);
        xpconnect->WrapNative(cx, JS::CurrentGlobalOrNull(cx), menuPopup,
                              NS_GET_IID(nsISupports), ignoredObj.address());
        mXBLAttached = true;
      }
    }
  }

  // Iterate over the kids
  uint32_t count = menuPopup->GetChildCount();
  for (uint32_t i = 0; i < count; i++) {
    nsIContent *child = menuPopup->GetChildAt_Deprecated(i);
    if (child) {
      // depending on the type, create a menu item, separator, or submenu
      if (child->IsAnyOfXULElements(nsGkAtoms::menuitem,
                                    nsGkAtoms::menuseparator)) {
        LoadMenuItem(child);
      } else if (child->IsXULElement(nsGkAtoms::menu)) {
        LoadSubMenu(child);
      }
    }
  } // for each menu item

  gConstructingMenu = false;
  mNeedsRebuild = false;
  // printf("Done building, mMenuObjectsArray.Count() = %d \n", mMenuObjectsArray.Count());
}

void nsMenuX::SetRebuild(bool aNeedsRebuild)
{
  if (!gConstructingMenu) {
    mNeedsRebuild = aNeedsRebuild;
    if (mParent->MenuObjectType() == eMenuBarObjectType) {
      nsMenuBarX* mb = static_cast<nsMenuBarX*>(mParent);
      mb->SetNeedsRebuild();
    }
  }
}

nsresult nsMenuX::SetEnabled(bool aIsEnabled)
{
  if (aIsEnabled != mIsEnabled) {
    // we always want to rebuild when this changes
    mIsEnabled = aIsEnabled;
    [mNativeMenuItem setEnabled:(BOOL)mIsEnabled];
  }
  return NS_OK;
}

nsresult nsMenuX::GetEnabled(bool* aIsEnabled)
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
  if (inMenuItemContent->IsElement()) {
    inMenuItemContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label, menuitemName);
  }

  // printf("menuitem %s \n", NS_LossyConvertUTF16toASCII(menuitemName).get());

  EMenuItemType itemType = eRegularMenuItemType;
  if (inMenuItemContent->IsXULElement(nsGkAtoms::menuseparator)) {
    itemType = eSeparatorMenuItemType;
  } else if (inMenuItemContent->IsElement()) {
    static Element::AttrValuesArray strings[] =
  {&nsGkAtoms::checkbox, &nsGkAtoms::radio, nullptr};
    switch (inMenuItemContent->AsElement()->FindAttrValueIn(kNameSpaceID_None,
                                                            nsGkAtoms::type,
                                                            strings, eCaseMatters)) {
      case 0: itemType = eCheckboxMenuItemType; break;
      case 1: itemType = eRadioMenuItemType; break;
    }
  }

  // Create the item.
  nsMenuItemX* menuItem = new nsMenuItemX();
  if (!menuItem)
    return;

  nsresult rv = menuItem->Create(this, menuitemName, itemType, mMenuGroupOwner, inMenuItemContent);
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
  auto menu = MakeUnique<nsMenuX>();
  if (!menu)
    return;

  nsresult rv = menu->Create(this, mMenuGroupOwner, inMenuContent);
  if (NS_FAILED(rv))
    return;

  // |menu|'s ownership is transfer to AddMenu but, if it is successfully
  // added, we can access it via the returned raw pointer.
  nsMenuX* menu_ptr = AddMenu(Move(menu));

  // This needs to happen after the nsIMenu object is inserted into
  // our item array in AddMenu()
  if (menu_ptr) {
    menu_ptr->SetupIcon();
  }
}

// This menu is about to open. Returns TRUE if we should keep processing the event,
// FALSE if the handler wants to stop the opening of the menu.
bool nsMenuX::OnOpen()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetMouseEvent event(true, eXULPopupShowing, nullptr,
                         WidgetMouseEvent::eReal);

  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));

  nsresult rv = NS_OK;
  nsIContent* dispatchTo = popupContent ? popupContent : mContent;
  rv = EventDispatcher::Dispatch(dispatchTo, nullptr, &event, nullptr, &status);
  if (NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault)
    return false;

  // If the open is going to succeed we need to walk our menu items, checking to
  // see if any of them have a command attribute. If so, several attributes
  // must potentially be updated.

  // Get new popup content first since it might have changed as a result of the
  // eXULPopupShowing event above.
  GetMenuPopupContent(getter_AddRefs(popupContent));
  if (!popupContent)
    return true;

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    pm->UpdateMenuItems(popupContent);
  }

  return true;
}

// Returns TRUE if we should keep processing the event, FALSE if the handler
// wants to stop the closing of the menu.
bool nsMenuX::OnClose()
{
  if (mDestroyHandlerCalled)
    return true;

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetMouseEvent event(true, eXULPopupHiding, nullptr,
                         WidgetMouseEvent::eReal);

  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));

  nsresult rv = NS_OK;
  nsIContent* dispatchTo = popupContent ? popupContent : mContent;
  rv = EventDispatcher::Dispatch(dispatchTo, nullptr, &event, nullptr, &status);

  mDestroyHandlerCalled = true;

  if (NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault)
    return false;

  return true;
}

// Find the |menupopup| child in the |popup| representing this menu. It should be one
// of a very few children so we won't be iterating over a bazillion menu items to find
// it (so the strcmp won't kill us).
void nsMenuX::GetMenuPopupContent(nsIContent** aResult)
{
  if (!aResult)
    return;
  *aResult = nullptr;

  // Check to see if we are a "menupopup" node (if we are a native menu).
  {
    int32_t dummy;
    RefPtr<nsAtom> tag = mContent->OwnerDoc()->BindingManager()->ResolveTag(mContent, &dummy);
    if (tag == nsGkAtoms::menupopup) {
      NS_ADDREF(*aResult = mContent);
      return;
    }
  }

  // Otherwise check our child nodes.

  uint32_t count = mContent->GetChildCount();

  for (uint32_t i = 0; i < count; i++) {
    int32_t dummy;
    nsIContent *child = mContent->GetChildAt_Deprecated(i);
    RefPtr<nsAtom> tag = child->OwnerDoc()->BindingManager()->ResolveTag(child, &dummy);
    if (tag == nsGkAtoms::menupopup) {
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

bool nsMenuX::IsXULHelpMenu(nsIContent* aMenuContent)
{
  bool retval = false;
  if (aMenuContent && aMenuContent->IsElement()) {
    nsAutoString id;
    aMenuContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::id, id);
    if (id.Equals(NS_LITERAL_STRING("helpMenu")))
      retval = true;
  }
  return retval;
}

//
// nsChangeObserver
//

void nsMenuX::ObserveAttributeChanged(nsIDocument *aDocument, nsIContent *aContent,
                                      nsAtom *aAttribute)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // ignore the |open| attribute, which is by far the most common
  if (gConstructingMenu || (aAttribute == nsGkAtoms::open))
    return;

  nsMenuObjectTypeX parentType = mParent->MenuObjectType();

  if (aAttribute == nsGkAtoms::disabled) {
    SetEnabled(!mContent->AsElement()->AttrValueIs(
      kNameSpaceID_None, nsGkAtoms::disabled, nsGkAtoms::_true, eCaseMatters));
  }
  else if (aAttribute == nsGkAtoms::label) {
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label, mLabel);

    // invalidate my parent. If we're a submenu parent, we have to rebuild
    // the parent menu in order for the changes to be picked up. If we're
    // a regular menu, just change the title and redraw the menubar.
    if (parentType == eMenuBarObjectType) {
      // reuse the existing menu, to avoid rebuilding the root menu bar.
      NS_ASSERTION(mNativeMenu, "nsMenuX::AttributeChanged: invalid menu handle.");
      NSString *newCocoaLabelString = nsMenuUtilsX::GetTruncatedCocoaLabel(mLabel);
      [mNativeMenu setTitle:newCocoaLabelString];
    }
    else if (parentType == eSubmenuObjectType) {
      static_cast<nsMenuX*>(mParent)->SetRebuild(true);
    }
    else if (parentType == eStandaloneNativeMenuObjectType) {
      static_cast<nsStandaloneNativeMenu*>(mParent)->GetMenuXObject()->SetRebuild(true);
    }
  }
  else if (aAttribute == nsGkAtoms::hidden || aAttribute == nsGkAtoms::collapsed) {
    SetRebuild(true);

    bool contentIsHiddenOrCollapsed = nsMenuUtilsX::NodeIsHiddenOrCollapsed(mContent);

    // don't do anything if the state is correct already
    if (contentIsHiddenOrCollapsed != mVisible)
      return;

    if (contentIsHiddenOrCollapsed) {
      if (parentType == eMenuBarObjectType ||
          parentType == eSubmenuObjectType ||
          parentType == eStandaloneNativeMenuObjectType) {
        NSMenu* parentMenu = (NSMenu*)mParent->NativeData();
        // An exception will get thrown if we try to remove an item that isn't
        // in the menu.
        if ([parentMenu indexOfItem:mNativeMenuItem] != -1)
          [parentMenu removeItem:mNativeMenuItem];
        mVisible = false;
      }
    }
    else {
      if (parentType == eMenuBarObjectType ||
          parentType == eSubmenuObjectType ||
          parentType == eStandaloneNativeMenuObjectType) {
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
        mVisible = true;
      }
    }
  }
  else if (aAttribute == nsGkAtoms::image) {
    SetupIcon();
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuX::ObserveContentRemoved(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aChild,
                                    nsIContent* aPreviousSibling)
{
  if (gConstructingMenu)
    return;

  SetRebuild(true);
  mMenuGroupOwner->UnregisterForContentChanges(aChild);
}

void nsMenuX::ObserveContentInserted(nsIDocument *aDocument, nsIContent* aContainer,
                                     nsIContent *aChild)
{
  if (gConstructingMenu)
    return;

  SetRebuild(true);
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
// MenuDelegate Objective-C class, used to set up Carbon events
//

@implementation MenuDelegate

- (id)initWithGeckoMenu:(nsMenuX*)geckoMenu
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super init])) {
    NS_ASSERTION(geckoMenu, "Cannot initialize native menu delegate with NULL gecko menu! Will crash!");
    mGeckoMenu = geckoMenu;
  }
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item
{
  if (!menu || !item || !mGeckoMenu)
    return;

  nsMenuObjectX* target = mGeckoMenu->GetVisibleItemAt((uint32_t)[menu indexOfItem:item]);
  if (target && (target->MenuObjectType() == eMenuItemObjectType)) {
    nsMenuItemX* targetMenuItem = static_cast<nsMenuItemX*>(target);
    bool handlerCalledPreventDefault; // but we don't actually care
    targetMenuItem->DispatchDOMEvent(NS_LITERAL_STRING("DOMMenuItemActive"), &handlerCalledPreventDefault);
  }
}

- (void)menuWillOpen:(NSMenu *)menu
{
  if (!mGeckoMenu)
    return;

  // Don't do anything while the OS is (re)indexing our menus (on Leopard and
  // higher).  This stops the Help menu from being able to search in our
  // menus, but it also resolves many other problems.
  if (nsMenuX::sIndexingMenuLevel > 0)
    return;

  nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
  if (rollupListener) {
    nsCOMPtr<nsIWidget> rollupWidget = rollupListener->GetRollupWidget();
    if (rollupWidget) {
      rollupListener->Rollup(0, true, nullptr, nullptr);
      [menu cancelTracking];
      return;
    }
  }
  mGeckoMenu->MenuOpened();
}

- (void)menuDidClose:(NSMenu *)menu
{
  if (!mGeckoMenu)
    return;

  // Don't do anything while the OS is (re)indexing our menus (on Leopard and
  // higher).  This stops the Help menu from being able to search in our
  // menus, but it also resolves many other problems.
  if (nsMenuX::sIndexingMenuLevel > 0)
    return;

  mGeckoMenu->MenuClosed();
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
  // menus.  sIndexingMenuLevel is a int32_t just in case it might be
  // reentered.  As it's running, it spawns calls to two undocumented
  // HIToolbox methods (_SimulateMenuOpening() and _SimulateMenuClosed()),
  // which "simulate" the opening and closing of our menus without actually
  // displaying them.
  ++nsMenuX::sIndexingMenuLevel;
  [self nsMenuX_SCTGRLIndex_indexMenuBarDynamically];
  --nsMenuX::sIndexingMenuLevel;
}

@end
