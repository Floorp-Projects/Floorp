/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuUtilsX.h"
#include "nsMenuBarX.h"
#include "nsMenuX.h"
#include "nsMenuItemX.h"
#include "nsStandaloneNativeMenu.h"
#include "nsObjCExceptions.h"
#include "nsCocoaUtils.h"
#include "nsCocoaWindow.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMXULCommandEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsPIDOMWindow.h"

void nsMenuUtilsX::DispatchCommandTo(nsIContent* aTargetContent)
{
  NS_PRECONDITION(aTargetContent, "null ptr");

  nsIDocument* doc = aTargetContent->OwnerDoc();
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(doc);
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(aTargetContent);
  if (domDoc && target) {
    nsCOMPtr<nsIDOMEvent> event;
    domDoc->CreateEvent(NS_LITERAL_STRING("xulcommandevent"),
                        getter_AddRefs(event));
    nsCOMPtr<nsIDOMXULCommandEvent> command = do_QueryInterface(event);
    nsCOMPtr<nsIPrivateDOMEvent> pEvent = do_QueryInterface(command);

    // FIXME: Should probably figure out how to init this with the actual
    // pressed keys, but this is a big old edge case anyway. -dwh
    if (pEvent &&
        NS_SUCCEEDED(command->InitCommandEvent(NS_LITERAL_STRING("command"),
                                               true, true,
                                               doc->GetWindow(), 0,
                                               false, false, false,
                                               false, nsnull))) {
      pEvent->SetTrusted(true);
      bool dummy;
      target->DispatchEvent(event, &dummy);
    }
  }
}

NSString* nsMenuUtilsX::GetTruncatedCocoaLabel(const nsString& itemLabel)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // We want to truncate long strings to some reasonable pixel length but there is no
  // good API for doing that which works for all OS versions and architectures. For now
  // we'll do nothing for consistency and depend on good user interface design to limit
  // string lengths.
  return [NSString stringWithCharacters:itemLabel.get() length:itemLabel.Length()];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

PRUint8 nsMenuUtilsX::GeckoModifiersForNodeAttribute(const nsString& modifiersAttribute)
{
  PRUint8 modifiers = knsMenuItemNoModifier;
  char* str = ToNewCString(modifiersAttribute);
  char* newStr;
  char* token = strtok_r(str, ", \t", &newStr);
  while (token != NULL) {
    if (strcmp(token, "shift") == 0)
      modifiers |= knsMenuItemShiftModifier;
    else if (strcmp(token, "alt") == 0) 
      modifiers |= knsMenuItemAltModifier;
    else if (strcmp(token, "control") == 0) 
      modifiers |= knsMenuItemControlModifier;
    else if ((strcmp(token, "accel") == 0) ||
             (strcmp(token, "meta") == 0)) {
      modifiers |= knsMenuItemCommandModifier;
    }
    token = strtok_r(newStr, ", \t", &newStr);
  }
  free(str);

  return modifiers;
}

unsigned int nsMenuUtilsX::MacModifiersForGeckoModifiers(PRUint8 geckoModifiers)
{
  unsigned int macModifiers = 0;
  
  if (geckoModifiers & knsMenuItemShiftModifier)
    macModifiers |= NSShiftKeyMask;
  if (geckoModifiers & knsMenuItemAltModifier)
    macModifiers |= NSAlternateKeyMask;
  if (geckoModifiers & knsMenuItemControlModifier)
    macModifiers |= NSControlKeyMask;
  if (geckoModifiers & knsMenuItemCommandModifier)
    macModifiers |= NSCommandKeyMask;

  return macModifiers;
}

nsMenuBarX* nsMenuUtilsX::GetHiddenWindowMenuBar()
{
  nsIWidget* hiddenWindowWidgetNoCOMPtr = nsCocoaUtils::GetHiddenWindowWidget();
  if (hiddenWindowWidgetNoCOMPtr)
    return static_cast<nsCocoaWindow*>(hiddenWindowWidgetNoCOMPtr)->GetMenuBar();
  else
    return nsnull;
}

// It would be nice if we could localize these edit menu names.
NSMenuItem* nsMenuUtilsX::GetStandardEditMenuItem()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // In principle we should be able to allocate this once and then always
  // return the same object.  But wierd interactions happen between native
  // app-modal dialogs and Gecko-modal dialogs that open above them.  So what
  // we return here isn't always released before it needs to be added to
  // another menu.  See bmo bug 468393.
  NSMenuItem* standardEditMenuItem =
    [[[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""] autorelease];
  NSMenu* standardEditMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
  [standardEditMenuItem setSubmenu:standardEditMenu];
  [standardEditMenu release];

  // Add Undo
  NSMenuItem* undoItem = [[NSMenuItem alloc] initWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
  [standardEditMenu addItem:undoItem];
  [undoItem release];

  // Add Redo
  NSMenuItem* redoItem = [[NSMenuItem alloc] initWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
  [standardEditMenu addItem:redoItem];
  [redoItem release];

  // Add separator
  [standardEditMenu addItem:[NSMenuItem separatorItem]];

  // Add Cut
  NSMenuItem* cutItem = [[NSMenuItem alloc] initWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
  [standardEditMenu addItem:cutItem];
  [cutItem release];

  // Add Copy
  NSMenuItem* copyItem = [[NSMenuItem alloc] initWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
  [standardEditMenu addItem:copyItem];
  [copyItem release];

  // Add Paste
  NSMenuItem* pasteItem = [[NSMenuItem alloc] initWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
  [standardEditMenu addItem:pasteItem];
  [pasteItem release];

  // Add Delete
  NSMenuItem* deleteItem = [[NSMenuItem alloc] initWithTitle:@"Delete" action:@selector(delete:) keyEquivalent:@""];
  [standardEditMenu addItem:deleteItem];
  [deleteItem release];

  // Add Select All
  NSMenuItem* selectAllItem = [[NSMenuItem alloc] initWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
  [standardEditMenu addItem:selectAllItem];
  [selectAllItem release];

  return standardEditMenuItem;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

bool nsMenuUtilsX::NodeIsHiddenOrCollapsed(nsIContent* inContent)
{
  return (inContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                                 nsGkAtoms::_true, eCaseMatters) ||
          inContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::collapsed,
                                 nsGkAtoms::_true, eCaseMatters));
}

// Determines how many items are visible among the siblings in a menu that are
// before the given child. This will not count the application menu.
int nsMenuUtilsX::CalculateNativeInsertionPoint(nsMenuObjectX* aParent,
                                                nsMenuObjectX* aChild)
{
  int insertionPoint = 0;
  nsMenuObjectTypeX parentType = aParent->MenuObjectType();
  if (parentType == eMenuBarObjectType) {
    nsMenuBarX* menubarParent = static_cast<nsMenuBarX*>(aParent);
    PRUint32 numMenus = menubarParent->GetMenuCount();
    for (PRUint32 i = 0; i < numMenus; i++) {
      nsMenuX* currMenu = menubarParent->GetMenuAt(i);
      if (currMenu == aChild)
        return insertionPoint; // we found ourselves, break out
      if (currMenu && [currMenu->NativeMenuItem() menu])
        insertionPoint++;
    }
  }
  else if (parentType == eSubmenuObjectType ||
           parentType == eStandaloneNativeMenuObjectType) {
    nsMenuX* menuParent;
    if (parentType == eSubmenuObjectType)
      menuParent = static_cast<nsMenuX*>(aParent);
    else
      menuParent = static_cast<nsStandaloneNativeMenu*>(aParent)->GetMenuXObject();

    PRUint32 numItems = menuParent->GetItemCount();
    for (PRUint32 i = 0; i < numItems; i++) {
      // Using GetItemAt instead of GetVisibleItemAt to avoid O(N^2)
      nsMenuObjectX* currItem = menuParent->GetItemAt(i);
      if (currItem == aChild)
        return insertionPoint; // we found ourselves, break out
      NSMenuItem* nativeItem = nil;
      nsMenuObjectTypeX currItemType = currItem->MenuObjectType();
      if (currItemType == eSubmenuObjectType)
        nativeItem = static_cast<nsMenuX*>(currItem)->NativeMenuItem();
      else
        nativeItem = (NSMenuItem*)(currItem->NativeData());
      if ([nativeItem menu])
        insertionPoint++;
    }
  }
  return insertionPoint;
}
