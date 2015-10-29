/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BasicEvents_h__
#define mozilla_BasicEvents_h__

#include <stdint.h>

#include "mozilla/dom/EventTarget.h"
#include "mozilla/EventForwards.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsISupportsImpl.h"
#include "nsIWidget.h"
#include "nsString.h"
#include "Units.h"

namespace IPC {
template<typename T>
struct ParamTraits;
} // namespace IPC

namespace mozilla {

/******************************************************************************
 * mozilla::BaseEventFlags
 *
 * BaseEventFlags must be a POD struct for safe to use memcpy (including
 * in ParamTraits<BaseEventFlags>).  So don't make virtual methods, constructor,
 * destructor and operators.
 * This is necessary for VC which is NOT C++0x compiler.
 ******************************************************************************/

struct BaseEventFlags
{
public:
  // If mIsTrusted is true, the event is a trusted event.  Otherwise, it's
  // an untrusted event.
  bool    mIsTrusted : 1;
  // If mInBubblingPhase is true, the event is in bubbling phase or target
  // phase.
  bool    mInBubblingPhase : 1;
  // If mInCapturePhase is true, the event is in capture phase or target phase.
  bool    mInCapturePhase : 1;
  // If mInSystemGroup is true, the event is being dispatched in system group.
  bool    mInSystemGroup: 1;
  // If mCancelable is true, the event can be consumed.  I.e., calling
  // dom::Event::PreventDefault() can prevent the default action.
  bool    mCancelable : 1;
  // If mBubbles is true, the event can bubble.  Otherwise, cannot be handled
  // in bubbling phase.
  bool    mBubbles : 1;
  // If mPropagationStopped is true, dom::Event::StopPropagation() or
  // dom::Event::StopImmediatePropagation() has been called.
  bool    mPropagationStopped : 1;
  // If mImmediatePropagationStopped is true,
  // dom::Event::StopImmediatePropagation() has been called.
  // Note that mPropagationStopped must be true when this is true.
  bool    mImmediatePropagationStopped : 1;
  // If mDefaultPrevented is true, the event has been consumed.
  // E.g., dom::Event::PreventDefault() has been called or
  // the default action has been performed.
  bool    mDefaultPrevented : 1;
  // If mDefaultPreventedByContent is true, the event has been
  // consumed by content.
  // Note that mDefaultPrevented must be true when this is true.
  bool    mDefaultPreventedByContent : 1;
  // If mDefaultPreventedByChrome is true, the event has been
  // consumed by chrome.
  // Note that mDefaultPrevented must be true when this is true.
  bool    mDefaultPreventedByChrome : 1;
  // mMultipleActionsPrevented may be used when default handling don't want to
  // be prevented, but only one of the event targets should handle the event.
  // For example, when a <label> element is in another <label> element and
  // the first <label> element is clicked, that one may set this true.
  // Then, the second <label> element won't handle the event.
  bool    mMultipleActionsPrevented : 1;
  // If mIsBeingDispatched is true, the DOM event created from the event is
  // dispatching into the DOM tree and not completed.
  bool    mIsBeingDispatched : 1;
  // If mDispatchedAtLeastOnce is true, the event has been dispatched
  // as a DOM event and the dispatch has been completed.
  bool    mDispatchedAtLeastOnce : 1;
  // If mIsSynthesizedForTests is true, the event has been synthesized for
  // automated tests or something hacky approach of an add-on.
  bool    mIsSynthesizedForTests : 1;
  // If mExceptionHasBeenRisen is true, one of the event handlers has risen an
  // exception.
  bool    mExceptionHasBeenRisen : 1;
  // If mRetargetToNonNativeAnonymous is true and the target is in a non-native
  // native anonymous subtree, the event target is set to originalTarget.
  bool    mRetargetToNonNativeAnonymous : 1;
  // If mNoCrossProcessBoundaryForwarding is true, the event is not allowed to
  // cross process boundary.
  bool    mNoCrossProcessBoundaryForwarding : 1;
  // If mNoContentDispatch is true, the event is never dispatched to the
  // event handlers which are added to the contents, onfoo attributes and
  // properties.  Note that this flag is ignored when
  // EventChainPreVisitor::mForceContentDispatch is set true.  For exapmle,
  // window and document object sets it true.  Therefore, web applications
  // can handle the event if they add event listeners to the window or the
  // document.
  bool    mNoContentDispatch : 1;
  // If mOnlyChromeDispatch is true, the event is dispatched to only chrome.
  bool    mOnlyChromeDispatch : 1;
  // If mWantReplyFromContentProcess is true, the event will be redispatched
  // in the parent process after the content process has handled it. Useful
  // for when the parent process need the know first how the event was used
  // by content before handling it itself.
  bool mWantReplyFromContentProcess : 1;
  // The event's action will be handled by APZ. The main thread should not
  // perform its associated action. This is currently only relevant for
  // wheel and touch events.
  bool mHandledByAPZ : 1;

  // If the event is being handled in target phase, returns true.
  inline bool InTargetPhase() const
  {
    return (mInBubblingPhase && mInCapturePhase);
  }

  inline void Clear()
  {
    SetRawFlags(0);
  }
  // Get if either the instance's bit or the aOther's bit is true, the
  // instance's bit becomes true.  In other words, this works like:
  // eventFlags |= aOther;
  inline void Union(const BaseEventFlags& aOther)
  {
    RawFlags rawFlags = GetRawFlags() | aOther.GetRawFlags();
    SetRawFlags(rawFlags);
  }

private:
  typedef uint32_t RawFlags;

  inline void SetRawFlags(RawFlags aRawFlags)
  {
    static_assert(sizeof(BaseEventFlags) <= sizeof(RawFlags),
      "mozilla::EventFlags must not be bigger than the RawFlags");
    memcpy(this, &aRawFlags, sizeof(BaseEventFlags));
  }
  inline RawFlags GetRawFlags() const
  {
    RawFlags result = 0;
    memcpy(&result, this, sizeof(BaseEventFlags));
    return result;
  }
};

/******************************************************************************
 * mozilla::EventFlags
 ******************************************************************************/

struct EventFlags : public BaseEventFlags
{
  EventFlags()
  {
    Clear();
  }
};

/******************************************************************************
 * mozilla::WidgetEvent
 ******************************************************************************/

class WidgetEvent
{
protected:
  WidgetEvent(bool aIsTrusted,
              EventMessage aMessage,
              EventClassID aEventClassID)
    : mClass(aEventClassID)
    , mMessage(aMessage)
    , refPoint(0, 0)
    , lastRefPoint(0, 0)
    , time(0)
    , timeStamp(TimeStamp::Now())
    , userType(nullptr)
  {
    MOZ_COUNT_CTOR(WidgetEvent);
    mFlags.Clear();
    mFlags.mIsTrusted = aIsTrusted;
    mFlags.mCancelable = true;
    mFlags.mBubbles = true;
  }

  WidgetEvent()
  {
    MOZ_COUNT_CTOR(WidgetEvent);
  }

public:
  WidgetEvent(bool aIsTrusted, EventMessage aMessage)
    : mClass(eBasicEventClass)
    , mMessage(aMessage)
    , refPoint(0, 0)
    , lastRefPoint(0, 0)
    , time(0)
    , timeStamp(TimeStamp::Now())
    , userType(nullptr)
  {
    MOZ_COUNT_CTOR(WidgetEvent);
    mFlags.Clear();
    mFlags.mIsTrusted = aIsTrusted;
    mFlags.mCancelable = true;
    mFlags.mBubbles = true;
  }

  virtual ~WidgetEvent()
  {
    MOZ_COUNT_DTOR(WidgetEvent);
  }

  WidgetEvent(const WidgetEvent& aOther)
  {
    MOZ_COUNT_CTOR(WidgetEvent);
    *this = aOther;
  }

  virtual WidgetEvent* Duplicate() const
  {
    MOZ_ASSERT(mClass == eBasicEventClass,
               "Duplicate() must be overridden by sub class");
    WidgetEvent* result = new WidgetEvent(false, mMessage);
    result->AssignEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  EventClassID mClass;
  EventMessage mMessage;
  // Relative to the widget of the event, or if there is no widget then it is
  // in screen coordinates. Not modified by layout code.
  LayoutDeviceIntPoint refPoint;
  // The previous refPoint, if known, used to calculate mouse movement deltas.
  LayoutDeviceIntPoint lastRefPoint;
  // Elapsed time, in milliseconds, from a platform-specific zero time
  // to the time the message was created
  uint64_t time;
  // Timestamp when the message was created. Set in parallel to 'time' until we
  // determine if it is safe to drop 'time' (see bug 77992).
  mozilla::TimeStamp timeStamp;
  // See BaseEventFlags definition for the detail.
  BaseEventFlags mFlags;

  // Additional type info for user defined events
  nsCOMPtr<nsIAtom> userType;

  nsString typeString; // always set on non-main-thread events

  // Event targets, needed by DOM Events
  nsCOMPtr<dom::EventTarget> target;
  nsCOMPtr<dom::EventTarget> currentTarget;
  nsCOMPtr<dom::EventTarget> originalTarget;

  void AssignEventData(const WidgetEvent& aEvent, bool aCopyTargets)
  {
    // mClass should be initialized with the constructor.
    // mMessage should be initialized with the constructor.
    refPoint = aEvent.refPoint;
    // lastRefPoint doesn't need to be copied.
    time = aEvent.time;
    timeStamp = aEvent.timeStamp;
    // mFlags should be copied manually if it's necessary.
    userType = aEvent.userType;
    // typeString should be copied manually if it's necessary.
    target = aCopyTargets ? aEvent.target : nullptr;
    currentTarget = aCopyTargets ? aEvent.currentTarget : nullptr;
    originalTarget = aCopyTargets ? aEvent.originalTarget : nullptr;
  }

  void PreventDefault()
  {
    mFlags.mDefaultPrevented = true;
    mFlags.mDefaultPreventedByChrome = true;
  }

  /**
   * Utils for checking event types
   */

  /**
   * As*Event() returns the pointer of the instance only when the instance is
   * the class or one of its derived class.
   */
#define NS_ROOT_EVENT_CLASS(aPrefix, aName)
#define NS_EVENT_CLASS(aPrefix, aName) \
  virtual aPrefix##aName* As##aName(); \
  const aPrefix##aName* As##aName() const;

#include "mozilla/EventClassList.h"

#undef NS_EVENT_CLASS
#undef NS_ROOT_EVENT_CLASS

  /**
   * Returns true if the event is a query content event.
   */
  bool IsQueryContentEvent() const;
  /**
   * Returns true if the event is a selection event.
   */
  bool IsSelectionEvent() const;
  /**
   * Returns true if the event is a content command event.
   */
  bool IsContentCommandEvent() const;
  /**
   * Returns true if the event is a native event deliverer event for plugin.
   */
  bool IsNativeEventDelivererForPlugin() const;

  /**
   * Returns true if the event mMessage is one of mouse events.
   */
  bool HasMouseEventMessage() const;
  /**
   * Returns true if the event mMessage is one of drag events.
   */
  bool HasDragEventMessage() const;
  /**
   * Returns true if the event mMessage is one of key events.
   */
  bool HasKeyEventMessage() const;
  /**
   * Returns true if the event mMessage is one of composition events or text
   * event.
   */
  bool HasIMEEventMessage() const;
  /**
   * Returns true if the event mMessage is one of plugin activation events.
   */
  bool HasPluginActivationEventMessage() const;

  /**
   * Returns true if the event is native event deliverer event for plugin and
   * it should be retarted to focused document.
   */
  bool IsRetargetedNativeEventDelivererForPlugin() const;
  /**
   * Returns true if the event is native event deliverer event for plugin and
   * it should NOT be retarted to focused document.
   */
  bool IsNonRetargetedNativeEventDelivererForPlugin() const;
  /**
   * Returns true if the event is related to IME handling.  It includes
   * IME events, query content events and selection events.
   * Be careful when you use this.
   */
  bool IsIMERelatedEvent() const;

  /**
   * Whether the event should be handled by the frame of the mouse cursor
   * position or not.  When it should be handled there (e.g., the mouse events),
   * this returns true.
   */
  bool IsUsingCoordinates() const;
  /**
   * Whether the event should be handled by the focused DOM window in the
   * same top level window's or not.  E.g., key events, IME related events
   * (including the query content events, they are used in IME transaction)
   * should be handled by the (last) focused window rather than the dispatched
   * window.
   *
   * NOTE: Even if this returns true, the event isn't going to be handled by the
   * application level active DOM window which is on another top level window.
   * So, when the event is fired on a deactive window, the event is going to be
   * handled by the last focused DOM window in the last focused window.
   */
  bool IsTargetedAtFocusedWindow() const;
  /**
   * Whether the event should be handled by the focused content or not.  E.g.,
   * key events, IME related events and other input events which are not handled
   * by the frame of the mouse cursor position.
   *
   * NOTE: Even if this returns true, the event isn't going to be handled by the
   * application level active DOM window which is on another top level window.
   * So, when the event is fired on a deactive window, the event is going to be
   * handled by the last focused DOM element of the last focused DOM window in
   * the last focused window.
   */
  bool IsTargetedAtFocusedContent() const;
  /**
   * Whether the event should cause a DOM event.
   */
  bool IsAllowedToDispatchDOMEvent() const;
};

/******************************************************************************
 * mozilla::WidgetGUIEvent
 ******************************************************************************/

class WidgetGUIEvent : public WidgetEvent
{
protected:
  WidgetGUIEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget,
                 EventClassID aEventClassID)
    : WidgetEvent(aIsTrusted, aMessage, aEventClassID)
    , widget(aWidget)
  {
  }

  WidgetGUIEvent()
  {
  }

public:
  virtual WidgetGUIEvent* AsGUIEvent() override { return this; }

  WidgetGUIEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget)
    : WidgetEvent(aIsTrusted, aMessage, eGUIEventClass)
    , widget(aWidget)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eGUIEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetGUIEvent* result = new WidgetGUIEvent(false, mMessage, nullptr);
    result->AssignGUIEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  /// Originator of the event
  nsCOMPtr<nsIWidget> widget;

  /*
   * Explanation for this PluginEvent class:
   *
   * WidgetGUIEvent's mPluginEvent member used to be a void* pointer,
   * used to reference external, OS-specific data structures.
   *
   * That void* pointer wasn't serializable by itself, causing
   * certain plugin events not to function in e10s. See bug 586656.
   *
   * To make this serializable, we changed this void* pointer into
   * a proper buffer, and copy these external data structures into this
   * buffer.
   *
   * That buffer is PluginEvent::mBuffer below.
   *
   * We wrap this in that PluginEvent class providing operators to
   * be compatible with existing code that was written around
   * the old void* field.
   *
   * Ideally though, we wouldn't allow arbitrary reinterpret_cast'ing here;
   * instead, we would at least store type information here so that
   * this class can't be used to reinterpret one structure type into another.
   * We can also wonder if it would be possible to properly extend
   * WidgetGUIEvent and other Event classes to remove the need for this
   * mPluginEvent field.
   */
  class PluginEvent final
  {
    nsTArray<uint8_t> mBuffer;

    friend struct IPC::ParamTraits<mozilla::WidgetGUIEvent>;

  public:

    MOZ_EXPLICIT_CONVERSION operator bool() const
    {
      return !mBuffer.IsEmpty();
    }

    template<typename T>
    MOZ_EXPLICIT_CONVERSION operator const T*() const
    {
      return mBuffer.IsEmpty()
             ? nullptr
             : reinterpret_cast<const T*>(mBuffer.Elements());
    }

    template <typename T>
    void Copy(const T& other)
    {
      static_assert(!mozilla::IsPointer<T>::value, "Don't want a pointer!");
      mBuffer.SetLength(sizeof(T));
      memcpy(mBuffer.Elements(), &other, mBuffer.Length());
    }

    void Clear()
    {
      mBuffer.Clear();
    }
  };

  /// Event for NPAPI plugin
  PluginEvent mPluginEvent;

  void AssignGUIEventData(const WidgetGUIEvent& aEvent, bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    // widget should be initialized with the constructor.

    mPluginEvent = aEvent.mPluginEvent;
  }
};

/******************************************************************************
 * mozilla::Modifier
 *
 * All modifier keys should be defined here.  This is used for managing
 * modifier states for DOM Level 3 or later.
 ******************************************************************************/

enum Modifier
{
  MODIFIER_NONE       = 0x0000,
  MODIFIER_ALT        = 0x0001,
  MODIFIER_ALTGRAPH   = 0x0002,
  MODIFIER_CAPSLOCK   = 0x0004,
  MODIFIER_CONTROL    = 0x0008,
  MODIFIER_FN         = 0x0010,
  MODIFIER_FNLOCK     = 0x0020,
  MODIFIER_META       = 0x0040,
  MODIFIER_NUMLOCK    = 0x0080,
  MODIFIER_SCROLLLOCK = 0x0100,
  MODIFIER_SHIFT      = 0x0200,
  MODIFIER_SYMBOL     = 0x0400,
  MODIFIER_SYMBOLLOCK = 0x0800,
  MODIFIER_OS         = 0x1000
};

/******************************************************************************
 * Modifier key names.
 ******************************************************************************/

#define NS_DOM_KEYNAME_ALT        "Alt"
#define NS_DOM_KEYNAME_ALTGRAPH   "AltGraph"
#define NS_DOM_KEYNAME_CAPSLOCK   "CapsLock"
#define NS_DOM_KEYNAME_CONTROL    "Control"
#define NS_DOM_KEYNAME_FN         "Fn"
#define NS_DOM_KEYNAME_FNLOCK     "FnLock"
#define NS_DOM_KEYNAME_META       "Meta"
#define NS_DOM_KEYNAME_NUMLOCK    "NumLock"
#define NS_DOM_KEYNAME_SCROLLLOCK "ScrollLock"
#define NS_DOM_KEYNAME_SHIFT      "Shift"
#define NS_DOM_KEYNAME_SYMBOL     "Symbol"
#define NS_DOM_KEYNAME_SYMBOLLOCK "SymbolLock"
#define NS_DOM_KEYNAME_OS         "OS"

/******************************************************************************
 * mozilla::Modifiers
 ******************************************************************************/

typedef uint16_t Modifiers;

/******************************************************************************
 * mozilla::WidgetInputEvent
 ******************************************************************************/

class WidgetInputEvent : public WidgetGUIEvent
{
protected:
  WidgetInputEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget,
                   EventClassID aEventClassID)
    : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, aEventClassID)
    , modifiers(0)
  {
  }

  WidgetInputEvent()
  {
  }

public:
  virtual WidgetInputEvent* AsInputEvent() override { return this; }

  WidgetInputEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget)
    : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eInputEventClass)
    , modifiers(0)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eInputEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetInputEvent* result = new WidgetInputEvent(false, mMessage, nullptr);
    result->AssignInputEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }


  /**
   * Returns a modifier of "Accel" virtual modifier which is used for shortcut
   * key.
   */
  static Modifier AccelModifier();

  /**
   * GetModifier() returns a modifier flag which is activated by aDOMKeyName.
   */
  static Modifier GetModifier(const nsAString& aDOMKeyName);

  // true indicates the accel key on the environment is down
  bool IsAccel() const
  {
    return ((modifiers & AccelModifier()) != 0);
  }

  // true indicates the shift key is down
  bool IsShift() const
  {
    return ((modifiers & MODIFIER_SHIFT) != 0);
  }
  // true indicates the control key is down
  bool IsControl() const
  {
    return ((modifiers & MODIFIER_CONTROL) != 0);
  }
  // true indicates the alt key is down
  bool IsAlt() const
  {
    return ((modifiers & MODIFIER_ALT) != 0);
  }
  // true indicates the meta key is down (or, on Mac, the Command key)
  bool IsMeta() const
  {
    return ((modifiers & MODIFIER_META) != 0);
  }
  // true indicates the win key is down on Windows. Or the Super or Hyper key
  // is down on Linux.
  bool IsOS() const
  {
    return ((modifiers & MODIFIER_OS) != 0);
  }
  // true indicates the alt graph key is down
  // NOTE: on Mac, the option key press causes both IsAlt() and IsAltGrpah()
  //       return true.
  bool IsAltGraph() const
  {
    return ((modifiers & MODIFIER_ALTGRAPH) != 0);
  }
  // true indicates the CapLock LED is turn on.
  bool IsCapsLocked() const
  {
    return ((modifiers & MODIFIER_CAPSLOCK) != 0);
  }
  // true indicates the NumLock LED is turn on.
  bool IsNumLocked() const
  {
    return ((modifiers & MODIFIER_NUMLOCK) != 0);
  }
  // true indicates the ScrollLock LED is turn on.
  bool IsScrollLocked() const
  {
    return ((modifiers & MODIFIER_SCROLLLOCK) != 0);
  }

  // true indicates the Fn key is down, but this is not supported by native
  // key event on any platform.
  bool IsFn() const
  {
    return ((modifiers & MODIFIER_FN) != 0);
  }
  // true indicates the FnLock LED is turn on, but we don't know such
  // keyboards nor platforms.
  bool IsFnLocked() const
  {
    return ((modifiers & MODIFIER_FNLOCK) != 0);
  }
  // true indicates the Symbol is down, but this is not supported by native
  // key event on any platforms.
  bool IsSymbol() const
  {
    return ((modifiers & MODIFIER_SYMBOL) != 0);
  }
  // true indicates the SymbolLock LED is turn on, but we don't know such
  // keyboards nor platforms.
  bool IsSymbolLocked() const
  {
    return ((modifiers & MODIFIER_SYMBOLLOCK) != 0);
  }

  void InitBasicModifiers(bool aCtrlKey,
                          bool aAltKey,
                          bool aShiftKey,
                          bool aMetaKey)
  {
    modifiers = 0;
    if (aCtrlKey) {
      modifiers |= MODIFIER_CONTROL;
    }
    if (aAltKey) {
      modifiers |= MODIFIER_ALT;
    }
    if (aShiftKey) {
      modifiers |= MODIFIER_SHIFT;
    }
    if (aMetaKey) {
      modifiers |= MODIFIER_META;
    }
  }

  Modifiers modifiers;

  void AssignInputEventData(const WidgetInputEvent& aEvent, bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);

    modifiers = aEvent.modifiers;
  }
};

/******************************************************************************
 * mozilla::InternalUIEvent
 *
 * XXX Why this inherits WidgetGUIEvent rather than WidgetEvent?
 ******************************************************************************/

class InternalUIEvent : public WidgetGUIEvent
{
protected:
  InternalUIEvent()
    : detail(0)
    , mCausedByUntrustedEvent(false)
  {
  }

  InternalUIEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget,
                  EventClassID aEventClassID)
    : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, aEventClassID)
    , detail(0)
    , mCausedByUntrustedEvent(false)
  {
  }

  InternalUIEvent(bool aIsTrusted, EventMessage aMessage,
                  EventClassID aEventClassID)
    : WidgetGUIEvent(aIsTrusted, aMessage, nullptr, aEventClassID)
    , detail(0)
    , mCausedByUntrustedEvent(false)
  {
  }

public:
  virtual InternalUIEvent* AsUIEvent() override { return this; }

  /**
   * If the UIEvent is caused by another event (e.g., click event),
   * aEventCausesThisEvent should be the event.  If there is no such event,
   * this should be nullptr.
   */
  InternalUIEvent(bool aIsTrusted, EventMessage aMessage,
                  const WidgetEvent* aEventCausesThisEvent)
    : WidgetGUIEvent(aIsTrusted, aMessage, nullptr, eUIEventClass)
    , detail(0)
    , mCausedByUntrustedEvent(
        aEventCausesThisEvent && !aEventCausesThisEvent->mFlags.mIsTrusted)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eUIEventClass,
               "Duplicate() must be overridden by sub class");
    InternalUIEvent* result = new InternalUIEvent(false, mMessage, nullptr);
    result->AssignUIEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  int32_t detail;
  // mCausedByUntrustedEvent is true if the event is caused by untrusted event.
  bool mCausedByUntrustedEvent;

  // If you check the event is a trusted event and NOT caused by an untrusted
  // event, IsTrustable() returns what you expected.
  bool IsTrustable() const
  {
    return mFlags.mIsTrusted && !mCausedByUntrustedEvent;
  }

  void AssignUIEventData(const InternalUIEvent& aEvent, bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);

    detail = aEvent.detail;
    mCausedByUntrustedEvent = aEvent.mCausedByUntrustedEvent;
  }
};

} // namespace mozilla

#endif // mozilla_BasicEvents_h__
