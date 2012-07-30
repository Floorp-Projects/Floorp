/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuItemX.h"
#include "nsMenuBarX.h"
#include "nsMenuX.h"
#include "nsMenuItemIconX.h"
#include "nsMenuUtilsX.h"

#include "nsObjCExceptions.h"

#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsGUIEvent.h"

#include "mozilla/dom/Element.h"
#include "nsIWidget.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMElement.h"

nsMenuItemX::nsMenuItemX()
{
  mType           = eRegularMenuItemType;
  mNativeMenuItem = nil;
  mMenuParent     = nullptr;
  mMenuGroupOwner = nullptr;
  mIsChecked      = false;

  MOZ_COUNT_CTOR(nsMenuItemX);
}

nsMenuItemX::~nsMenuItemX()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Prevent the icon object from outliving us.
  if (mIcon)
    mIcon->Destroy();

  // autorelease the native menu item so that anything else happening to this
  // object happens before the native menu item actually dies
  [mNativeMenuItem autorelease];

  if (mContent)
    mMenuGroupOwner->UnregisterForContentChanges(mContent);
  if (mCommandContent)
    mMenuGroupOwner->UnregisterForContentChanges(mCommandContent);

  MOZ_COUNT_DTOR(nsMenuItemX);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

struct macKeyCodeData {
  const char* str;
  size_t strlength;
  PRUint32 keycode;
};

static const macKeyCodeData gMacKeyCodes[] = {

#define KEYCODE_ENTRY(str, code) {#str, sizeof(#str) - 1, code}

  KEYCODE_ENTRY(VK_CANCEL, 0x001B),
  KEYCODE_ENTRY(VK_DELETE, NSBackspaceCharacter),
  KEYCODE_ENTRY(VK_BACK, NSBackspaceCharacter),
  KEYCODE_ENTRY(VK_BACK_SPACE, NSBackspaceCharacter),
  KEYCODE_ENTRY(VK_TAB, NSTabCharacter),
  KEYCODE_ENTRY(VK_CLEAR, NSClearLineFunctionKey),
  KEYCODE_ENTRY(VK_RETURN, NSEnterCharacter),
  KEYCODE_ENTRY(VK_ENTER, NSEnterCharacter),
  KEYCODE_ENTRY(VK_SHIFT, 0),
  KEYCODE_ENTRY(VK_CONTROL, 0),
  KEYCODE_ENTRY(VK_ALT, 0),
  KEYCODE_ENTRY(VK_PAUSE, NSPauseFunctionKey),
  KEYCODE_ENTRY(VK_CAPS_LOCK, 0),
  KEYCODE_ENTRY(VK_ESCAPE, 0),
  KEYCODE_ENTRY(VK_SPACE, ' '),
  KEYCODE_ENTRY(VK_PAGE_UP, NSPageUpFunctionKey),
  KEYCODE_ENTRY(VK_PAGE_DOWN, NSPageDownFunctionKey),
  KEYCODE_ENTRY(VK_END, NSEndFunctionKey),
  KEYCODE_ENTRY(VK_HOME, NSHomeFunctionKey),
  KEYCODE_ENTRY(VK_LEFT, NSLeftArrowFunctionKey),
  KEYCODE_ENTRY(VK_UP, NSUpArrowFunctionKey),
  KEYCODE_ENTRY(VK_RIGHT, NSRightArrowFunctionKey),
  KEYCODE_ENTRY(VK_DOWN, NSDownArrowFunctionKey),
  KEYCODE_ENTRY(VK_PRINTSCREEN, NSPrintScreenFunctionKey),
  KEYCODE_ENTRY(VK_INSERT, NSInsertFunctionKey),
  KEYCODE_ENTRY(VK_HELP, NSHelpFunctionKey),
  KEYCODE_ENTRY(VK_0, '0'),
  KEYCODE_ENTRY(VK_1, '1'),
  KEYCODE_ENTRY(VK_2, '2'),
  KEYCODE_ENTRY(VK_3, '3'),
  KEYCODE_ENTRY(VK_4, '4'),
  KEYCODE_ENTRY(VK_5, '5'),
  KEYCODE_ENTRY(VK_6, '6'),
  KEYCODE_ENTRY(VK_7, '7'),
  KEYCODE_ENTRY(VK_8, '8'),
  KEYCODE_ENTRY(VK_9, '9'),
  KEYCODE_ENTRY(VK_SEMICOLON, ':'),
  KEYCODE_ENTRY(VK_EQUALS, '='),
  KEYCODE_ENTRY(VK_A, 'A'),
  KEYCODE_ENTRY(VK_B, 'B'),
  KEYCODE_ENTRY(VK_C, 'C'),
  KEYCODE_ENTRY(VK_D, 'D'),
  KEYCODE_ENTRY(VK_E, 'E'),
  KEYCODE_ENTRY(VK_F, 'F'),
  KEYCODE_ENTRY(VK_G, 'G'),
  KEYCODE_ENTRY(VK_H, 'H'),
  KEYCODE_ENTRY(VK_I, 'I'),
  KEYCODE_ENTRY(VK_J, 'J'),
  KEYCODE_ENTRY(VK_K, 'K'),
  KEYCODE_ENTRY(VK_L, 'L'),
  KEYCODE_ENTRY(VK_M, 'M'),
  KEYCODE_ENTRY(VK_N, 'N'),
  KEYCODE_ENTRY(VK_O, 'O'),
  KEYCODE_ENTRY(VK_P, 'P'),
  KEYCODE_ENTRY(VK_Q, 'Q'),
  KEYCODE_ENTRY(VK_R, 'R'),
  KEYCODE_ENTRY(VK_S, 'S'),
  KEYCODE_ENTRY(VK_T, 'T'),
  KEYCODE_ENTRY(VK_U, 'U'),
  KEYCODE_ENTRY(VK_V, 'V'),
  KEYCODE_ENTRY(VK_W, 'W'),
  KEYCODE_ENTRY(VK_X, 'X'),
  KEYCODE_ENTRY(VK_Y, 'Y'),
  KEYCODE_ENTRY(VK_Z, 'Z'),
  KEYCODE_ENTRY(VK_CONTEXT_MENU, NSMenuFunctionKey),
  KEYCODE_ENTRY(VK_NUMPAD0, '0'),
  KEYCODE_ENTRY(VK_NUMPAD1, '1'),
  KEYCODE_ENTRY(VK_NUMPAD2, '2'),
  KEYCODE_ENTRY(VK_NUMPAD3, '3'),
  KEYCODE_ENTRY(VK_NUMPAD4, '4'),
  KEYCODE_ENTRY(VK_NUMPAD5, '5'),
  KEYCODE_ENTRY(VK_NUMPAD6, '6'),
  KEYCODE_ENTRY(VK_NUMPAD7, '7'),
  KEYCODE_ENTRY(VK_NUMPAD8, '8'),
  KEYCODE_ENTRY(VK_NUMPAD9, '9'),
  KEYCODE_ENTRY(VK_MULTIPLY, '*'),
  KEYCODE_ENTRY(VK_ADD, '+'),
  KEYCODE_ENTRY(VK_SEPARATOR, 0),
  KEYCODE_ENTRY(VK_SUBTRACT, '-'),
  KEYCODE_ENTRY(VK_DECIMAL, '.'),
  KEYCODE_ENTRY(VK_DIVIDE, '/'),
  KEYCODE_ENTRY(VK_F1, NSF1FunctionKey),
  KEYCODE_ENTRY(VK_F2, NSF2FunctionKey),
  KEYCODE_ENTRY(VK_F3, NSF3FunctionKey),
  KEYCODE_ENTRY(VK_F4, NSF4FunctionKey),
  KEYCODE_ENTRY(VK_F5, NSF5FunctionKey),
  KEYCODE_ENTRY(VK_F6, NSF6FunctionKey),
  KEYCODE_ENTRY(VK_F7, NSF7FunctionKey),
  KEYCODE_ENTRY(VK_F8, NSF8FunctionKey),
  KEYCODE_ENTRY(VK_F9, NSF9FunctionKey),
  KEYCODE_ENTRY(VK_F10, NSF10FunctionKey),
  KEYCODE_ENTRY(VK_F11, NSF11FunctionKey),
  KEYCODE_ENTRY(VK_F12, NSF12FunctionKey),
  KEYCODE_ENTRY(VK_F13, NSF13FunctionKey),
  KEYCODE_ENTRY(VK_F14, NSF14FunctionKey),
  KEYCODE_ENTRY(VK_F15, NSF15FunctionKey),
  KEYCODE_ENTRY(VK_F16, NSF16FunctionKey),
  KEYCODE_ENTRY(VK_F17, NSF17FunctionKey),
  KEYCODE_ENTRY(VK_F18, NSF18FunctionKey),
  KEYCODE_ENTRY(VK_F19, NSF19FunctionKey),
  KEYCODE_ENTRY(VK_F20, NSF20FunctionKey),
  KEYCODE_ENTRY(VK_F21, NSF21FunctionKey),
  KEYCODE_ENTRY(VK_F22, NSF22FunctionKey),
  KEYCODE_ENTRY(VK_F23, NSF23FunctionKey),
  KEYCODE_ENTRY(VK_F24, NSF24FunctionKey),
  KEYCODE_ENTRY(VK_NUM_LOCK, NSClearLineFunctionKey),
  KEYCODE_ENTRY(VK_SCROLL_LOCK, NSScrollLockFunctionKey),
  KEYCODE_ENTRY(VK_COMMA, ','),
  KEYCODE_ENTRY(VK_PERIOD, '.'),
  KEYCODE_ENTRY(VK_SLASH, '/'),
  KEYCODE_ENTRY(VK_BACK_QUOTE, '`'),
  KEYCODE_ENTRY(VK_OPEN_BRACKET, '['),
  KEYCODE_ENTRY(VK_BACK_SLASH, '\\'),
  KEYCODE_ENTRY(VK_CLOSE_BRACKET, ']'),
  KEYCODE_ENTRY(VK_QUOTE, '\'')

#undef KEYCODE_ENTRY

};

PRUint32 nsMenuItemX::ConvertGeckoToMacKeyCode(nsAString& aKeyCodeName)
{
  if (aKeyCodeName.IsEmpty()) {
    return 0;
  }

  nsCAutoString keyCodeName;
  keyCodeName.AssignWithConversion(aKeyCodeName);
  // We want case-insensitive comparison with data stored as uppercase.
  ToUpperCase(keyCodeName);

  PRUint32 keyCodeNameLength = keyCodeName.Length();
  const char* keyCodeNameStr = keyCodeName.get();
  for (PRUint16 i = 0; i < (sizeof(gMacKeyCodes) / sizeof(gMacKeyCodes[0])); ++i) {
    if (keyCodeNameLength == gMacKeyCodes[i].strlength &&
        nsCRT::strcmp(gMacKeyCodes[i].str, keyCodeNameStr) == 0) {
      return gMacKeyCodes[i].keycode;
    }
  }

  return 0;
}

nsresult nsMenuItemX::Create(nsMenuX* aParent, const nsString& aLabel, EMenuItemType aItemType,
                             nsMenuGroupOwnerX* aMenuGroupOwner, nsIContent* aNode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mType = aItemType;
  mMenuParent = aParent;
  mContent = aNode;

  mMenuGroupOwner = aMenuGroupOwner;
  NS_ASSERTION(mMenuGroupOwner, "No menu owner given, must have one!");

  mMenuGroupOwner->RegisterForContentChanges(mContent, this);

  nsIDocument *doc = mContent->GetCurrentDoc();

  // if we have a command associated with this menu item, register for changes
  // to the command DOM node
  if (doc) {
    nsAutoString ourCommand;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::command, ourCommand);

    if (!ourCommand.IsEmpty()) {
      nsIContent *commandElement = doc->GetElementById(ourCommand);

      if (commandElement) {
        mCommandContent = commandElement;
        // register to observe the command DOM element
        mMenuGroupOwner->RegisterForContentChanges(mCommandContent, this);
      }
    }
  }

  // decide enabled state based on command content if it exists, otherwise do it based
  // on our own content
  bool isEnabled;
  if (mCommandContent)
    isEnabled = !mCommandContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled, nsGkAtoms::_true, eCaseMatters);
  else
    isEnabled = !mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled, nsGkAtoms::_true, eCaseMatters);

  // set up the native menu item
  if (mType == eSeparatorMenuItemType) {
    mNativeMenuItem = [[NSMenuItem separatorItem] retain];
  }
  else {
    NSString *newCocoaLabelString = nsMenuUtilsX::GetTruncatedCocoaLabel(aLabel);
    mNativeMenuItem = [[NSMenuItem alloc] initWithTitle:newCocoaLabelString action:nil keyEquivalent:@""];

    [mNativeMenuItem setEnabled:(BOOL)isEnabled];

    SetChecked(mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::checked,
                                     nsGkAtoms::_true, eCaseMatters));
    SetKeyEquiv();
  }

  mIcon = new nsMenuItemIconX(this, mContent, mNativeMenuItem);
  if (!mIcon)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsMenuItemX::SetChecked(bool aIsChecked)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mIsChecked = aIsChecked;
  
  // update the content model. This will also handle unchecking our siblings
  // if we are a radiomenu
  mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::checked, 
                    mIsChecked ? NS_LITERAL_STRING("true") : NS_LITERAL_STRING("false"), true);
  
  // update native menu item
  if (mIsChecked)
    [mNativeMenuItem setState:NSOnState];
  else
    [mNativeMenuItem setState:NSOffState];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

EMenuItemType nsMenuItemX::GetMenuItemType()
{
  return mType;
}

// Executes the "cached" javaScript command.
// Returns NS_OK if the command was executed properly, otherwise an error code.
void nsMenuItemX::DoCommand()
{
  // flip "checked" state if we're a checkbox menu, or an un-checked radio menu
  if (mType == eCheckboxMenuItemType ||
      (mType == eRadioMenuItemType && !mIsChecked)) {
    if (!mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::autocheck,
                               nsGkAtoms::_false, eCaseMatters))
    SetChecked(!mIsChecked);
    /* the AttributeChanged code will update all the internal state */
  }

  nsMenuUtilsX::DispatchCommandTo(mContent);
}

nsresult nsMenuItemX::DispatchDOMEvent(const nsString &eventName, bool *preventDefaultCalled)
{
  if (!mContent)
    return NS_ERROR_FAILURE;

  // get owner document for content
  nsCOMPtr<nsIDocument> parentDoc = mContent->OwnerDoc();

  // get interface for creating DOM events from content owner document
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(parentDoc);
  if (!domDoc) {
    NS_WARNING("Failed to QI parent nsIDocument to nsIDOMDocument");
    return NS_ERROR_FAILURE;
  }

  // create DOM event
  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = domDoc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create nsIDOMEvent");
    return rv;
  }
  event->InitEvent(eventName, true, true);

  // mark DOM event as trusted
  event->SetTrusted(true);

  // send DOM event
  nsCOMPtr<nsIDOMEventTarget> eventTarget = do_QueryInterface(mContent);
  rv = eventTarget->DispatchEvent(event, preventDefaultCalled);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to send DOM event via nsIDOMEventTarget");
    return rv;
  }

  return NS_OK;  
}

// Walk the sibling list looking for nodes with the same name and
// uncheck them all.
void nsMenuItemX::UncheckRadioSiblings(nsIContent* inCheckedContent)
{
  nsAutoString myGroupName;
  inCheckedContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, myGroupName);
  if (!myGroupName.Length()) // no groupname, nothing to do
    return;
  
  nsCOMPtr<nsIContent> parent = inCheckedContent->GetParent();
  if (!parent)
    return;

  // loop over siblings
  PRUint32 count = parent->GetChildCount();
  for (PRUint32 i = 0; i < count; i++) {
    nsIContent *sibling = parent->GetChildAt(i);
    if (sibling) {      
      if (sibling != inCheckedContent) { // skip this node
        // if the current sibling is in the same group, clear it
        if (sibling->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                 myGroupName, eCaseMatters))
          sibling->SetAttr(kNameSpaceID_None, nsGkAtoms::checked, NS_LITERAL_STRING("false"), true);
      }
    }    
  }
}

void nsMenuItemX::SetKeyEquiv()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Set key shortcut and modifiers
  nsAutoString keyValue;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::key, keyValue);
  if (!keyValue.IsEmpty() && mContent->GetCurrentDoc()) {
    nsIContent *keyContent = mContent->GetCurrentDoc()->GetElementById(keyValue);
    if (keyContent) {
      nsAutoString keyChar;
      bool hasKey = keyContent->GetAttr(kNameSpaceID_None, nsGkAtoms::key, keyChar);

      if (!hasKey || keyChar.IsEmpty()) {
        nsAutoString keyCodeName;
        keyContent->GetAttr(kNameSpaceID_None, nsGkAtoms::keycode, keyCodeName);
        PRUint32 keycode = ConvertGeckoToMacKeyCode(keyCodeName);
        if (keycode) {
          keyChar.Assign(keycode);
        }
        else {
          keyChar.Assign(NS_LITERAL_STRING(" "));
        }
      }

      nsAutoString modifiersStr;
      keyContent->GetAttr(kNameSpaceID_None, nsGkAtoms::modifiers, modifiersStr);
      PRUint8 modifiers = nsMenuUtilsX::GeckoModifiersForNodeAttribute(modifiersStr);

      unsigned int macModifiers = nsMenuUtilsX::MacModifiersForGeckoModifiers(modifiers);
      [mNativeMenuItem setKeyEquivalentModifierMask:macModifiers];

      NSString *keyEquivalent = [[NSString stringWithCharacters:(unichar*)keyChar.get()
                                                         length:keyChar.Length()] lowercaseString];
      if ([keyEquivalent isEqualToString:@" "])
        [mNativeMenuItem setKeyEquivalent:@""];
      else
        [mNativeMenuItem setKeyEquivalent:keyEquivalent];

      return;
    }
  }

  // if the key was removed, clear the key
  [mNativeMenuItem setKeyEquivalent:@""];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

//
// nsChangeObserver
//

void
nsMenuItemX::ObserveAttributeChanged(nsIDocument *aDocument, nsIContent *aContent, nsIAtom *aAttribute)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!aContent)
    return;
  
  if (aContent == mContent) { // our own content node changed
    if (aAttribute == nsGkAtoms::checked) {
      // if we're a radio menu, uncheck our sibling radio items. No need to
      // do any of this if we're just a normal check menu.
      if (mType == eRadioMenuItemType) {
        if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::checked,
                                  nsGkAtoms::_true, eCaseMatters))
          UncheckRadioSiblings(mContent);
      }
      mMenuParent->SetRebuild(true);
    }
    else if (aAttribute == nsGkAtoms::hidden ||
             aAttribute == nsGkAtoms::collapsed ||
             aAttribute == nsGkAtoms::label) {
      mMenuParent->SetRebuild(true);
    }
    else if (aAttribute == nsGkAtoms::key) {
      SetKeyEquiv();
    }
    else if (aAttribute == nsGkAtoms::image) {
      SetupIcon();
    }
    else if (aAttribute == nsGkAtoms::disabled) {
      if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled, nsGkAtoms::_true, eCaseMatters))
        [mNativeMenuItem setEnabled:NO];
      else
        [mNativeMenuItem setEnabled:YES];
    }
  }
  else if (aContent == mCommandContent) {
    // the only thing that really matters when the menu isn't showing is the
    // enabled state since it enables/disables keyboard commands
    if (aAttribute == nsGkAtoms::disabled) {
      // first we sync our menu item DOM node with the command DOM node
      nsAutoString commandDisabled;
      nsAutoString menuDisabled;
      aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::disabled, commandDisabled);
      mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::disabled, menuDisabled);
      if (!commandDisabled.Equals(menuDisabled)) {
        // The menu's disabled state needs to be updated to match the command.
        if (commandDisabled.IsEmpty()) 
          mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
        else
          mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, commandDisabled, true);
      }
      // now we sync our native menu item with the command DOM node
      if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled, nsGkAtoms::_true, eCaseMatters))
        [mNativeMenuItem setEnabled:NO];
      else
        [mNativeMenuItem setEnabled:YES];
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuItemX::ObserveContentRemoved(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer)
{
  if (aChild == mCommandContent) {
    mMenuGroupOwner->UnregisterForContentChanges(mCommandContent);
    mCommandContent = nullptr;
  }

  mMenuParent->SetRebuild(true);
}

void nsMenuItemX::ObserveContentInserted(nsIDocument *aDocument, nsIContent* aContainer,
                                         nsIContent *aChild)
{
  mMenuParent->SetRebuild(true);
}

void nsMenuItemX::SetupIcon()
{
  if (mIcon)
    mIcon->SetupIcon();
}
