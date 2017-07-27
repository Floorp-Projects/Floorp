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
NativeKeyBindings*
NativeKeyBindings::GetInstance(NativeKeyBindingsType aType)
{
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
void
NativeKeyBindings::Shutdown()
{
  delete sInstanceForSingleLineEditor;
  sInstanceForSingleLineEditor = nullptr;
  delete sInstanceForMultiLineEditor;
  sInstanceForMultiLineEditor = nullptr;
}

NativeKeyBindings::NativeKeyBindings()
{
}

#define SEL_TO_COMMAND(aSel, aCommand) \
  mSelectorToCommand.Put( \
    reinterpret_cast<struct objc_selector *>(@selector(aSel)), aCommand)

void
NativeKeyBindings::Init(NativeKeyBindingsType aType)
{
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
  SEL_TO_COMMAND(copy:, CommandCopy);
  // SEL_TO_COMMAND(copyFont:, );
  // SEL_TO_COMMAND(copyRuler:, );
  SEL_TO_COMMAND(cut:, CommandCut);
  SEL_TO_COMMAND(delete:, CommandDelete);
  SEL_TO_COMMAND(deleteBackward:, CommandDeleteCharBackward);
  // SEL_TO_COMMAND(deleteBackwardByDecomposingPreviousCharacter:, );
  SEL_TO_COMMAND(deleteForward:, CommandDeleteCharForward);

  // TODO: deleteTo* selectors are also supposed to add text to a kill buffer
  SEL_TO_COMMAND(deleteToBeginningOfLine:, CommandDeleteToBeginningOfLine);
  SEL_TO_COMMAND(deleteToBeginningOfParagraph:, CommandDeleteToBeginningOfLine);
  SEL_TO_COMMAND(deleteToEndOfLine:, CommandDeleteToEndOfLine);
  SEL_TO_COMMAND(deleteToEndOfParagraph:, CommandDeleteToEndOfLine);
  // SEL_TO_COMMAND(deleteToMark:, );

  SEL_TO_COMMAND(deleteWordBackward:, CommandDeleteWordBackward);
  SEL_TO_COMMAND(deleteWordForward:, CommandDeleteWordForward);
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
  SEL_TO_COMMAND(moveBackward:, CommandCharPrevious);
  SEL_TO_COMMAND(moveBackwardAndModifySelection:, CommandSelectCharPrevious);
  if (aType == nsIWidget::NativeKeyBindingsForSingleLineEditor) {
    SEL_TO_COMMAND(moveDown:, CommandEndLine);
  } else {
    SEL_TO_COMMAND(moveDown:, CommandLineNext);
  }
  SEL_TO_COMMAND(moveDownAndModifySelection:, CommandSelectLineNext);
  SEL_TO_COMMAND(moveForward:, CommandCharNext);
  SEL_TO_COMMAND(moveForwardAndModifySelection:, CommandSelectCharNext);
  SEL_TO_COMMAND(moveLeft:, CommandCharPrevious);
  SEL_TO_COMMAND(moveLeftAndModifySelection:, CommandSelectCharPrevious);
  SEL_TO_COMMAND(moveParagraphBackwardAndModifySelection:,
    CommandSelectBeginLine);
  SEL_TO_COMMAND(moveParagraphForwardAndModifySelection:, CommandSelectEndLine);
  SEL_TO_COMMAND(moveRight:, CommandCharNext);
  SEL_TO_COMMAND(moveRightAndModifySelection:, CommandSelectCharNext);
  SEL_TO_COMMAND(moveToBeginningOfDocument:, CommandMoveTop);
  SEL_TO_COMMAND(moveToBeginningOfDocumentAndModifySelection:,
    CommandSelectTop);
  SEL_TO_COMMAND(moveToBeginningOfLine:, CommandBeginLine);
  SEL_TO_COMMAND(moveToBeginningOfLineAndModifySelection:,
    CommandSelectBeginLine);
  SEL_TO_COMMAND(moveToBeginningOfParagraph:, CommandBeginLine);
  SEL_TO_COMMAND(moveToBeginningOfParagraphAndModifySelection:,
    CommandSelectBeginLine);
  SEL_TO_COMMAND(moveToEndOfDocument:, CommandMoveBottom);
  SEL_TO_COMMAND(moveToEndOfDocumentAndModifySelection:, CommandSelectBottom);
  SEL_TO_COMMAND(moveToEndOfLine:, CommandEndLine);
  SEL_TO_COMMAND(moveToEndOfLineAndModifySelection:, CommandSelectEndLine);
  SEL_TO_COMMAND(moveToEndOfParagraph:, CommandEndLine);
  SEL_TO_COMMAND(moveToEndOfParagraphAndModifySelection:, CommandSelectEndLine);
  SEL_TO_COMMAND(moveToLeftEndOfLine:, CommandBeginLine);
  SEL_TO_COMMAND(moveToLeftEndOfLineAndModifySelection:,
    CommandSelectBeginLine);
  SEL_TO_COMMAND(moveToRightEndOfLine:, CommandEndLine);
  SEL_TO_COMMAND(moveToRightEndOfLineAndModifySelection:, CommandSelectEndLine);
  if (aType == nsIWidget::NativeKeyBindingsForSingleLineEditor) {
    SEL_TO_COMMAND(moveUp:, CommandBeginLine);
  } else {
    SEL_TO_COMMAND(moveUp:, CommandLinePrevious);
  }
  SEL_TO_COMMAND(moveUpAndModifySelection:, CommandSelectLinePrevious);
  SEL_TO_COMMAND(moveWordBackward:, CommandWordPrevious);
  SEL_TO_COMMAND(moveWordBackwardAndModifySelection:,
    CommandSelectWordPrevious);
  SEL_TO_COMMAND(moveWordForward:, CommandWordNext);
  SEL_TO_COMMAND(moveWordForwardAndModifySelection:, CommandSelectWordNext);
  SEL_TO_COMMAND(moveWordLeft:, CommandWordPrevious);
  SEL_TO_COMMAND(moveWordLeftAndModifySelection:, CommandSelectWordPrevious);
  SEL_TO_COMMAND(moveWordRight:, CommandWordNext);
  SEL_TO_COMMAND(moveWordRightAndModifySelection:, CommandSelectWordNext);
  SEL_TO_COMMAND(pageDown:, CommandMovePageDown);
  SEL_TO_COMMAND(pageDownAndModifySelection:, CommandSelectPageDown);
  SEL_TO_COMMAND(pageUp:, CommandMovePageUp);
  SEL_TO_COMMAND(pageUpAndModifySelection:, CommandSelectPageUp);
  SEL_TO_COMMAND(paste:, CommandPaste);
  // SEL_TO_COMMAND(pasteFont:, );
  // SEL_TO_COMMAND(pasteRuler:, );
  SEL_TO_COMMAND(scrollLineDown:, CommandScrollLineDown);
  SEL_TO_COMMAND(scrollLineUp:, CommandScrollLineUp);
  SEL_TO_COMMAND(scrollPageDown:, CommandScrollPageDown);
  SEL_TO_COMMAND(scrollPageUp:, CommandScrollPageUp);
  SEL_TO_COMMAND(scrollToBeginningOfDocument:, CommandScrollTop);
  SEL_TO_COMMAND(scrollToEndOfDocument:, CommandScrollBottom);
  SEL_TO_COMMAND(selectAll:, CommandSelectAll);
  // selectLine: is complex, see KeyDown
  if (aType == nsIWidget::NativeKeyBindingsForSingleLineEditor) {
    SEL_TO_COMMAND(selectParagraph:, CommandSelectAll);
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

void
NativeKeyBindings::GetEditCommands(const WidgetKeyboardEvent& aEvent,
                                   nsTArray<CommandInt>& aCommands)
{
  MOZ_ASSERT(aCommands.IsEmpty());

  MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info,
    ("%p NativeKeyBindings::GetEditCommands", this));

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
    ("%p NativeKeyBindings::GetEditCommands, bindingCommands=%zu",
     this, bindingCommands.Length()));

  for (uint32_t i = 0; i < bindingCommands.Length(); i++) {
    SEL selector = bindingCommands[i].selector;

    if (MOZ_LOG_TEST(gNativeKeyBindingsLog, LogLevel::Info)) {
      NSString* selectorString = NSStringFromSelector(selector);
      nsAutoString nsSelectorString;
      nsCocoaUtils::GetStringForNSString(selectorString, nsSelectorString);

      MOZ_LOG(gNativeKeyBindingsLog, LogLevel::Info,
        ("%p NativeKeyBindings::GetEditCommands, selector=%s",
         this, NS_LossyConvertUTF16toASCII(nsSelectorString).get()));
    }

    // Try to find a simple mapping in the hashtable
    CommandInt geckoCommand =
      mSelectorToCommand.Get(reinterpret_cast<struct objc_selector*>(selector));

    if (geckoCommand) {
      aCommands.AppendElement(geckoCommand);
    } else if (selector == @selector(selectLine:)) {
      // This is functional, but Cocoa's version is direction-less in that
      // selection direction is not determined until some future directed action
      // is taken. See bug 282097, comment 79 for more details.
      aCommands.AppendElement(CommandBeginLine);
      aCommands.AppendElement(CommandSelectEndLine);
    } else if (selector == @selector(selectWord:)) {
      // This is functional, but Cocoa's version is direction-less in that
      // selection direction is not determined until some future directed action
      // is taken. See bug 282097, comment 79 for more details.
      aCommands.AppendElement(CommandWordPrevious);
      aCommands.AppendElement(CommandSelectWordNext);
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
      ("%p NativeKeyBindings::GetEditCommands, command=%s",
       this, WidgetKeyboardEvent::GetCommandStr(geckoCommand)));
  }
}

} // namespace widget
} // namespace mozilla
