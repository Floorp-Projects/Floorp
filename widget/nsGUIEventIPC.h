/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGUIEventIPC_h__
#define nsGUIEventIPC_h__

#include "ipc/IPCMessageUtils.h"
#include "nsDOMTouchEvent.h"
#include "nsGUIEvent.h"

namespace IPC
{

template<>
struct ParamTraits<mozilla::widget::EventFlags>
{
  typedef mozilla::widget::EventFlags paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    const char* outp;
    if (!aMsg->ReadBytes(aIter, &outp, sizeof(*aResult))) {
      return false;
    }
    *aResult = *reinterpret_cast<const paramType*>(outp);
    return true;
  }
};

template<>
struct ParamTraits<nsEvent>
{
  typedef nsEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, (uint8_t) aParam.eventStructType);
    WriteParam(aMsg, aParam.message);
    WriteParam(aMsg, aParam.refPoint);
    WriteParam(aMsg, aParam.time);
    WriteParam(aMsg, aParam.mFlags);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint8_t eventStructType = 0;
    bool ret = ReadParam(aMsg, aIter, &eventStructType) &&
               ReadParam(aMsg, aIter, &aResult->message) &&
               ReadParam(aMsg, aIter, &aResult->refPoint) &&
               ReadParam(aMsg, aIter, &aResult->time) &&
               ReadParam(aMsg, aIter, &aResult->mFlags);
    aResult->eventStructType = static_cast<nsEventStructType>(eventStructType);
    return ret;
  }
};

template<>
struct ParamTraits<nsGUIEvent>
{
  typedef nsGUIEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<nsEvent>(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<nsEvent*>(aResult));
  }
};

template<>
struct ParamTraits<nsInputEvent>
{
  typedef nsInputEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<nsGUIEvent>(aParam));
    WriteParam(aMsg, aParam.modifiers);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<nsGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->modifiers);
  }
};

template<>
struct ParamTraits<nsMouseEvent_base>
{
  typedef nsMouseEvent_base paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<nsInputEvent>(aParam));
    WriteParam(aMsg, aParam.button);
    WriteParam(aMsg, aParam.buttons);
    WriteParam(aMsg, aParam.pressure);
    WriteParam(aMsg, aParam.inputSource);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<nsInputEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->button) &&
           ReadParam(aMsg, aIter, &aResult->buttons) &&
           ReadParam(aMsg, aIter, &aResult->pressure) &&
           ReadParam(aMsg, aIter, &aResult->inputSource);
  }
};

template<>
struct ParamTraits<mozilla::widget::WheelEvent>
{
  typedef mozilla::widget::WheelEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<nsMouseEvent_base>(aParam));
    WriteParam(aMsg, aParam.deltaX);
    WriteParam(aMsg, aParam.deltaY);
    WriteParam(aMsg, aParam.deltaZ);
    WriteParam(aMsg, aParam.deltaMode);
    WriteParam(aMsg, aParam.customizedByUserPrefs);
    WriteParam(aMsg, aParam.isMomentum);
    WriteParam(aMsg, aParam.isPixelOnlyDevice);
    WriteParam(aMsg, aParam.lineOrPageDeltaX);
    WriteParam(aMsg, aParam.lineOrPageDeltaY);
    WriteParam(aMsg, static_cast<int32_t>(aParam.scrollType));
    WriteParam(aMsg, aParam.overflowDeltaX);
    WriteParam(aMsg, aParam.overflowDeltaY);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    int32_t scrollType = 0;
    bool rv =
      ReadParam(aMsg, aIter, static_cast<nsMouseEvent_base*>(aResult)) &&
      ReadParam(aMsg, aIter, &aResult->deltaX) &&
      ReadParam(aMsg, aIter, &aResult->deltaY) &&
      ReadParam(aMsg, aIter, &aResult->deltaZ) &&
      ReadParam(aMsg, aIter, &aResult->deltaMode) &&
      ReadParam(aMsg, aIter, &aResult->customizedByUserPrefs) &&
      ReadParam(aMsg, aIter, &aResult->isMomentum) &&
      ReadParam(aMsg, aIter, &aResult->isPixelOnlyDevice) &&
      ReadParam(aMsg, aIter, &aResult->lineOrPageDeltaX) &&
      ReadParam(aMsg, aIter, &aResult->lineOrPageDeltaY) &&
      ReadParam(aMsg, aIter, &scrollType) &&
      ReadParam(aMsg, aIter, &aResult->overflowDeltaX) &&
      ReadParam(aMsg, aIter, &aResult->overflowDeltaY);
    aResult->scrollType =
      static_cast<mozilla::widget::WheelEvent::ScrollType>(scrollType);
    return rv;
  }
};

template<>
struct ParamTraits<nsMouseEvent>
{
  typedef nsMouseEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<nsMouseEvent_base>(aParam));
    WriteParam(aMsg, aParam.ignoreRootScrollFrame);
    WriteParam(aMsg, (uint8_t) aParam.reason);
    WriteParam(aMsg, (uint8_t) aParam.context);
    WriteParam(aMsg, (uint8_t) aParam.exit);
    WriteParam(aMsg, aParam.clickCount);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool rv;
    uint8_t reason = 0, context = 0, exit = 0;
    rv = ReadParam(aMsg, aIter, static_cast<nsMouseEvent_base*>(aResult)) &&
         ReadParam(aMsg, aIter, &aResult->ignoreRootScrollFrame) &&
         ReadParam(aMsg, aIter, &reason) &&
         ReadParam(aMsg, aIter, &context) &&
         ReadParam(aMsg, aIter, &exit) &&
         ReadParam(aMsg, aIter, &aResult->clickCount);
    aResult->reason = static_cast<nsMouseEvent::reasonType>(reason);
    aResult->context = static_cast<nsMouseEvent::contextType>(context);
    aResult->exit = static_cast<nsMouseEvent::exitType>(exit);
    return rv;
  }
};

template<>
struct ParamTraits<nsTouchEvent>
{
  typedef nsTouchEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<const nsInputEvent&>(aParam));
    // Sigh, nsDOMTouch bites us again!  We want to be able to do
    //   WriteParam(aMsg, aParam.touches);
    const nsTArray<nsCOMPtr<nsIDOMTouch> >& touches = aParam.touches;
    WriteParam(aMsg, touches.Length());
    for (uint32_t i = 0; i < touches.Length(); ++i) {
      nsDOMTouch* touch = static_cast<nsDOMTouch*>(touches[i].get());
      WriteParam(aMsg, touch->mIdentifier);
      WriteParam(aMsg, touch->mRefPoint);
      WriteParam(aMsg, touch->mRadius);
      WriteParam(aMsg, touch->mRotationAngle);
      WriteParam(aMsg, touch->mForce);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint32_t numTouches;
    if (!ReadParam(aMsg, aIter, static_cast<nsInputEvent*>(aResult)) ||
        !ReadParam(aMsg, aIter, &numTouches)) {
      return false;
    }
    for (uint32_t i = 0; i < numTouches; ++i) {
        int32_t identifier;
        nsIntPoint refPoint;
        nsIntPoint radius;
        float rotationAngle;
        float force;
        if (!ReadParam(aMsg, aIter, &identifier) ||
            !ReadParam(aMsg, aIter, &refPoint) ||
            !ReadParam(aMsg, aIter, &radius) ||
            !ReadParam(aMsg, aIter, &rotationAngle) ||
            !ReadParam(aMsg, aIter, &force)) {
          return false;
        }
        aResult->touches.AppendElement(
          new nsDOMTouch(identifier, refPoint, radius, rotationAngle, force));
    }
    return true;
  }
};

template<>
struct ParamTraits<nsKeyEvent>
{
  typedef nsKeyEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<nsInputEvent>(aParam));
    WriteParam(aMsg, aParam.keyCode);
    WriteParam(aMsg, aParam.charCode);
    WriteParam(aMsg, aParam.isChar);
    WriteParam(aMsg, aParam.location);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<nsInputEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->keyCode) &&
           ReadParam(aMsg, aIter, &aResult->charCode) &&
           ReadParam(aMsg, aIter, &aResult->isChar) &&
           ReadParam(aMsg, aIter, &aResult->location);
  }
};

template<>
struct ParamTraits<nsTextRangeStyle>
{
  typedef nsTextRangeStyle paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mDefinedStyles);
    WriteParam(aMsg, aParam.mLineStyle);
    WriteParam(aMsg, aParam.mIsBoldLine);
    WriteParam(aMsg, aParam.mForegroundColor);
    WriteParam(aMsg, aParam.mBackgroundColor);
    WriteParam(aMsg, aParam.mUnderlineColor);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
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
struct ParamTraits<nsTextRange>
{
  typedef nsTextRange paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mStartOffset);
    WriteParam(aMsg, aParam.mEndOffset);
    WriteParam(aMsg, aParam.mRangeType);
    WriteParam(aMsg, aParam.mRangeStyle);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mStartOffset) &&
           ReadParam(aMsg, aIter, &aResult->mEndOffset) &&
           ReadParam(aMsg, aIter, &aResult->mRangeType) &&
           ReadParam(aMsg, aIter, &aResult->mRangeStyle);
  }
};

template<>
struct ParamTraits<nsTextEvent>
{
  typedef nsTextEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<nsInputEvent>(aParam));
    WriteParam(aMsg, aParam.seqno);
    WriteParam(aMsg, aParam.theText);
    WriteParam(aMsg, aParam.isChar);
    WriteParam(aMsg, aParam.rangeCount);
    for (uint32_t index = 0; index < aParam.rangeCount; index++)
      WriteParam(aMsg, aParam.rangeArray[index]);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, static_cast<nsInputEvent*>(aResult)) ||
        !ReadParam(aMsg, aIter, &aResult->seqno) ||
        !ReadParam(aMsg, aIter, &aResult->theText) ||
        !ReadParam(aMsg, aIter, &aResult->isChar) ||
        !ReadParam(aMsg, aIter, &aResult->rangeCount))
      return false;

    if (!aResult->rangeCount) {
      aResult->rangeArray = nullptr;
      return true;
    }

    aResult->rangeArray = new nsTextRange[aResult->rangeCount];
    if (!aResult->rangeArray)
      return false;

    for (uint32_t index = 0; index < aResult->rangeCount; index++)
      if (!ReadParam(aMsg, aIter, &aResult->rangeArray[index])) {
        Free(*aResult);
        return false;
      }
    return true;
  }

  static void Free(const paramType& aResult)
  {
    if (aResult.rangeArray)
      delete [] aResult.rangeArray;
  }
};

template<>
struct ParamTraits<nsCompositionEvent>
{
  typedef nsCompositionEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<nsGUIEvent>(aParam));
    WriteParam(aMsg, aParam.seqno);
    WriteParam(aMsg, aParam.data);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<nsGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->seqno) &&
           ReadParam(aMsg, aIter, &aResult->data);
  }
};

template<>
struct ParamTraits<nsQueryContentEvent>
{
  typedef nsQueryContentEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<nsGUIEvent>(aParam));
    WriteParam(aMsg, aParam.mSucceeded);
    WriteParam(aMsg, aParam.mInput.mOffset);
    WriteParam(aMsg, aParam.mInput.mLength);
    WriteParam(aMsg, aParam.mReply.mOffset);
    WriteParam(aMsg, aParam.mReply.mString);
    WriteParam(aMsg, aParam.mReply.mRect);
    WriteParam(aMsg, aParam.mReply.mReversed);
    WriteParam(aMsg, aParam.mReply.mHasSelection);
    WriteParam(aMsg, aParam.mReply.mWidgetIsHit);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    aResult->mWasAsync = true;
    return ReadParam(aMsg, aIter, static_cast<nsGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mSucceeded) &&
           ReadParam(aMsg, aIter, &aResult->mInput.mOffset) &&
           ReadParam(aMsg, aIter, &aResult->mInput.mLength) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mOffset) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mString) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mRect) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mReversed) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mHasSelection) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mWidgetIsHit);
  }
};

template<>
struct ParamTraits<nsSelectionEvent>
{
  typedef nsSelectionEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<nsGUIEvent>(aParam));
    WriteParam(aMsg, aParam.seqno);
    WriteParam(aMsg, aParam.mOffset);
    WriteParam(aMsg, aParam.mLength);
    WriteParam(aMsg, aParam.mReversed);
    WriteParam(aMsg, aParam.mExpandToClusterBoundary);
    WriteParam(aMsg, aParam.mSucceeded);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<nsGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->seqno) &&
           ReadParam(aMsg, aIter, &aResult->mOffset) &&
           ReadParam(aMsg, aIter, &aResult->mLength) &&
           ReadParam(aMsg, aIter, &aResult->mReversed) &&
           ReadParam(aMsg, aIter, &aResult->mExpandToClusterBoundary) &&
           ReadParam(aMsg, aIter, &aResult->mSucceeded);
  }
};

template<>
struct ParamTraits<nsIMEUpdatePreference>
{
  typedef nsIMEUpdatePreference paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mWantUpdates);
    WriteParam(aMsg, aParam.mWantHints);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mWantUpdates) &&
           ReadParam(aMsg, aIter, &aResult->mWantHints);
  }
};

template<>
struct ParamTraits<nsPluginEvent>
{
  typedef nsPluginEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<nsGUIEvent>(aParam));
    WriteParam(aMsg, aParam.retargetToFocusedDocument);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<nsGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->retargetToFocusedDocument);
  }
};

} // namespace IPC

#endif // nsGUIEventIPC_h__

