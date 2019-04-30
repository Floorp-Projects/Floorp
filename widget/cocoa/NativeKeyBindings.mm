/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeKeyBindings.h"

#include "nsTArray.h"
#include "nsCocoaUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/TextEvents.h"

namespace mozilla {
namespace widget {

static LazyLogModule gNativeKeyBindingsLog("NativeKeyBindings");

NativeKeyBindings* NativeKeyBindings::sInstanceForSingleLineEditor = nullptr;
NativeKeyBindings* NativeKeyBindings::sInstanceForMultiLineEditor = nullptr;

// static
NativeKeyBindings* NativeKeyBindings::GetInstance(NativeKeyBindingsType aType) {
  switch (aType) {
    case nsIWidget::NativeKeyBindingsForSingleLineEditor:
      if (!sInstanceForSingleLineEditor) {
        sInstanceForSingleLineEditor = new NativeKeyBindings();
        sInstanceForSingleLineEditor->Init(aType);
      }
      return sInstanceForSingleLineEditor;
    case nsIWidget::NativeKeyBindingsForMultiLineEditor:
    case nsIWidget::NativeKeyBindingsForRichTextEditor:
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

#define SEL_TO_COMMAND(aSel, aCommand) \
  mSelectorToCommand.Put(reinterpret_cast<struct objc_selector*>(@selector(aSel)), aCommand)

void NativeKeyBindings::Init(NativeKeyBindingsType aType) {
  MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info, ("%p NativeKeyBindings::Init", this));

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
  SEL_TO_COMMAND(deleteToBeginningOfParagraph:, Command::DeleteToBeginningOfLine);
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
  if (aType == nsIWidget::NativeKeyBindingsForSingleLineEditor) {
    SEL_TO_COMMAND(moveDown:, Command::EndLine);
  } else {
    SEL_TO_COMMAND(moveDown:, Command::LineNext);
  }
  SEL_TO_COMMAND(moveDownAndModifySelection:, Command::SelectLineNext);
  SEL_TO_COMMAND(moveForward:, Command::CharNext);
  SEL_TO_COMMAND(moveForwardAndModifySelection:, Command::SelectCharNext);
  SEL_TO_COMMAND(moveLeft:, Command::CharPrevious);
  SEL_TO_COMMAND(moveLeftAndModifySelection:, Command::SelectCharPrevious);
  SEL_TO_COMMAND(moveParagraphBackwardAndModifySelection:, Command::SelectBeginLine);
  SEL_TO_COMMAND(moveParagraphForwardAndModifySelection:, Command::SelectEndLine);
  SEL_TO_COMMAND(moveRight:, Command::CharNext);
  SEL_TO_COMMAND(moveRightAndModifySelection:, Command::SelectCharNext);
  SEL_TO_COMMAND(moveToBeginningOfDocument:, Command::MoveTop);
  SEL_TO_COMMAND(moveToBeginningOfDocumentAndModifySelection:, Command::SelectTop);
  SEL_TO_COMMAND(moveToBeginningOfLine:, Command::BeginLine);
  SEL_TO_COMMAND(moveToBeginningOfLineAndModifySelection:, Command::SelectBeginLine);
  SEL_TO_COMMAND(moveToBeginningOfParagraph:, Command::BeginLine);
  SEL_TO_COMMAND(moveToBeginningOfParagraphAndModifySelection:, Command::SelectBeginLine);
  SEL_TO_COMMAND(moveToEndOfDocument:, Command::MoveBottom);
  SEL_TO_COMMAND(moveToEndOfDocumentAndModifySelection:, Command::SelectBottom);
  SEL_TO_COMMAND(moveToEndOfLine:, Command::EndLine);
  SEL_TO_COMMAND(moveToEndOfLineAndModifySelection:, Command::SelectEndLine);
  SEL_TO_COMMAND(moveToEndOfParagraph:, Command::EndLine);
  SEL_TO_COMMAND(moveToEndOfParagraphAndModifySelection:, Command::SelectEndLine);
  SEL_TO_COMMAND(moveToLeftEndOfLine:, Command::BeginLine);
  SEL_TO_COMMAND(moveToLeftEndOfLineAndModifySelection:, Command::SelectBeginLine);
  SEL_TO_COMMAND(moveToRightEndOfLine:, Command::EndLine);
  SEL_TO_COMMAND(moveToRightEndOfLineAndModifySelection:, Command::SelectEndLine);
  if (aType == nsIWidget::NativeKeyBindingsForSingleLineEditor) {
    SEL_TO_COMMAND(moveUp:, Command::BeginLine);
  } else {
    SEL_TO_COMMAND(moveUp:, Command::LinePrevious);
  }
  SEL_TO_COMMAND(moveUpAndModifySelection:, Command::SelectLinePrevious);
  SEL_TO_COMMAND(moveWordBackward:, Command::WordPrevious);
  SEL_TO_COMMAND(moveWordBackwardAndModifySelection:, Command::SelectWordPrevious);
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
  if (aType == nsIWidget::NativeKeyBindingsForSingleLineEditor) {
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
                                        nsTArray<CommandInt>& aCommands) {
  MOZ_ASSERT(aCommands.IsEmpty());

  MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info, ("%p NativeKeyBindings::GetEditCommands", this));

  // Recover the current event, which should always be the key down we are
  // responding to.

  NSEvent* cocoaEvent = reinterpret_cast<NSEvent*>(aEvent.mNativeKeyEvent);

  if (!cocoaEvent || [cocoaEvent type] != NSKeyDown) {
    MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info,
            ("%p NativeKeyBindings::GetEditCommands, no Cocoa key down event", this));

    return;
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

    // Try to find a simple mapping in the hashtable
    Command geckoCommand = Command::DoNothing;
    if (mSelectorToCommand.Get(reinterpret_cast<struct objc_selector*>(selector), &geckoCommand) &&
        geckoCommand != Command::DoNothing) {
      aCommands.AppendElement(static_cast<CommandInt>(geckoCommand));
    } else if (selector == @selector(selectLine:)) {
      // This is functional, but Cocoa's version is direction-less in that
      // selection direction is not determined until some future directed action
      // is taken. See bug 282097, comment 79 for more details.
      aCommands.AppendElement(static_cast<CommandInt>(Command::BeginLine));
      aCommands.AppendElement(static_cast<CommandInt>(Command::SelectEndLine));
    } else if (selector == @selector(selectWord:)) {
      // This is functional, but Cocoa's version is direction-less in that
      // selection direction is not determined until some future directed action
      // is taken. See bug 282097, comment 79 for more details.
      aCommands.AppendElement(static_cast<CommandInt>(Command::WordPrevious));
      aCommands.AppendElement(static_cast<CommandInt>(Command::SelectWordNext));
    }
  }

  if (!MOZ_LOG_TEST(gNativeKeyBindingsLog, LogLevel::Info)) {
    return;
  }

  if (aCommands.IsEmpty()) {
    MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info,
            ("%p NativeKeyBindings::GetEditCommands, handled=false", this));
    return;
  }

  for (CommandInt commandInt : aCommands) {
    Command geckoCommand = static_cast<Command>(commandInt);
    MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info,
            ("%p NativeKeyBindings::GetEditCommands, command=%s", this,
             WidgetKeyboardEvent::GetCommandStr(geckoCommand)));
  }
}

}  // namespace widget
}  // namespace mozilla
