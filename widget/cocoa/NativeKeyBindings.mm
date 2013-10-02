/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeKeyBindings.h"

#include "nsTArray.h"
#include "nsCocoaUtils.h"
#include "prlog.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;
using namespace mozilla::widget;

#ifdef PR_LOGGING
PRLogModuleInfo* gNativeKeyBindingsLog = nullptr;
#endif

NativeKeyBindings::NativeKeyBindings()
{
}

#define SEL_TO_COMMAND(aSel, aCommand) \
  mSelectorToCommand.Put( \
    reinterpret_cast<struct objc_selector *>(@selector(aSel)), aCommand)

NS_IMETHODIMP
NativeKeyBindings::Init(NativeKeyBindingsType aType)
{
#ifdef PR_LOGGING
  if (!gNativeKeyBindingsLog) {
    gNativeKeyBindingsLog = PR_NewLogModule("NativeKeyBindings");
  }
#endif

  PR_LOG(gNativeKeyBindingsLog, PR_LOG_ALWAYS,
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
  SEL_TO_COMMAND(copy:, "cmd_copy");
  // SEL_TO_COMMAND(copyFont:, );
  // SEL_TO_COMMAND(copyRuler:, );
  SEL_TO_COMMAND(cut:, "cmd_cut");
  SEL_TO_COMMAND(delete:, "cmd_delete");
  SEL_TO_COMMAND(deleteBackward:, "cmd_deleteCharBackward");
  // SEL_TO_COMMAND(deleteBackwardByDecomposingPreviousCharacter:, );
  SEL_TO_COMMAND(deleteForward:, "cmd_deleteCharForward");

  // TODO: deleteTo* selectors are also supposed to add text to a kill buffer
  SEL_TO_COMMAND(deleteToBeginningOfLine:, "cmd_deleteToBeginningOfLine");
  SEL_TO_COMMAND(deleteToBeginningOfParagraph:, "cmd_deleteToBeginningOfLine");
  SEL_TO_COMMAND(deleteToEndOfLine:, "cmd_deleteToEndOfLine");
  SEL_TO_COMMAND(deleteToEndOfParagraph:, "cmd_deleteToEndOfLine");
  // SEL_TO_COMMAND(deleteToMark:, );

  SEL_TO_COMMAND(deleteWordBackward:, "cmd_deleteWordBackward");
  SEL_TO_COMMAND(deleteWordForward:, "cmd_deleteWordForward");
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
  SEL_TO_COMMAND(moveBackward:, "cmd_charPrevious");
  SEL_TO_COMMAND(moveBackwardAndModifySelection:, "cmd_selectCharPrevious");
  if (aType == eNativeKeyBindingsType_Input) {
    SEL_TO_COMMAND(moveDown:, "cmd_endLine");
  } else {
    SEL_TO_COMMAND(moveDown:, "cmd_lineNext");
  }
  SEL_TO_COMMAND(moveDownAndModifySelection:, "cmd_selectLineNext");
  SEL_TO_COMMAND(moveForward:, "cmd_charNext");
  SEL_TO_COMMAND(moveForwardAndModifySelection:, "cmd_selectCharNext");
  SEL_TO_COMMAND(moveLeft:, "cmd_charPrevious");
  SEL_TO_COMMAND(moveLeftAndModifySelection:, "cmd_selectCharPrevious");
  SEL_TO_COMMAND(moveParagraphBackwardAndModifySelection:,
    "cmd_selectBeginLine");
  SEL_TO_COMMAND(moveParagraphForwardAndModifySelection:, "cmd_selectEndLine");
  SEL_TO_COMMAND(moveRight:, "cmd_charNext");
  SEL_TO_COMMAND(moveRightAndModifySelection:, "cmd_selectCharNext");
  SEL_TO_COMMAND(moveToBeginningOfDocument:, "cmd_moveTop");
  SEL_TO_COMMAND(moveToBeginningOfDocumentAndModifySelection:, "cmd_selectTop");
  SEL_TO_COMMAND(moveToBeginningOfLine:, "cmd_beginLine");
  SEL_TO_COMMAND(moveToBeginningOfLineAndModifySelection:,
    "cmd_selectBeginLine");
  SEL_TO_COMMAND(moveToBeginningOfParagraph:, "cmd_beginLine");
  SEL_TO_COMMAND(moveToBeginningOfParagraphAndModifySelection:,
    "cmd_selectBeginLine");
  SEL_TO_COMMAND(moveToEndOfDocument:, "cmd_moveBottom");
  SEL_TO_COMMAND(moveToEndOfDocumentAndModifySelection:, "cmd_selectBottom");
  SEL_TO_COMMAND(moveToEndOfLine:, "cmd_endLine");
  SEL_TO_COMMAND(moveToEndOfLineAndModifySelection:, "cmd_selectEndLine");
  SEL_TO_COMMAND(moveToEndOfParagraph:, "cmd_endLine");
  SEL_TO_COMMAND(moveToEndOfParagraphAndModifySelection:, "cmd_selectEndLine");
  SEL_TO_COMMAND(moveToLeftEndOfLine:, "cmd_beginLine");
  SEL_TO_COMMAND(moveToLeftEndOfLineAndModifySelection:, "cmd_selectBeginLine");
  SEL_TO_COMMAND(moveToRightEndOfLine:, "cmd_endLine");
  SEL_TO_COMMAND(moveToRightEndOfLineAndModifySelection:, "cmd_selectEndLine");
  if (aType == eNativeKeyBindingsType_Input) {
    SEL_TO_COMMAND(moveUp:, "cmd_beginLine");
  } else {
    SEL_TO_COMMAND(moveUp:, "cmd_linePrevious");
  }
  SEL_TO_COMMAND(moveUpAndModifySelection:, "cmd_selectLinePrevious");
  SEL_TO_COMMAND(moveWordBackward:, "cmd_wordPrevious");
  SEL_TO_COMMAND(moveWordBackwardAndModifySelection:, "cmd_selectWordPrevious");
  SEL_TO_COMMAND(moveWordForward:, "cmd_wordNext");
  SEL_TO_COMMAND(moveWordForwardAndModifySelection:, "cmd_selectWordNext");
  SEL_TO_COMMAND(moveWordLeft:, "cmd_wordPrevious");
  SEL_TO_COMMAND(moveWordLeftAndModifySelection:, "cmd_selectWordPrevious");
  SEL_TO_COMMAND(moveWordRight:, "cmd_wordNext");
  SEL_TO_COMMAND(moveWordRightAndModifySelection:, "cmd_selectWordNext");
  SEL_TO_COMMAND(pageDown:, "cmd_movePageDown");
  SEL_TO_COMMAND(pageDownAndModifySelection:, "cmd_selectPageDown");
  SEL_TO_COMMAND(pageUp:, "cmd_movePageUp");
  SEL_TO_COMMAND(pageUpAndModifySelection:, "cmd_selectPageUp");
  SEL_TO_COMMAND(paste:, "cmd_paste");
  // SEL_TO_COMMAND(pasteFont:, );
  // SEL_TO_COMMAND(pasteRuler:, );
  SEL_TO_COMMAND(scrollLineDown:, "cmd_scrollLineDown");
  SEL_TO_COMMAND(scrollLineUp:, "cmd_scrollLineUp");
  SEL_TO_COMMAND(scrollPageDown:, "cmd_scrollPageDown");
  SEL_TO_COMMAND(scrollPageUp:, "cmd_scrollPageUp");
  SEL_TO_COMMAND(scrollToBeginningOfDocument:, "cmd_scrollTop");
  SEL_TO_COMMAND(scrollToEndOfDocument:, "cmd_scrollBottom");
  SEL_TO_COMMAND(selectAll:, "cmd_selectAll");
  // selectLine: is complex, see KeyDown
  if (aType == eNativeKeyBindingsType_Input) {
    SEL_TO_COMMAND(selectParagraph:, "cmd_selectAll");
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

  return NS_OK;
}

#undef SEL_TO_COMMAND

NS_IMPL_ISUPPORTS1(NativeKeyBindings, nsINativeKeyBindings)

NS_IMETHODIMP_(bool)
NativeKeyBindings::KeyDown(const WidgetKeyboardEvent& aEvent,
                           DoCommandCallback aCallback, void* aCallbackData)
{
  return false;
}

NS_IMETHODIMP_(bool)
NativeKeyBindings::KeyPress(const WidgetKeyboardEvent& aEvent,
                            DoCommandCallback aCallback, void* aCallbackData)
{
  PR_LOG(gNativeKeyBindingsLog, PR_LOG_ALWAYS,
    ("%p NativeKeyBindings::KeyPress", this));

  // Recover the current event, which should always be the key down we are
  // responding to.

  NSEvent* cocoaEvent = reinterpret_cast<NSEvent*>(aEvent.mNativeKeyEvent);

  if (!cocoaEvent || [cocoaEvent type] != NSKeyDown) {
    PR_LOG(gNativeKeyBindingsLog, PR_LOG_ALWAYS,
      ("%p NativeKeyBindings::KeyPress, no Cocoa key down event", this));

    return false;
  }

  PR_LOG(gNativeKeyBindingsLog, PR_LOG_ALWAYS,
    ("%p NativeKeyBindings::KeyPress, interpreting", this));

  nsAutoTArray<KeyBindingsCommand, 2> bindingCommands;
  nsCocoaUtils::GetCommandsFromKeyEvent(cocoaEvent, bindingCommands);

  PR_LOG(gNativeKeyBindingsLog, PR_LOG_ALWAYS,
    ("%p NativeKeyBindings::KeyPress, bindingCommands=%u",
     this, bindingCommands.Length()));

  nsAutoTArray<const char*, 4> geckoCommands;

  for (uint32_t i = 0; i < bindingCommands.Length(); i++) {
    SEL selector = bindingCommands[i].selector;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gNativeKeyBindingsLog, PR_LOG_ALWAYS)) {
      NSString* selectorString = NSStringFromSelector(selector);
      nsAutoString nsSelectorString;
      nsCocoaUtils::GetStringForNSString(selectorString, nsSelectorString);

      PR_LOG(gNativeKeyBindingsLog, PR_LOG_ALWAYS,
        ("%p NativeKeyBindings::KeyPress, selector=%s",
         this, ToNewCString(nsSelectorString)));
    }
#endif

    // Try to find a simple mapping in the hashtable
    const char* commandStr = mSelectorToCommand.Get(
      reinterpret_cast<struct objc_selector*>(selector));

    if (commandStr) {
      geckoCommands.AppendElement(commandStr);
    } else if (selector == @selector(selectLine:)) {
      // This is functional, but Cocoa's version is direction-less in that
      // selection direction is not determined until some future directed action
      // is taken. See bug 282097, comment 79 for more details.
      geckoCommands.AppendElement("cmd_beginLine");
      geckoCommands.AppendElement("cmd_selectEndLine");
    } else if (selector == @selector(selectWord:)) {
      // This is functional, but Cocoa's version is direction-less in that
      // selection direction is not determined until some future directed action
      // is taken. See bug 282097, comment 79 for more details.
      geckoCommands.AppendElement("cmd_wordPrevious");
      geckoCommands.AppendElement("cmd_selectWordNext");
    }
  }

  if (geckoCommands.IsEmpty()) {
    PR_LOG(gNativeKeyBindingsLog, PR_LOG_ALWAYS,
      ("%p NativeKeyBindings::KeyPress, handled=false", this));

    return false;
  }

  for (uint32_t i = 0; i < geckoCommands.Length(); i++) {
    const char* geckoCommand = geckoCommands[i];

    PR_LOG(gNativeKeyBindingsLog, PR_LOG_ALWAYS,
      ("%p NativeKeyBindings::KeyPress, command=%s",
       this, geckoCommand));

    // Execute the Gecko command
    aCallback(geckoCommand, aCallbackData);
  }

  PR_LOG(gNativeKeyBindingsLog, PR_LOG_ALWAYS,
    ("%p NativeKeyBindings::KeyPress, handled=true", this));

  return true;
}

NS_IMETHODIMP_(bool)
NativeKeyBindings::KeyUp(const WidgetKeyboardEvent& aEvent,
                         DoCommandCallback aCallback, void* aCallbackData)
{
  return false;
}
