/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGUIEventIPC_h__
#define nsGUIEventIPC_h__

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/ContentCache.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/layers/LayersMessageUtils.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/WheelHandlingHelper.h"  // for WheelDeltaAdjustmentStrategy
#include "mozilla/dom/Selection.h"
#include "InputData.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::EventMessage>
    : public ContiguousEnumSerializer<
          mozilla::EventMessage, mozilla::EventMessage(0),
          mozilla::EventMessage::eEventMessage_MaxValue> {};

template <>
struct ParamTraits<mozilla::BaseEventFlags> {
  typedef mozilla::BaseEventFlags paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return aMsg->ReadBytesInto(aIter, aResult, sizeof(*aResult));
  }
};

template <>
struct ParamTraits<mozilla::WidgetEvent> {
  typedef mozilla::WidgetEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    // Mark the event as posted to another process.
    const_cast<mozilla::WidgetEvent&>(aParam).MarkAsPostedToRemoteProcess();

    WriteParam(aMsg, static_cast<mozilla::EventClassIDType>(aParam.mClass));
    WriteParam(aMsg, aParam.mMessage);
    WriteParam(aMsg, aParam.mRefPoint);
    WriteParam(aMsg, aParam.mFocusSequenceNumber);
    WriteParam(aMsg, aParam.mTime);
    WriteParam(aMsg, aParam.mTimeStamp);
    WriteParam(aMsg, aParam.mFlags);
    WriteParam(aMsg, aParam.mLayersId);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    mozilla::EventClassIDType eventClassID = 0;
    bool ret = ReadParam(aMsg, aIter, &eventClassID) &&
               ReadParam(aMsg, aIter, &aResult->mMessage) &&
               ReadParam(aMsg, aIter, &aResult->mRefPoint) &&
               ReadParam(aMsg, aIter, &aResult->mFocusSequenceNumber) &&
               ReadParam(aMsg, aIter, &aResult->mTime) &&
               ReadParam(aMsg, aIter, &aResult->mTimeStamp) &&
               ReadParam(aMsg, aIter, &aResult->mFlags) &&
               ReadParam(aMsg, aIter, &aResult->mLayersId);
    aResult->mClass = static_cast<mozilla::EventClassID>(eventClassID);
    if (ret) {
      // Reset cross process dispatching state here because the event has not
      // been dispatched to different process from current process.
      aResult->ResetCrossProcessDispatchingState();
      // Mark the event comes from another process.
      aResult->MarkAsComingFromAnotherProcess();
    }
    return ret;
  }
};

template <>
struct ParamTraits<mozilla::NativeEventData> {
  typedef mozilla::NativeEventData paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mBuffer);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mBuffer);
  }
};

template <>
struct ParamTraits<mozilla::WidgetGUIEvent> {
  typedef mozilla::WidgetGUIEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::WidgetEvent&>(aParam));
    WriteParam(aMsg, aParam.mPluginEvent);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mPluginEvent);
  }
};

template <>
struct ParamTraits<mozilla::WidgetInputEvent> {
  typedef mozilla::WidgetInputEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::WidgetGUIEvent&>(aParam));
    WriteParam(aMsg, aParam.mModifiers);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mModifiers);
  }
};

template <>
struct ParamTraits<mozilla::WidgetMouseEventBase> {
  typedef mozilla::WidgetMouseEventBase paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::WidgetInputEvent&>(aParam));
    WriteParam(aMsg, aParam.mButton);
    WriteParam(aMsg, aParam.mButtons);
    WriteParam(aMsg, aParam.mPressure);
    WriteParam(aMsg, aParam.mInputSource);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetInputEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mButton) &&
           ReadParam(aMsg, aIter, &aResult->mButtons) &&
           ReadParam(aMsg, aIter, &aResult->mPressure) &&
           ReadParam(aMsg, aIter, &aResult->mInputSource);
  }
};

template <>
struct ParamTraits<mozilla::WidgetWheelEvent> {
  typedef mozilla::WidgetWheelEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::WidgetMouseEventBase&>(aParam));
    WriteParam(aMsg, aParam.mDeltaX);
    WriteParam(aMsg, aParam.mDeltaY);
    WriteParam(aMsg, aParam.mDeltaZ);
    WriteParam(aMsg, aParam.mDeltaMode);
    WriteParam(aMsg, aParam.mWheelTicksX);
    WriteParam(aMsg, aParam.mWheelTicksY);
    WriteParam(aMsg, aParam.mCustomizedByUserPrefs);
    WriteParam(aMsg, aParam.mMayHaveMomentum);
    WriteParam(aMsg, aParam.mIsMomentum);
    WriteParam(aMsg, aParam.mIsNoLineOrPageDelta);
    WriteParam(aMsg, aParam.mLineOrPageDeltaX);
    WriteParam(aMsg, aParam.mLineOrPageDeltaY);
    WriteParam(aMsg, static_cast<uint8_t>(aParam.mScrollType));
    WriteParam(aMsg, aParam.mOverflowDeltaX);
    WriteParam(aMsg, aParam.mOverflowDeltaY);
    WriteParam(aMsg, aParam.mViewPortIsOverscrolled);
    WriteParam(aMsg, aParam.mCanTriggerSwipe);
    WriteParam(aMsg, aParam.mAllowToOverrideSystemScrollSpeed);
    WriteParam(aMsg, aParam.mDeltaValuesHorizontalizedForDefaultHandler);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uint8_t scrollType = 0;
    bool rv =
        ReadParam(aMsg, aIter,
                  static_cast<mozilla::WidgetMouseEventBase*>(aResult)) &&
        ReadParam(aMsg, aIter, &aResult->mDeltaX) &&
        ReadParam(aMsg, aIter, &aResult->mDeltaY) &&
        ReadParam(aMsg, aIter, &aResult->mDeltaZ) &&
        ReadParam(aMsg, aIter, &aResult->mDeltaMode) &&
        ReadParam(aMsg, aIter, &aResult->mWheelTicksX) &&
        ReadParam(aMsg, aIter, &aResult->mWheelTicksY) &&
        ReadParam(aMsg, aIter, &aResult->mCustomizedByUserPrefs) &&
        ReadParam(aMsg, aIter, &aResult->mMayHaveMomentum) &&
        ReadParam(aMsg, aIter, &aResult->mIsMomentum) &&
        ReadParam(aMsg, aIter, &aResult->mIsNoLineOrPageDelta) &&
        ReadParam(aMsg, aIter, &aResult->mLineOrPageDeltaX) &&
        ReadParam(aMsg, aIter, &aResult->mLineOrPageDeltaY) &&
        ReadParam(aMsg, aIter, &scrollType) &&
        ReadParam(aMsg, aIter, &aResult->mOverflowDeltaX) &&
        ReadParam(aMsg, aIter, &aResult->mOverflowDeltaY) &&
        ReadParam(aMsg, aIter, &aResult->mViewPortIsOverscrolled) &&
        ReadParam(aMsg, aIter, &aResult->mCanTriggerSwipe) &&
        ReadParam(aMsg, aIter, &aResult->mAllowToOverrideSystemScrollSpeed) &&
        ReadParam(aMsg, aIter,
                  &aResult->mDeltaValuesHorizontalizedForDefaultHandler);
    aResult->mScrollType =
        static_cast<mozilla::WidgetWheelEvent::ScrollType>(scrollType);
    return rv;
  }
};

template <>
struct ParamTraits<mozilla::WidgetPointerHelper> {
  typedef mozilla::WidgetPointerHelper paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.pointerId);
    WriteParam(aMsg, aParam.tiltX);
    WriteParam(aMsg, aParam.tiltY);
    WriteParam(aMsg, aParam.twist);
    WriteParam(aMsg, aParam.tangentialPressure);
    // We don't serialize convertToPointer since it's temporarily variable and
    // should be reset to default.
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    bool rv;
    rv = ReadParam(aMsg, aIter, &aResult->pointerId) &&
         ReadParam(aMsg, aIter, &aResult->tiltX) &&
         ReadParam(aMsg, aIter, &aResult->tiltY) &&
         ReadParam(aMsg, aIter, &aResult->twist) &&
         ReadParam(aMsg, aIter, &aResult->tangentialPressure);
    return rv;
  }
};

template <>
struct ParamTraits<mozilla::WidgetMouseEvent> {
  typedef mozilla::WidgetMouseEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::WidgetMouseEventBase&>(aParam));
    WriteParam(aMsg, static_cast<const mozilla::WidgetPointerHelper&>(aParam));
    WriteParam(aMsg, aParam.mIgnoreRootScrollFrame);
    WriteParam(aMsg, aParam.mClickEventPrevented);
    WriteParam(aMsg, static_cast<paramType::ReasonType>(aParam.mReason));
    WriteParam(aMsg, static_cast<paramType::ContextMenuTriggerType>(
                         aParam.mContextMenuTrigger));
    WriteParam(aMsg, aParam.mExitFrom.isSome());
    if (aParam.mExitFrom.isSome()) {
      WriteParam(
          aMsg, static_cast<paramType::ExitFromType>(aParam.mExitFrom.value()));
    }
    WriteParam(aMsg, aParam.mClickCount);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    bool rv;
    paramType::ReasonType reason = 0;
    paramType::ContextMenuTriggerType contextMenuTrigger = 0;
    bool hasExitFrom = false;
    rv = ReadParam(aMsg, aIter,
                   static_cast<mozilla::WidgetMouseEventBase*>(aResult)) &&
         ReadParam(aMsg, aIter,
                   static_cast<mozilla::WidgetPointerHelper*>(aResult)) &&
         ReadParam(aMsg, aIter, &aResult->mIgnoreRootScrollFrame) &&
         ReadParam(aMsg, aIter, &aResult->mClickEventPrevented) &&
         ReadParam(aMsg, aIter, &reason) &&
         ReadParam(aMsg, aIter, &contextMenuTrigger);
    aResult->mReason = static_cast<paramType::Reason>(reason);
    aResult->mContextMenuTrigger =
        static_cast<paramType::ContextMenuTrigger>(contextMenuTrigger);
    rv = rv && ReadParam(aMsg, aIter, &hasExitFrom);
    if (hasExitFrom) {
      paramType::ExitFromType exitFrom = 0;
      rv = rv && ReadParam(aMsg, aIter, &exitFrom);
      aResult->mExitFrom = Some(static_cast<paramType::ExitFrom>(exitFrom));
    }
    rv = rv && ReadParam(aMsg, aIter, &aResult->mClickCount);
    return rv;
  }
};

template <>
struct ParamTraits<mozilla::WidgetDragEvent> {
  typedef mozilla::WidgetDragEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::WidgetMouseEvent&>(aParam));
    WriteParam(aMsg, aParam.mUserCancelled);
    WriteParam(aMsg, aParam.mDefaultPreventedOnContent);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    bool rv = ReadParam(aMsg, aIter,
                        static_cast<mozilla::WidgetMouseEvent*>(aResult)) &&
              ReadParam(aMsg, aIter, &aResult->mUserCancelled) &&
              ReadParam(aMsg, aIter, &aResult->mDefaultPreventedOnContent);
    return rv;
  }
};

template <>
struct ParamTraits<mozilla::WidgetPointerEvent> {
  typedef mozilla::WidgetPointerEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::WidgetMouseEvent&>(aParam));
    WriteParam(aMsg, aParam.mWidth);
    WriteParam(aMsg, aParam.mHeight);
    WriteParam(aMsg, aParam.mIsPrimary);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    bool rv = ReadParam(aMsg, aIter,
                        static_cast<mozilla::WidgetMouseEvent*>(aResult)) &&
              ReadParam(aMsg, aIter, &aResult->mWidth) &&
              ReadParam(aMsg, aIter, &aResult->mHeight) &&
              ReadParam(aMsg, aIter, &aResult->mIsPrimary);
    return rv;
  }
};

template <>
struct ParamTraits<mozilla::WidgetTouchEvent> {
  using paramType = mozilla::WidgetTouchEvent;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::WidgetInputEvent&>(aParam));
    WriteParam(aMsg, aParam.mInputSource);
    WriteParam(aMsg, aParam.mButton);
    WriteParam(aMsg, aParam.mButtons);
    // Sigh, Touch bites us again!  We want to be able to do
    //   WriteParam(aMsg, aParam.mTouches);
    const paramType::TouchArray& touches = aParam.mTouches;
    WriteParam(aMsg, touches.Length());
    for (uint32_t i = 0; i < touches.Length(); ++i) {
      mozilla::dom::Touch* touch = touches[i];
      WriteParam(aMsg, touch->mIdentifier);
      WriteParam(aMsg, touch->mRefPoint);
      WriteParam(aMsg, touch->mRadius);
      WriteParam(aMsg, touch->mRotationAngle);
      WriteParam(aMsg, touch->mForce);
      WriteParam(aMsg, touch->tiltX);
      WriteParam(aMsg, touch->tiltY);
      WriteParam(aMsg, touch->twist);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    paramType::TouchArray::size_type numTouches;
    if (!ReadParam(aMsg, aIter,
                   static_cast<mozilla::WidgetInputEvent*>(aResult)) ||
        !ReadParam(aMsg, aIter, &aResult->mInputSource) ||
        !ReadParam(aMsg, aIter, &aResult->mButton) ||
        !ReadParam(aMsg, aIter, &aResult->mButtons) ||
        !ReadParam(aMsg, aIter, &numTouches)) {
      return false;
    }
    for (uint32_t i = 0; i < numTouches; ++i) {
      int32_t identifier;
      mozilla::LayoutDeviceIntPoint refPoint;
      mozilla::LayoutDeviceIntPoint radius;
      float rotationAngle;
      float force;
      uint32_t tiltX;
      uint32_t tiltY;
      uint32_t twist;
      if (!ReadParam(aMsg, aIter, &identifier) ||
          !ReadParam(aMsg, aIter, &refPoint) ||
          !ReadParam(aMsg, aIter, &radius) ||
          !ReadParam(aMsg, aIter, &rotationAngle) ||
          !ReadParam(aMsg, aIter, &force) || !ReadParam(aMsg, aIter, &tiltX) ||
          !ReadParam(aMsg, aIter, &tiltY) || !ReadParam(aMsg, aIter, &twist)) {
        return false;
      }
      auto* touch = new mozilla::dom::Touch(identifier, refPoint, radius,
                                            rotationAngle, force);
      touch->tiltX = tiltX;
      touch->tiltY = tiltY;
      touch->twist = twist;
      aResult->mTouches.AppendElement(touch);
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::AlternativeCharCode> {
  typedef mozilla::AlternativeCharCode paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mUnshiftedCharCode);
    WriteParam(aMsg, aParam.mShiftedCharCode);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mUnshiftedCharCode) &&
           ReadParam(aMsg, aIter, &aResult->mShiftedCharCode);
  }
};

template <>
struct ParamTraits<mozilla::ShortcutKeyCandidate> {
  typedef mozilla::ShortcutKeyCandidate paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mCharCode);
    WriteParam(aMsg, aParam.mIgnoreShift);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mCharCode) &&
           ReadParam(aMsg, aIter, &aResult->mIgnoreShift);
  }
};

template <>
struct ParamTraits<mozilla::WidgetKeyboardEvent> {
  typedef mozilla::WidgetKeyboardEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::WidgetInputEvent&>(aParam));
    WriteParam(aMsg,
               static_cast<mozilla::KeyNameIndexType>(aParam.mKeyNameIndex));
    WriteParam(aMsg,
               static_cast<mozilla::CodeNameIndexType>(aParam.mCodeNameIndex));
    WriteParam(aMsg, aParam.mKeyValue);
    WriteParam(aMsg, aParam.mCodeValue);
    WriteParam(aMsg, aParam.mKeyCode);
    WriteParam(aMsg, aParam.mCharCode);
    WriteParam(aMsg, aParam.mPseudoCharCode);
    WriteParam(aMsg, aParam.mAlternativeCharCodes);
    WriteParam(aMsg, aParam.mIsRepeat);
    WriteParam(aMsg, aParam.mLocation);
    WriteParam(aMsg, aParam.mUniqueId);
    WriteParam(aMsg, aParam.mIsSynthesizedByTIP);
    WriteParam(aMsg, aParam.mMaybeSkippableInRemoteProcess);

    // An OS-specific native event might be attached in |mNativeKeyEvent|,  but
    // that cannot be copied across process boundaries.

    WriteParam(aMsg, aParam.mEditCommandsForSingleLineEditor);
    WriteParam(aMsg, aParam.mEditCommandsForMultiLineEditor);
    WriteParam(aMsg, aParam.mEditCommandsForRichTextEditor);
    WriteParam(aMsg, aParam.mEditCommandsForSingleLineEditorInitialized);
    WriteParam(aMsg, aParam.mEditCommandsForMultiLineEditorInitialized);
    WriteParam(aMsg, aParam.mEditCommandsForRichTextEditorInitialized);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    mozilla::KeyNameIndexType keyNameIndex = 0;
    mozilla::CodeNameIndexType codeNameIndex = 0;
    if (ReadParam(aMsg, aIter,
                  static_cast<mozilla::WidgetInputEvent*>(aResult)) &&
        ReadParam(aMsg, aIter, &keyNameIndex) &&
        ReadParam(aMsg, aIter, &codeNameIndex) &&
        ReadParam(aMsg, aIter, &aResult->mKeyValue) &&
        ReadParam(aMsg, aIter, &aResult->mCodeValue) &&
        ReadParam(aMsg, aIter, &aResult->mKeyCode) &&
        ReadParam(aMsg, aIter, &aResult->mCharCode) &&
        ReadParam(aMsg, aIter, &aResult->mPseudoCharCode) &&
        ReadParam(aMsg, aIter, &aResult->mAlternativeCharCodes) &&
        ReadParam(aMsg, aIter, &aResult->mIsRepeat) &&
        ReadParam(aMsg, aIter, &aResult->mLocation) &&
        ReadParam(aMsg, aIter, &aResult->mUniqueId) &&
        ReadParam(aMsg, aIter, &aResult->mIsSynthesizedByTIP) &&
        ReadParam(aMsg, aIter, &aResult->mMaybeSkippableInRemoteProcess) &&
        ReadParam(aMsg, aIter, &aResult->mEditCommandsForSingleLineEditor) &&
        ReadParam(aMsg, aIter, &aResult->mEditCommandsForMultiLineEditor) &&
        ReadParam(aMsg, aIter, &aResult->mEditCommandsForRichTextEditor) &&
        ReadParam(aMsg, aIter,
                  &aResult->mEditCommandsForSingleLineEditorInitialized) &&
        ReadParam(aMsg, aIter,
                  &aResult->mEditCommandsForMultiLineEditorInitialized) &&
        ReadParam(aMsg, aIter,
                  &aResult->mEditCommandsForRichTextEditorInitialized)) {
      aResult->mKeyNameIndex = static_cast<mozilla::KeyNameIndex>(keyNameIndex);
      aResult->mCodeNameIndex =
          static_cast<mozilla::CodeNameIndex>(codeNameIndex);
      aResult->mNativeKeyEvent = nullptr;
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<mozilla::TextRangeStyle> {
  typedef mozilla::TextRangeStyle paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mDefinedStyles);
    WriteParam(aMsg, static_cast<mozilla::TextRangeStyle::LineStyleType>(
                         aParam.mLineStyle));
    WriteParam(aMsg, aParam.mIsBoldLine);
    WriteParam(aMsg, aParam.mForegroundColor);
    WriteParam(aMsg, aParam.mBackgroundColor);
    WriteParam(aMsg, aParam.mUnderlineColor);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    mozilla::TextRangeStyle::LineStyleType lineStyle;
    if (!ReadParam(aMsg, aIter, &aResult->mDefinedStyles) ||
        !ReadParam(aMsg, aIter, &lineStyle) ||
        !ReadParam(aMsg, aIter, &aResult->mIsBoldLine) ||
        !ReadParam(aMsg, aIter, &aResult->mForegroundColor) ||
        !ReadParam(aMsg, aIter, &aResult->mBackgroundColor) ||
        !ReadParam(aMsg, aIter, &aResult->mUnderlineColor)) {
      return false;
    }
    aResult->mLineStyle = mozilla::TextRangeStyle::ToLineStyle(lineStyle);
    return true;
  }
};

template <>
struct ParamTraits<mozilla::TextRange> {
  typedef mozilla::TextRange paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mStartOffset);
    WriteParam(aMsg, aParam.mEndOffset);
    WriteParam(aMsg, mozilla::ToRawTextRangeType(aParam.mRangeType));
    WriteParam(aMsg, aParam.mRangeStyle);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    mozilla::RawTextRangeType rawTextRangeType;
    if (ReadParam(aMsg, aIter, &aResult->mStartOffset) &&
        ReadParam(aMsg, aIter, &aResult->mEndOffset) &&
        ReadParam(aMsg, aIter, &rawTextRangeType) &&
        ReadParam(aMsg, aIter, &aResult->mRangeStyle)) {
      aResult->mRangeType = mozilla::ToTextRangeType(rawTextRangeType);
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<mozilla::TextRangeArray> {
  typedef mozilla::TextRangeArray paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.Length());
    for (uint32_t index = 0; index < aParam.Length(); index++) {
      WriteParam(aMsg, aParam[index]);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    paramType::size_type length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }
    for (uint32_t index = 0; index < length; index++) {
      mozilla::TextRange textRange;
      if (!ReadParam(aMsg, aIter, &textRange)) {
        aResult->Clear();
        return false;
      }
      aResult->AppendElement(textRange);
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::WidgetCompositionEvent> {
  typedef mozilla::WidgetCompositionEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::WidgetGUIEvent&>(aParam));
    WriteParam(aMsg, aParam.mData);
    WriteParam(aMsg, aParam.mNativeIMEContext);
    bool hasRanges = !!aParam.mRanges;
    WriteParam(aMsg, hasRanges);
    if (hasRanges) {
      WriteParam(aMsg, *aParam.mRanges.get());
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    bool hasRanges;
    if (!ReadParam(aMsg, aIter,
                   static_cast<mozilla::WidgetGUIEvent*>(aResult)) ||
        !ReadParam(aMsg, aIter, &aResult->mData) ||
        !ReadParam(aMsg, aIter, &aResult->mNativeIMEContext) ||
        !ReadParam(aMsg, aIter, &hasRanges)) {
      return false;
    }

    if (!hasRanges) {
      aResult->mRanges = nullptr;
    } else {
      aResult->mRanges = new mozilla::TextRangeArray();
      if (!ReadParam(aMsg, aIter, aResult->mRanges.get())) {
        return false;
      }
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::FontRange> {
  typedef mozilla::FontRange paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mStartOffset);
    WriteParam(aMsg, aParam.mFontName);
    WriteParam(aMsg, aParam.mFontSize);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mStartOffset) &&
           ReadParam(aMsg, aIter, &aResult->mFontName) &&
           ReadParam(aMsg, aIter, &aResult->mFontSize);
  }
};

template <>
struct ParamTraits<mozilla::WidgetSelectionEvent> {
  typedef mozilla::WidgetSelectionEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::WidgetGUIEvent&>(aParam));
    WriteParam(aMsg, aParam.mOffset);
    WriteParam(aMsg, aParam.mLength);
    WriteParam(aMsg, aParam.mReversed);
    WriteParam(aMsg, aParam.mExpandToClusterBoundary);
    WriteParam(aMsg, aParam.mSucceeded);
    WriteParam(aMsg, aParam.mUseNativeLineBreak);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mOffset) &&
           ReadParam(aMsg, aIter, &aResult->mLength) &&
           ReadParam(aMsg, aIter, &aResult->mReversed) &&
           ReadParam(aMsg, aIter, &aResult->mExpandToClusterBoundary) &&
           ReadParam(aMsg, aIter, &aResult->mSucceeded) &&
           ReadParam(aMsg, aIter, &aResult->mUseNativeLineBreak);
  }
};

template <>
struct ParamTraits<mozilla::widget::IMENotificationRequests> {
  typedef mozilla::widget::IMENotificationRequests paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mWantUpdates);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mWantUpdates);
  }
};

template <>
struct ParamTraits<mozilla::widget::NativeIMEContext> {
  typedef mozilla::widget::NativeIMEContext paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mRawNativeIMEContext);
    WriteParam(aMsg, aParam.mOriginProcessID);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mRawNativeIMEContext) &&
           ReadParam(aMsg, aIter, &aResult->mOriginProcessID);
  }
};

template <>
struct ParamTraits<mozilla::widget::IMENotification::SelectionChangeDataBase> {
  typedef mozilla::widget::IMENotification::SelectionChangeDataBase paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    MOZ_RELEASE_ASSERT(aParam.mString);
    WriteParam(aMsg, aParam.mOffset);
    WriteParam(aMsg, *aParam.mString);
    WriteParam(aMsg, aParam.mWritingMode);
    WriteParam(aMsg, aParam.mReversed);
    WriteParam(aMsg, aParam.mCausedByComposition);
    WriteParam(aMsg, aParam.mCausedBySelectionEvent);
    WriteParam(aMsg, aParam.mOccurredDuringComposition);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    aResult->mString = new nsString();
    return ReadParam(aMsg, aIter, &aResult->mOffset) &&
           ReadParam(aMsg, aIter, aResult->mString) &&
           ReadParam(aMsg, aIter, &aResult->mWritingMode) &&
           ReadParam(aMsg, aIter, &aResult->mReversed) &&
           ReadParam(aMsg, aIter, &aResult->mCausedByComposition) &&
           ReadParam(aMsg, aIter, &aResult->mCausedBySelectionEvent) &&
           ReadParam(aMsg, aIter, &aResult->mOccurredDuringComposition);
  }
};

template <>
struct ParamTraits<mozilla::widget::IMENotification::TextChangeDataBase> {
  typedef mozilla::widget::IMENotification::TextChangeDataBase paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mStartOffset);
    WriteParam(aMsg, aParam.mRemovedEndOffset);
    WriteParam(aMsg, aParam.mAddedEndOffset);
    WriteParam(aMsg, aParam.mCausedOnlyByComposition);
    WriteParam(aMsg, aParam.mIncludingChangesDuringComposition);
    WriteParam(aMsg, aParam.mIncludingChangesWithoutComposition);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mStartOffset) &&
           ReadParam(aMsg, aIter, &aResult->mRemovedEndOffset) &&
           ReadParam(aMsg, aIter, &aResult->mAddedEndOffset) &&
           ReadParam(aMsg, aIter, &aResult->mCausedOnlyByComposition) &&
           ReadParam(aMsg, aIter,
                     &aResult->mIncludingChangesDuringComposition) &&
           ReadParam(aMsg, aIter,
                     &aResult->mIncludingChangesWithoutComposition);
  }
};

template <>
struct ParamTraits<mozilla::widget::IMENotification::MouseButtonEventData> {
  typedef mozilla::widget::IMENotification::MouseButtonEventData paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mEventMessage);
    WriteParam(aMsg, aParam.mOffset);
    WriteParam(aMsg, aParam.mCursorPos);
    WriteParam(aMsg, aParam.mCharRect);
    WriteParam(aMsg, aParam.mButton);
    WriteParam(aMsg, aParam.mButtons);
    WriteParam(aMsg, aParam.mModifiers);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mEventMessage) &&
           ReadParam(aMsg, aIter, &aResult->mOffset) &&
           ReadParam(aMsg, aIter, &aResult->mCursorPos) &&
           ReadParam(aMsg, aIter, &aResult->mCharRect) &&
           ReadParam(aMsg, aIter, &aResult->mButton) &&
           ReadParam(aMsg, aIter, &aResult->mButtons) &&
           ReadParam(aMsg, aIter, &aResult->mModifiers);
  }
};

template <>
struct ParamTraits<mozilla::widget::IMENotification> {
  typedef mozilla::widget::IMENotification paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg,
               static_cast<mozilla::widget::IMEMessageType>(aParam.mMessage));
    switch (aParam.mMessage) {
      case mozilla::widget::NOTIFY_IME_OF_SELECTION_CHANGE:
        WriteParam(aMsg, aParam.mSelectionChangeData);
        return;
      case mozilla::widget::NOTIFY_IME_OF_TEXT_CHANGE:
        WriteParam(aMsg, aParam.mTextChangeData);
        return;
      case mozilla::widget::NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
        WriteParam(aMsg, aParam.mMouseButtonEventData);
        return;
      default:
        return;
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    mozilla::widget::IMEMessageType IMEMessage = 0;
    if (!ReadParam(aMsg, aIter, &IMEMessage)) {
      return false;
    }
    aResult->mMessage = static_cast<mozilla::widget::IMEMessage>(IMEMessage);
    switch (aResult->mMessage) {
      case mozilla::widget::NOTIFY_IME_OF_SELECTION_CHANGE:
        return ReadParam(aMsg, aIter, &aResult->mSelectionChangeData);
      case mozilla::widget::NOTIFY_IME_OF_TEXT_CHANGE:
        return ReadParam(aMsg, aIter, &aResult->mTextChangeData);
      case mozilla::widget::NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
        return ReadParam(aMsg, aIter, &aResult->mMouseButtonEventData);
      default:
        return true;
    }
  }
};

template <>
struct ParamTraits<mozilla::widget::IMEEnabled>
    : ContiguousEnumSerializer<mozilla::widget::IMEEnabled,
                               mozilla::widget::IMEEnabled::Disabled,
                               mozilla::widget::IMEEnabled::Unknown> {};

template <>
struct ParamTraits<mozilla::widget::IMEState::Open>
    : ContiguousEnumSerializerInclusive<
          mozilla::widget::IMEState::Open,
          mozilla::widget::IMEState::Open::OPEN_STATE_NOT_SUPPORTED,
          mozilla::widget::IMEState::Open::CLOSED> {};

template <>
struct ParamTraits<mozilla::widget::IMEState> {
  typedef mozilla::widget::IMEState paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mEnabled);
    WriteParam(aMsg, aParam.mOpen);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mEnabled) &&
           ReadParam(aMsg, aIter, &aResult->mOpen);
  }
};

template <>
struct ParamTraits<mozilla::widget::InputContext::Origin>
    : ContiguousEnumSerializerInclusive<
          mozilla::widget::InputContext::Origin,
          mozilla::widget::InputContext::Origin::ORIGIN_MAIN,
          mozilla::widget::InputContext::Origin::ORIGIN_CONTENT> {};

template <>
struct ParamTraits<mozilla::widget::InputContext> {
  typedef mozilla::widget::InputContext paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mIMEState);
    WriteParam(aMsg, aParam.mHTMLInputType);
    WriteParam(aMsg, aParam.mHTMLInputInputmode);
    WriteParam(aMsg, aParam.mActionHint);
    WriteParam(aMsg, aParam.mAutocapitalize);
    WriteParam(aMsg, aParam.mOrigin);
    WriteParam(aMsg, aParam.mMayBeIMEUnaware);
    WriteParam(aMsg, aParam.mHasHandledUserInput);
    WriteParam(aMsg, aParam.mInPrivateBrowsing);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mIMEState) &&
           ReadParam(aMsg, aIter, &aResult->mHTMLInputType) &&
           ReadParam(aMsg, aIter, &aResult->mHTMLInputInputmode) &&
           ReadParam(aMsg, aIter, &aResult->mActionHint) &&
           ReadParam(aMsg, aIter, &aResult->mAutocapitalize) &&
           ReadParam(aMsg, aIter, &aResult->mOrigin) &&
           ReadParam(aMsg, aIter, &aResult->mMayBeIMEUnaware) &&
           ReadParam(aMsg, aIter, &aResult->mHasHandledUserInput) &&
           ReadParam(aMsg, aIter, &aResult->mInPrivateBrowsing);
  }
};

template <>
struct ParamTraits<mozilla::widget::InputContextAction::Cause>
    : ContiguousEnumSerializerInclusive<
          mozilla::widget::InputContextAction::Cause,
          mozilla::widget::InputContextAction::Cause::CAUSE_UNKNOWN,
          mozilla::widget::InputContextAction::Cause::
              CAUSE_UNKNOWN_DURING_KEYBOARD_INPUT> {};

template <>
struct ParamTraits<mozilla::widget::InputContextAction::FocusChange>
    : ContiguousEnumSerializerInclusive<
          mozilla::widget::InputContextAction::FocusChange,
          mozilla::widget::InputContextAction::FocusChange::FOCUS_NOT_CHANGED,
          mozilla::widget::InputContextAction::FocusChange::WIDGET_CREATED> {};

template <>
struct ParamTraits<mozilla::widget::InputContextAction> {
  typedef mozilla::widget::InputContextAction paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mCause);
    WriteParam(aMsg, aParam.mFocusChange);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mCause) &&
           ReadParam(aMsg, aIter, &aResult->mFocusChange);
  }
};

template <>
struct ParamTraits<mozilla::WritingMode> {
  typedef mozilla::WritingMode paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mWritingMode.bits);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mWritingMode.bits);
  }
};

template <>
struct ParamTraits<mozilla::ContentCache::Selection> {
  typedef mozilla::ContentCache::Selection paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mAnchor);
    WriteParam(aMsg, aParam.mFocus);
    WriteParam(aMsg, aParam.mWritingMode);
    WriteParam(aMsg, aParam.mAnchorCharRects[0]);
    WriteParam(aMsg, aParam.mAnchorCharRects[1]);
    WriteParam(aMsg, aParam.mFocusCharRects[0]);
    WriteParam(aMsg, aParam.mFocusCharRects[1]);
    WriteParam(aMsg, aParam.mRect);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mAnchor) &&
           ReadParam(aMsg, aIter, &aResult->mFocus) &&
           ReadParam(aMsg, aIter, &aResult->mWritingMode) &&
           ReadParam(aMsg, aIter, &aResult->mAnchorCharRects[0]) &&
           ReadParam(aMsg, aIter, &aResult->mAnchorCharRects[1]) &&
           ReadParam(aMsg, aIter, &aResult->mFocusCharRects[0]) &&
           ReadParam(aMsg, aIter, &aResult->mFocusCharRects[1]) &&
           ReadParam(aMsg, aIter, &aResult->mRect);
  }
};

template <>
struct ParamTraits<mozilla::ContentCache::Caret> {
  typedef mozilla::ContentCache::Caret paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mOffset);
    WriteParam(aMsg, aParam.mRect);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mOffset) &&
           ReadParam(aMsg, aIter, &aResult->mRect);
  }
};

template <>
struct ParamTraits<mozilla::ContentCache::TextRectArray> {
  typedef mozilla::ContentCache::TextRectArray paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mStart);
    WriteParam(aMsg, aParam.mRects);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mStart) &&
           ReadParam(aMsg, aIter, &aResult->mRects);
  }
};

template <>
struct ParamTraits<mozilla::ContentCache> {
  typedef mozilla::ContentCache paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mCompositionStart);
    WriteParam(aMsg, aParam.mText);
    WriteParam(aMsg, aParam.mSelection);
    WriteParam(aMsg, aParam.mFirstCharRect);
    WriteParam(aMsg, aParam.mCaret);
    WriteParam(aMsg, aParam.mTextRectArray);
    WriteParam(aMsg, aParam.mLastCommitStringTextRectArray);
    WriteParam(aMsg, aParam.mEditorRect);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mCompositionStart) &&
           ReadParam(aMsg, aIter, &aResult->mText) &&
           ReadParam(aMsg, aIter, &aResult->mSelection) &&
           ReadParam(aMsg, aIter, &aResult->mFirstCharRect) &&
           ReadParam(aMsg, aIter, &aResult->mCaret) &&
           ReadParam(aMsg, aIter, &aResult->mTextRectArray) &&
           ReadParam(aMsg, aIter, &aResult->mLastCommitStringTextRectArray) &&
           ReadParam(aMsg, aIter, &aResult->mEditorRect);
  }
};

template <>
struct ParamTraits<mozilla::widget::CandidateWindowPosition> {
  typedef mozilla::widget::CandidateWindowPosition paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mPoint);
    WriteParam(aMsg, aParam.mRect);
    WriteParam(aMsg, aParam.mExcludeRect);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mPoint) &&
           ReadParam(aMsg, aIter, &aResult->mRect) &&
           ReadParam(aMsg, aIter, &aResult->mExcludeRect);
  }
};

// InputData.h

template <>
struct ParamTraits<mozilla::InputType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::InputType, mozilla::InputType::MULTITOUCH_INPUT,
          mozilla::kHighestInputType> {};

template <>
struct ParamTraits<mozilla::InputData> {
  typedef mozilla::InputData paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mInputType);
    WriteParam(aMsg, aParam.mTime);
    WriteParam(aMsg, aParam.mTimeStamp);
    WriteParam(aMsg, aParam.modifiers);
    WriteParam(aMsg, aParam.mFocusSequenceNumber);
    WriteParam(aMsg, aParam.mLayersId);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mInputType) &&
           ReadParam(aMsg, aIter, &aResult->mTime) &&
           ReadParam(aMsg, aIter, &aResult->mTimeStamp) &&
           ReadParam(aMsg, aIter, &aResult->modifiers) &&
           ReadParam(aMsg, aIter, &aResult->mFocusSequenceNumber) &&
           ReadParam(aMsg, aIter, &aResult->mLayersId);
  }
};

template <>
struct ParamTraits<mozilla::SingleTouchData::HistoricalTouchData> {
  typedef mozilla::SingleTouchData::HistoricalTouchData paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mTimeStamp);
    WriteParam(aMsg, aParam.mScreenPoint);
    WriteParam(aMsg, aParam.mLocalScreenPoint);
    WriteParam(aMsg, aParam.mRadius);
    WriteParam(aMsg, aParam.mRotationAngle);
    WriteParam(aMsg, aParam.mForce);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mTimeStamp) &&
            ReadParam(aMsg, aIter, &aResult->mScreenPoint) &&
            ReadParam(aMsg, aIter, &aResult->mLocalScreenPoint) &&
            ReadParam(aMsg, aIter, &aResult->mRadius) &&
            ReadParam(aMsg, aIter, &aResult->mRotationAngle) &&
            ReadParam(aMsg, aIter, &aResult->mForce));
  }
};

template <>
struct ParamTraits<mozilla::SingleTouchData> {
  using paramType = mozilla::SingleTouchData;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mHistoricalData);
    WriteParam(aMsg, aParam.mIdentifier);
    WriteParam(aMsg, aParam.mScreenPoint);
    WriteParam(aMsg, aParam.mLocalScreenPoint);
    WriteParam(aMsg, aParam.mRadius);
    WriteParam(aMsg, aParam.mRotationAngle);
    WriteParam(aMsg, aParam.mForce);
    WriteParam(aMsg, aParam.mTiltX);
    WriteParam(aMsg, aParam.mTiltY);
    WriteParam(aMsg, aParam.mTwist);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mHistoricalData) &&
            ReadParam(aMsg, aIter, &aResult->mIdentifier) &&
            ReadParam(aMsg, aIter, &aResult->mScreenPoint) &&
            ReadParam(aMsg, aIter, &aResult->mLocalScreenPoint) &&
            ReadParam(aMsg, aIter, &aResult->mRadius) &&
            ReadParam(aMsg, aIter, &aResult->mRotationAngle) &&
            ReadParam(aMsg, aIter, &aResult->mForce) &&
            ReadParam(aMsg, aIter, &aResult->mTiltX) &&
            ReadParam(aMsg, aIter, &aResult->mTiltY) &&
            ReadParam(aMsg, aIter, &aResult->mTwist));
  }
};

template <>
struct ParamTraits<mozilla::MultiTouchInput::MultiTouchType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::MultiTouchInput::MultiTouchType,
          mozilla::MultiTouchInput::MultiTouchType::MULTITOUCH_START,
          mozilla::MultiTouchInput::sHighestMultiTouchType> {};

template <>
struct ParamTraits<mozilla::MultiTouchInput> {
  using paramType = mozilla::MultiTouchInput;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mTouches);
    WriteParam(aMsg, aParam.mHandledByAPZ);
    WriteParam(aMsg, aParam.mScreenOffset);
    WriteParam(aMsg, aParam.mButton);
    WriteParam(aMsg, aParam.mButtons);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mTouches) &&
           ReadParam(aMsg, aIter, &aResult->mHandledByAPZ) &&
           ReadParam(aMsg, aIter, &aResult->mScreenOffset) &&
           ReadParam(aMsg, aIter, &aResult->mButton) &&
           ReadParam(aMsg, aIter, &aResult->mButtons);
  }
};

template <>
struct ParamTraits<mozilla::MouseInput::MouseType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::MouseInput::MouseType,
          mozilla::MouseInput::MouseType::MOUSE_NONE,
          mozilla::MouseInput::sHighestMouseType> {};

template <>
struct ParamTraits<mozilla::MouseInput::ButtonType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::MouseInput::ButtonType,
          mozilla::MouseInput::ButtonType::PRIMARY_BUTTON,
          mozilla::MouseInput::sHighestButtonType> {};

template <>
struct ParamTraits<mozilla::MouseInput> {
  typedef mozilla::MouseInput paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aMsg, aParam.mButtonType);
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mInputSource);
    WriteParam(aMsg, aParam.mButtons);
    WriteParam(aMsg, aParam.mOrigin);
    WriteParam(aMsg, aParam.mLocalOrigin);
    WriteParam(aMsg, aParam.mHandledByAPZ);
    WriteParam(aMsg, aParam.mPreventClickEvent);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mButtonType) &&
           ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mInputSource) &&
           ReadParam(aMsg, aIter, &aResult->mButtons) &&
           ReadParam(aMsg, aIter, &aResult->mOrigin) &&
           ReadParam(aMsg, aIter, &aResult->mLocalOrigin) &&
           ReadParam(aMsg, aIter, &aResult->mHandledByAPZ) &&
           ReadParam(aMsg, aIter, &aResult->mPreventClickEvent);
  }
};

template <>
struct ParamTraits<mozilla::PanGestureInput::PanGestureType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::PanGestureInput::PanGestureType,
          mozilla::PanGestureInput::PanGestureType::PANGESTURE_MAYSTART,
          mozilla::PanGestureInput::sHighestPanGestureType> {};

template <>
struct ParamTraits<mozilla::PanGestureInput::PanDeltaType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::PanGestureInput::PanDeltaType,
          mozilla::PanGestureInput::PanDeltaType::PANDELTA_PAGE,
          mozilla::PanGestureInput::sHighestPanDeltaType> {};

template <>
struct ParamTraits<mozilla::PanGestureInput>
    : BitfieldHelper<mozilla::PanGestureInput> {
  typedef mozilla::PanGestureInput paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mPanStartPoint);
    WriteParam(aMsg, aParam.mPanDisplacement);
    WriteParam(aMsg, aParam.mLocalPanStartPoint);
    WriteParam(aMsg, aParam.mLocalPanDisplacement);
    WriteParam(aMsg, aParam.mLineOrPageDeltaX);
    WriteParam(aMsg, aParam.mLineOrPageDeltaY);
    WriteParam(aMsg, aParam.mUserDeltaMultiplierX);
    WriteParam(aMsg, aParam.mUserDeltaMultiplierY);
    WriteParam(aMsg, aParam.mDeltaType);
    WriteParam(aMsg, aParam.mHandledByAPZ);
    WriteParam(aMsg, aParam.mFollowedByMomentum);
    WriteParam(
        aMsg,
        aParam
            .mRequiresContentResponseIfCannotScrollHorizontallyInStartDirection);
    WriteParam(aMsg, aParam.mOverscrollBehaviorAllowsSwipe);
    WriteParam(aMsg, aParam.mSimulateMomentum);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mPanStartPoint) &&
           ReadParam(aMsg, aIter, &aResult->mPanDisplacement) &&
           ReadParam(aMsg, aIter, &aResult->mLocalPanStartPoint) &&
           ReadParam(aMsg, aIter, &aResult->mLocalPanDisplacement) &&
           ReadParam(aMsg, aIter, &aResult->mLineOrPageDeltaX) &&
           ReadParam(aMsg, aIter, &aResult->mLineOrPageDeltaY) &&
           ReadParam(aMsg, aIter, &aResult->mUserDeltaMultiplierX) &&
           ReadParam(aMsg, aIter, &aResult->mUserDeltaMultiplierY) &&
           ReadParam(aMsg, aIter, &aResult->mDeltaType) &&
           ReadBoolForBitfield(aMsg, aIter, aResult,
                               &paramType::SetHandledByAPZ) &&
           ReadBoolForBitfield(aMsg, aIter, aResult,
                               &paramType::SetFollowedByMomentum) &&
           ReadBoolForBitfield(
               aMsg, aIter, aResult,
               &paramType::
                   SetRequiresContentResponseIfCannotScrollHorizontallyInStartDirection) &&
           ReadBoolForBitfield(aMsg, aIter, aResult,
                               &paramType::SetOverscrollBehaviorAllowsSwipe) &&
           ReadBoolForBitfield(aMsg, aIter, aResult,
                               &paramType::SetSimulateMomentum);
  }
};

template <>
struct ParamTraits<mozilla::PinchGestureInput::PinchGestureType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::PinchGestureInput::PinchGestureType,
          mozilla::PinchGestureInput::PinchGestureType::PINCHGESTURE_START,
          mozilla::PinchGestureInput::sHighestPinchGestureType> {};

template <>
struct ParamTraits<mozilla::PinchGestureInput::PinchGestureSource>
    : public ContiguousEnumSerializerInclusive<
          mozilla::PinchGestureInput::PinchGestureSource,
          // Set the min to TOUCH, to ensure UNKNOWN is never sent over IPC
          mozilla::PinchGestureInput::PinchGestureSource::TOUCH,
          mozilla::PinchGestureInput::sHighestPinchGestureSource> {};

template <>
struct ParamTraits<mozilla::PinchGestureInput> {
  typedef mozilla::PinchGestureInput paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mSource);
    WriteParam(aMsg, aParam.mScreenOffset);
    WriteParam(aMsg, aParam.mFocusPoint);
    WriteParam(aMsg, aParam.mLocalFocusPoint);
    WriteParam(aMsg, aParam.mCurrentSpan);
    WriteParam(aMsg, aParam.mPreviousSpan);
    WriteParam(aMsg, aParam.mLineOrPageDeltaY);
    WriteParam(aMsg, aParam.mHandledByAPZ);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mSource) &&
           ReadParam(aMsg, aIter, &aResult->mScreenOffset) &&
           ReadParam(aMsg, aIter, &aResult->mFocusPoint) &&
           ReadParam(aMsg, aIter, &aResult->mLocalFocusPoint) &&
           ReadParam(aMsg, aIter, &aResult->mCurrentSpan) &&
           ReadParam(aMsg, aIter, &aResult->mPreviousSpan) &&
           ReadParam(aMsg, aIter, &aResult->mLineOrPageDeltaY) &&
           ReadParam(aMsg, aIter, &aResult->mHandledByAPZ);
  }
};

template <>
struct ParamTraits<mozilla::TapGestureInput::TapGestureType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::TapGestureInput::TapGestureType,
          mozilla::TapGestureInput::TapGestureType::TAPGESTURE_LONG,
          mozilla::TapGestureInput::sHighestTapGestureType> {};

template <>
struct ParamTraits<mozilla::TapGestureInput> {
  typedef mozilla::TapGestureInput paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mPoint);
    WriteParam(aMsg, aParam.mLocalPoint);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mPoint) &&
           ReadParam(aMsg, aIter, &aResult->mLocalPoint);
  }
};

template <>
struct ParamTraits<mozilla::ScrollWheelInput::ScrollDeltaType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::ScrollWheelInput::ScrollDeltaType,
          mozilla::ScrollWheelInput::ScrollDeltaType::SCROLLDELTA_LINE,
          mozilla::ScrollWheelInput::sHighestScrollDeltaType> {};

template <>
struct ParamTraits<mozilla::ScrollWheelInput::ScrollMode>
    : public ContiguousEnumSerializerInclusive<
          mozilla::ScrollWheelInput::ScrollMode,
          mozilla::ScrollWheelInput::ScrollMode::SCROLLMODE_INSTANT,
          mozilla::ScrollWheelInput::sHighestScrollMode> {};

template <>
struct ParamTraits<mozilla::WheelDeltaAdjustmentStrategy>
    : public ContiguousEnumSerializer<
          mozilla::WheelDeltaAdjustmentStrategy,
          mozilla::WheelDeltaAdjustmentStrategy(0),
          mozilla::WheelDeltaAdjustmentStrategy::eSentinel> {};

template <>
struct ParamTraits<mozilla::layers::APZWheelAction>
    : public ContiguousEnumSerializerInclusive<
          mozilla::layers::APZWheelAction,
          mozilla::layers::APZWheelAction::Scroll,
          mozilla::layers::kHighestAPZWheelAction> {};

template <>
struct ParamTraits<mozilla::ScrollWheelInput> {
  typedef mozilla::ScrollWheelInput paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aMsg, aParam.mDeltaType);
    WriteParam(aMsg, aParam.mScrollMode);
    WriteParam(aMsg, aParam.mOrigin);
    WriteParam(aMsg, aParam.mHandledByAPZ);
    WriteParam(aMsg, aParam.mDeltaX);
    WriteParam(aMsg, aParam.mDeltaY);
    WriteParam(aMsg, aParam.mWheelTicksX);
    WriteParam(aMsg, aParam.mWheelTicksY);
    WriteParam(aMsg, aParam.mLocalOrigin);
    WriteParam(aMsg, aParam.mLineOrPageDeltaX);
    WriteParam(aMsg, aParam.mLineOrPageDeltaY);
    WriteParam(aMsg, aParam.mScrollSeriesNumber);
    WriteParam(aMsg, aParam.mUserDeltaMultiplierX);
    WriteParam(aMsg, aParam.mUserDeltaMultiplierY);
    WriteParam(aMsg, aParam.mMayHaveMomentum);
    WriteParam(aMsg, aParam.mIsMomentum);
    WriteParam(aMsg, aParam.mAllowToOverrideSystemScrollSpeed);
    WriteParam(aMsg, aParam.mWheelDeltaAdjustmentStrategy);
    WriteParam(aMsg, aParam.mAPZAction);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mDeltaType) &&
           ReadParam(aMsg, aIter, &aResult->mScrollMode) &&
           ReadParam(aMsg, aIter, &aResult->mOrigin) &&
           ReadParam(aMsg, aIter, &aResult->mHandledByAPZ) &&
           ReadParam(aMsg, aIter, &aResult->mDeltaX) &&
           ReadParam(aMsg, aIter, &aResult->mDeltaY) &&
           ReadParam(aMsg, aIter, &aResult->mWheelTicksX) &&
           ReadParam(aMsg, aIter, &aResult->mWheelTicksY) &&
           ReadParam(aMsg, aIter, &aResult->mLocalOrigin) &&
           ReadParam(aMsg, aIter, &aResult->mLineOrPageDeltaX) &&
           ReadParam(aMsg, aIter, &aResult->mLineOrPageDeltaY) &&
           ReadParam(aMsg, aIter, &aResult->mScrollSeriesNumber) &&
           ReadParam(aMsg, aIter, &aResult->mUserDeltaMultiplierX) &&
           ReadParam(aMsg, aIter, &aResult->mUserDeltaMultiplierY) &&
           ReadParam(aMsg, aIter, &aResult->mMayHaveMomentum) &&
           ReadParam(aMsg, aIter, &aResult->mIsMomentum) &&
           ReadParam(aMsg, aIter,
                     &aResult->mAllowToOverrideSystemScrollSpeed) &&
           ReadParam(aMsg, aIter, &aResult->mWheelDeltaAdjustmentStrategy) &&
           ReadParam(aMsg, aIter, &aResult->mAPZAction);
  }
};

template <>
struct ParamTraits<mozilla::KeyboardInput::KeyboardEventType>
    : public ContiguousEnumSerializer<
          mozilla::KeyboardInput::KeyboardEventType,
          mozilla::KeyboardInput::KeyboardEventType::KEY_DOWN,
          mozilla::KeyboardInput::KeyboardEventType::KEY_SENTINEL> {};

template <>
struct ParamTraits<mozilla::KeyboardInput> {
  typedef mozilla::KeyboardInput paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mKeyCode);
    WriteParam(aMsg, aParam.mCharCode);
    WriteParam(aMsg, aParam.mShortcutCandidates);
    WriteParam(aMsg, aParam.mHandledByAPZ);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mKeyCode) &&
           ReadParam(aMsg, aIter, &aResult->mCharCode) &&
           ReadParam(aMsg, aIter, &aResult->mShortcutCandidates) &&
           ReadParam(aMsg, aIter, &aResult->mHandledByAPZ);
  }
};

}  // namespace IPC

#endif  // nsGUIEventIPC_h__
