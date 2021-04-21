/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuItemX.h"
#include "nsMenuBarX.h"
#include "nsMenuX.h"
#include "nsMenuItemIconX.h"
#include "nsMenuUtilsX.h"
#include "nsCocoaUtils.h"

#include "nsObjCExceptions.h"

#include "nsCOMPtr.h"
#include "nsGkAtoms.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/ErrorResult.h"
#include "nsIWidget.h"
#include "mozilla/dom/Document.h"

using namespace mozilla;

using mozilla::dom::Event;
using mozilla::dom::CallerType;

nsMenuItemX::nsMenuItemX(nsMenuX* aParent, const nsString& aLabel, EMenuItemType aItemType,
                         nsMenuGroupOwnerX* aMenuGroupOwner, nsIContent* aNode)
    : mContent(aNode), mType(aItemType), mMenuParent(aParent), mMenuGroupOwner(aMenuGroupOwner) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_COUNT_CTOR(nsMenuItemX);

  MOZ_RELEASE_ASSERT(mContent->IsElement(), "nsMenuItemX should only be created for elements");
  NS_ASSERTION(mMenuGroupOwner, "No menu owner given, must have one!");

  mMenuGroupOwner->RegisterForContentChanges(mContent, this);

  dom::Document* doc = mContent->GetUncomposedDoc();

  // if we have a command associated with this menu item, register for changes
  // to the command DOM node
  if (doc) {
    nsAutoString ourCommand;
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::command, ourCommand);

    if (!ourCommand.IsEmpty()) {
      dom::Element* commandElement = doc->GetElementById(ourCommand);

      if (commandElement) {
        mCommandElement = commandElement;
        // register to observe the command DOM element
        mMenuGroupOwner->RegisterForContentChanges(mCommandElement, this);
      }
    }
  }

  // decide enabled state based on command content if it exists, otherwise do it based
  // on our own content
  bool isEnabled;
  if (mCommandElement) {
    isEnabled = !mCommandElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                              nsGkAtoms::_true, eCaseMatters);
  } else {
    isEnabled = !mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                                    nsGkAtoms::_true, eCaseMatters);
  }

  // set up the native menu item
  if (mType == eSeparatorMenuItemType) {
    mNativeMenuItem = [[NSMenuItem separatorItem] retain];
  } else {
    NSString* newCocoaLabelString = nsMenuUtilsX::GetTruncatedCocoaLabel(aLabel);
    mNativeMenuItem = [[NSMenuItem alloc] initWithTitle:newCocoaLabelString
                                                 action:nil
                                          keyEquivalent:@""];

    mIsChecked = mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::checked,
                                                    nsGkAtoms::_true, eCaseMatters);

    mNativeMenuItem.enabled = isEnabled;
    mNativeMenuItem.state = mIsChecked ? NSOnState : NSOffState;

    SetKeyEquiv();
  }

  mIcon = MakeUnique<nsMenuItemIconX>(this);

  mIsVisible = !nsMenuUtilsX::NodeIsHiddenOrCollapsed(mContent);

  // All menu items share the same target and action, and are differentiated
  // be a unique (representedObject, tag) pair.
  mNativeMenuItem.target = nsMenuBarX::sNativeEventTarget;
  mNativeMenuItem.action = @selector(menuItemHit:);
  mNativeMenuItem.representedObject = mMenuGroupOwner->GetRepresentedObject();
  mNativeMenuItem.tag = mMenuGroupOwner->RegisterForCommand(this);

  if (mIsVisible) {
    SetupIcon();
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsMenuItemX::~nsMenuItemX() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // autorelease the native menu item so that anything else happening to this
  // object happens before the native menu item actually dies
  [mNativeMenuItem autorelease];

  DetachFromGroupOwner();

  MOZ_COUNT_DTOR(nsMenuItemX);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuItemX::DetachFromGroupOwner() {
  if (mMenuGroupOwner) {
    mMenuGroupOwner->UnregisterCommand(mNativeMenuItem.tag);

    if (mContent) {
      mMenuGroupOwner->UnregisterForContentChanges(mContent);
    }
    if (mCommandElement) {
      mMenuGroupOwner->UnregisterForContentChanges(mCommandElement);
    }
  }

  mMenuGroupOwner = nullptr;
}

nsresult nsMenuItemX::SetChecked(bool aIsChecked) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mIsChecked = aIsChecked;

  // update the content model. This will also handle unchecking our siblings
  // if we are a radiomenu
  if (mIsChecked) {
    mContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::checked, u"true"_ns, true);
  } else {
    mContent->AsElement()->UnsetAttr(kNameSpaceID_None, nsGkAtoms::checked, true);
  }

  // update native menu item
  mNativeMenuItem.state = mIsChecked ? NSOnState : NSOffState;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

EMenuItemType nsMenuItemX::GetMenuItemType() { return mType; }

// Executes the "cached" javaScript command.
// Returns NS_OK if the command was executed properly, otherwise an error code.
void nsMenuItemX::DoCommand(NSEventModifierFlags aModifierFlags, int16_t aButton) {
  // flip "checked" state if we're a checkbox menu, or an un-checked radio menu
  if (mType == eCheckboxMenuItemType || (mType == eRadioMenuItemType && !mIsChecked)) {
    if (!mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::autocheck,
                                            nsGkAtoms::_false, eCaseMatters)) {
      SetChecked(!mIsChecked);
    }
    /* the AttributeChanged code will update all the internal state */
  }

  nsMenuUtilsX::DispatchCommandTo(mContent, aModifierFlags, aButton);
}

nsresult nsMenuItemX::DispatchDOMEvent(const nsString& eventName, bool* preventDefaultCalled) {
  if (!mContent) {
    return NS_ERROR_FAILURE;
  }

  // get owner document for content
  nsCOMPtr<dom::Document> parentDoc = mContent->OwnerDoc();

  // create DOM event
  ErrorResult rv;
  RefPtr<Event> event = parentDoc->CreateEvent(u"Events"_ns, CallerType::System, rv);
  if (rv.Failed()) {
    NS_WARNING("Failed to create Event");
    return rv.StealNSResult();
  }
  event->InitEvent(eventName, true, true);

  // mark DOM event as trusted
  event->SetTrusted(true);

  // send DOM event
  *preventDefaultCalled = mContent->DispatchEvent(*event, CallerType::System, rv);
  if (rv.Failed()) {
    NS_WARNING("Failed to send DOM event via EventTarget");
    return rv.StealNSResult();
  }

  return NS_OK;
}

// Walk the sibling list looking for nodes with the same name and
// uncheck them all.
void nsMenuItemX::UncheckRadioSiblings(nsIContent* aCheckedContent) {
  nsAutoString myGroupName;
  aCheckedContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::name, myGroupName);
  if (!myGroupName.Length()) {  // no groupname, nothing to do
    return;
  }

  nsCOMPtr<nsIContent> parent = aCheckedContent->GetParent();
  if (!parent) {
    return;
  }

  // loop over siblings
  for (nsIContent* sibling = parent->GetFirstChild(); sibling;
       sibling = sibling->GetNextSibling()) {
    if (sibling != aCheckedContent && sibling->IsElement()) {  // skip this node
      // if the current sibling is in the same group, clear it
      if (sibling->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, myGroupName,
                                            eCaseMatters)) {
        sibling->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::checked, u"false"_ns, true);
      }
    }
  }
}

void nsMenuItemX::SetKeyEquiv() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Set key shortcut and modifiers
  nsAutoString keyValue;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::key, keyValue);

  if (!keyValue.IsEmpty() && mContent->GetUncomposedDoc()) {
    dom::Element* keyContent = mContent->GetUncomposedDoc()->GetElementById(keyValue);
    if (keyContent) {
      nsAutoString keyChar;
      bool hasKey = keyContent->GetAttr(kNameSpaceID_None, nsGkAtoms::key, keyChar);

      if (!hasKey || keyChar.IsEmpty()) {
        nsAutoString keyCodeName;
        keyContent->GetAttr(kNameSpaceID_None, nsGkAtoms::keycode, keyCodeName);
        uint32_t charCode = nsCocoaUtils::ConvertGeckoNameToMacCharCode(keyCodeName);
        if (charCode) {
          keyChar.Assign(charCode);
        } else {
          keyChar.AssignLiteral(u" ");
        }
      }

      nsAutoString modifiersStr;
      keyContent->GetAttr(kNameSpaceID_None, nsGkAtoms::modifiers, modifiersStr);
      uint8_t modifiers = nsMenuUtilsX::GeckoModifiersForNodeAttribute(modifiersStr);

      unsigned int macModifiers = nsMenuUtilsX::MacModifiersForGeckoModifiers(modifiers);
      mNativeMenuItem.keyEquivalentModifierMask = macModifiers;

      NSString* keyEquivalent = [[NSString stringWithCharacters:(unichar*)keyChar.get()
                                                         length:keyChar.Length()] lowercaseString];
      if ([keyEquivalent isEqualToString:@" "]) {
        mNativeMenuItem.keyEquivalent = @"";
      } else {
        mNativeMenuItem.keyEquivalent = keyEquivalent;
      }

      return;
    }
  }

  // if the key was removed, clear the key
  mNativeMenuItem.keyEquivalent = @"";

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuItemX::Dump(uint32_t aIndent) const {
  printf("%*s - item [%p] %-16s <%s>\n", aIndent * 2, "", this,
         mType == eSeparatorMenuItemType ? "----" : [mNativeMenuItem.title UTF8String],
         NS_ConvertUTF16toUTF8(mContent->NodeName()).get());
}

//
// nsChangeObserver
//

void nsMenuItemX::ObserveAttributeChanged(dom::Document* aDocument, nsIContent* aContent,
                                          nsAtom* aAttribute) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!aContent) {
    return;
  }

  if (aContent == mContent) {  // our own content node changed
    if (aAttribute == nsGkAtoms::checked) {
      // if we're a radio menu, uncheck our sibling radio items. No need to
      // do any of this if we're just a normal check menu.
      if (mType == eRadioMenuItemType &&
          mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::checked,
                                             nsGkAtoms::_true, eCaseMatters)) {
        UncheckRadioSiblings(mContent);
      }
      mMenuParent->SetRebuild(true);
    } else if (aAttribute == nsGkAtoms::hidden || aAttribute == nsGkAtoms::collapsed) {
      bool isVisible = !nsMenuUtilsX::NodeIsHiddenOrCollapsed(mContent);
      if (isVisible != mIsVisible) {
        mIsVisible = isVisible;
        RefPtr<nsMenuItemX> self = this;
        mMenuParent->MenuChildChangedVisibility(nsMenuParentX::MenuChild(self), isVisible);
        if (mIsVisible) {
          SetupIcon();
        }
      }
      mMenuParent->SetRebuild(true);
    } else if (aAttribute == nsGkAtoms::label) {
      if (mType != eSeparatorMenuItemType) {
        nsAutoString newLabel;
        mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label, newLabel);
        mNativeMenuItem.title = nsMenuUtilsX::GetTruncatedCocoaLabel(newLabel);
      }
    } else if (aAttribute == nsGkAtoms::key) {
      SetKeyEquiv();
    } else if (aAttribute == nsGkAtoms::image) {
      SetupIcon();
    } else if (aAttribute == nsGkAtoms::disabled) {
      mNativeMenuItem.enabled = !aContent->AsElement()->AttrValueIs(
          kNameSpaceID_None, nsGkAtoms::disabled, nsGkAtoms::_true, eCaseMatters);
    }
  } else if (aContent == mCommandElement) {
    // the only thing that really matters when the menu isn't showing is the
    // enabled state since it enables/disables keyboard commands
    if (aAttribute == nsGkAtoms::disabled) {
      // first we sync our menu item DOM node with the command DOM node
      nsAutoString commandDisabled;
      nsAutoString menuDisabled;
      aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::disabled, commandDisabled);
      mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::disabled, menuDisabled);
      if (!commandDisabled.Equals(menuDisabled)) {
        // The menu's disabled state needs to be updated to match the command.
        if (commandDisabled.IsEmpty()) {
          mContent->AsElement()->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
        } else {
          mContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, commandDisabled,
                                         true);
        }
      }
      // now we sync our native menu item with the command DOM node
      mNativeMenuItem.enabled = !aContent->AsElement()->AttrValueIs(
          kNameSpaceID_None, nsGkAtoms::disabled, nsGkAtoms::_true, eCaseMatters);
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

bool IsMenuStructureElement(nsIContent* aContent) {
  return aContent->IsAnyOfXULElements(nsGkAtoms::menu, nsGkAtoms::menuitem,
                                      nsGkAtoms::menuseparator);
}

void nsMenuItemX::ObserveContentRemoved(dom::Document* aDocument, nsIContent* aContainer,
                                        nsIContent* aChild, nsIContent* aPreviousSibling) {
  MOZ_RELEASE_ASSERT(mMenuGroupOwner);
  MOZ_RELEASE_ASSERT(mMenuParent);

  if (aChild == mCommandElement) {
    mMenuGroupOwner->UnregisterForContentChanges(mCommandElement);
    mCommandElement = nullptr;
  }
  if (IsMenuStructureElement(aChild)) {
    mMenuParent->SetRebuild(true);
  }
}

void nsMenuItemX::ObserveContentInserted(dom::Document* aDocument, nsIContent* aContainer,
                                         nsIContent* aChild) {
  MOZ_RELEASE_ASSERT(mMenuParent);

  // The child node could come from the custom element that is for display, so
  // only rebuild the menu if the child is related to the structure of the
  // menu.
  if (IsMenuStructureElement(aChild)) {
    mMenuParent->SetRebuild(true);
  }
}

void nsMenuItemX::SetupIcon() {
  if (mType != eRegularMenuItemType) {
    // Don't support icons on checkbox and radio menuitems, for consistency with Windows & Linux.
    return;
  }

  mIcon->SetupIcon(mContent);
  mNativeMenuItem.image = mIcon->GetIconImage();
}

void nsMenuItemX::IconUpdated() { mNativeMenuItem.image = mIcon->GetIconImage(); }
