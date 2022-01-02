/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuUtilsX.h"
#include <unordered_set>

#include "mozilla/EventForwards.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/XULCommandEvent.h"
#include "nsMenuBarX.h"
#include "nsMenuX.h"
#include "nsMenuItemX.h"
#include "NativeMenuMac.h"
#include "nsObjCExceptions.h"
#include "nsCocoaUtils.h"
#include "nsCocoaWindow.h"
#include "nsGkAtoms.h"
#include "nsGlobalWindowInner.h"
#include "nsPIDOMWindow.h"
#include "nsQueryObject.h"

using namespace mozilla;

void nsMenuUtilsX::DispatchCommandTo(nsIContent* aTargetContent,
                                     NSEventModifierFlags aModifierFlags, int16_t aButton) {
  MOZ_ASSERT(aTargetContent, "null ptr");

  dom::Document* doc = aTargetContent->OwnerDoc();
  if (doc) {
    RefPtr<dom::XULCommandEvent> event =
        new dom::XULCommandEvent(doc, doc->GetPresContext(), nullptr);

    bool ctrlKey = aModifierFlags & NSEventModifierFlagControl;
    bool altKey = aModifierFlags & NSEventModifierFlagOption;
    bool shiftKey = aModifierFlags & NSEventModifierFlagShift;
    bool cmdKey = aModifierFlags & NSEventModifierFlagCommand;

    IgnoredErrorResult rv;
    event->InitCommandEvent(u"command"_ns, true, true,
                            nsGlobalWindowInner::Cast(doc->GetInnerWindow()), 0, ctrlKey, altKey,
                            shiftKey, cmdKey, aButton, nullptr, 0, rv);
    if (!rv.Failed()) {
      event->SetTrusted(true);
      aTargetContent->DispatchEvent(*event);
    }
  }
}

NSString* nsMenuUtilsX::GetTruncatedCocoaLabel(const nsString& itemLabel) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // We want to truncate long strings to some reasonable pixel length but there is no
  // good API for doing that which works for all OS versions and architectures. For now
  // we'll do nothing for consistency and depend on good user interface design to limit
  // string lengths.
  return [NSString stringWithCharacters:reinterpret_cast<const unichar*>(itemLabel.get())
                                 length:itemLabel.Length()];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

uint8_t nsMenuUtilsX::GeckoModifiersForNodeAttribute(const nsString& modifiersAttribute) {
  uint8_t modifiers = knsMenuItemNoModifier;
  char* str = ToNewCString(modifiersAttribute);
  char* newStr;
  char* token = strtok_r(str, ", \t", &newStr);
  while (token != nullptr) {
    if (strcmp(token, "shift") == 0) {
      modifiers |= knsMenuItemShiftModifier;
    } else if (strcmp(token, "alt") == 0) {
      modifiers |= knsMenuItemAltModifier;
    } else if (strcmp(token, "control") == 0) {
      modifiers |= knsMenuItemControlModifier;
    } else if ((strcmp(token, "accel") == 0) || (strcmp(token, "meta") == 0)) {
      modifiers |= knsMenuItemCommandModifier;
    }
    token = strtok_r(newStr, ", \t", &newStr);
  }
  free(str);

  return modifiers;
}

unsigned int nsMenuUtilsX::MacModifiersForGeckoModifiers(uint8_t geckoModifiers) {
  unsigned int macModifiers = 0;

  if (geckoModifiers & knsMenuItemShiftModifier) {
    macModifiers |= NSEventModifierFlagShift;
  }
  if (geckoModifiers & knsMenuItemAltModifier) {
    macModifiers |= NSEventModifierFlagOption;
  }
  if (geckoModifiers & knsMenuItemControlModifier) {
    macModifiers |= NSEventModifierFlagControl;
  }
  if (geckoModifiers & knsMenuItemCommandModifier) {
    macModifiers |= NSEventModifierFlagCommand;
  }

  return macModifiers;
}

nsMenuBarX* nsMenuUtilsX::GetHiddenWindowMenuBar() {
  nsIWidget* hiddenWindowWidgetNoCOMPtr = nsCocoaUtils::GetHiddenWindowWidget();
  if (hiddenWindowWidgetNoCOMPtr) {
    return static_cast<nsCocoaWindow*>(hiddenWindowWidgetNoCOMPtr)->GetMenuBar();
  }
  return nullptr;
}

// It would be nice if we could localize these edit menu names.
NSMenuItem* nsMenuUtilsX::GetStandardEditMenuItem() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // In principle we should be able to allocate this once and then always
  // return the same object.  But weird interactions happen between native
  // app-modal dialogs and Gecko-modal dialogs that open above them.  So what
  // we return here isn't always released before it needs to be added to
  // another menu.  See bmo bug 468393.
  NSMenuItem* standardEditMenuItem = [[[NSMenuItem alloc] initWithTitle:@"Edit"
                                                                 action:nil
                                                          keyEquivalent:@""] autorelease];
  NSMenu* standardEditMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
  standardEditMenuItem.submenu = standardEditMenu;
  [standardEditMenu release];

  // Add Undo
  NSMenuItem* undoItem = [[NSMenuItem alloc] initWithTitle:@"Undo"
                                                    action:@selector(undo:)
                                             keyEquivalent:@"z"];
  [standardEditMenu addItem:undoItem];
  [undoItem release];

  // Add Redo
  NSMenuItem* redoItem = [[NSMenuItem alloc] initWithTitle:@"Redo"
                                                    action:@selector(redo:)
                                             keyEquivalent:@"Z"];
  [standardEditMenu addItem:redoItem];
  [redoItem release];

  // Add separator
  [standardEditMenu addItem:[NSMenuItem separatorItem]];

  // Add Cut
  NSMenuItem* cutItem = [[NSMenuItem alloc] initWithTitle:@"Cut"
                                                   action:@selector(cut:)
                                            keyEquivalent:@"x"];
  [standardEditMenu addItem:cutItem];
  [cutItem release];

  // Add Copy
  NSMenuItem* copyItem = [[NSMenuItem alloc] initWithTitle:@"Copy"
                                                    action:@selector(copy:)
                                             keyEquivalent:@"c"];
  [standardEditMenu addItem:copyItem];
  [copyItem release];

  // Add Paste
  NSMenuItem* pasteItem = [[NSMenuItem alloc] initWithTitle:@"Paste"
                                                     action:@selector(paste:)
                                              keyEquivalent:@"v"];
  [standardEditMenu addItem:pasteItem];
  [pasteItem release];

  // Add Delete
  NSMenuItem* deleteItem = [[NSMenuItem alloc] initWithTitle:@"Delete"
                                                      action:@selector(delete:)
                                               keyEquivalent:@""];
  [standardEditMenu addItem:deleteItem];
  [deleteItem release];

  // Add Select All
  NSMenuItem* selectAllItem = [[NSMenuItem alloc] initWithTitle:@"Select All"
                                                         action:@selector(selectAll:)
                                                  keyEquivalent:@"a"];
  [standardEditMenu addItem:selectAllItem];
  [selectAllItem release];

  return standardEditMenuItem;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

bool nsMenuUtilsX::NodeIsHiddenOrCollapsed(nsIContent* aContent) {
  return aContent->IsElement() &&
         (aContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden, nsGkAtoms::_true,
                                             eCaseMatters) ||
          aContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::collapsed,
                                             nsGkAtoms::_true, eCaseMatters));
}

NSMenuItem* nsMenuUtilsX::NativeMenuItemWithLocation(NSMenu* aRootMenu, NSString* aLocationString,
                                                     bool aIsMenuBar) {
  NSArray<NSString*>* indexes = [aLocationString componentsSeparatedByString:@"|"];
  unsigned int pathLength = indexes.count;
  if (pathLength == 0) {
    return nil;
  }

  NSMenu* currentSubmenu = aRootMenu;
  for (unsigned int depth = 0; depth < pathLength; depth++) {
    NSInteger targetIndex = [indexes objectAtIndex:depth].integerValue;
    if (aIsMenuBar && depth == 0) {
      // We remove the application menu from consideration for the top-level menu.
      targetIndex++;
    }
    int itemCount = currentSubmenu.numberOfItems;
    if (targetIndex >= itemCount) {
      return nil;
    }
    NSMenuItem* menuItem = [currentSubmenu itemAtIndex:targetIndex];
    // if this is the last index just return the menu item
    if (depth == pathLength - 1) {
      return menuItem;
    }
    // if this is not the last index find the submenu and keep going
    if (menuItem.hasSubmenu) {
      currentSubmenu = menuItem.submenu;
    } else {
      return nil;
    }
  }

  return nil;
}

static void CheckNativeMenuConsistencyImpl(NSMenu* aMenu, std::unordered_set<void*>& aSeenObjects);

static void CheckNativeMenuItemConsistencyImpl(NSMenuItem* aMenuItem,
                                               std::unordered_set<void*>& aSeenObjects) {
  bool inserted = aSeenObjects.insert(aMenuItem).second;
  MOZ_RELEASE_ASSERT(inserted, "Duplicate NSMenuItem object in native menu structure");
  if (aMenuItem.hasSubmenu) {
    CheckNativeMenuConsistencyImpl(aMenuItem.submenu, aSeenObjects);
  }
}

static void CheckNativeMenuConsistencyImpl(NSMenu* aMenu, std::unordered_set<void*>& aSeenObjects) {
  bool inserted = aSeenObjects.insert(aMenu).second;
  MOZ_RELEASE_ASSERT(inserted, "Duplicate NSMenu object in native menu structure");
  for (NSMenuItem* item in aMenu.itemArray) {
    CheckNativeMenuItemConsistencyImpl(item, aSeenObjects);
  }
}

void nsMenuUtilsX::CheckNativeMenuConsistency(NSMenu* aMenu) {
  std::unordered_set<void*> seenObjects;
  CheckNativeMenuConsistencyImpl(aMenu, seenObjects);
}

void nsMenuUtilsX::CheckNativeMenuConsistency(NSMenuItem* aMenuItem) {
  std::unordered_set<void*> seenObjects;
  CheckNativeMenuItemConsistencyImpl(aMenuItem, seenObjects);
}

static void DumpNativeNSMenuItemImpl(NSMenuItem* aItem, uint32_t aIndent,
                                     const Maybe<int>& aIndexInParentMenu);

static void DumpNativeNSMenuImpl(NSMenu* aMenu, uint32_t aIndent) {
  printf("%*sNSMenu [%p] %-16s\n", aIndent * 2, "", aMenu,
         (aMenu.title.length == 0 ? "(no title)" : aMenu.title.UTF8String));
  int index = 0;
  for (NSMenuItem* item in aMenu.itemArray) {
    DumpNativeNSMenuItemImpl(item, aIndent + 1, Some(index));
    index++;
  }
}

static void DumpNativeNSMenuItemImpl(NSMenuItem* aItem, uint32_t aIndent,
                                     const Maybe<int>& aIndexInParentMenu) {
  printf("%*s", aIndent * 2, "");
  if (aIndexInParentMenu) {
    printf("[%d] ", *aIndexInParentMenu);
  }
  printf("NSMenuItem [%p] %-16s%s\n", aItem,
         aItem.isSeparatorItem ? "----"
                               : (aItem.title.length == 0 ? "(no title)" : aItem.title.UTF8String),
         aItem.hasSubmenu ? " [hasSubmenu]" : "");
  if (aItem.hasSubmenu) {
    DumpNativeNSMenuImpl(aItem.submenu, aIndent + 1);
  }
}

void nsMenuUtilsX::DumpNativeMenu(NSMenu* aMenu) { DumpNativeNSMenuImpl(aMenu, 0); }

void nsMenuUtilsX::DumpNativeMenuItem(NSMenuItem* aMenuItem) {
  DumpNativeNSMenuItemImpl(aMenuItem, 0, Nothing());
}
