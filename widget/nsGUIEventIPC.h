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
#include "mozilla/ipc/IPDLParamTraits.h"  // for ReadIPDLParam and WriteIPDLParam
#include "mozilla/ipc/URIUtils.h"         // for IPDLParamTraits<nsIURI*>
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    aWriter->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return aReader->ReadBytesInto(aResult, sizeof(*aResult));
  }
};

template <>
struct ParamTraits<mozilla::WidgetEvent> {
  typedef mozilla::WidgetEvent paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    // Mark the event as posted to another process.
    const_cast<mozilla::WidgetEvent&>(aParam).MarkAsPostedToRemoteProcess();

    WriteParam(aWriter, static_cast<mozilla::EventClassIDType>(aParam.mClass));
    WriteParam(aWriter, aParam.mMessage);
    WriteParam(aWriter, aParam.mRefPoint);
    WriteParam(aWriter, aParam.mFocusSequenceNumber);
    WriteParam(aWriter, aParam.mTimeStamp);
    WriteParam(aWriter, aParam.mFlags);
    WriteParam(aWriter, aParam.mLayersId);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    mozilla::EventClassIDType eventClassID = 0;
    bool ret = ReadParam(aReader, &eventClassID) &&
               ReadParam(aReader, &aResult->mMessage) &&
               ReadParam(aReader, &aResult->mRefPoint) &&
               ReadParam(aReader, &aResult->mFocusSequenceNumber) &&
               ReadParam(aReader, &aResult->mTimeStamp) &&
               ReadParam(aReader, &aResult->mFlags) &&
               ReadParam(aReader, &aResult->mLayersId);
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
struct ParamTraits<mozilla::WidgetGUIEvent> {
  typedef mozilla::WidgetGUIEvent paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::WidgetEvent&>(aParam));
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, static_cast<mozilla::WidgetEvent*>(aResult));
  }
};

template <>
struct ParamTraits<mozilla::WidgetInputEvent> {
  typedef mozilla::WidgetInputEvent paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::WidgetGUIEvent&>(aParam));
    WriteParam(aWriter, aParam.mModifiers);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, static_cast<mozilla::WidgetGUIEvent*>(aResult)) &&
           ReadParam(aReader, &aResult->mModifiers);
  }
};

template <>
struct ParamTraits<mozilla::WidgetMouseEventBase> {
  typedef mozilla::WidgetMouseEventBase paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::WidgetInputEvent&>(aParam));
    WriteParam(aWriter, aParam.mButton);
    WriteParam(aWriter, aParam.mButtons);
    WriteParam(aWriter, aParam.mPressure);
    WriteParam(aWriter, aParam.mInputSource);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader,
                     static_cast<mozilla::WidgetInputEvent*>(aResult)) &&
           ReadParam(aReader, &aResult->mButton) &&
           ReadParam(aReader, &aResult->mButtons) &&
           ReadParam(aReader, &aResult->mPressure) &&
           ReadParam(aReader, &aResult->mInputSource);
  }
};

template <>
struct ParamTraits<mozilla::WidgetWheelEvent> {
  typedef mozilla::WidgetWheelEvent paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter,
               static_cast<const mozilla::WidgetMouseEventBase&>(aParam));
    WriteParam(aWriter, aParam.mDeltaX);
    WriteParam(aWriter, aParam.mDeltaY);
    WriteParam(aWriter, aParam.mDeltaZ);
    WriteParam(aWriter, aParam.mDeltaMode);
    WriteParam(aWriter, aParam.mWheelTicksX);
    WriteParam(aWriter, aParam.mWheelTicksY);
    WriteParam(aWriter, aParam.mCustomizedByUserPrefs);
    WriteParam(aWriter, aParam.mMayHaveMomentum);
    WriteParam(aWriter, aParam.mIsMomentum);
    WriteParam(aWriter, aParam.mIsNoLineOrPageDelta);
    WriteParam(aWriter, aParam.mLineOrPageDeltaX);
    WriteParam(aWriter, aParam.mLineOrPageDeltaY);
    WriteParam(aWriter, static_cast<uint8_t>(aParam.mScrollType));
    WriteParam(aWriter, aParam.mOverflowDeltaX);
    WriteParam(aWriter, aParam.mOverflowDeltaY);
    WriteParam(aWriter, aParam.mViewPortIsOverscrolled);
    WriteParam(aWriter, aParam.mCanTriggerSwipe);
    WriteParam(aWriter, aParam.mAllowToOverrideSystemScrollSpeed);
    WriteParam(aWriter, aParam.mDeltaValuesHorizontalizedForDefaultHandler);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    uint8_t scrollType = 0;
    bool rv = ReadParam(aReader,
                        static_cast<mozilla::WidgetMouseEventBase*>(aResult)) &&
              ReadParam(aReader, &aResult->mDeltaX) &&
              ReadParam(aReader, &aResult->mDeltaY) &&
              ReadParam(aReader, &aResult->mDeltaZ) &&
              ReadParam(aReader, &aResult->mDeltaMode) &&
              ReadParam(aReader, &aResult->mWheelTicksX) &&
              ReadParam(aReader, &aResult->mWheelTicksY) &&
              ReadParam(aReader, &aResult->mCustomizedByUserPrefs) &&
              ReadParam(aReader, &aResult->mMayHaveMomentum) &&
              ReadParam(aReader, &aResult->mIsMomentum) &&
              ReadParam(aReader, &aResult->mIsNoLineOrPageDelta) &&
              ReadParam(aReader, &aResult->mLineOrPageDeltaX) &&
              ReadParam(aReader, &aResult->mLineOrPageDeltaY) &&
              ReadParam(aReader, &scrollType) &&
              ReadParam(aReader, &aResult->mOverflowDeltaX) &&
              ReadParam(aReader, &aResult->mOverflowDeltaY) &&
              ReadParam(aReader, &aResult->mViewPortIsOverscrolled) &&
              ReadParam(aReader, &aResult->mCanTriggerSwipe) &&
              ReadParam(aReader, &aResult->mAllowToOverrideSystemScrollSpeed) &&
              ReadParam(aReader,
                        &aResult->mDeltaValuesHorizontalizedForDefaultHandler);
    aResult->mScrollType =
        static_cast<mozilla::WidgetWheelEvent::ScrollType>(scrollType);
    return rv;
  }
};

template <>
struct ParamTraits<mozilla::WidgetPointerHelper> {
  typedef mozilla::WidgetPointerHelper paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.pointerId);
    WriteParam(aWriter, aParam.tiltX);
    WriteParam(aWriter, aParam.tiltY);
    WriteParam(aWriter, aParam.twist);
    WriteParam(aWriter, aParam.tangentialPressure);
    // We don't serialize convertToPointer since it's temporarily variable and
    // should be reset to default.
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    bool rv;
    rv = ReadParam(aReader, &aResult->pointerId) &&
         ReadParam(aReader, &aResult->tiltX) &&
         ReadParam(aReader, &aResult->tiltY) &&
         ReadParam(aReader, &aResult->twist) &&
         ReadParam(aReader, &aResult->tangentialPressure);
    return rv;
  }
};

template <>
struct ParamTraits<mozilla::WidgetMouseEvent> {
  typedef mozilla::WidgetMouseEvent paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter,
               static_cast<const mozilla::WidgetMouseEventBase&>(aParam));
    WriteParam(aWriter,
               static_cast<const mozilla::WidgetPointerHelper&>(aParam));
    WriteParam(aWriter, aParam.mIgnoreRootScrollFrame);
    WriteParam(aWriter, aParam.mClickEventPrevented);
    WriteParam(aWriter, static_cast<paramType::ReasonType>(aParam.mReason));
    WriteParam(aWriter, static_cast<paramType::ContextMenuTriggerType>(
                            aParam.mContextMenuTrigger));
    WriteParam(aWriter, aParam.mExitFrom.isSome());
    if (aParam.mExitFrom.isSome()) {
      WriteParam(aWriter, static_cast<paramType::ExitFromType>(
                              aParam.mExitFrom.value()));
    }
    WriteParam(aWriter, aParam.mClickCount);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    bool rv;
    paramType::ReasonType reason = 0;
    paramType::ContextMenuTriggerType contextMenuTrigger = 0;
    bool hasExitFrom = false;
    rv = ReadParam(aReader,
                   static_cast<mozilla::WidgetMouseEventBase*>(aResult)) &&
         ReadParam(aReader,
                   static_cast<mozilla::WidgetPointerHelper*>(aResult)) &&
         ReadParam(aReader, &aResult->mIgnoreRootScrollFrame) &&
         ReadParam(aReader, &aResult->mClickEventPrevented) &&
         ReadParam(aReader, &reason) && ReadParam(aReader, &contextMenuTrigger);
    aResult->mReason = static_cast<paramType::Reason>(reason);
    aResult->mContextMenuTrigger =
        static_cast<paramType::ContextMenuTrigger>(contextMenuTrigger);
    rv = rv && ReadParam(aReader, &hasExitFrom);
    if (hasExitFrom) {
      paramType::ExitFromType exitFrom = 0;
      rv = rv && ReadParam(aReader, &exitFrom);
      aResult->mExitFrom = Some(static_cast<paramType::ExitFrom>(exitFrom));
    }
    rv = rv && ReadParam(aReader, &aResult->mClickCount);
    return rv;
  }
};

template <>
struct ParamTraits<mozilla::WidgetDragEvent> {
  typedef mozilla::WidgetDragEvent paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::WidgetMouseEvent&>(aParam));
    WriteParam(aWriter, aParam.mUserCancelled);
    WriteParam(aWriter, aParam.mDefaultPreventedOnContent);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    bool rv =
        ReadParam(aReader, static_cast<mozilla::WidgetMouseEvent*>(aResult)) &&
        ReadParam(aReader, &aResult->mUserCancelled) &&
        ReadParam(aReader, &aResult->mDefaultPreventedOnContent);
    return rv;
  }
};

template <>
struct ParamTraits<mozilla::WidgetPointerEvent> {
  typedef mozilla::WidgetPointerEvent paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::WidgetMouseEvent&>(aParam));
    WriteParam(aWriter, aParam.mWidth);
    WriteParam(aWriter, aParam.mHeight);
    WriteParam(aWriter, aParam.mIsPrimary);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    bool rv =
        ReadParam(aReader, static_cast<mozilla::WidgetMouseEvent*>(aResult)) &&
        ReadParam(aReader, &aResult->mWidth) &&
        ReadParam(aReader, &aResult->mHeight) &&
        ReadParam(aReader, &aResult->mIsPrimary);
    return rv;
  }
};

template <>
struct ParamTraits<mozilla::WidgetTouchEvent> {
  using paramType = mozilla::WidgetTouchEvent;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::WidgetInputEvent&>(aParam));
    WriteParam(aWriter, aParam.mInputSource);
    WriteParam(aWriter, aParam.mButton);
    WriteParam(aWriter, aParam.mButtons);
    // Sigh, Touch bites us again!  We want to be able to do
    //   WriteParam(aWriter, aParam.mTouches);
    const paramType::TouchArray& touches = aParam.mTouches;
    WriteParam(aWriter, touches.Length());
    for (uint32_t i = 0; i < touches.Length(); ++i) {
      mozilla::dom::Touch* touch = touches[i];
      WriteParam(aWriter, touch->mIdentifier);
      WriteParam(aWriter, touch->mRefPoint);
      WriteParam(aWriter, touch->mRadius);
      WriteParam(aWriter, touch->mRotationAngle);
      WriteParam(aWriter, touch->mForce);
      WriteParam(aWriter, touch->tiltX);
      WriteParam(aWriter, touch->tiltY);
      WriteParam(aWriter, touch->twist);
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    paramType::TouchArray::size_type numTouches;
    if (!ReadParam(aReader, static_cast<mozilla::WidgetInputEvent*>(aResult)) ||
        !ReadParam(aReader, &aResult->mInputSource) ||
        !ReadParam(aReader, &aResult->mButton) ||
        !ReadParam(aReader, &aResult->mButtons) ||
        !ReadParam(aReader, &numTouches)) {
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
      if (!ReadParam(aReader, &identifier) || !ReadParam(aReader, &refPoint) ||
          !ReadParam(aReader, &radius) || !ReadParam(aReader, &rotationAngle) ||
          !ReadParam(aReader, &force) || !ReadParam(aReader, &tiltX) ||
          !ReadParam(aReader, &tiltY) || !ReadParam(aReader, &twist)) {
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mUnshiftedCharCode);
    WriteParam(aWriter, aParam.mShiftedCharCode);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mUnshiftedCharCode) &&
           ReadParam(aReader, &aResult->mShiftedCharCode);
  }
};

template <>
struct ParamTraits<mozilla::ShortcutKeyCandidate> {
  typedef mozilla::ShortcutKeyCandidate paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mCharCode);
    WriteParam(aWriter, aParam.mIgnoreShift);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mCharCode) &&
           ReadParam(aReader, &aResult->mIgnoreShift);
  }
};

template <>
struct ParamTraits<mozilla::WidgetKeyboardEvent> {
  typedef mozilla::WidgetKeyboardEvent paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::WidgetInputEvent&>(aParam));
    WriteParam(aWriter,
               static_cast<mozilla::KeyNameIndexType>(aParam.mKeyNameIndex));
    WriteParam(aWriter,
               static_cast<mozilla::CodeNameIndexType>(aParam.mCodeNameIndex));
    WriteParam(aWriter, aParam.mKeyValue);
    WriteParam(aWriter, aParam.mCodeValue);
    WriteParam(aWriter, aParam.mKeyCode);
    WriteParam(aWriter, aParam.mCharCode);
    WriteParam(aWriter, aParam.mPseudoCharCode);
    WriteParam(aWriter, aParam.mAlternativeCharCodes);
    WriteParam(aWriter, aParam.mIsRepeat);
    WriteParam(aWriter, aParam.mLocation);
    WriteParam(aWriter, aParam.mUniqueId);
    WriteParam(aWriter, aParam.mIsSynthesizedByTIP);
    WriteParam(aWriter, aParam.mMaybeSkippableInRemoteProcess);

    // An OS-specific native event might be attached in |mNativeKeyEvent|,  but
    // that cannot be copied across process boundaries.

    WriteParam(aWriter, aParam.mEditCommandsForSingleLineEditor);
    WriteParam(aWriter, aParam.mEditCommandsForMultiLineEditor);
    WriteParam(aWriter, aParam.mEditCommandsForRichTextEditor);
    WriteParam(aWriter, aParam.mEditCommandsForSingleLineEditorInitialized);
    WriteParam(aWriter, aParam.mEditCommandsForMultiLineEditorInitialized);
    WriteParam(aWriter, aParam.mEditCommandsForRichTextEditorInitialized);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    mozilla::KeyNameIndexType keyNameIndex = 0;
    mozilla::CodeNameIndexType codeNameIndex = 0;
    if (ReadParam(aReader, static_cast<mozilla::WidgetInputEvent*>(aResult)) &&
        ReadParam(aReader, &keyNameIndex) &&
        ReadParam(aReader, &codeNameIndex) &&
        ReadParam(aReader, &aResult->mKeyValue) &&
        ReadParam(aReader, &aResult->mCodeValue) &&
        ReadParam(aReader, &aResult->mKeyCode) &&
        ReadParam(aReader, &aResult->mCharCode) &&
        ReadParam(aReader, &aResult->mPseudoCharCode) &&
        ReadParam(aReader, &aResult->mAlternativeCharCodes) &&
        ReadParam(aReader, &aResult->mIsRepeat) &&
        ReadParam(aReader, &aResult->mLocation) &&
        ReadParam(aReader, &aResult->mUniqueId) &&
        ReadParam(aReader, &aResult->mIsSynthesizedByTIP) &&
        ReadParam(aReader, &aResult->mMaybeSkippableInRemoteProcess) &&
        ReadParam(aReader, &aResult->mEditCommandsForSingleLineEditor) &&
        ReadParam(aReader, &aResult->mEditCommandsForMultiLineEditor) &&
        ReadParam(aReader, &aResult->mEditCommandsForRichTextEditor) &&
        ReadParam(aReader,
                  &aResult->mEditCommandsForSingleLineEditorInitialized) &&
        ReadParam(aReader,
                  &aResult->mEditCommandsForMultiLineEditorInitialized) &&
        ReadParam(aReader,
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mDefinedStyles);
    WriteParam(aWriter, static_cast<mozilla::TextRangeStyle::LineStyleType>(
                            aParam.mLineStyle));
    WriteParam(aWriter, aParam.mIsBoldLine);
    WriteParam(aWriter, aParam.mForegroundColor);
    WriteParam(aWriter, aParam.mBackgroundColor);
    WriteParam(aWriter, aParam.mUnderlineColor);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    mozilla::TextRangeStyle::LineStyleType lineStyle;
    if (!ReadParam(aReader, &aResult->mDefinedStyles) ||
        !ReadParam(aReader, &lineStyle) ||
        !ReadParam(aReader, &aResult->mIsBoldLine) ||
        !ReadParam(aReader, &aResult->mForegroundColor) ||
        !ReadParam(aReader, &aResult->mBackgroundColor) ||
        !ReadParam(aReader, &aResult->mUnderlineColor)) {
      return false;
    }
    aResult->mLineStyle = mozilla::TextRangeStyle::ToLineStyle(lineStyle);
    return true;
  }
};

template <>
struct ParamTraits<mozilla::TextRange> {
  typedef mozilla::TextRange paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mStartOffset);
    WriteParam(aWriter, aParam.mEndOffset);
    WriteParam(aWriter, mozilla::ToRawTextRangeType(aParam.mRangeType));
    WriteParam(aWriter, aParam.mRangeStyle);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    mozilla::RawTextRangeType rawTextRangeType;
    if (ReadParam(aReader, &aResult->mStartOffset) &&
        ReadParam(aReader, &aResult->mEndOffset) &&
        ReadParam(aReader, &rawTextRangeType) &&
        ReadParam(aReader, &aResult->mRangeStyle)) {
      aResult->mRangeType = mozilla::ToTextRangeType(rawTextRangeType);
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<mozilla::TextRangeArray> {
  typedef mozilla::TextRangeArray paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.Length());
    for (uint32_t index = 0; index < aParam.Length(); index++) {
      WriteParam(aWriter, aParam[index]);
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    paramType::size_type length;
    if (!ReadParam(aReader, &length)) {
      return false;
    }
    for (uint32_t index = 0; index < length; index++) {
      mozilla::TextRange textRange;
      if (!ReadParam(aReader, &textRange)) {
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::WidgetGUIEvent&>(aParam));
    WriteParam(aWriter, aParam.mData);
    WriteParam(aWriter, aParam.mNativeIMEContext);
    bool hasRanges = !!aParam.mRanges;
    WriteParam(aWriter, hasRanges);
    if (hasRanges) {
      WriteParam(aWriter, *aParam.mRanges.get());
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    bool hasRanges;
    if (!ReadParam(aReader, static_cast<mozilla::WidgetGUIEvent*>(aResult)) ||
        !ReadParam(aReader, &aResult->mData) ||
        !ReadParam(aReader, &aResult->mNativeIMEContext) ||
        !ReadParam(aReader, &hasRanges)) {
      return false;
    }

    if (!hasRanges) {
      aResult->mRanges = nullptr;
    } else {
      aResult->mRanges = new mozilla::TextRangeArray();
      if (!ReadParam(aReader, aResult->mRanges.get())) {
        return false;
      }
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::FontRange> {
  typedef mozilla::FontRange paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mStartOffset);
    WriteParam(aWriter, aParam.mFontName);
    WriteParam(aWriter, aParam.mFontSize);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mStartOffset) &&
           ReadParam(aReader, &aResult->mFontName) &&
           ReadParam(aReader, &aResult->mFontSize);
  }
};

template <>
struct ParamTraits<mozilla::WidgetSelectionEvent> {
  typedef mozilla::WidgetSelectionEvent paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::WidgetGUIEvent&>(aParam));
    WriteParam(aWriter, aParam.mOffset);
    WriteParam(aWriter, aParam.mLength);
    WriteParam(aWriter, aParam.mReversed);
    WriteParam(aWriter, aParam.mExpandToClusterBoundary);
    WriteParam(aWriter, aParam.mSucceeded);
    WriteParam(aWriter, aParam.mUseNativeLineBreak);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, static_cast<mozilla::WidgetGUIEvent*>(aResult)) &&
           ReadParam(aReader, &aResult->mOffset) &&
           ReadParam(aReader, &aResult->mLength) &&
           ReadParam(aReader, &aResult->mReversed) &&
           ReadParam(aReader, &aResult->mExpandToClusterBoundary) &&
           ReadParam(aReader, &aResult->mSucceeded) &&
           ReadParam(aReader, &aResult->mUseNativeLineBreak);
  }
};

template <>
struct ParamTraits<mozilla::widget::IMENotificationRequests> {
  typedef mozilla::widget::IMENotificationRequests paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mWantUpdates);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mWantUpdates);
  }
};

template <>
struct ParamTraits<mozilla::widget::NativeIMEContext> {
  typedef mozilla::widget::NativeIMEContext paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mRawNativeIMEContext);
    WriteParam(aWriter, aParam.mOriginProcessID);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mRawNativeIMEContext) &&
           ReadParam(aReader, &aResult->mOriginProcessID);
  }
};

template <>
struct ParamTraits<mozilla::widget::IMENotification::SelectionChangeDataBase> {
  typedef mozilla::widget::IMENotification::SelectionChangeDataBase paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    MOZ_RELEASE_ASSERT(aParam.mString);
    WriteParam(aWriter, aParam.mOffset);
    WriteParam(aWriter, *aParam.mString);
    WriteParam(aWriter, aParam.mWritingModeBits);
    WriteParam(aWriter, aParam.mIsInitialized);
    WriteParam(aWriter, aParam.mHasRange);
    WriteParam(aWriter, aParam.mReversed);
    WriteParam(aWriter, aParam.mCausedByComposition);
    WriteParam(aWriter, aParam.mCausedBySelectionEvent);
    WriteParam(aWriter, aParam.mOccurredDuringComposition);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    aResult->mString = new nsString();
    return ReadParam(aReader, &aResult->mOffset) &&
           ReadParam(aReader, aResult->mString) &&
           ReadParam(aReader, &aResult->mWritingModeBits) &&
           ReadParam(aReader, &aResult->mIsInitialized) &&
           ReadParam(aReader, &aResult->mHasRange) &&
           ReadParam(aReader, &aResult->mReversed) &&
           ReadParam(aReader, &aResult->mCausedByComposition) &&
           ReadParam(aReader, &aResult->mCausedBySelectionEvent) &&
           ReadParam(aReader, &aResult->mOccurredDuringComposition);
  }
};

template <>
struct ParamTraits<mozilla::widget::IMENotification::TextChangeDataBase> {
  typedef mozilla::widget::IMENotification::TextChangeDataBase paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mStartOffset);
    WriteParam(aWriter, aParam.mRemovedEndOffset);
    WriteParam(aWriter, aParam.mAddedEndOffset);
    WriteParam(aWriter, aParam.mCausedOnlyByComposition);
    WriteParam(aWriter, aParam.mIncludingChangesDuringComposition);
    WriteParam(aWriter, aParam.mIncludingChangesWithoutComposition);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mStartOffset) &&
           ReadParam(aReader, &aResult->mRemovedEndOffset) &&
           ReadParam(aReader, &aResult->mAddedEndOffset) &&
           ReadParam(aReader, &aResult->mCausedOnlyByComposition) &&
           ReadParam(aReader, &aResult->mIncludingChangesDuringComposition) &&
           ReadParam(aReader, &aResult->mIncludingChangesWithoutComposition);
  }
};

template <>
struct ParamTraits<mozilla::widget::IMENotification::MouseButtonEventData> {
  typedef mozilla::widget::IMENotification::MouseButtonEventData paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mEventMessage);
    WriteParam(aWriter, aParam.mOffset);
    WriteParam(aWriter, aParam.mCursorPos);
    WriteParam(aWriter, aParam.mCharRect);
    WriteParam(aWriter, aParam.mButton);
    WriteParam(aWriter, aParam.mButtons);
    WriteParam(aWriter, aParam.mModifiers);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mEventMessage) &&
           ReadParam(aReader, &aResult->mOffset) &&
           ReadParam(aReader, &aResult->mCursorPos) &&
           ReadParam(aReader, &aResult->mCharRect) &&
           ReadParam(aReader, &aResult->mButton) &&
           ReadParam(aReader, &aResult->mButtons) &&
           ReadParam(aReader, &aResult->mModifiers);
  }
};

template <>
struct ParamTraits<mozilla::widget::IMENotification> {
  typedef mozilla::widget::IMENotification paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter,
               static_cast<mozilla::widget::IMEMessageType>(aParam.mMessage));
    switch (aParam.mMessage) {
      case mozilla::widget::NOTIFY_IME_OF_SELECTION_CHANGE:
        WriteParam(aWriter, aParam.mSelectionChangeData);
        return;
      case mozilla::widget::NOTIFY_IME_OF_TEXT_CHANGE:
        WriteParam(aWriter, aParam.mTextChangeData);
        return;
      case mozilla::widget::NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
        WriteParam(aWriter, aParam.mMouseButtonEventData);
        return;
      default:
        return;
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    mozilla::widget::IMEMessageType IMEMessage = 0;
    if (!ReadParam(aReader, &IMEMessage)) {
      return false;
    }
    aResult->mMessage = static_cast<mozilla::widget::IMEMessage>(IMEMessage);
    switch (aResult->mMessage) {
      case mozilla::widget::NOTIFY_IME_OF_SELECTION_CHANGE:
        return ReadParam(aReader, &aResult->mSelectionChangeData);
      case mozilla::widget::NOTIFY_IME_OF_TEXT_CHANGE:
        return ReadParam(aReader, &aResult->mTextChangeData);
      case mozilla::widget::NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
        return ReadParam(aReader, &aResult->mMouseButtonEventData);
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mEnabled);
    WriteParam(aWriter, aParam.mOpen);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mEnabled) &&
           ReadParam(aReader, &aResult->mOpen);
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mIMEState);
    WriteParam(aWriter, aParam.mHTMLInputType);
    WriteParam(aWriter, aParam.mHTMLInputMode);
    WriteParam(aWriter, aParam.mActionHint);
    WriteParam(aWriter, aParam.mAutocapitalize);
    WriteParam(aWriter, aParam.mOrigin);
    WriteParam(aWriter, aParam.mHasHandledUserInput);
    WriteParam(aWriter, aParam.mInPrivateBrowsing);
    mozilla::ipc::WriteIPDLParam(aWriter, aWriter->GetActor(), aParam.mURI);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mIMEState) &&
           ReadParam(aReader, &aResult->mHTMLInputType) &&
           ReadParam(aReader, &aResult->mHTMLInputMode) &&
           ReadParam(aReader, &aResult->mActionHint) &&
           ReadParam(aReader, &aResult->mAutocapitalize) &&
           ReadParam(aReader, &aResult->mOrigin) &&
           ReadParam(aReader, &aResult->mHasHandledUserInput) &&
           ReadParam(aReader, &aResult->mInPrivateBrowsing) &&
           mozilla::ipc::ReadIPDLParam(aReader, aReader->GetActor(),
                                       address_of(aResult->mURI));
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mCause);
    WriteParam(aWriter, aParam.mFocusChange);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mCause) &&
           ReadParam(aReader, &aResult->mFocusChange);
  }
};

template <>
struct ParamTraits<mozilla::WritingMode> {
  typedef mozilla::WritingMode paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mWritingMode.bits);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mWritingMode.bits);
  }
};

template <>
struct ParamTraits<mozilla::ContentCache::Selection> {
  typedef mozilla::ContentCache::Selection paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mAnchor);
    WriteParam(aWriter, aParam.mFocus);
    WriteParam(aWriter, aParam.mWritingMode);
    WriteParam(aWriter, aParam.mHasRange);
    WriteParam(aWriter, aParam.mAnchorCharRects[0]);
    WriteParam(aWriter, aParam.mAnchorCharRects[1]);
    WriteParam(aWriter, aParam.mFocusCharRects[0]);
    WriteParam(aWriter, aParam.mFocusCharRects[1]);
    WriteParam(aWriter, aParam.mRect);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mAnchor) &&
           ReadParam(aReader, &aResult->mFocus) &&
           ReadParam(aReader, &aResult->mWritingMode) &&
           ReadParam(aReader, &aResult->mHasRange) &&
           ReadParam(aReader, &aResult->mAnchorCharRects[0]) &&
           ReadParam(aReader, &aResult->mAnchorCharRects[1]) &&
           ReadParam(aReader, &aResult->mFocusCharRects[0]) &&
           ReadParam(aReader, &aResult->mFocusCharRects[1]) &&
           ReadParam(aReader, &aResult->mRect);
  }
};

template <>
struct ParamTraits<mozilla::ContentCache::Caret> {
  typedef mozilla::ContentCache::Caret paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mOffset);
    WriteParam(aWriter, aParam.mRect);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mOffset) &&
           ReadParam(aReader, &aResult->mRect);
  }
};

template <>
struct ParamTraits<mozilla::ContentCache::TextRectArray> {
  typedef mozilla::ContentCache::TextRectArray paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mStart);
    WriteParam(aWriter, aParam.mRects);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mStart) &&
           ReadParam(aReader, &aResult->mRects);
  }
};

template <>
struct ParamTraits<mozilla::ContentCache> {
  typedef mozilla::ContentCache paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mCompositionStart);
    WriteParam(aWriter, aParam.mText);
    WriteParam(aWriter, aParam.mSelection);
    WriteParam(aWriter, aParam.mFirstCharRect);
    WriteParam(aWriter, aParam.mCaret);
    WriteParam(aWriter, aParam.mTextRectArray);
    WriteParam(aWriter, aParam.mLastCommitStringTextRectArray);
    WriteParam(aWriter, aParam.mEditorRect);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mCompositionStart) &&
           ReadParam(aReader, &aResult->mText) &&
           ReadParam(aReader, &aResult->mSelection) &&
           ReadParam(aReader, &aResult->mFirstCharRect) &&
           ReadParam(aReader, &aResult->mCaret) &&
           ReadParam(aReader, &aResult->mTextRectArray) &&
           ReadParam(aReader, &aResult->mLastCommitStringTextRectArray) &&
           ReadParam(aReader, &aResult->mEditorRect);
  }
};

template <>
struct ParamTraits<mozilla::widget::CandidateWindowPosition> {
  typedef mozilla::widget::CandidateWindowPosition paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mPoint);
    WriteParam(aWriter, aParam.mRect);
    WriteParam(aWriter, aParam.mExcludeRect);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mPoint) &&
           ReadParam(aReader, &aResult->mRect) &&
           ReadParam(aReader, &aResult->mExcludeRect);
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mInputType);
    WriteParam(aWriter, aParam.mTimeStamp);
    WriteParam(aWriter, aParam.modifiers);
    WriteParam(aWriter, aParam.mFocusSequenceNumber);
    WriteParam(aWriter, aParam.mLayersId);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mInputType) &&
           ReadParam(aReader, &aResult->mTimeStamp) &&
           ReadParam(aReader, &aResult->modifiers) &&
           ReadParam(aReader, &aResult->mFocusSequenceNumber) &&
           ReadParam(aReader, &aResult->mLayersId);
  }
};

template <>
struct ParamTraits<mozilla::SingleTouchData::HistoricalTouchData> {
  typedef mozilla::SingleTouchData::HistoricalTouchData paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mTimeStamp);
    WriteParam(aWriter, aParam.mScreenPoint);
    WriteParam(aWriter, aParam.mLocalScreenPoint);
    WriteParam(aWriter, aParam.mRadius);
    WriteParam(aWriter, aParam.mRotationAngle);
    WriteParam(aWriter, aParam.mForce);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return (ReadParam(aReader, &aResult->mTimeStamp) &&
            ReadParam(aReader, &aResult->mScreenPoint) &&
            ReadParam(aReader, &aResult->mLocalScreenPoint) &&
            ReadParam(aReader, &aResult->mRadius) &&
            ReadParam(aReader, &aResult->mRotationAngle) &&
            ReadParam(aReader, &aResult->mForce));
  }
};

template <>
struct ParamTraits<mozilla::SingleTouchData> {
  using paramType = mozilla::SingleTouchData;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mHistoricalData);
    WriteParam(aWriter, aParam.mIdentifier);
    WriteParam(aWriter, aParam.mScreenPoint);
    WriteParam(aWriter, aParam.mLocalScreenPoint);
    WriteParam(aWriter, aParam.mRadius);
    WriteParam(aWriter, aParam.mRotationAngle);
    WriteParam(aWriter, aParam.mForce);
    WriteParam(aWriter, aParam.mTiltX);
    WriteParam(aWriter, aParam.mTiltY);
    WriteParam(aWriter, aParam.mTwist);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return (ReadParam(aReader, &aResult->mHistoricalData) &&
            ReadParam(aReader, &aResult->mIdentifier) &&
            ReadParam(aReader, &aResult->mScreenPoint) &&
            ReadParam(aReader, &aResult->mLocalScreenPoint) &&
            ReadParam(aReader, &aResult->mRadius) &&
            ReadParam(aReader, &aResult->mRotationAngle) &&
            ReadParam(aReader, &aResult->mForce) &&
            ReadParam(aReader, &aResult->mTiltX) &&
            ReadParam(aReader, &aResult->mTiltY) &&
            ReadParam(aReader, &aResult->mTwist));
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aWriter, aParam.mType);
    WriteParam(aWriter, aParam.mTouches);
    WriteParam(aWriter, aParam.mHandledByAPZ);
    WriteParam(aWriter, aParam.mScreenOffset);
    WriteParam(aWriter, aParam.mButton);
    WriteParam(aWriter, aParam.mButtons);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aReader, &aResult->mType) &&
           ReadParam(aReader, &aResult->mTouches) &&
           ReadParam(aReader, &aResult->mHandledByAPZ) &&
           ReadParam(aReader, &aResult->mScreenOffset) &&
           ReadParam(aReader, &aResult->mButton) &&
           ReadParam(aReader, &aResult->mButtons);
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aWriter, aParam.mButtonType);
    WriteParam(aWriter, aParam.mType);
    WriteParam(aWriter, aParam.mInputSource);
    WriteParam(aWriter, aParam.mButtons);
    WriteParam(aWriter, aParam.mOrigin);
    WriteParam(aWriter, aParam.mLocalOrigin);
    WriteParam(aWriter, aParam.mHandledByAPZ);
    WriteParam(aWriter, aParam.mPreventClickEvent);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aReader, &aResult->mButtonType) &&
           ReadParam(aReader, &aResult->mType) &&
           ReadParam(aReader, &aResult->mInputSource) &&
           ReadParam(aReader, &aResult->mButtons) &&
           ReadParam(aReader, &aResult->mOrigin) &&
           ReadParam(aReader, &aResult->mLocalOrigin) &&
           ReadParam(aReader, &aResult->mHandledByAPZ) &&
           ReadParam(aReader, &aResult->mPreventClickEvent);
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aWriter, aParam.mType);
    WriteParam(aWriter, aParam.mPanStartPoint);
    WriteParam(aWriter, aParam.mPanDisplacement);
    WriteParam(aWriter, aParam.mLocalPanStartPoint);
    WriteParam(aWriter, aParam.mLocalPanDisplacement);
    WriteParam(aWriter, aParam.mLineOrPageDeltaX);
    WriteParam(aWriter, aParam.mLineOrPageDeltaY);
    WriteParam(aWriter, aParam.mUserDeltaMultiplierX);
    WriteParam(aWriter, aParam.mUserDeltaMultiplierY);
    WriteParam(aWriter, aParam.mDeltaType);
    WriteParam(aWriter, aParam.mHandledByAPZ);
    WriteParam(aWriter, aParam.mMayTriggerSwipe);
    WriteParam(aWriter, aParam.mOverscrollBehaviorAllowsSwipe);
    WriteParam(aWriter, aParam.mSimulateMomentum);
    WriteParam(aWriter, aParam.mIsNoLineOrPageDelta);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aReader, &aResult->mType) &&
           ReadParam(aReader, &aResult->mPanStartPoint) &&
           ReadParam(aReader, &aResult->mPanDisplacement) &&
           ReadParam(aReader, &aResult->mLocalPanStartPoint) &&
           ReadParam(aReader, &aResult->mLocalPanDisplacement) &&
           ReadParam(aReader, &aResult->mLineOrPageDeltaX) &&
           ReadParam(aReader, &aResult->mLineOrPageDeltaY) &&
           ReadParam(aReader, &aResult->mUserDeltaMultiplierX) &&
           ReadParam(aReader, &aResult->mUserDeltaMultiplierY) &&
           ReadParam(aReader, &aResult->mDeltaType) &&
           ReadBoolForBitfield(aReader, aResult, &paramType::SetHandledByAPZ) &&
           ReadBoolForBitfield(aReader, aResult,
                               &paramType::SetMayTriggerSwipe) &&
           ReadBoolForBitfield(aReader, aResult,
                               &paramType::SetOverscrollBehaviorAllowsSwipe) &&
           ReadBoolForBitfield(aReader, aResult,
                               &paramType::SetSimulateMomentum) &&
           ReadBoolForBitfield(aReader, aResult,
                               &paramType::SetIsNoLineOrPageDelta);
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aWriter, aParam.mType);
    WriteParam(aWriter, aParam.mSource);
    WriteParam(aWriter, aParam.mScreenOffset);
    WriteParam(aWriter, aParam.mFocusPoint);
    WriteParam(aWriter, aParam.mLocalFocusPoint);
    WriteParam(aWriter, aParam.mCurrentSpan);
    WriteParam(aWriter, aParam.mPreviousSpan);
    WriteParam(aWriter, aParam.mLineOrPageDeltaY);
    WriteParam(aWriter, aParam.mHandledByAPZ);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aReader, &aResult->mType) &&
           ReadParam(aReader, &aResult->mSource) &&
           ReadParam(aReader, &aResult->mScreenOffset) &&
           ReadParam(aReader, &aResult->mFocusPoint) &&
           ReadParam(aReader, &aResult->mLocalFocusPoint) &&
           ReadParam(aReader, &aResult->mCurrentSpan) &&
           ReadParam(aReader, &aResult->mPreviousSpan) &&
           ReadParam(aReader, &aResult->mLineOrPageDeltaY) &&
           ReadParam(aReader, &aResult->mHandledByAPZ);
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aWriter, aParam.mType);
    WriteParam(aWriter, aParam.mPoint);
    WriteParam(aWriter, aParam.mLocalPoint);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aReader, &aResult->mType) &&
           ReadParam(aReader, &aResult->mPoint) &&
           ReadParam(aReader, &aResult->mLocalPoint);
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aWriter, aParam.mDeltaType);
    WriteParam(aWriter, aParam.mScrollMode);
    WriteParam(aWriter, aParam.mOrigin);
    WriteParam(aWriter, aParam.mHandledByAPZ);
    WriteParam(aWriter, aParam.mDeltaX);
    WriteParam(aWriter, aParam.mDeltaY);
    WriteParam(aWriter, aParam.mWheelTicksX);
    WriteParam(aWriter, aParam.mWheelTicksY);
    WriteParam(aWriter, aParam.mLocalOrigin);
    WriteParam(aWriter, aParam.mLineOrPageDeltaX);
    WriteParam(aWriter, aParam.mLineOrPageDeltaY);
    WriteParam(aWriter, aParam.mScrollSeriesNumber);
    WriteParam(aWriter, aParam.mUserDeltaMultiplierX);
    WriteParam(aWriter, aParam.mUserDeltaMultiplierY);
    WriteParam(aWriter, aParam.mMayHaveMomentum);
    WriteParam(aWriter, aParam.mIsMomentum);
    WriteParam(aWriter, aParam.mAllowToOverrideSystemScrollSpeed);
    WriteParam(aWriter, aParam.mWheelDeltaAdjustmentStrategy);
    WriteParam(aWriter, aParam.mAPZAction);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aReader, &aResult->mDeltaType) &&
           ReadParam(aReader, &aResult->mScrollMode) &&
           ReadParam(aReader, &aResult->mOrigin) &&
           ReadParam(aReader, &aResult->mHandledByAPZ) &&
           ReadParam(aReader, &aResult->mDeltaX) &&
           ReadParam(aReader, &aResult->mDeltaY) &&
           ReadParam(aReader, &aResult->mWheelTicksX) &&
           ReadParam(aReader, &aResult->mWheelTicksY) &&
           ReadParam(aReader, &aResult->mLocalOrigin) &&
           ReadParam(aReader, &aResult->mLineOrPageDeltaX) &&
           ReadParam(aReader, &aResult->mLineOrPageDeltaY) &&
           ReadParam(aReader, &aResult->mScrollSeriesNumber) &&
           ReadParam(aReader, &aResult->mUserDeltaMultiplierX) &&
           ReadParam(aReader, &aResult->mUserDeltaMultiplierY) &&
           ReadParam(aReader, &aResult->mMayHaveMomentum) &&
           ReadParam(aReader, &aResult->mIsMomentum) &&
           ReadParam(aReader, &aResult->mAllowToOverrideSystemScrollSpeed) &&
           ReadParam(aReader, &aResult->mWheelDeltaAdjustmentStrategy) &&
           ReadParam(aReader, &aResult->mAPZAction);
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aWriter, aParam.mType);
    WriteParam(aWriter, aParam.mKeyCode);
    WriteParam(aWriter, aParam.mCharCode);
    WriteParam(aWriter, aParam.mShortcutCandidates);
    WriteParam(aWriter, aParam.mHandledByAPZ);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aReader, &aResult->mType) &&
           ReadParam(aReader, &aResult->mKeyCode) &&
           ReadParam(aReader, &aResult->mCharCode) &&
           ReadParam(aReader, &aResult->mShortcutCandidates) &&
           ReadParam(aReader, &aResult->mHandledByAPZ);
  }
};

}  // namespace IPC

#endif  // nsGUIEventIPC_h__
