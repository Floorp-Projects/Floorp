/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>
#include "nsThreadUtils.h"
#include "mozilla/dom/Document.h"

#include "NativeMenuMac.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/Element.h"
#include "nsISupports.h"
#include "nsGkAtoms.h"
#include "nsGkAtoms.h"
#include "nsMenuGroupOwnerX.h"
#include "nsMenuItemX.h"
#include "nsMenuUtilsX.h"
#include "nsObjCExceptions.h"
#include "mozilla/dom/Document.h"
#include "PresShell.h"
#include "nsCocoaUtils.h"
#include "nsIFrame.h"
#include "nsCocoaFeatures.h"

#if !defined(MAC_OS_X_VERSION_10_14) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_14
@interface NSApplication (NSApplicationAppearance)
@property(readonly, strong) NSAppearance* effectiveAppearance NS_AVAILABLE_MAC(10_14);
@end
#endif

#if !defined(MAC_OS_VERSION_11_0) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_11_0
@interface NSMenu (NSMenuAppearance)
// In reality, NSMenu implements the NSAppearanceCustomization protocol, and picks up the appearance
// property from that protocol. But we can't tack on protocol implementations, so we just declare
// the property setter here.
- (void)setAppearance:(NSAppearance*)appearance;
@end
#endif

namespace mozilla {

using dom::Element;

namespace widget {

NativeMenuMac::NativeMenuMac(mozilla::dom::Element* aElement)
    : mElement(aElement), mContainerStatusBarItem(nil) {
  MOZ_RELEASE_ASSERT(aElement->IsAnyOfXULElements(nsGkAtoms::menu, nsGkAtoms::menupopup));
  mMenuGroupOwner = new nsMenuGroupOwnerX(aElement, nullptr);
  mMenu = MakeRefPtr<nsMenuX>(nullptr, mMenuGroupOwner, aElement);
  mMenu->SetObserver(this);
  mMenu->SetIconListener(this);
  mMenu->SetupIcon();
}

NativeMenuMac::~NativeMenuMac() {
  mMenu->DetachFromGroupOwnerRecursive();
  mMenu->ClearObserver();
  mMenu->ClearIconListener();
}

static void UpdateMenu(nsMenuX* aMenu) {
  aMenu->MenuOpened();
  aMenu->MenuClosed();

  uint32_t itemCount = aMenu->GetItemCount();
  for (uint32_t i = 0; i < itemCount; i++) {
    nsMenuX::MenuChild menuObject = *aMenu->GetItemAt(i);
    if (menuObject.is<RefPtr<nsMenuX>>()) {
      UpdateMenu(menuObject.as<RefPtr<nsMenuX>>());
    }
  }
}

void NativeMenuMac::MenuWillOpen() {
  // Force an update on the mMenu by faking an open/close on all of
  // its submenus.
  UpdateMenu(mMenu.get());
}

bool NativeMenuMac::ActivateNativeMenuItemAt(const nsAString& aIndexString) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSMenu* menu = mMenu->NativeNSMenu();

  nsMenuUtilsX::CheckNativeMenuConsistency(menu);

  NSString* locationString =
      [NSString stringWithCharacters:reinterpret_cast<const unichar*>(aIndexString.BeginReading())
                              length:aIndexString.Length()];
  NSMenuItem* item = nsMenuUtilsX::NativeMenuItemWithLocation(menu, locationString, false);

  // We can't perform an action on an item with a submenu, that will raise
  // an obj-c exception.
  if (item && !item.hasSubmenu) {
    NSMenu* parent = item.menu;
    if (parent) {
      // NSLog(@"Performing action for native menu item titled: %@\n",
      //       [[currentSubmenu itemAtIndex:targetIndex] title]);
      [parent performActionForItemAtIndex:[parent indexOfItem:item]];
      return true;
    }
  }

  return false;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void NativeMenuMac::ForceUpdateNativeMenuAt(const nsAString& aIndexString) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSString* locationString =
      [NSString stringWithCharacters:reinterpret_cast<const unichar*>(aIndexString.BeginReading())
                              length:aIndexString.Length()];
  NSArray<NSString*>* indexes = [locationString componentsSeparatedByString:@"|"];
  RefPtr<nsMenuX> currentMenu = mMenu.get();

  // now find the correct submenu
  unsigned int indexCount = indexes.count;
  for (unsigned int i = 1; currentMenu && i < indexCount; i++) {
    int targetIndex = [indexes objectAtIndex:i].intValue;
    int visible = 0;
    uint32_t length = currentMenu->GetItemCount();
    for (unsigned int j = 0; j < length; j++) {
      Maybe<nsMenuX::MenuChild> targetMenu = currentMenu->GetItemAt(j);
      if (!targetMenu) {
        return;
      }
      RefPtr<nsIContent> content = targetMenu->match(
          [](const RefPtr<nsMenuX>& aMenu) { return aMenu->Content(); },
          [](const RefPtr<nsMenuItemX>& aMenuItem) { return aMenuItem->Content(); });
      if (!nsMenuUtilsX::NodeIsHiddenOrCollapsed(content)) {
        visible++;
        if (targetMenu->is<RefPtr<nsMenuX>>() && visible == (targetIndex + 1)) {
          currentMenu = targetMenu->as<RefPtr<nsMenuX>>();
          break;
        }
      }
    }
  }

  // fake open/close to cause lazy update to happen
  currentMenu->MenuOpened();
  currentMenu->MenuClosed();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void NativeMenuMac::IconUpdated() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mContainerStatusBarItem) {
    NSImage* menuImage = mMenu->NativeNSMenuItem().image;
    if (menuImage) {
      [menuImage setTemplate:YES];
    }
    mContainerStatusBarItem.image = menuImage;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void NativeMenuMac::SetContainerStatusBarItem(NSStatusItem* aItem) {
  mContainerStatusBarItem = aItem;
  IconUpdated();
}

void NativeMenuMac::Dump() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mMenu->Dump(0);
  nsMenuUtilsX::DumpNativeMenu(mMenu->NativeNSMenu());

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void NativeMenuMac::OnMenuOpened() {
  // Our caller isn't keeping us alive, so make sure we stay alive throughout this function in case
  // one of the observer notifications destroys us.
  RefPtr<NativeMenuMac> kungFuDeathGrip(this);

  for (NativeMenu::Observer* observer : mObservers.Clone()) {
    observer->OnNativeMenuOpened();
  }
}
void NativeMenuMac::OnMenuClosed() {
  // Our caller isn't keeping us alive, so make sure we stay alive throughout this function in case
  // one of the observer notifications destroys us.
  RefPtr<NativeMenuMac> kungFuDeathGrip(this);

  for (NativeMenu::Observer* observer : mObservers.Clone()) {
    observer->OnNativeMenuClosed();
  }
}

static NSView* NativeViewForContent(nsIContent* aContent) {
  mozilla::dom::Document* doc = aContent->GetUncomposedDoc();
  if (!doc) {
    return nil;
  }

  PresShell* presShell = doc->GetPresShell();
  if (!presShell) {
    return nil;
  }

  nsIFrame* frame = presShell->GetRootFrame();
  if (!frame) {
    return nil;
  }

  nsIWidget* widget = frame->GetNearestWidget();
  return (NSView*)widget->GetNativeData(NS_NATIVE_WIDGET);
}

void NativeMenuMac::ShowAsContextMenu(const mozilla::DesktopPoint& aPosition) {
  mMenu->PopupShowingEventWasSentAndApprovedExternally();

  // Do the actual opening off of a runnable, so that this ShowAsContextMenu call does not spawn a
  // nested event loop, which would be surprising to our callers.
  mozilla::DesktopPoint position = aPosition;
  RefPtr<NativeMenuMac> self = this;
  NS_DispatchToCurrentThread(NS_NewRunnableFunction("nsStandaloneNativeMenu::OpenMenu",
                                                    [=]() { self->OpenMenu(position); }));
}

void NativeMenuMac::OpenMenu(const mozilla::DesktopPoint& aPosition) {
  // There are multiple ways to display an NSMenu as a context menu.
  //
  //  1. We can return the NSMenu from -[ChildView menuForEvent:] and the NSView will open it for
  //     us.
  //  2. We can call +[NSMenu popUpContextMenu:withEvent:forView:] inside a mouseDown handler with a
  //     real mouse down event.
  //  3. We can call +[NSMenu popUpContextMenu:withEvent:forView:] at a later time, with a real
  //     mouse event that we stored earlier.
  //  4. We can call +[NSMenu popUpContextMenu:withEvent:forView:] at any time, with a synthetic
  //     mouse event that we create just for that purpose.
  //  5. We can call -[NSMenu popUpMenuPositioningItem:atLocation:inView:] and it just takes a
  //     position, not an event.
  //
  // 1-4 look the same, 5 looks different: 5 is made for use with NSPopUpButton, where the selected
  // item needs to be shown at a specific position. If a tall menu is opened with a position close
  // to the bottom edge of the screen, 5 results in a cropped menu with scroll arrows, even if the
  // entire menu would fit on the screen, due to the positioning constraint.
  // 1-2 only work if the menu contents are known synchronously during the call to menuForEvent or
  // during the mouseDown event handler.
  // NativeMenuMac::ShowAsContextMenu can be called at any time. It could be called during a
  // menuForEvent call (during a "contextmenu" event handler), or during a mouseDown handler, or at
  // a later time.
  // The code below uses option 4 as the preferred option because it's the simplest: It works in all
  // scenarios and it doesn't have the positioning drawbacks of option 5.

  NSView* view = NativeViewForContent(mMenu->Content());
  NSMenu* nativeMenu = mMenu->NativeNSMenu();

  if (@available(macOS 10.14, *)) {
#if !defined(MAC_OS_VERSION_11_0) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_11_0
    if (nsCocoaFeatures::OnBigSurOrLater()) {
#else
    if (@available(macOS 11.0, *)) {
#endif
      // Make native context menus respect the NSApp appearance rather than the NSWindow appearance.
      [nativeMenu setAppearance:NSApp.effectiveAppearance];
    }
  }

  NSPoint locationOnScreen = nsCocoaUtils::GeckoPointToCocoaPoint(aPosition);
  if (view) {
    // Create a synthetic event at the right location and open the menu [option 4].
    NSPoint locationInWindow = nsCocoaUtils::ConvertPointFromScreen(view.window, locationOnScreen);
    NSEvent* event = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                                        location:locationInWindow
                                   modifierFlags:0
                                       timestamp:[[NSProcessInfo processInfo] systemUptime]
                                    windowNumber:view.window.windowNumber
                                         context:nil
                                     eventNumber:0
                                      clickCount:1
                                        pressure:0.0f];
    [NSMenu popUpContextMenu:nativeMenu withEvent:event forView:view];
  } else {
    // Open the menu using popUpMenuPositioningItem:atLocation:inView: [option 5].
    // This is not preferred, because it positions the menu differently from how a native context
    // menu would be positioned; it enforces locationOnScreen for the top left corner even if this
    // means that the menu will be displayed in a clipped fashion with scroll arrows.
    [nativeMenu popUpMenuPositioningItem:nil atLocation:locationOnScreen inView:nil];
  }
}

bool NativeMenuMac::Close() { return mMenu->Close(); }

RefPtr<Element> NativeMenuMac::Element() { return mElement; }

}  // namespace widget
}  // namespace mozilla
