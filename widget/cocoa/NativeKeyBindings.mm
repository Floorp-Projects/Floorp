/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeKeyBindings.h"

#include "nsTArray.h"
#include "nsCocoaUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/NativeKeyBindingsType.h"
#include "mozilla/TextEvents.h"
#include "mozilla/WritingModes.h"

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

namespace mozilla {
namespace widget {

static LazyLogModule gNativeKeyBindingsLog("NativeKeyBindings");

NativeKeyBindings* NativeKeyBindings::sInstanceForSingleLineEditor = nullptr;
NativeKeyBindings* NativeKeyBindings::sInstanceForMultiLineEditor = nullptr;

// static
NativeKeyBindings* NativeKeyBindings::GetInstance(NativeKeyBindingsType aType) {
  switch (aType) {
    case NativeKeyBindingsType::SingleLineEditor:
      if (!sInstanceForSingleLineEditor) {
        sInstanceForSingleLineEditor = new NativeKeyBindings();
        sInstanceForSingleLineEditor->Init(aType);
      }
      return sInstanceForSingleLineEditor;
    case NativeKeyBindingsType::MultiLineEditor:
    case NativeKeyBindingsType::RichTextEditor:
      if (!sInstanceForMultiLineEditor) {
        sInstanceForMultiLineEditor = new NativeKeyBindings();
        sInstanceForMultiLineEditor->Init(aType);
      }
      return sInstanceForMultiLineEditor;
    default:
      MOZ_CRASH("Not implemented");
      return nullptr;
  }
}

// static
void NativeKeyBindings::Shutdown() {
  delete sInstanceForSingleLineEditor;
  sInstanceForSingleLineEditor = nullptr;
  delete sInstanceForMultiLineEditor;
  sInstanceForMultiLineEditor = nullptr;
}

NativeKeyBindings::NativeKeyBindings() {}

inline objc_selector* ToObjcSelectorPtr(SEL aSel) {
  return reinterpret_cast<objc_selector*>(aSel);
}
#define SEL_TO_COMMAND(aSel, aCommand)                                  \
  mSelectorToCommand.InsertOrUpdate(ToObjcSelectorPtr(@selector(aSel)), \
                                    aCommand)

void NativeKeyBindings::Init(NativeKeyBindingsType aType) {
  MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info,
          ("%p NativeKeyBindings::Init", this));

  // Many selectors have a one-to-one mapping to a Gecko command. Those mappings
  // are registered in mSelectorToCommand.

  // Selectors from NSResponder's "Responding to Action Messages" section and
  // from NSText's "Action Methods for Editing" section

  // TODO: Improves correctness of left / right meaning
  // TODO: Add real paragraph motions

  // SEL_TO_COMMAND(cancelOperation:, );
  // SEL_TO_COMMAND(capitalizeWord:, );
  // SEL_TO_COMMAND(centerSelectionInVisibleArea:, );
  // SEL_TO_COMMAND(changeCaseOfLetter:, );
  // SEL_TO_COMMAND(complete:, );
  SEL_TO_COMMAND(copy:, Command::Copy);
  // SEL_TO_COMMAND(copyFont:, );
  // SEL_TO_COMMAND(copyRuler:, );
  SEL_TO_COMMAND(cut:, Command::Cut);
  SEL_TO_COMMAND(delete:, Command::Delete);
  SEL_TO_COMMAND(deleteBackward:, Command::DeleteCharBackward);
  // SEL_TO_COMMAND(deleteBackwardByDecomposingPreviousCharacter:, );
  SEL_TO_COMMAND(deleteForward:, Command::DeleteCharForward);

  // TODO: deleteTo* selectors are also supposed to add text to a kill buffer
  SEL_TO_COMMAND(deleteToBeginningOfLine:, Command::DeleteToBeginningOfLine);
  SEL_TO_COMMAND(deleteToBeginningOfParagraph:,
                 Command::DeleteToBeginningOfLine);
  SEL_TO_COMMAND(deleteToEndOfLine:, Command::DeleteToEndOfLine);
  SEL_TO_COMMAND(deleteToEndOfParagraph:, Command::DeleteToEndOfLine);
  // SEL_TO_COMMAND(deleteToMark:, );

  SEL_TO_COMMAND(deleteWordBackward:, Command::DeleteWordBackward);
  SEL_TO_COMMAND(deleteWordForward:, Command::DeleteWordForward);
  // SEL_TO_COMMAND(indent:, );
  // SEL_TO_COMMAND(insertBacktab:, );
  // SEL_TO_COMMAND(insertContainerBreak:, );
  // SEL_TO_COMMAND(insertLineBreak:, );
  // SEL_TO_COMMAND(insertNewline:, );
  // SEL_TO_COMMAND(insertNewlineIgnoringFieldEditor:, );
  // SEL_TO_COMMAND(insertParagraphSeparator:, );
  // SEL_TO_COMMAND(insertTab:, );
  // SEL_TO_COMMAND(insertTabIgnoringFieldEditor:, );
  // SEL_TO_COMMAND(insertDoubleQuoteIgnoringSubstitution:, );
  // SEL_TO_COMMAND(insertSingleQuoteIgnoringSubstitution:, );
  // SEL_TO_COMMAND(lowercaseWord:, );
  SEL_TO_COMMAND(moveBackward:, Command::CharPrevious);
  SEL_TO_COMMAND(moveBackwardAndModifySelection:, Command::SelectCharPrevious);
  if (aType == NativeKeyBindingsType::SingleLineEditor) {
    SEL_TO_COMMAND(moveDown:, Command::EndLine);
  } else {
    SEL_TO_COMMAND(moveDown:, Command::LineNext);
  }
  SEL_TO_COMMAND(moveDownAndModifySelection:, Command::SelectLineNext);
  SEL_TO_COMMAND(moveForward:, Command::CharNext);
  SEL_TO_COMMAND(moveForwardAndModifySelection:, Command::SelectCharNext);
  SEL_TO_COMMAND(moveLeft:, Command::CharPrevious);
  SEL_TO_COMMAND(moveLeftAndModifySelection:, Command::SelectCharPrevious);
  SEL_TO_COMMAND(moveParagraphBackwardAndModifySelection:,
                 Command::SelectBeginLine);
  SEL_TO_COMMAND(moveParagraphForwardAndModifySelection:,
                 Command::SelectEndLine);
  SEL_TO_COMMAND(moveRight:, Command::CharNext);
  SEL_TO_COMMAND(moveRightAndModifySelection:, Command::SelectCharNext);
  SEL_TO_COMMAND(moveToBeginningOfDocument:, Command::MoveTop);
  SEL_TO_COMMAND(moveToBeginningOfDocumentAndModifySelection:,
                 Command::SelectTop);
  SEL_TO_COMMAND(moveToBeginningOfLine:, Command::BeginLine);
  SEL_TO_COMMAND(moveToBeginningOfLineAndModifySelection:,
                 Command::SelectBeginLine);
  SEL_TO_COMMAND(moveToBeginningOfParagraph:, Command::BeginLine);
  SEL_TO_COMMAND(moveToBeginningOfParagraphAndModifySelection:,
                 Command::SelectBeginLine);
  SEL_TO_COMMAND(moveToEndOfDocument:, Command::MoveBottom);
  SEL_TO_COMMAND(moveToEndOfDocumentAndModifySelection:, Command::SelectBottom);
  SEL_TO_COMMAND(moveToEndOfLine:, Command::EndLine);
  SEL_TO_COMMAND(moveToEndOfLineAndModifySelection:, Command::SelectEndLine);
  SEL_TO_COMMAND(moveToEndOfParagraph:, Command::EndLine);
  SEL_TO_COMMAND(moveToEndOfParagraphAndModifySelection:,
                 Command::SelectEndLine);
  SEL_TO_COMMAND(moveToLeftEndOfLine:, Command::BeginLine);
  SEL_TO_COMMAND(moveToLeftEndOfLineAndModifySelection:,
                 Command::SelectBeginLine);
  SEL_TO_COMMAND(moveToRightEndOfLine:, Command::EndLine);
  SEL_TO_COMMAND(moveToRightEndOfLineAndModifySelection:,
                 Command::SelectEndLine);
  if (aType == NativeKeyBindingsType::SingleLineEditor) {
    SEL_TO_COMMAND(moveUp:, Command::BeginLine);
  } else {
    SEL_TO_COMMAND(moveUp:, Command::LinePrevious);
  }
  SEL_TO_COMMAND(moveUpAndModifySelection:, Command::SelectLinePrevious);
  SEL_TO_COMMAND(moveWordBackward:, Command::WordPrevious);
  SEL_TO_COMMAND(moveWordBackwardAndModifySelection:,
                 Command::SelectWordPrevious);
  SEL_TO_COMMAND(moveWordForward:, Command::WordNext);
  SEL_TO_COMMAND(moveWordForwardAndModifySelection:, Command::SelectWordNext);
  SEL_TO_COMMAND(moveWordLeft:, Command::WordPrevious);
  SEL_TO_COMMAND(moveWordLeftAndModifySelection:, Command::SelectWordPrevious);
  SEL_TO_COMMAND(moveWordRight:, Command::WordNext);
  SEL_TO_COMMAND(moveWordRightAndModifySelection:, Command::SelectWordNext);
  SEL_TO_COMMAND(pageDown:, Command::MovePageDown);
  SEL_TO_COMMAND(pageDownAndModifySelection:, Command::SelectPageDown);
  SEL_TO_COMMAND(pageUp:, Command::MovePageUp);
  SEL_TO_COMMAND(pageUpAndModifySelection:, Command::SelectPageUp);
  SEL_TO_COMMAND(paste:, Command::Paste);
  // SEL_TO_COMMAND(pasteFont:, );
  // SEL_TO_COMMAND(pasteRuler:, );
  SEL_TO_COMMAND(scrollLineDown:, Command::ScrollLineDown);
  SEL_TO_COMMAND(scrollLineUp:, Command::ScrollLineUp);
  SEL_TO_COMMAND(scrollPageDown:, Command::ScrollPageDown);
  SEL_TO_COMMAND(scrollPageUp:, Command::ScrollPageUp);
  SEL_TO_COMMAND(scrollToBeginningOfDocument:, Command::ScrollTop);
  SEL_TO_COMMAND(scrollToEndOfDocument:, Command::ScrollBottom);
  SEL_TO_COMMAND(selectAll:, Command::SelectAll);
  // selectLine: is complex, see KeyDown
  if (aType == NativeKeyBindingsType::SingleLineEditor) {
    SEL_TO_COMMAND(selectParagraph:, Command::SelectAll);
  }
  // SEL_TO_COMMAND(selectToMark:, );
  // selectWord: is complex, see KeyDown
  // SEL_TO_COMMAND(setMark:, );
  // SEL_TO_COMMAND(showContextHelp:, );
  // SEL_TO_COMMAND(supplementalTargetForAction:sender:, );
  // SEL_TO_COMMAND(swapWithMark:, );
  // SEL_TO_COMMAND(transpose:, );
  // SEL_TO_COMMAND(transposeWords:, );
  // SEL_TO_COMMAND(uppercaseWord:, );
  // SEL_TO_COMMAND(yank:, );
}

#undef SEL_TO_COMMAND

void NativeKeyBindings::GetEditCommands(const WidgetKeyboardEvent& aEvent,
                                        const Maybe<WritingMode>& aWritingMode,
                                        nsTArray<CommandInt>& aCommands) {
  MOZ_ASSERT(!aEvent.mFlags.mIsSynthesizedForTests);
  MOZ_ASSERT(aCommands.IsEmpty());

  MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info,
          ("%p NativeKeyBindings::GetEditCommands", this));

  // Recover the current event, which should always be the key down we are
  // responding to.

  NSEvent* cocoaEvent = reinterpret_cast<NSEvent*>(aEvent.mNativeKeyEvent);

  if (!cocoaEvent || [cocoaEvent type] != NSEventTypeKeyDown) {
    MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info,
            ("%p NativeKeyBindings::GetEditCommands, no Cocoa key down event",
             this));

    return;
  }

  if (aWritingMode.isSome() && aEvent.NeedsToRemapNavigationKey() &&
      aWritingMode.ref().IsVertical()) {
    NSEvent* originalEvent = cocoaEvent;

    // TODO: Use KeyNameIndex rather than legacy keyCode.
    uint32_t remappedGeckoKeyCode =
        aEvent.GetRemappedKeyCode(aWritingMode.ref());
    uint32_t remappedCocoaKeyCode = 0;
    switch (remappedGeckoKeyCode) {
      case NS_VK_UP:
        remappedCocoaKeyCode = kVK_UpArrow;
        break;
      case NS_VK_DOWN:
        remappedCocoaKeyCode = kVK_DownArrow;
        break;
      case NS_VK_LEFT:
        remappedCocoaKeyCode = kVK_LeftArrow;
        break;
      case NS_VK_RIGHT:
        remappedCocoaKeyCode = kVK_RightArrow;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Add a case for the new remapped key");
        return;
    }
    unichar ch =
        nsCocoaUtils::ConvertGeckoKeyCodeToMacCharCode(remappedGeckoKeyCode);
    NSString* chars = [[[NSString alloc] initWithCharacters:&ch
                                                     length:1] autorelease];
    cocoaEvent = [NSEvent keyEventWithType:[originalEvent type]
                                  location:[originalEvent locationInWindow]
                             modifierFlags:[originalEvent modifierFlags]
                                 timestamp:[originalEvent timestamp]
                              windowNumber:[originalEvent windowNumber]
                                   context:nil
                                characters:chars
               charactersIgnoringModifiers:chars
                                 isARepeat:[originalEvent isARepeat]
                                   keyCode:remappedCocoaKeyCode];
  }

  MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info,
          ("%p NativeKeyBindings::GetEditCommands, interpreting", this));

  AutoTArray<KeyBindingsCommand, 2> bindingCommands;
  nsCocoaUtils::GetCommandsFromKeyEvent(cocoaEvent, bindingCommands);

  MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info,
          ("%p NativeKeyBindings::GetEditCommands, bindingCommands=%zu", this,
           bindingCommands.Length()));

  for (uint32_t i = 0; i < bindingCommands.Length(); i++) {
    SEL selector = bindingCommands[i].selector;

    if (MOZ_LOG_TEST(gNativeKeyBindingsLog, LogLevel::Info)) {
      NSString* selectorString = NSStringFromSelector(selector);
      nsAutoString nsSelectorString;
      nsCocoaUtils::GetStringForNSString(selectorString, nsSelectorString);

      MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info,
              ("%p NativeKeyBindings::GetEditCommands, selector=%s", this,
               NS_LossyConvertUTF16toASCII(nsSelectorString).get()));
    }

    AppendEditCommandsForSelector(ToObjcSelectorPtr(selector), aCommands);
  }

  LogEditCommands(aCommands, "NativeKeyBindings::GetEditCommands");
}

void NativeKeyBindings::AppendEditCommandsForSelector(
    objc_selector* aSelector, nsTArray<CommandInt>& aCommands) const {
  // Try to find a simple mapping in the hashtable
  Command geckoCommand = Command::DoNothing;
  if (mSelectorToCommand.Get(aSelector, &geckoCommand) &&
      geckoCommand != Command::DoNothing) {
    aCommands.AppendElement(static_cast<CommandInt>(geckoCommand));
  } else if (aSelector == ToObjcSelectorPtr(@selector(selectLine:))) {
    // This is functional, but Cocoa's version is direction-less in that
    // selection direction is not determined until some future directed action
    // is taken. See bug 282097, comment 79 for more details.
    aCommands.AppendElement(static_cast<CommandInt>(Command::BeginLine));
    aCommands.AppendElement(static_cast<CommandInt>(Command::SelectEndLine));
  } else if (aSelector == ToObjcSelectorPtr(@selector(selectWord:))) {
    // This is functional, but Cocoa's version is direction-less in that
    // selection direction is not determined until some future directed action
    // is taken. See bug 282097, comment 79 for more details.
    aCommands.AppendElement(static_cast<CommandInt>(Command::WordPrevious));
    aCommands.AppendElement(static_cast<CommandInt>(Command::SelectWordNext));
  }
}

void NativeKeyBindings::LogEditCommands(const nsTArray<CommandInt>& aCommands,
                                        const char* aDescription) const {
  if (!MOZ_LOG_TEST(gNativeKeyBindingsLog, LogLevel::Info)) {
    return;
  }

  if (aCommands.IsEmpty()) {
    MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info,
            ("%p %s, no edit commands", this, aDescription));
    return;
  }

  for (CommandInt commandInt : aCommands) {
    Command geckoCommand = static_cast<Command>(commandInt);
    MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info,
            ("%p %s, command=%s", this, aDescription,
             WidgetKeyboardEvent::GetCommandStr(geckoCommand)));
  }
}

// static
void NativeKeyBindings::GetEditCommandsForTests(
    NativeKeyBindingsType aType, const WidgetKeyboardEvent& aEvent,
    const Maybe<WritingMode>& aWritingMode, nsTArray<CommandInt>& aCommands) {
  MOZ_DIAGNOSTIC_ASSERT(aEvent.IsTrusted());

  // The following mapping is checked on Big Sur. Some of them are defined in:
  // https://support.apple.com/en-us/HT201236#text
  NativeKeyBindings* instance = NativeKeyBindings::GetInstance(aType);
  if (NS_WARN_IF(!instance)) {
    return;
  }
  switch (aWritingMode.isSome()
              ? aEvent.GetRemappedKeyNameIndex(aWritingMode.ref())
              : aEvent.mKeyNameIndex) {
    case KEY_NAME_INDEX_USE_STRING:
      if (!aEvent.IsControl() || aEvent.IsAlt() || aEvent.IsMeta()) {
        break;
      }
      switch (aEvent.PseudoCharCode()) {
        case 'a':
        case 'A':
          instance->AppendEditCommandsForSelector(
              !aEvent.IsShift()
                  ? ToObjcSelectorPtr(@selector(moveToBeginningOfParagraph:))
                  : ToObjcSelectorPtr(@selector(
                        moveToBeginningOfParagraphAndModifySelection:)),
              aCommands);
          break;
        case 'b':
        case 'B':
          instance->AppendEditCommandsForSelector(
              !aEvent.IsShift() ? ToObjcSelectorPtr(@selector(moveBackward:))
                                : ToObjcSelectorPtr(@selector(
                                      moveBackwardAndModifySelection:)),
              aCommands);
          break;
        case 'd':
        case 'D':
          if (!aEvent.IsShift()) {
            instance->AppendEditCommandsForSelector(
                ToObjcSelectorPtr(@selector(deleteForward:)), aCommands);
          }
          break;
        case 'e':
        case 'E':
          instance->AppendEditCommandsForSelector(
              !aEvent.IsShift()
                  ? ToObjcSelectorPtr(@selector(moveToEndOfParagraph:))
                  : ToObjcSelectorPtr(
                        @selector(moveToEndOfParagraphAndModifySelection:)),
              aCommands);
          break;
        case 'f':
        case 'F':
          instance->AppendEditCommandsForSelector(
              !aEvent.IsShift() ? ToObjcSelectorPtr(@selector(moveForward:))
                                : ToObjcSelectorPtr(@selector(
                                      moveForwardAndModifySelection:)),
              aCommands);
          break;
        case 'h':
        case 'H':
          if (!aEvent.IsShift()) {
            instance->AppendEditCommandsForSelector(
                ToObjcSelectorPtr(@selector(deleteBackward:)), aCommands);
          }
          break;
        case 'k':
        case 'K':
          if (!aEvent.IsShift()) {
            instance->AppendEditCommandsForSelector(
                ToObjcSelectorPtr(@selector(deleteToEndOfParagraph:)),
                aCommands);
          }
          break;
        case 'n':
        case 'N':
          instance->AppendEditCommandsForSelector(
              !aEvent.IsShift()
                  ? ToObjcSelectorPtr(@selector(moveDown:))
                  : ToObjcSelectorPtr(@selector(moveDownAndModifySelection:)),
              aCommands);
          break;
        case 'p':
        case 'P':
          instance->AppendEditCommandsForSelector(
              !aEvent.IsShift()
                  ? ToObjcSelectorPtr(@selector(moveUp:))
                  : ToObjcSelectorPtr(@selector(moveUpAndModifySelection:)),
              aCommands);
          break;
        default:
          break;
      }
      break;
    case KEY_NAME_INDEX_Backspace:
      if (aEvent.IsMeta()) {
        if (aEvent.IsAlt() || aEvent.IsControl()) {
          break;
        }
        // Shift is ignored.
        instance->AppendEditCommandsForSelector(
            ToObjcSelectorPtr(@selector(deleteToBeginningOfLine:)), aCommands);
        break;
      }
      if (aEvent.IsAlt()) {
        // Shift and Control are ignored.
        instance->AppendEditCommandsForSelector(
            ToObjcSelectorPtr(@selector(deleteWordBackward:)), aCommands);
        break;
      }
      if (aEvent.IsControl()) {
        if (aEvent.IsShift()) {
          instance->AppendEditCommandsForSelector(
              ToObjcSelectorPtr(
                  @selector(deleteBackwardByDecomposingPreviousCharacter:)),
              aCommands);
        }
        break;
      }
      // Shift is ignored.
      instance->AppendEditCommandsForSelector(
          ToObjcSelectorPtr(@selector(deleteBackward:)), aCommands);
      break;
    case KEY_NAME_INDEX_Delete:
      if (aEvent.IsControl() || aEvent.IsMeta()) {
        break;
      }
      if (aEvent.IsAlt()) {
        // Shift is ignored.
        instance->AppendEditCommandsForSelector(
            ToObjcSelectorPtr(@selector(deleteWordForward:)), aCommands);
        break;
      }
      // Shift is ignored.
      instance->AppendEditCommandsForSelector(
          ToObjcSelectorPtr(@selector(deleteForward:)), aCommands);
      break;
    case KEY_NAME_INDEX_PageDown:
      if (aEvent.IsControl() || aEvent.IsMeta()) {
        break;
      }
      if (aEvent.IsAlt()) {
        // Shift is ignored.
        instance->AppendEditCommandsForSelector(
            ToObjcSelectorPtr(@selector(pageDown:)), aCommands);
        break;
      }
      instance->AppendEditCommandsForSelector(
          !aEvent.IsShift()
              ? ToObjcSelectorPtr(@selector(scrollPageDown:))
              : ToObjcSelectorPtr(@selector(pageDownAndModifySelection:)),
          aCommands);
      break;
    case KEY_NAME_INDEX_PageUp:
      if (aEvent.IsControl() || aEvent.IsMeta()) {
        break;
      }
      if (aEvent.IsAlt()) {
        // Shift is ignored.
        instance->AppendEditCommandsForSelector(
            ToObjcSelectorPtr(@selector(pageUp:)), aCommands);
        break;
      }
      instance->AppendEditCommandsForSelector(
          !aEvent.IsShift()
              ? ToObjcSelectorPtr(@selector(scrollPageUp:))
              : ToObjcSelectorPtr(@selector(pageUpAndModifySelection:)),
          aCommands);
      break;
    case KEY_NAME_INDEX_Home:
      if (aEvent.IsAlt() || aEvent.IsControl() || aEvent.IsMeta()) {
        break;
      }
      instance->AppendEditCommandsForSelector(
          !aEvent.IsShift()
              ? ToObjcSelectorPtr(@selector(scrollToBeginningOfDocument:))
              : ToObjcSelectorPtr(
                    @selector(moveToBeginningOfDocumentAndModifySelection:)),
          aCommands);
      break;
    case KEY_NAME_INDEX_End:
      if (aEvent.IsAlt() || aEvent.IsControl() || aEvent.IsMeta()) {
        break;
      }
      instance->AppendEditCommandsForSelector(
          !aEvent.IsShift()
              ? ToObjcSelectorPtr(@selector(scrollToEndOfDocument:))
              : ToObjcSelectorPtr(@selector
                                  (moveToEndOfDocumentAndModifySelection:)),
          aCommands);
      break;
    case KEY_NAME_INDEX_ArrowLeft:
      if (aEvent.IsAlt()) {
        break;
      }
      if (aEvent.IsMeta() || (aEvent.IsControl() && aEvent.IsShift())) {
        instance->AppendEditCommandsForSelector(
            !aEvent.IsShift()
                ? ToObjcSelectorPtr(@selector(moveToLeftEndOfLine:))
                : ToObjcSelectorPtr(@selector
                                    (moveToLeftEndOfLineAndModifySelection:)),
            aCommands);
        break;
      }
      if (aEvent.IsControl()) {
        break;
      }
      instance->AppendEditCommandsForSelector(
          !aEvent.IsShift()
              ? ToObjcSelectorPtr(@selector(moveLeft:))
              : ToObjcSelectorPtr(@selector(moveLeftAndModifySelection:)),
          aCommands);
      break;
    case KEY_NAME_INDEX_ArrowRight:
      if (aEvent.IsAlt()) {
        break;
      }
      if (aEvent.IsMeta() || (aEvent.IsControl() && aEvent.IsShift())) {
        instance->AppendEditCommandsForSelector(
            !aEvent.IsShift()
                ? ToObjcSelectorPtr(@selector(moveToRightEndOfLine:))
                : ToObjcSelectorPtr(@selector
                                    (moveToRightEndOfLineAndModifySelection:)),
            aCommands);
        break;
      }
      if (aEvent.IsControl()) {
        break;
      }
      instance->AppendEditCommandsForSelector(
          !aEvent.IsShift()
              ? ToObjcSelectorPtr(@selector(moveRight:))
              : ToObjcSelectorPtr(@selector(moveRightAndModifySelection:)),
          aCommands);
      break;
    case KEY_NAME_INDEX_ArrowUp:
      if (aEvent.IsControl()) {
        break;
      }
      if (aEvent.IsMeta()) {
        if (aEvent.IsAlt()) {
          break;
        }
        instance->AppendEditCommandsForSelector(
            !aEvent.IsShift()
                ? ToObjcSelectorPtr(@selector(moveToBeginningOfDocument:))
                : ToObjcSelectorPtr(
                      @selector(moveToBegginingOfDocumentAndModifySelection:)),
            aCommands);
        break;
      }
      if (aEvent.IsAlt()) {
        if (!aEvent.IsShift()) {
          instance->AppendEditCommandsForSelector(
              ToObjcSelectorPtr(@selector(moveBackward:)), aCommands);
          instance->AppendEditCommandsForSelector(
              ToObjcSelectorPtr(@selector(moveToBeginningOfParagraph:)),
              aCommands);
          break;
        }
        instance->AppendEditCommandsForSelector(
            ToObjcSelectorPtr(@selector
                              (moveParagraphBackwardAndModifySelection:)),
            aCommands);
        break;
      }
      instance->AppendEditCommandsForSelector(
          !aEvent.IsShift()
              ? ToObjcSelectorPtr(@selector(moveUp:))
              : ToObjcSelectorPtr(@selector(moveUpAndModifySelection:)),
          aCommands);
      break;
    case KEY_NAME_INDEX_ArrowDown:
      if (aEvent.IsControl()) {
        break;
      }
      if (aEvent.IsMeta()) {
        if (aEvent.IsAlt()) {
          break;
        }
        instance->AppendEditCommandsForSelector(
            !aEvent.IsShift()
                ? ToObjcSelectorPtr(@selector(moveToEndOfDocument:))
                : ToObjcSelectorPtr(@selector
                                    (moveToEndOfDocumentAndModifySelection:)),
            aCommands);
        break;
      }
      if (aEvent.IsAlt()) {
        if (!aEvent.IsShift()) {
          instance->AppendEditCommandsForSelector(
              ToObjcSelectorPtr(@selector(moveForward:)), aCommands);
          instance->AppendEditCommandsForSelector(
              ToObjcSelectorPtr(@selector(moveToEndOfParagraph:)), aCommands);
          break;
        }
        instance->AppendEditCommandsForSelector(
            ToObjcSelectorPtr(@selector
                              (moveParagraphForwardAndModifySelection:)),
            aCommands);
        break;
      }
      instance->AppendEditCommandsForSelector(
          !aEvent.IsShift()
              ? ToObjcSelectorPtr(@selector(moveDown:))
              : ToObjcSelectorPtr(@selector(moveDownAndModifySelection:)),
          aCommands);
      break;
    default:
      break;
  }

  instance->LogEditCommands(aCommands,
                            "NativeKeyBindings::GetEditCommandsForTests");
}

}  // namespace widget
}  // namespace mozilla
