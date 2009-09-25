/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Sylvain Pasche <sylvain.pasche@gmail.com>
 *   Stuart Morgan <stuart.morgan@alumni.case.edu>
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

#include "nsCocoaUtils.h"
#include "nsMenuBarX.h"
#include "nsCocoaWindow.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIAppShellService.h"
#include "nsIXULWindow.h"
#include "nsIBaseWindow.h"
#include "nsIServiceManager.h"
#include "nsMenuUtilsX.h"
#include "nsToolkit.h"

float nsCocoaUtils::MenuBarScreenHeight()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSArray* allScreens = [NSScreen screens];
  if ([allScreens count])
    return [[allScreens objectAtIndex:0] frame].size.height;
  else
    return 0.0; // If there are no screens, there's not much we can say.

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0.0);
}

float nsCocoaUtils::FlippedScreenY(float y)
{
  return MenuBarScreenHeight() - y;
}

NSRect nsCocoaUtils::GeckoRectToCocoaRect(const nsIntRect &geckoRect)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // We only need to change the Y coordinate by starting with the primary screen
  // height, subtracting the gecko Y coordinate, and subtracting the height.
  return NSMakeRect(geckoRect.x,
                    MenuBarScreenHeight() - (geckoRect.y + geckoRect.height),
                    geckoRect.width,
                    geckoRect.height);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRect(0.0, 0.0, 0.0, 0.0));
}

nsIntRect nsCocoaUtils::CocoaRectToGeckoRect(const NSRect &cocoaRect)
{
  // We only need to change the Y coordinate by starting with the primary screen
  // height and subtracting both the cocoa y origin and the height of the
  // cocoa rect.
  nsIntRect rect;
  rect.x = NSToIntRound(cocoaRect.origin.x);
  rect.y = NSToIntRound(FlippedScreenY(cocoaRect.origin.y + cocoaRect.size.height));
  rect.width = NSToIntRound(cocoaRect.origin.x + cocoaRect.size.width) - rect.x;
  rect.height = NSToIntRound(FlippedScreenY(cocoaRect.origin.y)) - rect.y;
  return rect;
}

NSPoint nsCocoaUtils::ScreenLocationForEvent(NSEvent* anEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // Don't trust mouse locations of mouse move events, see bug 443178.
  if ([anEvent type] == NSMouseMoved)
    return [NSEvent mouseLocation];

  return [[anEvent window] convertBaseToScreen:[anEvent locationInWindow]];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakePoint(0.0, 0.0));
}

BOOL nsCocoaUtils::IsEventOverWindow(NSEvent* anEvent, NSWindow* aWindow)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return NSPointInRect(ScreenLocationForEvent(anEvent), [aWindow frame]);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

NSPoint nsCocoaUtils::EventLocationForWindow(NSEvent* anEvent, NSWindow* aWindow)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return [aWindow convertScreenToBase:ScreenLocationForEvent(anEvent)];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakePoint(0.0, 0.0));
}

void nsCocoaUtils::HideOSChromeOnScreen(PRBool aShouldHide, NSScreen* aScreen)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Keep track of how many hiding requests have been made, so that they can
  // be nested.
  static int sMenuBarHiddenCount = 0, sDockHiddenCount = 0;

  // Always hide the Dock, since it's not necessarily on the primary screen.
  sDockHiddenCount += aShouldHide ? 1 : -1;
  NS_ASSERTION(sMenuBarHiddenCount >= 0, "Unbalanced HideMenuAndDockForWindow calls");

  // Only hide the menu bar if the window is on the same screen.
  // The menu bar is always on the first screen in the screen list.
  if (aScreen == [[NSScreen screens] objectAtIndex:0]) {
    sMenuBarHiddenCount += aShouldHide ? 1 : -1;
    NS_ASSERTION(sDockHiddenCount >= 0, "Unbalanced HideMenuAndDockForWindow calls");
  }

  if (sMenuBarHiddenCount > 0) {
    ::SetSystemUIMode(kUIModeAllHidden, 0);
  } else if (sDockHiddenCount > 0) {
    ::SetSystemUIMode(kUIModeContentHidden, 0);
  } else {
    ::SetSystemUIMode(kUIModeNormal, 0);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


#define NS_APPSHELLSERVICE_CONTRACTID "@mozilla.org/appshell/appShellService;1"
nsIWidget* nsCocoaUtils::GetHiddenWindowWidget()
{
  nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  if (!appShell) {
    NS_WARNING("Couldn't get AppShellService in order to get hidden window ref");
    return nsnull;
  }
  
  nsCOMPtr<nsIXULWindow> hiddenWindow;
  appShell->GetHiddenWindow(getter_AddRefs(hiddenWindow));
  if (!hiddenWindow) {
    // Don't warn, this happens during shutdown, bug 358607.
    return nsnull;
  }
  
  nsCOMPtr<nsIBaseWindow> baseHiddenWindow;
  baseHiddenWindow = do_GetInterface(hiddenWindow);
  if (!baseHiddenWindow) {
    NS_WARNING("Couldn't get nsIBaseWindow from hidden window (nsIXULWindow)");
    return nsnull;
  }
  
  nsCOMPtr<nsIWidget> hiddenWindowWidget;
  if (NS_FAILED(baseHiddenWindow->GetMainWidget(getter_AddRefs(hiddenWindowWidget)))) {
    NS_WARNING("Couldn't get nsIWidget from hidden window (nsIBaseWindow)");
    return nsnull;
  }
  
  return hiddenWindowWidget;
}

void nsCocoaUtils::PrepareForNativeAppModalDialog()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Don't do anything if this is embedding. We'll assume that if there is no hidden
  // window we shouldn't do anything, and that should cover the embedding case.
  nsMenuBarX* hiddenWindowMenuBar = nsMenuUtilsX::GetHiddenWindowMenuBar();
  if (!hiddenWindowMenuBar)
    return;

  // First put up the hidden window menu bar so that app menu event handling is correct.
  hiddenWindowMenuBar->Paint();

  NSMenu* mainMenu = [NSApp mainMenu];
  NS_ASSERTION([mainMenu numberOfItems] > 0, "Main menu does not have any items, something is terribly wrong!");
  
  // Create new menu bar for use with modal dialog
  NSMenu* newMenuBar = [[NSMenu alloc] initWithTitle:@""];
  
  // Swap in our app menu. Note that the event target is whatever window is up when
  // the app modal dialog goes up.
  NSMenuItem* firstMenuItem = [[mainMenu itemAtIndex:0] retain];
  [mainMenu removeItemAtIndex:0];
  [newMenuBar insertItem:firstMenuItem atIndex:0];
  [firstMenuItem release];
  
  // Add standard edit menu
  [newMenuBar addItem:nsMenuUtilsX::GetStandardEditMenuItem()];
  
  // Show the new menu bar
  [NSApp setMainMenu:newMenuBar];
  [newMenuBar release];
  
  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaUtils::CleanUpAfterNativeAppModalDialog()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Don't do anything if this is embedding. We'll assume that if there is no hidden
  // window we shouldn't do anything, and that should cover the embedding case.
  nsMenuBarX* hiddenWindowMenuBar = nsMenuUtilsX::GetHiddenWindowMenuBar();
  if (!hiddenWindowMenuBar)
    return;

  NSWindow* mainWindow = [NSApp mainWindow];
  if (!mainWindow)
    hiddenWindowMenuBar->Paint();
  else
    [WindowDelegate paintMenubarForWindow:mainWindow];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

unsigned short nsCocoaUtils::GetCocoaEventKeyCode(NSEvent *theEvent)
{
  unsigned short keyCode = [theEvent keyCode];
  if (nsToolkit::OnLeopardOrLater())
    return keyCode;
  NSEventType type = [theEvent type];
  // GetCocoaEventKeyCode() can get called with theEvent set to a FlagsChanged
  // event, which triggers an NSInternalInconsistencyException when
  // charactersIgnoringModifiers is called on it.  For some reason there's no
  // problem calling keyCode on it (as we do above).
  if ((type != NSKeyDown) && (type != NSKeyUp))
    return keyCode;
  NSString *unmodchars = [theEvent charactersIgnoringModifiers];
  if (!keyCode && ([unmodchars length] == 1)) {
    // An OS-X-10.4.X-specific Apple bug causes the 'theEvent' parameter of
    // all calls to performKeyEquivalent: (whether on NSMenu, NSWindow or
    // NSView objects) to have most of its fields zeroed on a ctrl-ESC event.
    // These include its keyCode and modifierFlags fields, but fortunately
    // not its characters and charactersIgnoringModifiers fields.  So if
    // charactersIgnoringModifiers has length == 1 and corresponds to the ESC
    // character (0x1b), we correct keyCode to 0x35 (kEscapeKeyCode).
    if ([unmodchars characterAtIndex:0] == 0x1b)
      keyCode = 0x35;
  }
  return keyCode;
}

NSUInteger nsCocoaUtils::GetCocoaEventModifierFlags(NSEvent *theEvent)
{
  NSUInteger modifierFlags = [theEvent modifierFlags];
  if (nsToolkit::OnLeopardOrLater())
    return modifierFlags;
  NSEventType type = [theEvent type];
  if ((type != NSKeyDown) && (type != NSKeyUp))
    return modifierFlags;
  NSString *unmodchars = [theEvent charactersIgnoringModifiers];
  if (!modifierFlags && ([unmodchars length] == 1)) {
    // An OS-X-10.4.X-specific Apple bug causes the 'theEvent' parameter of
    // all calls to performKeyEquivalent: (whether on NSMenu, NSWindow or
    // NSView objects) to have most of its fields zeroed on a ctrl-ESC event.
    // These include its keyCode and modifierFlags fields, but fortunately
    // not its characters and charactersIgnoringModifiers fields.  So if
    // charactersIgnoringModifiers has length == 1 and corresponds to the ESC
    // character (0x1b), we correct modifierFlags to NSControlKeyMask.  (ESC
    // key events don't get messed up (anywhere they're sent) on opt-ESC,
    // shift-ESC or cmd-ESC.)
    if ([unmodchars characterAtIndex:0] == 0x1b)
      modifierFlags = NSControlKeyMask;
  }
  return modifierFlags;
}
