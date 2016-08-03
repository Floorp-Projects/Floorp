/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGUIEventIPC_h__
#define nsGUIEventIPC_h__

#include "ipc/IPCMessageUtils.h"
#include "mozilla/ContentCache.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "InputData.h"

namespace IPC
{

template<>
struct ParamTraits<mozilla::EventMessage>
{
  typedef mozilla::EventMessage paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<const mozilla::EventMessageType&>(aParam));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    mozilla::EventMessageType eventMessage = 0;
    bool ret = ReadParam(aMsg, aIter, &eventMessage);
    *aResult = static_cast<paramType>(eventMessage);
    return ret;
  }
};

template<>
struct ParamTraits<mozilla::BaseEventFlags>
{
  typedef mozilla::BaseEventFlags paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return aMsg->ReadBytesInto(aIter, aResult, sizeof(*aResult));
  }
};

template<>
struct ParamTraits<mozilla::WidgetEvent>
{
  typedef mozilla::WidgetEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg,
      static_cast<mozilla::EventClassIDType>(aParam.mClass));
    WriteParam(aMsg, aParam.mMessage);
    WriteParam(aMsg, aParam.mRefPoint);
    WriteParam(aMsg, aParam.mTime);
    WriteParam(aMsg, aParam.mTimeStamp);
    WriteParam(aMsg, aParam.mFlags);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    mozilla::EventClassIDType eventClassID = 0;
    bool ret = ReadParam(aMsg, aIter, &eventClassID) &&
               ReadParam(aMsg, aIter, &aResult->mMessage) &&
               ReadParam(aMsg, aIter, &aResult->mRefPoint) &&
               ReadParam(aMsg, aIter, &aResult->mTime) &&
               ReadParam(aMsg, aIter, &aResult->mTimeStamp) &&
               ReadParam(aMsg, aIter, &aResult->mFlags);
    aResult->mClass = static_cast<mozilla::EventClassID>(eventClassID);
    return ret;
  }
};

template<>
struct ParamTraits<mozilla::NativeEventData>
{
  typedef mozilla::NativeEventData paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mBuffer);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mBuffer);
  }
};

template<>
struct ParamTraits<mozilla::WidgetGUIEvent>
{
  typedef mozilla::WidgetGUIEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetEvent>(aParam));
    WriteParam(aMsg, aParam.mPluginEvent);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<mozilla::WidgetEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mPluginEvent);
  }
};

template<>
struct ParamTraits<mozilla::WidgetInputEvent>
{
  typedef mozilla::WidgetInputEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetGUIEvent>(aParam));
    WriteParam(aMsg, aParam.mModifiers);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mModifiers);
  }
};

template<>
struct ParamTraits<mozilla::WidgetMouseEventBase>
{
  typedef mozilla::WidgetMouseEventBase paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetInputEvent>(aParam));
    WriteParam(aMsg, aParam.button);
    WriteParam(aMsg, aParam.buttons);
    WriteParam(aMsg, aParam.pressure);
    WriteParam(aMsg, aParam.hitCluster);
    WriteParam(aMsg, aParam.inputSource);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetInputEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->button) &&
           ReadParam(aMsg, aIter, &aResult->buttons) &&
           ReadParam(aMsg, aIter, &aResult->pressure) &&
           ReadParam(aMsg, aIter, &aResult->hitCluster) &&
           ReadParam(aMsg, aIter, &aResult->inputSource);
  }
};

template<>
struct ParamTraits<mozilla::WidgetWheelEvent>
{
  typedef mozilla::WidgetWheelEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetMouseEventBase>(aParam));
    WriteParam(aMsg, aParam.mDeltaX);
    WriteParam(aMsg, aParam.mDeltaY);
    WriteParam(aMsg, aParam.mDeltaZ);
    WriteParam(aMsg, aParam.mDeltaMode);
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
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    uint8_t scrollType = 0;
    bool rv =
      ReadParam(aMsg, aIter,
                static_cast<mozilla::WidgetMouseEventBase*>(aResult)) &&
      ReadParam(aMsg, aIter, &aResult->mDeltaX) &&
      ReadParam(aMsg, aIter, &aResult->mDeltaY) &&
      ReadParam(aMsg, aIter, &aResult->mDeltaZ) &&
      ReadParam(aMsg, aIter, &aResult->mDeltaMode) &&
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
      ReadParam(aMsg, aIter, &aResult->mAllowToOverrideSystemScrollSpeed);
    aResult->mScrollType =
      static_cast<mozilla::WidgetWheelEvent::ScrollType>(scrollType);
    return rv;
  }
};

template<>
struct ParamTraits<mozilla::WidgetMouseEvent>
{
  typedef mozilla::WidgetMouseEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetMouseEventBase>(aParam));
    WriteParam(aMsg, aParam.mIgnoreRootScrollFrame);
    WriteParam(aMsg, static_cast<paramType::ReasonType>(aParam.mReason));
    WriteParam(aMsg, static_cast<paramType::ContextMenuTriggerType>(
                       aParam.mContextMenuTrigger));
    WriteParam(aMsg, static_cast<paramType::ExitFromType>(aParam.mExitFrom));
    WriteParam(aMsg, aParam.mClickCount);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    bool rv;
    paramType::ReasonType reason = 0;
    paramType::ContextMenuTriggerType contextMenuTrigger = 0;
    paramType::ExitFromType exitFrom = 0;
    rv = ReadParam(aMsg, aIter,
                   static_cast<mozilla::WidgetMouseEventBase*>(aResult)) &&
         ReadParam(aMsg, aIter, &aResult->mIgnoreRootScrollFrame) &&
         ReadParam(aMsg, aIter, &reason) &&
         ReadParam(aMsg, aIter, &contextMenuTrigger) &&
         ReadParam(aMsg, aIter, &exitFrom) &&
         ReadParam(aMsg, aIter, &aResult->mClickCount);
    aResult->mReason = static_cast<paramType::Reason>(reason);
    aResult->mContextMenuTrigger =
      static_cast<paramType::ContextMenuTrigger>(contextMenuTrigger);
    aResult->mExitFrom = static_cast<paramType::ExitFrom>(exitFrom);
    return rv;
  }
};


template<>
struct ParamTraits<mozilla::WidgetDragEvent>
{
  typedef mozilla::WidgetDragEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetMouseEvent>(aParam));
    WriteParam(aMsg, aParam.mUserCancelled);
    WriteParam(aMsg, aParam.mDefaultPreventedOnContent);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    bool rv =
      ReadParam(aMsg, aIter, static_cast<mozilla::WidgetMouseEvent*>(aResult)) &&
      ReadParam(aMsg, aIter, &aResult->mUserCancelled) &&
      ReadParam(aMsg, aIter, &aResult->mDefaultPreventedOnContent);
    return rv;
  }
};

template<>
struct ParamTraits<mozilla::WidgetPointerEvent>
{
  typedef mozilla::WidgetPointerEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetMouseEvent>(aParam));
    WriteParam(aMsg, aParam.pointerId);
    WriteParam(aMsg, aParam.mWidth);
    WriteParam(aMsg, aParam.mHeight);
    WriteParam(aMsg, aParam.tiltX);
    WriteParam(aMsg, aParam.tiltY);
    WriteParam(aMsg, aParam.mIsPrimary);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    bool rv =
      ReadParam(aMsg, aIter, static_cast<mozilla::WidgetMouseEvent*>(aResult)) &&
      ReadParam(aMsg, aIter, &aResult->pointerId) &&
      ReadParam(aMsg, aIter, &aResult->mWidth) &&
      ReadParam(aMsg, aIter, &aResult->mHeight) &&
      ReadParam(aMsg, aIter, &aResult->tiltX) &&
      ReadParam(aMsg, aIter, &aResult->tiltY) &&
      ReadParam(aMsg, aIter, &aResult->mIsPrimary);
    return rv;
  }
};

template<>
struct ParamTraits<mozilla::WidgetTouchEvent>
{
  typedef mozilla::WidgetTouchEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<const mozilla::WidgetInputEvent&>(aParam));
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
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    paramType::TouchArray::size_type numTouches;
    if (!ReadParam(aMsg, aIter,
                   static_cast<mozilla::WidgetInputEvent*>(aResult)) ||
        !ReadParam(aMsg, aIter, &numTouches)) {
      return false;
    }
    for (uint32_t i = 0; i < numTouches; ++i) {
        int32_t identifier;
        mozilla::LayoutDeviceIntPoint refPoint;
        mozilla::LayoutDeviceIntPoint radius;
        float rotationAngle;
        float force;
        if (!ReadParam(aMsg, aIter, &identifier) ||
            !ReadParam(aMsg, aIter, &refPoint) ||
            !ReadParam(aMsg, aIter, &radius) ||
            !ReadParam(aMsg, aIter, &rotationAngle) ||
            !ReadParam(aMsg, aIter, &force)) {
          return false;
        }
        aResult->mTouches.AppendElement(
          new mozilla::dom::Touch(
            identifier, refPoint, radius, rotationAngle, force));
    }
    return true;
  }
};

template<>
struct ParamTraits<mozilla::AlternativeCharCode>
{
  typedef mozilla::AlternativeCharCode paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mUnshiftedCharCode);
    WriteParam(aMsg, aParam.mShiftedCharCode);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mUnshiftedCharCode) &&
           ReadParam(aMsg, aIter, &aResult->mShiftedCharCode);
  }
};


template<>
struct ParamTraits<mozilla::WidgetKeyboardEvent>
{
  typedef mozilla::WidgetKeyboardEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetInputEvent>(aParam));
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
    WriteParam(aMsg, aParam.mIsChar);
    WriteParam(aMsg, aParam.mIsRepeat);
    WriteParam(aMsg, aParam.mIsReserved);
    WriteParam(aMsg, aParam.mAccessKeyForwardedToChild);
    WriteParam(aMsg, aParam.mLocation);
    WriteParam(aMsg, aParam.mUniqueId);
    WriteParam(aMsg, aParam.mIsSynthesizedByTIP);
    WriteParam(aMsg,
               static_cast<paramType::InputMethodAppStateType>
                 (aParam.mInputMethodAppState));
#ifdef XP_MACOSX
    WriteParam(aMsg, aParam.mNativeKeyCode);
    WriteParam(aMsg, aParam.mNativeModifierFlags);
    WriteParam(aMsg, aParam.mNativeCharacters);
    WriteParam(aMsg, aParam.mNativeCharactersIgnoringModifiers);
    WriteParam(aMsg, aParam.mPluginTextEventString);
#endif
    // An OS-specific native event might be attached in |mNativeKeyEvent|,  but
    // that cannot be copied across process boundaries.
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    mozilla::KeyNameIndexType keyNameIndex = 0;
    mozilla::CodeNameIndexType codeNameIndex = 0;
    paramType::InputMethodAppStateType inputMethodAppState = 0;
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
        ReadParam(aMsg, aIter, &aResult->mIsChar) &&
        ReadParam(aMsg, aIter, &aResult->mIsRepeat) &&
        ReadParam(aMsg, aIter, &aResult->mIsReserved) &&
        ReadParam(aMsg, aIter, &aResult->mAccessKeyForwardedToChild) &&
        ReadParam(aMsg, aIter, &aResult->mLocation) &&
        ReadParam(aMsg, aIter, &aResult->mUniqueId) &&
        ReadParam(aMsg, aIter, &aResult->mIsSynthesizedByTIP) &&
        ReadParam(aMsg, aIter, &inputMethodAppState)
#ifdef XP_MACOSX
        && ReadParam(aMsg, aIter, &aResult->mNativeKeyCode)
        && ReadParam(aMsg, aIter, &aResult->mNativeModifierFlags)
        && ReadParam(aMsg, aIter, &aResult->mNativeCharacters)
        && ReadParam(aMsg, aIter, &aResult->mNativeCharactersIgnoringModifiers)
        && ReadParam(aMsg, aIter, &aResult->mPluginTextEventString)
#endif
        )
    {
      aResult->mKeyNameIndex = static_cast<mozilla::KeyNameIndex>(keyNameIndex);
      aResult->mCodeNameIndex =
        static_cast<mozilla::CodeNameIndex>(codeNameIndex);
      aResult->mNativeKeyEvent = nullptr;
      aResult->mInputMethodAppState =
        static_cast<paramType::InputMethodAppState>(inputMethodAppState);
      return true;
    }
    return false;
  }
};

template<>
struct ParamTraits<mozilla::InternalBeforeAfterKeyboardEvent>
{
  typedef mozilla::InternalBeforeAfterKeyboardEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetKeyboardEvent>(aParam));
    WriteParam(aMsg, aParam.mEmbeddedCancelled.IsNull());
    WriteParam(aMsg, aParam.mEmbeddedCancelled.Value());
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    bool isNull;
    bool value;
    bool rv =
      ReadParam(aMsg, aIter,
                static_cast<mozilla::WidgetKeyboardEvent*>(aResult)) &&
      ReadParam(aMsg, aIter, &isNull) &&
      ReadParam(aMsg, aIter, &value);

    aResult->mEmbeddedCancelled = Nullable<bool>();
    if (rv && !isNull) {
      aResult->mEmbeddedCancelled.SetValue(value);
    }

    return rv;
  }
};

template<>
struct ParamTraits<mozilla::TextRangeStyle>
{
  typedef mozilla::TextRangeStyle paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mDefinedStyles);
    WriteParam(aMsg, aParam.mLineStyle);
    WriteParam(aMsg, aParam.mIsBoldLine);
    WriteParam(aMsg, aParam.mForegroundColor);
    WriteParam(aMsg, aParam.mBackgroundColor);
    WriteParam(aMsg, aParam.mUnderlineColor);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mDefinedStyles) &&
           ReadParam(aMsg, aIter, &aResult->mLineStyle) &&
           ReadParam(aMsg, aIter, &aResult->mIsBoldLine) &&
           ReadParam(aMsg, aIter, &aResult->mForegroundColor) &&
           ReadParam(aMsg, aIter, &aResult->mBackgroundColor) &&
           ReadParam(aMsg, aIter, &aResult->mUnderlineColor);
  }
};

template<>
struct ParamTraits<mozilla::TextRange>
{
  typedef mozilla::TextRange paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mStartOffset);
    WriteParam(aMsg, aParam.mEndOffset);
    WriteParam(aMsg, mozilla::ToRawTextRangeType(aParam.mRangeType));
    WriteParam(aMsg, aParam.mRangeStyle);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
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

template<>
struct ParamTraits<mozilla::TextRangeArray>
{
  typedef mozilla::TextRangeArray paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.Length());
    for (uint32_t index = 0; index < aParam.Length(); index++) {
      WriteParam(aMsg, aParam[index]);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
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

template<>
struct ParamTraits<mozilla::WidgetCompositionEvent>
{
  typedef mozilla::WidgetCompositionEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetGUIEvent>(aParam));
    WriteParam(aMsg, aParam.mData);
    WriteParam(aMsg, aParam.mNativeIMEContext);
    bool hasRanges = !!aParam.mRanges;
    WriteParam(aMsg, hasRanges);
    if (hasRanges) {
      WriteParam(aMsg, *aParam.mRanges.get());
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
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

template<>
struct ParamTraits<mozilla::FontRange>
{
  typedef mozilla::FontRange paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mStartOffset);
    WriteParam(aMsg, aParam.mFontName);
    WriteParam(aMsg, aParam.mFontSize);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mStartOffset) &&
           ReadParam(aMsg, aIter, &aResult->mFontName) &&
           ReadParam(aMsg, aIter, &aResult->mFontSize);
  }
};

template<>
struct ParamTraits<mozilla::WidgetQueryContentEvent::Input>
{
  typedef mozilla::WidgetQueryContentEvent::Input paramType;
  typedef mozilla::WidgetQueryContentEvent event;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mOffset);
    WriteParam(aMsg, aParam.mLength);
    WriteParam(aMsg, mozilla::ToRawSelectionType(aParam.mSelectionType));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    mozilla::RawSelectionType rawSelectionType = 0;
    bool ok = ReadParam(aMsg, aIter, &aResult->mOffset) &&
              ReadParam(aMsg, aIter, &aResult->mLength) &&
              ReadParam(aMsg, aIter, &rawSelectionType);
    aResult->mSelectionType = mozilla::ToSelectionType(rawSelectionType);
    return ok;
  }
};

template<>
struct ParamTraits<mozilla::WidgetQueryContentEvent>
{
  typedef mozilla::WidgetQueryContentEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetGUIEvent>(aParam));
    WriteParam(aMsg, aParam.mSucceeded);
    WriteParam(aMsg, aParam.mUseNativeLineBreak);
    WriteParam(aMsg, aParam.mWithFontRanges);
    WriteParam(aMsg, aParam.mInput);
    WriteParam(aMsg, aParam.mReply.mOffset);
    WriteParam(aMsg, aParam.mReply.mTentativeCaretOffset);
    WriteParam(aMsg, aParam.mReply.mString);
    WriteParam(aMsg, aParam.mReply.mRect);
    WriteParam(aMsg, aParam.mReply.mReversed);
    WriteParam(aMsg, aParam.mReply.mHasSelection);
    WriteParam(aMsg, aParam.mReply.mWidgetIsHit);
    WriteParam(aMsg, aParam.mReply.mFontRanges);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mSucceeded) &&
           ReadParam(aMsg, aIter, &aResult->mUseNativeLineBreak) &&
           ReadParam(aMsg, aIter, &aResult->mWithFontRanges) &&
           ReadParam(aMsg, aIter, &aResult->mInput) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mOffset) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mTentativeCaretOffset) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mString) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mRect) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mReversed) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mHasSelection) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mWidgetIsHit) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mFontRanges);
  }
};

template<>
struct ParamTraits<mozilla::WidgetSelectionEvent>
{
  typedef mozilla::WidgetSelectionEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetGUIEvent>(aParam));
    WriteParam(aMsg, aParam.mOffset);
    WriteParam(aMsg, aParam.mLength);
    WriteParam(aMsg, aParam.mReversed);
    WriteParam(aMsg, aParam.mExpandToClusterBoundary);
    WriteParam(aMsg, aParam.mSucceeded);
    WriteParam(aMsg, aParam.mUseNativeLineBreak);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
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

template<>
struct ParamTraits<nsIMEUpdatePreference>
{
  typedef nsIMEUpdatePreference paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mWantUpdates);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mWantUpdates);
  }
};

template<>
struct ParamTraits<mozilla::widget::NativeIMEContext>
{
  typedef mozilla::widget::NativeIMEContext paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mRawNativeIMEContext);
    WriteParam(aMsg, aParam.mOriginProcessID);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mRawNativeIMEContext) &&
           ReadParam(aMsg, aIter, &aResult->mOriginProcessID);
  }
};

template<>
struct ParamTraits<mozilla::widget::IMENotification::Point>
{
  typedef mozilla::widget::IMENotification::Point paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mX);
    WriteParam(aMsg, aParam.mY);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mX) &&
           ReadParam(aMsg, aIter, &aResult->mY);
  }
};

template<>
struct ParamTraits<mozilla::widget::IMENotification::Rect>
{
  typedef mozilla::widget::IMENotification::Rect paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mX);
    WriteParam(aMsg, aParam.mY);
    WriteParam(aMsg, aParam.mWidth);
    WriteParam(aMsg, aParam.mHeight);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mX) &&
           ReadParam(aMsg, aIter, &aResult->mY) &&
           ReadParam(aMsg, aIter, &aResult->mWidth) &&
           ReadParam(aMsg, aIter, &aResult->mHeight);
  }
};

template<>
struct ParamTraits<mozilla::widget::IMENotification::SelectionChangeDataBase>
{
  typedef mozilla::widget::IMENotification::SelectionChangeDataBase paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    MOZ_RELEASE_ASSERT(aParam.mString);
    WriteParam(aMsg, aParam.mOffset);
    WriteParam(aMsg, *aParam.mString);
    WriteParam(aMsg, aParam.mWritingMode);
    WriteParam(aMsg, aParam.mReversed);
    WriteParam(aMsg, aParam.mCausedByComposition);
    WriteParam(aMsg, aParam.mCausedBySelectionEvent);
    WriteParam(aMsg, aParam.mOccurredDuringComposition);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
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

template<>
struct ParamTraits<mozilla::widget::IMENotification::TextChangeDataBase>
{
  typedef mozilla::widget::IMENotification::TextChangeDataBase paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mStartOffset);
    WriteParam(aMsg, aParam.mRemovedEndOffset);
    WriteParam(aMsg, aParam.mAddedEndOffset);
    WriteParam(aMsg, aParam.mCausedOnlyByComposition);
    WriteParam(aMsg, aParam.mIncludingChangesDuringComposition);
    WriteParam(aMsg, aParam.mIncludingChangesWithoutComposition);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
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

template<>
struct ParamTraits<mozilla::widget::IMENotification::MouseButtonEventData>
{
  typedef mozilla::widget::IMENotification::MouseButtonEventData paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mEventMessage);
    WriteParam(aMsg, aParam.mOffset);
    WriteParam(aMsg, aParam.mCursorPos);
    WriteParam(aMsg, aParam.mCharRect);
    WriteParam(aMsg, aParam.mButton);
    WriteParam(aMsg, aParam.mButtons);
    WriteParam(aMsg, aParam.mModifiers);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mEventMessage) &&
           ReadParam(aMsg, aIter, &aResult->mOffset) &&
           ReadParam(aMsg, aIter, &aResult->mCursorPos) &&
           ReadParam(aMsg, aIter, &aResult->mCharRect) &&
           ReadParam(aMsg, aIter, &aResult->mButton) &&
           ReadParam(aMsg, aIter, &aResult->mButtons) &&
           ReadParam(aMsg, aIter, &aResult->mModifiers);
  }
};

template<>
struct ParamTraits<mozilla::widget::IMENotification>
{
  typedef mozilla::widget::IMENotification paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
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

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
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

template<>
struct ParamTraits<mozilla::WidgetPluginEvent>
{
  typedef mozilla::WidgetPluginEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetGUIEvent>(aParam));
    WriteParam(aMsg, aParam.mRetargetToFocusedDocument);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mRetargetToFocusedDocument);
  }
};

template<>
struct ParamTraits<mozilla::WritingMode>
{
  typedef mozilla::WritingMode paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mWritingMode);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mWritingMode);
  }
};

template<>
struct ParamTraits<mozilla::ContentCache>
{
  typedef mozilla::ContentCache paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mCompositionStart);
    WriteParam(aMsg, aParam.mText);
    WriteParam(aMsg, aParam.mSelection.mAnchor);
    WriteParam(aMsg, aParam.mSelection.mFocus);
    WriteParam(aMsg, aParam.mSelection.mWritingMode);
    WriteParam(aMsg, aParam.mSelection.mAnchorCharRect);
    WriteParam(aMsg, aParam.mSelection.mFocusCharRect);
    WriteParam(aMsg, aParam.mSelection.mRect);
    WriteParam(aMsg, aParam.mFirstCharRect);
    WriteParam(aMsg, aParam.mCaret.mOffset);
    WriteParam(aMsg, aParam.mCaret.mRect);
    WriteParam(aMsg, aParam.mTextRectArray.mStart);
    WriteParam(aMsg, aParam.mTextRectArray.mRects);
    WriteParam(aMsg, aParam.mEditorRect);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mCompositionStart) &&
           ReadParam(aMsg, aIter, &aResult->mText) &&
           ReadParam(aMsg, aIter, &aResult->mSelection.mAnchor) &&
           ReadParam(aMsg, aIter, &aResult->mSelection.mFocus) &&
           ReadParam(aMsg, aIter, &aResult->mSelection.mWritingMode) &&
           ReadParam(aMsg, aIter, &aResult->mSelection.mAnchorCharRect) &&
           ReadParam(aMsg, aIter, &aResult->mSelection.mFocusCharRect) &&
           ReadParam(aMsg, aIter, &aResult->mSelection.mRect) &&
           ReadParam(aMsg, aIter, &aResult->mFirstCharRect) &&
           ReadParam(aMsg, aIter, &aResult->mCaret.mOffset) &&
           ReadParam(aMsg, aIter, &aResult->mCaret.mRect) &&
           ReadParam(aMsg, aIter, &aResult->mTextRectArray.mStart) &&
           ReadParam(aMsg, aIter, &aResult->mTextRectArray.mRects) &&
           ReadParam(aMsg, aIter, &aResult->mEditorRect);
  }
};

template<>
struct ParamTraits<mozilla::widget::CandidateWindowPosition>
{
  typedef mozilla::widget::CandidateWindowPosition paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mPoint);
    WriteParam(aMsg, aParam.mRect);
    WriteParam(aMsg, aParam.mExcludeRect);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mPoint) &&
           ReadParam(aMsg, aIter, &aResult->mRect) &&
           ReadParam(aMsg, aIter, &aResult->mExcludeRect);
  }
};

// InputData.h

template<>
struct ParamTraits<mozilla::InputType>
  : public ContiguousEnumSerializer<
             mozilla::InputType,
             mozilla::InputType::MULTITOUCH_INPUT,
             mozilla::InputType::SENTINEL_INPUT>
{};

template<>
struct ParamTraits<mozilla::InputData>
{
  typedef mozilla::InputData paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mInputType);
    WriteParam(aMsg, aParam.mTime);
    WriteParam(aMsg, aParam.mTimeStamp);
    WriteParam(aMsg, aParam.modifiers);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mInputType) &&
           ReadParam(aMsg, aIter, &aResult->mTime) &&
           ReadParam(aMsg, aIter, &aResult->mTimeStamp) &&
           ReadParam(aMsg, aIter, &aResult->modifiers);
  }
};

template<>
struct ParamTraits<mozilla::SingleTouchData>
{
  typedef mozilla::SingleTouchData paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mIdentifier);
    WriteParam(aMsg, aParam.mScreenPoint);
    WriteParam(aMsg, aParam.mLocalScreenPoint);
    WriteParam(aMsg, aParam.mRadius);
    WriteParam(aMsg, aParam.mRotationAngle);
    WriteParam(aMsg, aParam.mForce);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mIdentifier) &&
            ReadParam(aMsg, aIter, &aResult->mScreenPoint) &&
            ReadParam(aMsg, aIter, &aResult->mLocalScreenPoint) &&
            ReadParam(aMsg, aIter, &aResult->mRadius) &&
            ReadParam(aMsg, aIter, &aResult->mRotationAngle) &&
            ReadParam(aMsg, aIter, &aResult->mForce));
  }
};

template<>
struct ParamTraits<mozilla::MultiTouchInput::MultiTouchType>
  : public ContiguousEnumSerializer<
             mozilla::MultiTouchInput::MultiTouchType,
             mozilla::MultiTouchInput::MultiTouchType::MULTITOUCH_START,
             mozilla::MultiTouchInput::MultiTouchType::MULTITOUCH_SENTINEL>
{};

template<>
struct ParamTraits<mozilla::MultiTouchInput>
{
  typedef mozilla::MultiTouchInput paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::InputData>(aParam));
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mTouches);
    WriteParam(aMsg, aParam.mHandledByAPZ);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mTouches) &&
           ReadParam(aMsg, aIter, &aResult->mHandledByAPZ);
  }
};

template<>
struct ParamTraits<mozilla::MouseInput::MouseType>
  : public ContiguousEnumSerializer<
             mozilla::MouseInput::MouseType,
             mozilla::MouseInput::MouseType::MOUSE_NONE,
             mozilla::MouseInput::MouseType::MOUSE_SENTINEL>
{};

template<>
struct ParamTraits<mozilla::MouseInput::ButtonType>
  : public ContiguousEnumSerializer<
             mozilla::MouseInput::ButtonType,
             mozilla::MouseInput::ButtonType::LEFT_BUTTON,
             mozilla::MouseInput::ButtonType::BUTTON_SENTINEL>
{};

template<>
struct ParamTraits<mozilla::MouseInput>
{
  typedef mozilla::MouseInput paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::InputData>(aParam));
    WriteParam(aMsg, aParam.mButtonType);
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mInputSource);
    WriteParam(aMsg, aParam.mButtons);
    WriteParam(aMsg, aParam.mOrigin);
    WriteParam(aMsg, aParam.mLocalOrigin);
    WriteParam(aMsg, aParam.mHandledByAPZ);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mButtonType) &&
           ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mInputSource) &&
           ReadParam(aMsg, aIter, &aResult->mButtons) &&
           ReadParam(aMsg, aIter, &aResult->mOrigin) &&
           ReadParam(aMsg, aIter, &aResult->mLocalOrigin) &&
           ReadParam(aMsg, aIter, &aResult->mHandledByAPZ);
  }
};

template<>
struct ParamTraits<mozilla::PanGestureInput::PanGestureType>
  : public ContiguousEnumSerializer<
             mozilla::PanGestureInput::PanGestureType,
             mozilla::PanGestureInput::PanGestureType::PANGESTURE_MAYSTART,
             mozilla::PanGestureInput::PanGestureType::PANGESTURE_SENTINEL>
{};

template<>
struct ParamTraits<mozilla::PanGestureInput>
{
  typedef mozilla::PanGestureInput paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::InputData>(aParam));
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mPanStartPoint);
    WriteParam(aMsg, aParam.mPanDisplacement);
    WriteParam(aMsg, aParam.mLocalPanStartPoint);
    WriteParam(aMsg, aParam.mLocalPanDisplacement);
    WriteParam(aMsg, aParam.mLineOrPageDeltaX);
    WriteParam(aMsg, aParam.mLineOrPageDeltaY);
    WriteParam(aMsg, aParam.mUserDeltaMultiplierX);
    WriteParam(aMsg, aParam.mUserDeltaMultiplierY);
    WriteParam(aMsg, aParam.mHandledByAPZ);
    WriteParam(aMsg, aParam.mFollowedByMomentum);
    WriteParam(aMsg, aParam.mRequiresContentResponseIfCannotScrollHorizontallyInStartDirection);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
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
           ReadParam(aMsg, aIter, &aResult->mHandledByAPZ) &&
           ReadParam(aMsg, aIter, &aResult->mFollowedByMomentum) &&
           ReadParam(aMsg, aIter, &aResult->mRequiresContentResponseIfCannotScrollHorizontallyInStartDirection);
  }
};

template<>
struct ParamTraits<mozilla::PinchGestureInput::PinchGestureType>
  : public ContiguousEnumSerializer<
             mozilla::PinchGestureInput::PinchGestureType,
             mozilla::PinchGestureInput::PinchGestureType::PINCHGESTURE_START,
             mozilla::PinchGestureInput::PinchGestureType::PINCHGESTURE_SENTINEL>
{};

template<>
struct ParamTraits<mozilla::PinchGestureInput>
{
  typedef mozilla::PinchGestureInput paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::InputData>(aParam));
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mFocusPoint);
    WriteParam(aMsg, aParam.mLocalFocusPoint);
    WriteParam(aMsg, aParam.mCurrentSpan);
    WriteParam(aMsg, aParam.mPreviousSpan);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mFocusPoint) &&
           ReadParam(aMsg, aIter, &aResult->mLocalFocusPoint) &&
           ReadParam(aMsg, aIter, &aResult->mCurrentSpan) &&
           ReadParam(aMsg, aIter, &aResult->mPreviousSpan);
  }
};

template<>
struct ParamTraits<mozilla::TapGestureInput::TapGestureType>
  : public ContiguousEnumSerializer<
             mozilla::TapGestureInput::TapGestureType,
             mozilla::TapGestureInput::TapGestureType::TAPGESTURE_LONG,
             mozilla::TapGestureInput::TapGestureType::TAPGESTURE_SENTINEL>
{};

template<>
struct ParamTraits<mozilla::TapGestureInput>
{
  typedef mozilla::TapGestureInput paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::InputData>(aParam));
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mPoint);
    WriteParam(aMsg, aParam.mLocalPoint);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mPoint) &&
           ReadParam(aMsg, aIter, &aResult->mLocalPoint);
  }
};

template<>
struct ParamTraits<mozilla::ScrollWheelInput::ScrollDeltaType>
  : public ContiguousEnumSerializer<
             mozilla::ScrollWheelInput::ScrollDeltaType,
             mozilla::ScrollWheelInput::ScrollDeltaType::SCROLLDELTA_LINE,
             mozilla::ScrollWheelInput::ScrollDeltaType::SCROLLDELTA_SENTINEL>
{};

template<>
struct ParamTraits<mozilla::ScrollWheelInput::ScrollMode>
  : public ContiguousEnumSerializer<
             mozilla::ScrollWheelInput::ScrollMode,
             mozilla::ScrollWheelInput::ScrollMode::SCROLLMODE_INSTANT,
             mozilla::ScrollWheelInput::ScrollMode::SCROLLMODE_SENTINEL>
{};

template<>
struct ParamTraits<mozilla::ScrollWheelInput>
{
  typedef mozilla::ScrollWheelInput paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::InputData>(aParam));
    WriteParam(aMsg, aParam.mDeltaType);
    WriteParam(aMsg, aParam.mScrollMode);
    WriteParam(aMsg, aParam.mOrigin);
    WriteParam(aMsg, aParam.mHandledByAPZ);
    WriteParam(aMsg, aParam.mDeltaX);
    WriteParam(aMsg, aParam.mDeltaY);
    WriteParam(aMsg, aParam.mLocalOrigin);
    WriteParam(aMsg, aParam.mLineOrPageDeltaX);
    WriteParam(aMsg, aParam.mLineOrPageDeltaY);
    WriteParam(aMsg, aParam.mScrollSeriesNumber);
    WriteParam(aMsg, aParam.mUserDeltaMultiplierX);
    WriteParam(aMsg, aParam.mUserDeltaMultiplierY);
    WriteParam(aMsg, aParam.mMayHaveMomentum);
    WriteParam(aMsg, aParam.mIsMomentum);
    WriteParam(aMsg, aParam.mAllowToOverrideSystemScrollSpeed);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mDeltaType) &&
           ReadParam(aMsg, aIter, &aResult->mScrollMode) &&
           ReadParam(aMsg, aIter, &aResult->mOrigin) &&
           ReadParam(aMsg, aIter, &aResult->mHandledByAPZ) &&
           ReadParam(aMsg, aIter, &aResult->mDeltaX) &&
           ReadParam(aMsg, aIter, &aResult->mDeltaY) &&
           ReadParam(aMsg, aIter, &aResult->mLocalOrigin) &&
           ReadParam(aMsg, aIter, &aResult->mLineOrPageDeltaX) &&
           ReadParam(aMsg, aIter, &aResult->mLineOrPageDeltaY) &&
           ReadParam(aMsg, aIter, &aResult->mScrollSeriesNumber) &&
           ReadParam(aMsg, aIter, &aResult->mUserDeltaMultiplierX) &&
           ReadParam(aMsg, aIter, &aResult->mUserDeltaMultiplierY) &&
           ReadParam(aMsg, aIter, &aResult->mMayHaveMomentum) &&
           ReadParam(aMsg, aIter, &aResult->mIsMomentum) &&
           ReadParam(aMsg, aIter, &aResult->mAllowToOverrideSystemScrollSpeed);
  }
};

} // namespace IPC

#endif // nsGUIEventIPC_h__
