/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Compile without ARC: Gecko's TextInputHandler includes MRC Cocoa helpers.
#include "MacWebAppInput.h"

#import <Cocoa/Cocoa.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include "NativeKeyBindings.h"
#include "Protocol.h"
#include "TextInputHandler.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/widget/IMEData.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"
#include "nsIWidget.h"
#include "nsIWidgetListener.h"
#include "nsThreadUtils.h"

namespace mozilla::widget {
namespace {
using floorp::shim::ReadNumber;
using floorp::shim::ReadString;
using floorp::shim::ReadUInt;
// Even fully JSON-escaped UTF-16 text must fit the 64 KiB wire envelope.
constexpr uint32_t kMaximumText = 8192;

nsString GeckoString(NSString* aValue) {
  nsString result;
  result.SetLength(aValue.length);
  [aValue getCharacters:reinterpret_cast<unichar*>(result.BeginWriting())
                  range:NSMakeRange(0, aValue.length)];
  return result;
}

NSString* CocoaString(const nsAString& aValue) {
  return [[[NSString alloc]
      initWithCharacters:reinterpret_cast<const unichar*>(aValue.BeginReading())
                  length:aValue.Length()] autorelease];
}

Modifiers GeckoModifiers(uint32_t aFlags) {
  Modifiers result = 0;
  if (aFlags & NSEventModifierFlagShift) result |= MODIFIER_SHIFT;
  if (aFlags & NSEventModifierFlagControl) result |= MODIFIER_CONTROL;
  if (aFlags & NSEventModifierFlagOption) result |= MODIFIER_ALT;
  if (aFlags & NSEventModifierFlagCommand) result |= MODIFIER_META;
  if (aFlags & NSEventModifierFlagCapsLock) result |= MODIFIER_CAPSLOCK;
  return result;
}

bool Boolean(NSDictionary* aPayload, NSString* aKey, bool* aValue) {
  id value = aPayload[aKey];
  if (!value || CFGetTypeID((CFTypeRef)value) != CFBooleanGetTypeID())
    return false;
  *aValue = [value boolValue];
  return true;
}

Maybe<Command> SelectorCommand(NSString* aSelector) {
#define MAP(selector, command) \
  if ([aSelector isEqualToString:@selector]) return Some(Command::command);
  MAP("copy:", Copy)
  MAP("cut:", Cut)
  MAP("paste:", Paste)
  MAP("selectAll:", SelectAll)
  MAP("delete:", Delete)
  MAP("deleteBackward:", DeleteCharBackward)
  MAP("deleteForward:", DeleteCharForward)
  MAP("deleteWordBackward:", DeleteWordBackward)
  MAP("deleteWordForward:", DeleteWordForward)
  MAP("deleteToBeginningOfLine:", DeleteToBeginningOfLine)
  MAP("deleteToEndOfLine:", DeleteToEndOfLine)
  MAP("moveLeft:", CharPrevious)
  MAP("moveRight:", CharNext)
  MAP("moveBackward:", CharPrevious)
  MAP("moveForward:", CharNext)
  MAP("moveUp:", LinePrevious)
  MAP("moveDown:", LineNext)
  MAP("moveLeftAndModifySelection:", SelectCharPrevious)
  MAP("moveRightAndModifySelection:", SelectCharNext)
  MAP("moveUpAndModifySelection:", SelectLinePrevious)
  MAP("moveDownAndModifySelection:", SelectLineNext)
  MAP("moveWordLeft:", WordPrevious)
  MAP("moveWordRight:", WordNext)
  MAP("moveWordBackward:", WordPrevious)
  MAP("moveWordForward:", WordNext)
  MAP("moveWordLeftAndModifySelection:", SelectWordPrevious)
  MAP("moveWordRightAndModifySelection:", SelectWordNext)
  MAP("moveToBeginningOfLine:", MoveLeft3)
  MAP("moveToEndOfLine:", MoveRight3)
  MAP("moveToBeginningOfLineAndModifySelection:", SelectLeft3)
  MAP("moveToEndOfLineAndModifySelection:", SelectRight3)
  MAP("moveToBeginningOfDocument:", MoveTop)
  MAP("moveToEndOfDocument:", MoveBottom)
  MAP("moveToBeginningOfDocumentAndModifySelection:", SelectTop)
  MAP("moveToEndOfDocumentAndModifySelection:", SelectBottom)
  MAP("pageUp:", MovePageUp)
  MAP("pageDown:", MovePageDown)
  MAP("pageUpAndModifySelection:", SelectPageUp)
  MAP("pageDownAndModifySelection:", SelectPageDown)
#undef MAP
  return Nothing();
}
}  // namespace

NS_IMPL_ISUPPORTS(MacWebAppInput, TextEventDispatcherListener,
                  nsISupportsWeakReference)

MacWebAppInput::MacWebAppInput(
    nsIWidget* aWidget, std::function<void(NSDictionary*)> aSendEditorState)
    : mWidget(aWidget), mSendEditorState(std::move(aSendEditorState)) {}

MacWebAppInput::~MacWebAppInput() { OnDestroy(); }

bool MacWebAppInput::IsAlive() const {
  return mWidget && !mWidget->Destroyed();
}

void MacWebAppInput::OnShimDisconnected() {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<MacWebAppInput> self(this);
  mButtons = 0;
  mKeyDownConsumed = false;
  mCommand.reset();
  [mLastKeyEvent release];
  mLastKeyEvent = nullptr;
  // The native input context vanished. Cancel uncommitted text, but retain the
  // document's focus and widget so a new Shim can receive fresh editor state.
  // CommitComposition can synchronously run script and destroy the widget.
  FinishComposition(true);
}

void MacWebAppInput::OnDestroy() {
  mWidget = nullptr;
  mSendEditorState = nullptr;
  [mLastKeyEvent release];
  mLastKeyEvent = nullptr;
  mComposition.Truncate();
}

void MacWebAppInput::Handle(NSDictionary* aPayload) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!IsAlive()) return;
  RefPtr<MacWebAppInput> self(this);
  NSString* kind = ReadString(aPayload, @"kind", 32);
  if (!kind) return;
  if ([kind isEqualToString:@"focus"]) {
    bool focused;
    if (!Boolean(aPayload, @"focused", &focused)) return;
    if (nsIWidgetListener* listener = mWidget->GetWidgetListener()) {
      if (focused)
        listener->WindowActivated();
      else
        listener->WindowDeactivated();
    }
    if (!focused) mButtons = 0;
    UpdateEditorState();
    return;
  }
  if ([kind isEqualToString:@"keyDown"] || [kind isEqualToString:@"keyUp"] ||
      [kind isEqualToString:@"flagsChanged"]) {
    HandleKey(aPayload);
    return;
  }
  if ([kind isEqualToString:@"insertText"] ||
      [kind isEqualToString:@"setMarkedText"] ||
      [kind isEqualToString:@"unmarkText"] ||
      [kind isEqualToString:@"textCommand"]) {
    HandleText(aPayload);
    return;
  }
  double x, y;
  uint32_t modifiers;
  if (!ReadNumber(aPayload, @"x", &x, -65536, 65536) ||
      !ReadNumber(aPayload, @"y", &y, -65536, 65536) ||
      !ReadUInt(aPayload, @"modifiers", &modifiers, true))
    return;
  const double scale = mWidget->GetDefaultScale().scale;
  LayoutDeviceIntPoint point(std::lround(x * scale), std::lround(y * scale));
  if ([kind isEqualToString:@"scroll"]) {
    double dx, dy;
    bool precise;
    uint32_t momentum;
    if (!ReadNumber(aPayload, @"deltaX", &dx, -65536, 65536) ||
        !ReadNumber(aPayload, @"deltaY", &dy, -65536, 65536) ||
        !Boolean(aPayload, @"precise", &precise) ||
        !ReadUInt(aPayload, @"momentumPhase", &momentum, true))
      return;
    WidgetWheelEvent event(true, eWheel, mWidget);
    event.mRefPoint = point;
    event.mModifiers = GeckoModifiers(modifiers);
    event.mDeltaMode = precise ? 0 : 1;
    event.mDeltaX = -dx * (precise ? scale : 1);
    event.mDeltaY = -dy * (precise ? scale : 1);
    event.mIsMomentum = momentum != NSEventPhaseNone;
    event.mLineOrPageDeltaX = precise ? 0 : std::lround(-dx);
    event.mLineOrPageDeltaY = precise ? 0 : std::lround(-dy);
    mWidget->DispatchInputEvent(&event);
    return;
  }
  EventMessage message;
  if ([kind isEqualToString:@"mouseDown"])
    message = eMouseDown;
  else if ([kind isEqualToString:@"mouseUp"])
    message = eMouseUp;
  else if ([kind isEqualToString:@"mouseMove"])
    message = eMouseMove;
  else if ([kind isEqualToString:@"mouseEnter"])
    message = eMouseEnterIntoWidget;
  else if ([kind isEqualToString:@"mouseExit"])
    message = eMouseExitFromWidget;
  else
    return;
  uint32_t button, clicks;
  if (!ReadUInt(aPayload, @"button", &button, true) || button > 4 ||
      !ReadUInt(aPayload, @"clickCount", &clicks, true) || clicks > 32)
    return;
  // Cocoa numbers right/middle as 1/2; DOM numbers them as 2/1.
  const int16_t domButton = button == 1 ? 2 : button == 2 ? 1 : button;
  const int16_t flag = button == 0   ? 1
                       : button == 1 ? 2
                       : button == 2 ? 4
                                     : 1 << button;
  if (message == eMouseDown) mButtons |= flag;
  if (message == eMouseUp) mButtons &= ~flag;
  WidgetMouseEvent event(true, message, mWidget, WidgetMouseEvent::eReal);
  event.mRefPoint = point;
  event.mModifiers = GeckoModifiers(modifiers);
  event.mButton = domButton;
  event.mButtons = mButtons;
  event.mClickCount = clicks;
  mWidget->DispatchInputEvent(&event);
  if (message == eMouseUp && domButton == 2 && IsAlive()) {
    WidgetMouseEvent menu(true, eContextMenu, mWidget, WidgetMouseEvent::eReal);
    menu.mRefPoint = point;
    menu.mModifiers = event.mModifiers;
    menu.mButton = domButton;
    mWidget->DispatchInputEvent(&menu);
  }
}

void MacWebAppInput::HandleKey(NSDictionary* aPayload) {
  uint32_t code, flags;
  double timestamp;
  NSString* kind = aPayload[@"kind"];
  if (!ReadUInt(aPayload, @"keyCode", &code, true) || code > UINT16_MAX ||
      !ReadUInt(aPayload, @"modifiers", &flags, true) ||
      !ReadNumber(aPayload, @"timestamp", &timestamp, 0, 1e12))
    return;
  bool isDown = [kind isEqualToString:@"keyDown"];
  bool repeat = false;
  NSString* characters = @"";
  NSString* ignoring = @"";
  if ([kind isEqualToString:@"flagsChanged"]) {
    uint32_t flag = 0;
    switch (code) {
      case kVK_Shift:
      case kVK_RightShift:
        flag = NSEventModifierFlagShift;
        break;
      case kVK_Control:
      case kVK_RightControl:
        flag = NSEventModifierFlagControl;
        break;
      case kVK_Option:
      case kVK_RightOption:
        flag = NSEventModifierFlagOption;
        break;
      case kVK_Command:
      case kVK_RightCommand:
        flag = NSEventModifierFlagCommand;
        break;
      case kVK_CapsLock:
        flag = NSEventModifierFlagCapsLock;
        break;
      default:
        return;
    }
    isDown = (flags & flag) != 0;
  } else {
    characters = ReadString(aPayload, @"characters", 32768);
    ignoring = ReadString(aPayload, @"charactersIgnoringModifiers", 32768);
    if (!characters || !ignoring || !Boolean(aPayload, @"repeat", &repeat))
      return;
  }
  NSEvent* native =
      [NSEvent keyEventWithType:isDown ? NSEventTypeKeyDown : NSEventTypeKeyUp
                             location:NSZeroPoint
                        modifierFlags:flags
                            timestamp:timestamp
                         windowNumber:0
                              context:nil
                           characters:characters
          charactersIgnoringModifiers:ignoring
                            isARepeat:repeat
                              keyCode:code];
  RefPtr<TextEventDispatcher> dispatcher = mWidget->GetTextEventDispatcher();
  if (NS_FAILED(dispatcher->BeginNativeInputTransaction())) return;
  WidgetKeyboardEvent event(true, isDown ? eKeyDown : eKeyUp, mWidget);
  TISInputSourceWrapper::CurrentInputSource().InitKeyEvent(native, event,
                                                           mComposing);
  nsEventStatus status = nsEventStatus_eIgnore;
  dispatcher->DispatchKeyboardEvent(event.mMessage, event, status);
  if (isDown && IsAlive()) {
    [mLastKeyEvent release];
    mLastKeyEvent = [native retain];
    mKeyDownConsumed = status == nsEventStatus_eConsumeNoDefault;
    // Editable text is dispatched only by insertText/setMarkedText/textCommand.
    // This prevents the raw key and NSTextInputClient callback inserting twice.
    if (!mWidget->GetInputContext().mIMEState.IsEditable()) {
      dispatcher->MaybeDispatchKeypressEvents(event, status);
    }
  } else if (!isDown && IsAlive()) {
    // NSTextInputClient callbacks for this key precede keyUp on the ordered
    // channel. Later dictation/paste must not inherit a previous key's status.
    [mLastKeyEvent release];
    mLastKeyEvent = nullptr;
    mKeyDownConsumed = false;
  }
}

bool MacWebAppInput::ApplyReplacement(NSDictionary* aPayload) {
  NSDictionary* replacement = aPayload[@"replacement"];
  if (![replacement isKindOfClass:NSDictionary.class]) return false;
  double location;
  uint32_t length;
  if (!ReadNumber(replacement, @"location", &location, -1, UINT32_MAX) ||
      std::floor(location) != location ||
      !ReadUInt(replacement, @"length", &length, true))
    return false;
  if (location == -1) return true;
  uint32_t revision;
  if (!ReadUInt(aPayload, @"editorRevision", &revision, true) ||
      revision != mRevision || length > UINT32_MAX - uint32_t(location))
    return false;
  if (mComposing) {
    // Replacement of the existing marked range is handled by compositionchange.
    return uint32_t(location) == mCompositionStart &&
           length == mComposition.Length();
  }
  WidgetSelectionEvent selection(true, eSetSelection, mWidget);
  selection.mOffset = uint32_t(location);
  selection.mLength = length;
  mWidget->DispatchWindowEvent(selection);
  return IsAlive() && selection.mSucceeded;
}

void MacWebAppInput::HandleText(NSDictionary* aPayload) {
  uint32_t revision;
  if (!ReadUInt(aPayload, @"editorRevision", &revision, true) ||
      revision < mFocusRevision || revision > mRevision || mKeyDownConsumed ||
      !mIMEFocused || !mWidget->GetInputContext().mIMEState.IsEditable())
    return;
  RefPtr<TextEventDispatcher> dispatcher = mWidget->GetTextEventDispatcher();
  if (NS_FAILED(dispatcher->BeginNativeInputTransaction())) return;
  NSString* kind = aPayload[@"kind"];
  nsEventStatus status = nsEventStatus_eIgnore;
  if ([kind isEqualToString:@"unmarkText"]) {
    FinishComposition(false);
    return;
  }
  if ([kind isEqualToString:@"textCommand"]) {
    NSString* selector = ReadString(aPayload, @"selector", 128);
    if (!selector || !mLastKeyEvent || mComposing) return;
    AutoRestore<Maybe<Command>> command(mCommand);
    mCommand = SelectorCommand(selector);
    WidgetKeyboardEvent event(true, eKeyDown, mWidget);
    TISInputSourceWrapper::CurrentInputSource().InitKeyEvent(mLastKeyEvent,
                                                             event, false);
    event.InitAllEditCommands(dispatcher->MaybeWritingModeRefAtSelection());
    dispatcher->MaybeDispatchKeypressEvents(event, status);
    return;
  }
  NSString* text = ReadString(aPayload, @"text", 32768);
  if (!text || !ApplyReplacement(aPayload)) {
    UpdateEditorState();
    return;
  }
  nsString value = GeckoString(text);
  if ([kind isEqualToString:@"insertText"]) {
    if (mComposing) {
      mComposing = false;
      mComposition.Truncate();
      dispatcher->CommitComposition(status, &value);
    } else if (mLastKeyEvent) {
      WidgetKeyboardEvent event(true, eKeyDown, mWidget);
      TISInputSourceWrapper::CurrentInputSource().InitKeyEvent(
          mLastKeyEvent, event, false, &value);
      event.PreventNativeKeyBindings();
      dispatcher->MaybeDispatchKeypressEvents(event, status);
    } else {
      WidgetContentCommandEvent event(true, eContentCommandInsertText, mWidget);
      event.mString = Some(value);
      mWidget->DispatchWindowEvent(event);
    }
    return;
  }
  NSDictionary* selected = aPayload[@"selected"];
  uint32_t selectedStart, selectedLength;
  if (![selected isKindOfClass:NSDictionary.class] ||
      !ReadUInt(selected, @"location", &selectedStart, true) ||
      !ReadUInt(selected, @"length", &selectedLength, true) ||
      selectedStart > value.Length() ||
      selectedLength > value.Length() - selectedStart)
    return;
  if (!mComposing) {
    WidgetQueryContentEvent selection(true, eQuerySelectedText, mWidget);
    mWidget->DispatchWindowEvent(selection);
    if (!IsAlive() || selection.Failed() || !selection.mReply->mOffsetAndData)
      return;
    mCompositionStart = selection.mReply->StartOffset();
    mComposing = true;
    if (NS_FAILED(dispatcher->StartComposition(status)) || !IsAlive() ||
        !dispatcher->IsComposing()) {
      mComposing = false;
      return;
    }
  }
  mComposition = value;
  if (NS_FAILED(dispatcher->SetPendingCompositionString(value))) return;
  if (value.Length())
    dispatcher->AppendClauseToPendingComposition(value.Length(),
                                                 TextRangeType::eRawClause);
  dispatcher->SetCaretInPendingComposition(selectedStart, selectedLength);
  dispatcher->FlushPendingComposition(status);
}

bool MacWebAppInput::GetEditCommands(NativeKeyBindingsType aType,
                                     const WidgetKeyboardEvent& aEvent,
                                     nsTArray<CommandInt>& aCommands) {
  if (!IsAlive()) return false;
  if (mCommand)
    aCommands.AppendElement(static_cast<CommandInt>(*mCommand));
  else
    NativeKeyBindings::GetInstance(aType)->GetEditCommands(
        aEvent,
        mWidget->GetTextEventDispatcher()->MaybeWritingModeRefAtSelection(),
        aCommands);
  return true;
}

void MacWebAppInput::FinishComposition(bool aCancel) {
  if (!IsAlive() || !mComposing) return;
  RefPtr<TextEventDispatcher> dispatcher = mWidget->GetTextEventDispatcher();
  mComposing = false;
  mComposition.Truncate();
  nsEventStatus status = nsEventStatus_eIgnore;
  nsString empty;
  dispatcher->CommitComposition(status, aCancel ? &empty : nullptr);
  if (IsAlive()) SendEditorState(true);
}

void MacWebAppInput::UpdateEditorState() { SendEditorState(false); }

void MacWebAppInput::SendEditorState(bool aDiscardMarkedText) {
  if (!IsAlive() || !mSendEditorState || mUpdatingEditorState ||
      mRevision == UINT32_MAX)
    return;
  RefPtr<MacWebAppInput> self(this);
  AutoRestore<bool> updating(mUpdatingEditorState);
  mUpdatingEditorState = true;
  bool editable =
      mIMEFocused && mWidget->GetInputContext().mIMEState.IsEditable();
  uint32_t selectionStart = 0, selectionLength = 0, textOffset = 0;
  nsString text;
  LayoutDeviceIntRect caret;
  if (editable) {
    WidgetQueryContentEvent selection(true, eQuerySelectedText, mWidget);
    mWidget->DispatchWindowEvent(selection);
    if (!IsAlive()) return;
    if (selection.Succeeded() && selection.mReply->mOffsetAndData) {
      selectionStart = selection.mReply->StartOffset();
      selectionLength = selection.mReply->DataLength();
      textOffset = selectionStart > kMaximumText / 2
                       ? selectionStart - kMaximumText / 2
                       : 0;
      WidgetQueryContentEvent content(true, eQueryTextContent, mWidget);
      content.InitForQueryTextContent(textOffset, kMaximumText);
      mWidget->DispatchWindowEvent(content);
      if (!IsAlive()) return;
      if (content.Succeeded()) text = content.mReply->DataRef();
      if (selectionStart < textOffset ||
          selectionStart - textOffset > text.Length()) {
        textOffset = selectionStart;
        text.Truncate();
        selectionLength = 0;
      } else
        selectionLength = std::min<uint32_t>(
            selectionLength, text.Length() - (selectionStart - textOffset));
      if (mWidget->GetInputContext().IsPasswordEditor()) {
        for (uint32_t i = 0; i < text.Length(); ++i)
          text.SetCharAt(u'\u2022', i);
      }
      WidgetQueryContentEvent position(true, eQueryCaretRect, mWidget);
      position.InitForQueryCaretRect(selectionStart);
      mWidget->DispatchWindowEvent(position);
      if (!IsAlive()) return;
      if (position.Succeeded()) caret = position.mReply->mRect;
    }
  }
  const double scale = mWidget->GetDefaultScale().scale;
  NSMutableDictionary* payload = [@{
    @"revision" : @(++mRevision),
    @"text" : CocoaString(text),
    @"textOffset" : @(textOffset),
    @"selectionStart" : @(selectionStart),
    @"selectionLength" : @(selectionLength),
    @"caretX" : @(caret.x / scale),
    @"caretY" : @(caret.y / scale),
    @"caretWidth" : @(caret.width / scale),
    @"caretHeight" : @(caret.height / scale),
    @"editable" : @(editable),
    @"password" : @(editable && mWidget->GetInputContext().IsPasswordEditor()),
    @"discardMarkedText" : @(aDiscardMarkedText)
  } mutableCopy];
  if (mComposing && mCompositionStart >= textOffset &&
      uint64_t(mCompositionStart) + mComposition.Length() <=
          uint64_t(textOffset) + text.Length()) {
    payload[@"markedStart"] = @(mCompositionStart);
    payload[@"markedLength"] = @(mComposition.Length());
  }
  mSendEditorState(payload);
  [payload release];
}

NS_IMETHODIMP MacWebAppInput::NotifyIME(TextEventDispatcher* aDispatcher,
                                        const IMENotification& aNotification) {
  RefPtr<MacWebAppInput> self(this);
  switch (aNotification.mMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION:
      FinishComposition(false);
      return NS_OK;
    case REQUEST_TO_CANCEL_COMPOSITION:
      FinishComposition(true);
      return NS_OK;
    case NOTIFY_IME_OF_FOCUS:
    case NOTIFY_IME_OF_BLUR:
      mIMEFocused = aNotification.mMessage == NOTIFY_IME_OF_FOCUS;
      if (!mIMEFocused) {
        mComposing = false;
        mComposition.Truncate();
      }
      mFocusRevision = mRevision + 1;
      mKeyDownConsumed = false;
      [mLastKeyEvent release];
      mLastKeyEvent = nullptr;
      SendEditorState(true);
      return NS_OK;
    case NOTIFY_IME_OF_SELECTION_CHANGE:
    case NOTIFY_IME_OF_TEXT_CHANGE:
    case NOTIFY_IME_OF_POSITION_CHANGE:
    case NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED:
      UpdateEditorState();
      return NS_OK;
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

NS_IMETHODIMP_(IMENotificationRequests)
MacWebAppInput::GetIMENotificationRequests() {
  return {IMENotificationRequest::TextChange,
          IMENotificationRequest::PositionChange};
}
NS_IMETHODIMP_(void) MacWebAppInput::OnRemovedFrom(TextEventDispatcher*) {}
NS_IMETHODIMP_(void)
MacWebAppInput::WillDispatchKeyboardEvent(TextEventDispatcher*,
                                          WidgetKeyboardEvent&, uint32_t,
                                          void*) {}
}  // namespace mozilla::widget
