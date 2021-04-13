/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuX.h"

#include <_types/_uint32_t.h>
#include <dlfcn.h>

#include "mozilla/dom/Document.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/MouseEvents.h"

#include "nsMenuItemX.h"
#include "nsMenuUtilsX.h"
#include "nsMenuItemIconX.h"

#include "nsObjCExceptions.h"

#include "nsThreadUtils.h"
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

#include "nsIContent.h"
#include "nsIDocumentObserver.h"
#include "nsIComponentManager.h"
#include "nsIRollupListener.h"
#include "nsIServiceManager.h"
#include "nsXULPopupManager.h"

using namespace mozilla;
using namespace mozilla::dom;

static bool gConstructingMenu = false;
static bool gMenuMethodsSwizzled = false;

int32_t nsMenuX::sIndexingMenuLevel = 0;

// TODO: It is unclear whether this is still needed.
static void SwizzleDynamicIndexingMethods() {
  if (gMenuMethodsSwizzled) {
    return;
  }

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

//
// nsMenuX
//

nsMenuX::nsMenuX(nsMenuParentX* aParent, nsMenuGroupOwnerX* aMenuGroupOwner, nsIContent* aContent)
    : mContent(aContent), mParent(aParent), mMenuGroupOwner(aMenuGroupOwner) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_COUNT_CTOR(nsMenuX);

  SwizzleDynamicIndexingMethods();

  mMenuDelegate = [[MenuDelegate alloc] initWithGeckoMenu:this];
  mMenuDelegate.menuIsInMenubar = mMenuGroupOwner->GetMenuBar() != nullptr;

  if (!nsMenuBarX::sNativeEventTarget) {
    nsMenuBarX::sNativeEventTarget = [[NativeMenuItemTarget alloc] init];
  }

  if (mContent->IsElement()) {
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label, mLabel);
  }
  mNativeMenu = CreateMenuWithGeckoString(mLabel);

  // register this menu to be notified when changes are made to our content object
  NS_ASSERTION(mMenuGroupOwner, "No menu owner given, must have one");
  mMenuGroupOwner->RegisterForContentChanges(mContent, this);

  if (nsMenuUtilsX::NodeIsHiddenOrCollapsed(mContent)) {
    mVisible = false;
  }
  if (mContent->GetChildCount() == 0) {
    mVisible = false;
  }

  NSString* newCocoaLabelString = nsMenuUtilsX::GetTruncatedCocoaLabel(mLabel);
  mNativeMenuItem = [[NSMenuItem alloc] initWithTitle:newCocoaLabelString
                                               action:nil
                                        keyEquivalent:@""];
  [mNativeMenuItem setSubmenu:mNativeMenu];

  SetEnabled(!mContent->IsElement() ||
             !mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                                 nsGkAtoms::_true, eCaseMatters));

  // We call RebuildMenu here because keyboard commands are dependent upon
  // native menu items being created. If we only call RebuildMenu when a menu
  // is actually selected, then we can't access keyboard commands until the
  // menu gets selected, which is bad.
  RebuildMenu();

  mIcon = MakeUnique<nsMenuItemIconX>(this);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsMenuX::~nsMenuX() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Make sure a pending popupshown event isn't dropped.
  FlushMenuOpenedRunnable();

  if (mIsOpen) {
    [mNativeMenu cancelTracking];
  }

  // Make sure pending popuphiding/popuphidden events aren't dropped.
  FlushMenuClosedRunnable();

  OnHighlightedItemChanged(Nothing());
  RemoveAll();

  mNativeMenu.delegate = nil;
  [mNativeMenu release];
  [mMenuDelegate release];
  // autorelease the native menu item so that anything else happening to this
  // object happens before the native menu item actually dies
  [mNativeMenuItem autorelease];

  DetachFromGroupOwnerRecursive();

  MOZ_COUNT_DTOR(nsMenuX);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuX::DetachFromGroupOwnerRecursive() {
  if (!mMenuGroupOwner) {
    // Don't recurse if this subtree is already detached.
    // This avoids repeated recursion during the destruction of nested nsMenuX structures.
    // Our invariant is: If we are detached, all of our contents are also detached.
    return;
  }

  if (mMenuGroupOwner && mContent) {
    mMenuGroupOwner->UnregisterForContentChanges(mContent);
  }
  UnregisterCommands();
  mMenuGroupOwner = nullptr;

  // Also detach all our children.
  for (auto& child : mMenuChildren) {
    child.match([](const RefPtr<nsMenuX>& aMenu) { aMenu->DetachFromGroupOwnerRecursive(); },
                [](const RefPtr<nsMenuItemX>& aMenuItem) { aMenuItem->DetachFromGroupOwner(); });
  }
}

void nsMenuX::UnregisterCommands() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mMenuGroupOwner || !mNativeMenu) {
    return;
  }

  for (NSMenuItem* item in mNativeMenu.itemArray) {
    mMenuGroupOwner->UnregisterCommand(static_cast<uint32_t>(item.tag));
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuX::AddMenuItem(RefPtr<nsMenuItemX>&& aMenuItem) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mMenuChildren.AppendElement(aMenuItem);

  if (nsMenuUtilsX::NodeIsHiddenOrCollapsed(aMenuItem->Content())) {
    return;
  }

  ++mVisibleItemsCount;

  NSMenuItem* newNativeMenuItem = aMenuItem->NativeNSMenuItem();

  // add the menu item to this menu
  [mNativeMenu addItem:newNativeMenuItem];

  // set up target/action
  newNativeMenuItem.target = nsMenuBarX::sNativeEventTarget;
  newNativeMenuItem.action = @selector(menuItemHit:);

  // set its command. we get the unique command id from the menubar
  newNativeMenuItem.tag = mMenuGroupOwner->RegisterForCommand(aMenuItem);
  newNativeMenuItem.representedObject = mMenuGroupOwner->GetRepresentedObject();

  aMenuItem->SetupIcon();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuX::AddMenu(RefPtr<nsMenuX>&& aMenu) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mMenuChildren.AppendElement(aMenu);

  if (nsMenuUtilsX::NodeIsHiddenOrCollapsed(aMenu->Content())) {
    return;
  }

  ++mVisibleItemsCount;

  // We have to add a menu item and then associate the menu with it
  NSMenuItem* newNativeMenuItem = aMenu->NativeNSMenuItem();
  if (newNativeMenuItem) {
    [mNativeMenu addItem:newNativeMenuItem];
    newNativeMenuItem.submenu = aMenu->NativeNSMenu();
  }

  aMenu->SetupIcon();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Includes all items, including hidden/collapsed ones
uint32_t nsMenuX::GetItemCount() { return mMenuChildren.Length(); }

// Includes all items, including hidden/collapsed ones
mozilla::Maybe<nsMenuX::MenuChild> nsMenuX::GetItemAt(uint32_t aPos) {
  if (aPos >= (uint32_t)mMenuChildren.Length()) {
    return {};
  }

  return Some(mMenuChildren[aPos]);
}

// Only includes visible items
nsresult nsMenuX::GetVisibleItemCount(uint32_t& aCount) {
  aCount = mVisibleItemsCount;
  return NS_OK;
}

// Only includes visible items. Note that this is provides O(N) access
// If you need to iterate or search, consider using GetItemAt and doing your own filtering
Maybe<nsMenuX::MenuChild> nsMenuX::GetVisibleItemAt(uint32_t aPos) {
  uint32_t count = mMenuChildren.Length();
  if (aPos >= mVisibleItemsCount || aPos >= count) {
    return {};
  }

  // If there are no invisible items, can provide direct access
  if (mVisibleItemsCount == count) {
    return GetItemAt(aPos);
  }

  // Otherwise, traverse the array until we find the the item we're looking for.
  uint32_t visibleNodeIndex = 0;
  for (uint32_t i = 0; i < count; i++) {
    MenuChild item = *GetItemAt(i);
    RefPtr<nsIContent> content =
        item.match([](const RefPtr<nsMenuX>& aMenu) { return aMenu->Content(); },
                   [](const RefPtr<nsMenuItemX>& aMenuItem) { return aMenuItem->Content(); });
    if (!nsMenuUtilsX::NodeIsHiddenOrCollapsed(content)) {
      if (aPos == visibleNodeIndex) {
        // we found the visible node we're looking for, return it
        return Some(item);
      }
      visibleNodeIndex++;
    }
  }

  return {};
}

Maybe<nsMenuX::MenuChild> nsMenuX::GetItemForElement(Element* aMenuChildElement) {
  for (auto& child : mMenuChildren) {
    RefPtr<nsIContent> content =
        child.match([](const RefPtr<nsMenuX>& aMenu) { return aMenu->Content(); },
                    [](const RefPtr<nsMenuItemX>& aMenuItem) { return aMenuItem->Content(); });
    if (content == aMenuChildElement) {
      return Some(child);
    }
  }
  return {};
}

nsresult nsMenuX::RemoveAll() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  UnregisterCommands();
  [mNativeMenu removeAllItems];

  for (auto& child : mMenuChildren) {
    child.match(
        [](const RefPtr<nsMenuX>& aMenu) {
          aMenu->DetachFromGroupOwnerRecursive();
          aMenu->DetachFromParent();
        },
        [](const RefPtr<nsMenuItemX>& aMenuItem) {
          aMenuItem->DetachFromGroupOwner();
          aMenuItem->DetachFromParent();
        });
  }

  mMenuChildren.Clear();
  mVisibleItemsCount = 0;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuX::MenuOpened() {
  if (mIsOpen) {
    return;
  }

  // Make sure we fire any pending popupshown / popuphiding / popuphidden events first.
  FlushMenuOpenedRunnable();
  FlushMenuClosedRunnable();

  if (!mDidFirePopupshowingAndIsApprovedToOpen) {
    // Fire popupshowing now.
    bool approvedToOpen = OnOpen();
    if (!approvedToOpen) {
      // We can only stop menus from opening which we open ourselves. We cannot stop menubar root
      // menus or menu submenus from opening.
      // For context menus, we can call OnOpen() before we ask the system to open the menu.
      NS_WARNING("The popupshowing event had preventDefault() called on it, but in MenuOpened() it "
                 "is too late to stop the menu from opening.");
    }
  }

  mIsOpen = true;

  // Reset mDidFirePopupshowingAndIsApprovedToOpen for the next menu opening.
  mDidFirePopupshowingAndIsApprovedToOpen = false;

  if (mNeedsRebuild) {
    OnHighlightedItemChanged(Nothing());
    RemoveAll();
    RebuildMenu();
  }

  // Fire the popupshown event in MenuOpenedAsync.
  // MenuOpened() is called during menuWillOpen, and if cancelTracking is called now, menuDidClose
  // will not be called.
  // The runnable object must not hold a strong reference to the nsMenuX, so that there is no
  // reference cycle.
  class MenuOpenedAsyncRunnable final : public mozilla::CancelableRunnable {
   public:
    explicit MenuOpenedAsyncRunnable(nsMenuX* aMenu)
        : CancelableRunnable("MenuOpenedAsyncRunnable"), mMenu(aMenu) {}

    nsresult Run() override {
      if (mMenu) {
        RefPtr<nsMenuX> menu = mMenu;
        menu->MenuOpenedAsync();
        mMenu = nullptr;
      }
      return NS_OK;
    }
    nsresult Cancel() override {
      mMenu = nullptr;
      return NS_OK;
    }

   private:
    nsMenuX* mMenu;  // weak, cleared by Cancel() and Run()
  };
  mPendingAsyncMenuOpenRunnable = new MenuOpenedAsyncRunnable(this);
  NS_DispatchToCurrentThread(mPendingAsyncMenuOpenRunnable);
}

void nsMenuX::FlushMenuOpenedRunnable() {
  if (mPendingAsyncMenuOpenRunnable) {
    MenuOpenedAsync();
  }
}

void nsMenuX::MenuOpenedAsync() {
  if (mPendingAsyncMenuOpenRunnable) {
    mPendingAsyncMenuOpenRunnable->Cancel();
    mPendingAsyncMenuOpenRunnable = nullptr;
  }

  mIsOpenForGecko = true;

  // Open the node.
  if (mContent->IsElement()) {
    mContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::open, u"true"_ns, true);
  }

  // Notify our observer.
  if (mObserver) {
    mObserver->OnMenuOpened();
  }

  // Fire popupshown.
  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetMouseEvent event(true, eXULPopupShown, nullptr, WidgetMouseEvent::eReal);
  nsCOMPtr<nsIContent> popupContent = GetMenuPopupContent();
  nsIContent* dispatchTo = popupContent ? popupContent : mContent;
  EventDispatcher::Dispatch(dispatchTo, nullptr, &event, nullptr, &status);
}

void nsMenuX::MenuClosed() {
  if (!mIsOpen) {
    return;
  }

  // Make sure we fire any pending popupshown events first.
  FlushMenuOpenedRunnable();

  // If any of our submenus were opened programmatically, make sure they get closed first.
  for (auto& child : mMenuChildren) {
    if (child.is<RefPtr<nsMenuX>>()) {
      child.as<RefPtr<nsMenuX>>()->MenuClosed();
    }
  }

  mIsOpen = false;

  // Do the rest of the MenuClosed work in MenuClosedAsync.
  // MenuClosed() is called from -[NSMenuDelegate menuDidClose:]. If a menuitem was clicked,
  // menuDidClose is called *before* menuItemHit for the clicked menu item is called.
  // This runnable will be canceled if ~nsMenuX runs before the runnable.
  // The runnable object must not hold a strong reference to the nsMenuX, so that there is no
  // reference cycle.
  class MenuClosedAsyncRunnable final : public mozilla::CancelableRunnable {
   public:
    explicit MenuClosedAsyncRunnable(nsMenuX* aMenu)
        : CancelableRunnable("MenuClosedAsyncRunnable"), mMenu(aMenu) {}

    nsresult Run() override {
      if (mMenu) {
        RefPtr<nsMenuX> menu = mMenu;
        menu->MenuClosedAsync();
        mMenu = nullptr;
      }
      return NS_OK;
    }
    nsresult Cancel() override {
      mMenu = nullptr;
      return NS_OK;
    }

   private:
    nsMenuX* mMenu;  // weak, cleared by Cancel() and Run()
  };
  mPendingAsyncMenuCloseRunnable = new MenuClosedAsyncRunnable(this);
  NS_DispatchToCurrentThread(mPendingAsyncMenuCloseRunnable);
}

void nsMenuX::FlushMenuClosedRunnable() {
  // If any of our submenus have a pending menu closed runnable, make sure those run first.
  for (auto& child : mMenuChildren) {
    if (child.is<RefPtr<nsMenuX>>()) {
      child.as<RefPtr<nsMenuX>>()->FlushMenuClosedRunnable();
    }
  }

  if (mPendingAsyncMenuCloseRunnable) {
    MenuClosedAsync();
  }
}

void nsMenuX::MenuClosedAsync() {
  if (mPendingAsyncMenuCloseRunnable) {
    mPendingAsyncMenuCloseRunnable->Cancel();
    mPendingAsyncMenuCloseRunnable = nullptr;
  }

  // If we have pending command events, run those first.
  for (auto& runnable : mPendingCommandRunnables) {
    runnable->Run();
    runnable->Cancel();  // The runnable is still in the event loop, make sure it doesn't run again.
  }
  mPendingCommandRunnables.Clear();

  // Make sure no item is highlighted.
  OnHighlightedItemChanged(Nothing());

  nsCOMPtr<nsIContent> popupContent = GetMenuPopupContent();
  nsCOMPtr<nsIContent> dispatchTo = popupContent ? popupContent : mContent;

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetMouseEvent popupHiding(true, eXULPopupHiding, nullptr, WidgetMouseEvent::eReal);
  EventDispatcher::Dispatch(dispatchTo, nullptr, &popupHiding, nullptr, &status);

  mIsOpenForGecko = false;

  if (mContent->IsElement()) {
    mContent->AsElement()->UnsetAttr(kNameSpaceID_None, nsGkAtoms::open, true);
  }

  WidgetMouseEvent popupHidden(true, eXULPopupHidden, nullptr, WidgetMouseEvent::eReal);
  EventDispatcher::Dispatch(dispatchTo, nullptr, &popupHidden, nullptr, &status);

  // Notify our observer.
  if (mObserver) {
    mObserver->OnMenuClosed();
  }
}

void nsMenuX::ActivateItemAndClose(RefPtr<nsMenuItemX>&& aItem, NSEventModifierFlags aModifiers) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Run the command asynchronously so that the menu can hide before the command runs.
  class DoCommandRunnable final : public mozilla::CancelableRunnable {
   public:
    explicit DoCommandRunnable(RefPtr<nsMenuItemX>&& aItem, NSEventModifierFlags aModifiers)
        : CancelableRunnable("DoCommandRunnable"), mMenuItem(aItem), mModifiers(aModifiers) {}

    nsresult Run() override {
      if (mMenuItem) {
        RefPtr<nsMenuItemX> menuItem = std::move(mMenuItem);
        menuItem->DoCommand(mModifiers);
      }
      return NS_OK;
    }
    nsresult Cancel() override {
      mMenuItem = nullptr;
      return NS_OK;
    }

   private:
    RefPtr<nsMenuItemX> mMenuItem;  // cleared by Cancel() and Run()
    NSEventModifierFlags mModifiers;
  };
  RefPtr<CancelableRunnable> doCommandAsync = new DoCommandRunnable(std::move(aItem), aModifiers);
  mPendingCommandRunnables.AppendElement(doCommandAsync);
  NS_DispatchToCurrentThread(doCommandAsync);

  if (mIsOpen) {
    // cancelTracking(WithoutAnimation) is asynchronous; the menu only hides once the stack unwinds
    // from NSMenu's nested "tracking" event loop.
    [mNativeMenu cancelTrackingWithoutAnimation];
  }

  // We don't call FlushMenuClosedRunnable() here, so that the command event runs before
  // MenuClosedAsync().

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

bool nsMenuX::Close() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  FlushMenuOpenedRunnable();

  bool wasOpen = mIsOpenForGecko;

  if (mIsOpen) {
    // Close the menu.
    // We usually don't get here during normal Firefox usage: If the user closes the menu by
    // clicking an item, or by clicking outside the menu, or by pressing escape, then the menu gets
    // closed by macOS, and not by a call to nsMenuX::Close().
    // If we do get here, it's usually because we're running an automated test. Close the menu
    // without the fade-out animation so that we don't unnecessarily slow down the automated tests.
    [mNativeMenu cancelTrackingWithoutAnimation];
  }

  FlushMenuClosedRunnable();

  return wasOpen;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuX::OnHighlightedItemChanged(const Maybe<uint32_t>& aNewHighlightedIndex) {
  if (mHighlightedItemIndex == aNewHighlightedIndex) {
    return;
  }

  if (mHighlightedItemIndex) {
    Maybe<nsMenuX::MenuChild> target = GetVisibleItemAt(*mHighlightedItemIndex);
    if (target && target->is<RefPtr<nsMenuItemX>>()) {
      bool handlerCalledPreventDefault;  // but we don't actually care
      target->as<RefPtr<nsMenuItemX>>()->DispatchDOMEvent(u"DOMMenuItemInactive"_ns,
                                                          &handlerCalledPreventDefault);
    }
  }
  if (aNewHighlightedIndex) {
    Maybe<nsMenuX::MenuChild> target = GetVisibleItemAt(*aNewHighlightedIndex);
    if (target && target->is<RefPtr<nsMenuItemX>>()) {
      bool handlerCalledPreventDefault;  // but we don't actually care
      target->as<RefPtr<nsMenuItemX>>()->DispatchDOMEvent(u"DOMMenuItemActive"_ns,
                                                          &handlerCalledPreventDefault);
    }
  }
  mHighlightedItemIndex = aNewHighlightedIndex;
}

void nsMenuX::RebuildMenu() {
  MOZ_RELEASE_ASSERT(mNeedsRebuild);
  gConstructingMenu = true;

  // Retrieve our menupopup.
  nsCOMPtr<nsIContent> menuPopup = GetMenuPopupContent();
  if (!menuPopup) {
    gConstructingMenu = false;
    return;
  }

  // Iterate over the kids
  for (nsIContent* child = menuPopup->GetFirstChild(); child; child = child->GetNextSibling()) {
    // depending on the type, create a menu item, separator, or submenu
    if (child->IsAnyOfXULElements(nsGkAtoms::menuitem, nsGkAtoms::menuseparator)) {
      LoadMenuItem(child);
    } else if (child->IsXULElement(nsGkAtoms::menu)) {
      LoadSubMenu(child);
    }
  }  // for each menu item

  gConstructingMenu = false;
  mNeedsRebuild = false;
}

void nsMenuX::SetRebuild(bool aNeedsRebuild) {
  if (!gConstructingMenu) {
    mNeedsRebuild = aNeedsRebuild;
    if (mParent && mParent->AsMenuBar()) {
      mParent->AsMenuBar()->SetNeedsRebuild();
    }
  }
}

nsresult nsMenuX::SetEnabled(bool aIsEnabled) {
  if (aIsEnabled != mIsEnabled) {
    // we always want to rebuild when this changes
    mIsEnabled = aIsEnabled;
    mNativeMenuItem.enabled = mIsEnabled;
  }
  return NS_OK;
}

nsresult nsMenuX::GetEnabled(bool* aIsEnabled) {
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  *aIsEnabled = mIsEnabled;
  return NS_OK;
}

GeckoNSMenu* nsMenuX::CreateMenuWithGeckoString(nsString& aMenuTitle) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSString* title = [NSString stringWithCharacters:(UniChar*)aMenuTitle.get()
                                            length:aMenuTitle.Length()];
  GeckoNSMenu* myMenu = [[GeckoNSMenu alloc] initWithTitle:title];
  myMenu.delegate = mMenuDelegate;

  // We don't want this menu to auto-enable menu items because then Cocoa
  // overrides our decisions and things get incorrectly enabled/disabled.
  myMenu.autoenablesItems = NO;

  // Disable the Services item for now. Bug 660452 tracks turning this on for the appropriate menus.
  myMenu.allowsContextMenuPlugIns = NO;

  // we used to install Carbon event handlers here, but since NSMenu* doesn't
  // create its underlying MenuRef until just before display, we delay until
  // that happens. Now we install the event handlers when Cocoa notifies
  // us that a menu is about to display - see the Cocoa MenuDelegate class.

  return myMenu;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuX::LoadMenuItem(nsIContent* aMenuItemContent) {
  if (!aMenuItemContent) {
    return;
  }

  nsAutoString menuitemName;
  if (aMenuItemContent->IsElement()) {
    aMenuItemContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label, menuitemName);
  }

  EMenuItemType itemType = eRegularMenuItemType;
  if (aMenuItemContent->IsXULElement(nsGkAtoms::menuseparator)) {
    itemType = eSeparatorMenuItemType;
  } else if (aMenuItemContent->IsElement()) {
    static Element::AttrValuesArray strings[] = {nsGkAtoms::checkbox, nsGkAtoms::radio, nullptr};
    switch (aMenuItemContent->AsElement()->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::type,
                                                           strings, eCaseMatters)) {
      case 0:
        itemType = eCheckboxMenuItemType;
        break;
      case 1:
        itemType = eRadioMenuItemType;
        break;
    }
  }

  AddMenuItem(
      MakeRefPtr<nsMenuItemX>(this, menuitemName, itemType, mMenuGroupOwner, aMenuItemContent));
}

void nsMenuX::LoadSubMenu(nsIContent* aMenuContent) {
  AddMenu(MakeRefPtr<nsMenuX>(this, mMenuGroupOwner, aMenuContent));
}

// This menu is about to open. Returns false if the handler wants to stop the opening of the menu.
bool nsMenuX::OnOpen() {
  if (mDidFirePopupshowingAndIsApprovedToOpen) {
    return true;
  }

  if (mIsOpen) {
    NS_WARNING("nsMenuX::OnOpen() called while the menu is already considered to be open. This "
               "seems odd.");
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetMouseEvent event(true, eXULPopupShowing, nullptr, WidgetMouseEvent::eReal);

  nsCOMPtr<nsIContent> popupContent = GetMenuPopupContent();

  nsresult rv = NS_OK;
  nsIContent* dispatchTo = popupContent ? popupContent : mContent;
  rv = EventDispatcher::Dispatch(dispatchTo, nullptr, &event, nullptr, &status);
  if (NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault) {
    return false;
  }

  DidFirePopupShowing();

  return true;
}

void nsMenuX::DidFirePopupShowing() {
  mDidFirePopupshowingAndIsApprovedToOpen = true;

  // If the open is going to succeed we need to walk our menu items, checking to
  // see if any of them have a command attribute. If so, several attributes
  // must potentially be updated.

  nsCOMPtr<nsIContent> popupContent = GetMenuPopupContent();
  if (!popupContent) {
    return;
  }

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    pm->UpdateMenuItems(popupContent);
  }
}

// Find the |menupopup| child in the |popup| representing this menu. It should be one
// of a very few children so we won't be iterating over a bazillion menu items to find
// it (so the strcmp won't kill us).
already_AddRefed<nsIContent> nsMenuX::GetMenuPopupContent() {
  // Check to see if we are a "menupopup" node (if we are a native menu).
  if (mContent->IsXULElement(nsGkAtoms::menupopup)) {
    return do_AddRef(mContent);
  }

  // Otherwise check our child nodes.

  for (RefPtr<nsIContent> child = mContent->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsXULElement(nsGkAtoms::menupopup)) {
      return child.forget();
    }
  }

  return nullptr;
}

bool nsMenuX::IsXULHelpMenu(nsIContent* aMenuContent) {
  bool retval = false;
  if (aMenuContent && aMenuContent->IsElement()) {
    nsAutoString id;
    aMenuContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::id, id);
    if (id.Equals(u"helpMenu"_ns)) {
      retval = true;
    }
  }
  return retval;
}

//
// nsChangeObserver
//

void nsMenuX::ObserveAttributeChanged(dom::Document* aDocument, nsIContent* aContent,
                                      nsAtom* aAttribute) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // ignore the |open| attribute, which is by far the most common
  if (gConstructingMenu || (aAttribute == nsGkAtoms::open)) {
    return;
  }

  if (aAttribute == nsGkAtoms::disabled) {
    SetEnabled(!mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                                   nsGkAtoms::_true, eCaseMatters));
  } else if (aAttribute == nsGkAtoms::label) {
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label, mLabel);
    NSString* newCocoaLabelString = nsMenuUtilsX::GetTruncatedCocoaLabel(mLabel);
    mNativeMenu.title = newCocoaLabelString;
    mNativeMenuItem.title = newCocoaLabelString;
  } else if (aAttribute == nsGkAtoms::hidden || aAttribute == nsGkAtoms::collapsed) {
    SetRebuild(true);

    bool newVisible = !nsMenuUtilsX::NodeIsHiddenOrCollapsed(mContent);

    // don't do anything if the state is correct already
    if (newVisible == mVisible) {
      return;
    }

    if (mParent) {
      if (newVisible) {
        mParent->InsertChildNativeMenuItem(this);
      } else {
        mParent->RemoveChildNativeMenuItem(this);
      }
    }
    mVisible = newVisible;
  } else if (aAttribute == nsGkAtoms::image) {
    SetupIcon();
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuX::ObserveContentRemoved(dom::Document* aDocument, nsIContent* aContainer,
                                    nsIContent* aChild, nsIContent* aPreviousSibling) {
  if (gConstructingMenu) {
    return;
  }

  SetRebuild(true);
  mMenuGroupOwner->UnregisterForContentChanges(aChild);
}

void nsMenuX::ObserveContentInserted(dom::Document* aDocument, nsIContent* aContainer,
                                     nsIContent* aChild) {
  if (gConstructingMenu) {
    return;
  }

  SetRebuild(true);
}

void nsMenuX::SetupIcon() {
  mIcon->SetupIcon(mContent);
  mNativeMenuItem.image = mIcon->GetIconImage();
}

void nsMenuX::IconUpdated() {
  mNativeMenuItem.image = mIcon->GetIconImage();
  if (mIconListener) {
    mIconListener->IconUpdated();
  }
}

void nsMenuX::InsertChildNativeMenuItem(nsMenuX* aChild) {
  NSInteger insertionPoint = CalculateNativeInsertionPoint(aChild);
  [mNativeMenu insertItem:aChild->NativeNSMenuItem() atIndex:insertionPoint];
}

void nsMenuX::RemoveChildNativeMenuItem(nsMenuX* aChild) {
  NSMenuItem* item = aChild->NativeNSMenuItem();
  if ([mNativeMenu indexOfItem:item] != -1) {
    [mNativeMenu removeItem:item];
  }
}

NSInteger nsMenuX::CalculateNativeInsertionPoint(nsMenuX* aChild) {
  NSInteger insertionPoint = 0;
  for (auto& currItem : mMenuChildren) {
    // Using GetItemAt instead of GetVisibleItemAt to avoid O(N^2)
    if (currItem.is<RefPtr<nsMenuX>>() && currItem.as<RefPtr<nsMenuX>>() == aChild) {
      return insertionPoint;
    }
    NSMenuItem* nativeItem = currItem.match(
        [](const RefPtr<nsMenuX>& aMenu) { return aMenu->NativeNSMenuItem(); },
        [](const RefPtr<nsMenuItemX>& aMenuItem) { return aMenuItem->NativeNSMenuItem(); });
    // Only count items that are inside a menu.
    // XXXmstange Not sure what would cause free-standing items. Maybe for collapsed/hidden menus?
    // In that case, an nsMenuX::IsVisible() method would be better.
    if (nativeItem.menu) {
      insertionPoint++;
    }
  }
  return insertionPoint;
}

void nsMenuX::Dump(uint32_t aIndent) const {
  printf("%*s - menu [%p] %-16s <%s>", aIndent * 2, "", this,
         mLabel.IsEmpty() ? "(empty label)" : NS_ConvertUTF16toUTF8(mLabel).get(),
         NS_ConvertUTF16toUTF8(mContent->NodeName()).get());
  if (mNeedsRebuild) {
    printf(" [NeedsRebuild]");
  }
  if (mIsOpen) {
    printf(" [Open]");
  }
  if (mVisible) {
    printf(" [Visible]");
  }
  if (mIsEnabled) {
    printf(" [IsEnabled]");
  }
  printf(" (%d visible items)", int(mVisibleItemsCount));
  printf("\n");
  for (const auto& subitem : mMenuChildren) {
    subitem.match([=](const RefPtr<nsMenuX>& aMenu) { aMenu->Dump(aIndent + 1); },
                  [=](const RefPtr<nsMenuItemX>& aMenuItem) { aMenuItem->Dump(aIndent + 1); });
  }
}

//
// MenuDelegate Objective-C class, used to set up Carbon events
//

@implementation MenuDelegate

- (id)initWithGeckoMenu:(nsMenuX*)geckoMenu {
  if ((self = [super init])) {
    NS_ASSERTION(geckoMenu,
                 "Cannot initialize native menu delegate with NULL gecko menu! Will crash!");
    mGeckoMenu = geckoMenu;
  }
  return self;
}

- (void)menu:(NSMenu*)aMenu willHighlightItem:(NSMenuItem*)aItem {
  if (!aMenu || !mGeckoMenu) {
    return;
  }

  Maybe<uint32_t> index =
      aItem ? Some(static_cast<uint32_t>([aMenu indexOfItem:aItem])) : Nothing();
  mGeckoMenu->OnHighlightedItemChanged(index);
}

- (void)menuWillOpen:(NSMenu*)menu {
  if (!mGeckoMenu) {
    return;
  }

  // Don't do anything while the OS is (re)indexing our menus (on Leopard and
  // higher).  This stops the Help menu from being able to search in our
  // menus, but it also resolves many other problems.
  if (nsMenuX::sIndexingMenuLevel > 0) {
    return;
  }

  if (self.menuIsInMenubar) {
    // If a menu in the menubar is trying open while a non-native menu is open, roll up the
    // non-native menu and reject the menubar opening attempt, effectively consuming the event.
    nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
    if (rollupListener) {
      nsCOMPtr<nsIWidget> rollupWidget = rollupListener->GetRollupWidget();
      if (rollupWidget) {
        rollupListener->Rollup(0, true, nullptr, nullptr);
        [menu cancelTracking];
        return;
      }
    }
  }

  // Hold a strong reference to mGeckoMenu while calling its methods.
  RefPtr<nsMenuX> geckoMenu = mGeckoMenu;
  geckoMenu->MenuOpened();
}

- (void)menuDidClose:(NSMenu*)menu {
  if (!mGeckoMenu) {
    return;
  }

  // Don't do anything while the OS is (re)indexing our menus (on Leopard and
  // higher).  This stops the Help menu from being able to search in our
  // menus, but it also resolves many other problems.
  if (nsMenuX::sIndexingMenuLevel > 0) {
    return;
  }

  // Hold a strong reference to mGeckoMenu while calling its methods.
  RefPtr<nsMenuX> geckoMenu = mGeckoMenu;
  geckoMenu->MenuClosed();
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

static NSMutableDictionary* gShadowKeyEquivDB = nil;

// Class for values in gShadowKeyEquivDB.

@interface KeyEquivDBItem : NSObject {
  NSMenuItem* mItem;
  NSMutableSet* mTables;
}

- (id)initWithItem:(NSMenuItem*)aItem table:(NSMapTable*)aTable;
- (BOOL)hasTable:(NSMapTable*)aTable;
- (int)addTable:(NSMapTable*)aTable;
- (int)removeTable:(NSMapTable*)aTable;

@end

@implementation KeyEquivDBItem

- (id)initWithItem:(NSMenuItem*)aItem table:(NSMapTable*)aTable {
  if (!gShadowKeyEquivDB) {
    gShadowKeyEquivDB = [[NSMutableDictionary alloc] init];
  }
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
}

- (void)dealloc {
  if (mTables) {
    [mTables release];
  }
  if (mItem) {
    [mItem release];
  }
  [super dealloc];
}

- (BOOL)hasTable:(NSMapTable*)aTable {
  return [mTables member:[NSValue valueWithPointer:aTable]] ? YES : NO;
}

// Does nothing if aTable (its index value) is already present in mTables.
- (int)addTable:(NSMapTable*)aTable {
  if (aTable) {
    [mTables addObject:[NSValue valueWithPointer:aTable]];
  }
  return [mTables count];
}

- (int)removeTable:(NSMapTable*)aTable {
  if (aTable) {
    NSValue* objectToRemove = [mTables member:[NSValue valueWithPointer:aTable]];
    if (objectToRemove) {
      [mTables removeObject:objectToRemove];
    }
  }
  return [mTables count];
}

@end

@interface NSMenu (MethodSwizzling)
+ (void)nsMenuX_NSMenu_addItem:(NSMenuItem*)aItem toTable:(NSMapTable*)aTable;
+ (void)nsMenuX_NSMenu_removeItem:(NSMenuItem*)aItem fromTable:(NSMapTable*)aTable;
@end

@implementation NSMenu (MethodSwizzling)

+ (void)nsMenuX_NSMenu_addItem:(NSMenuItem*)aItem toTable:(NSMapTable*)aTable {
  if (aItem && aTable) {
    NSValue* key = [NSValue valueWithPointer:aItem];
    KeyEquivDBItem* shadowItem = [gShadowKeyEquivDB objectForKey:key];
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

  [self nsMenuX_NSMenu_addItem:aItem toTable:aTable];
}

+ (void)nsMenuX_NSMenu_removeItem:(NSMenuItem*)aItem fromTable:(NSMapTable*)aTable {
  [self nsMenuX_NSMenu_removeItem:aItem fromTable:aTable];

  if (aItem && aTable) {
    NSValue* key = [NSValue valueWithPointer:aItem];
    KeyEquivDBItem* shadowItem = [gShadowKeyEquivDB objectForKey:key];
    if (shadowItem && [shadowItem hasTable:aTable]) {
      if (![shadowItem removeTable:aTable]) {
        [gShadowKeyEquivDB removeObjectForKey:key];
      }
    }
  }
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

- (void)nsMenuX_SCTGRLIndex_indexMenuBarDynamically {
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
