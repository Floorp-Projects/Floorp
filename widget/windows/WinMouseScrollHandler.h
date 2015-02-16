/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_WinMouseScrollHandler_h__
#define mozilla_widget_WinMouseScrollHandler_h__

#include "nscore.h"
#include "nsDebug.h"
#include "mozilla/Assertions.h"
#include "mozilla/EventForwards.h"
#include "mozilla/TimeStamp.h"
#include "Units.h"
#include <windows.h>

class nsWindowBase;
struct nsIntPoint;

namespace mozilla {
namespace widget {

class ModifierKeyState;

struct MSGResult;

class MouseScrollHandler {
public:
  static MouseScrollHandler* GetInstance();

  static void Initialize();
  static void Shutdown();

  static bool NeedsMessage(UINT aMsg);
  static bool ProcessMessage(nsWindowBase* aWidget,
                             UINT msg,
                             WPARAM wParam,
                             LPARAM lParam,
                             MSGResult& aResult);

  /**
   * See nsIWidget::SynthesizeNativeMouseScrollEvent() for the detail about
   * this method.
   */
  static nsresult SynthesizeNativeMouseScrollEvent(nsWindowBase* aWidget,
                                                   const LayoutDeviceIntPoint& aPoint,
                                                   uint32_t aNativeMessage,
                                                   int32_t aDelta,
                                                   uint32_t aModifierFlags,
                                                   uint32_t aAdditionalFlags);

  /**
   * IsWaitingInternalMessage() returns true if MouseScrollHandler posted
   * an internal message for a native mouse wheel message and has not
   * received it. Otherwise, false.
   */
  static bool IsWaitingInternalMessage()
  {
    return sInstance && sInstance->mIsWaitingInternalMessage;
  }

private:
  MouseScrollHandler();
  ~MouseScrollHandler();

  bool mIsWaitingInternalMessage;

  static MouseScrollHandler* sInstance;

  /**
   * DispatchEvent() dispatches aEvent on aWidget.
   *
   * @return TRUE if the event was consumed.  Otherwise, FALSE.
   */
  static bool DispatchEvent(nsWindowBase* aWidget, WidgetGUIEvent& aEvent);

  /**
   * InitEvent() initializes the aEvent.  If aPoint is null, the result of
   * GetCurrentMessagePos() will be used.
   */
  static void InitEvent(nsWindowBase* aWidget,
                        WidgetGUIEvent& aEvent,
                        nsIntPoint* aPoint = nullptr);

  /**
   * GetModifierKeyState() returns current modifier key state.
   * Note that some devices need some hack for the modifier key state.
   * This method does it automatically.
   *
   * @param aMessage    Handling message.
   */
  static ModifierKeyState GetModifierKeyState(UINT aMessage);

  /**
   * MozGetMessagePos() returns the mouse cursor position when GetMessage()
   * was called last time.  However, if we're sending a native message,
   * this returns the specified cursor position by
   * SynthesizeNativeMouseScrollEvent().
   */
  static POINTS GetCurrentMessagePos();

  /**
   * ProcessNativeMouseWheelMessage() processes WM_MOUSEWHEEL and
   * WM_MOUSEHWHEEL.  Additionally, processes WM_VSCROLL and WM_HSCROLL if they
   * should be processed as mouse wheel message.
   * This method posts MOZ_WM_MOUSEVWHEEL, MOZ_WM_MOUSEHWHEEL,
   * MOZ_WM_VSCROLL or MOZ_WM_HSCROLL if we need to dispatch mouse scroll
   * events.  That avoids deadlock with plugin process.
   *
   * @param aWidget     A window which receives the message.
   * @param aMessage    WM_MOUSEWHEEL, WM_MOUSEHWHEEL, WM_VSCROLL or
   *                    WM_HSCROLL.
   * @param aWParam     The wParam value of the message.
   * @param aLParam     The lParam value of the message.
   */
  void ProcessNativeMouseWheelMessage(nsWindowBase* aWidget,
                                      UINT aMessage,
                                      WPARAM aWParam,
                                      LPARAM aLParam);

  /**
   * ProcessNativeScrollMessage() processes WM_VSCROLL and WM_HSCROLL.
   * This method just call ProcessMouseWheelMessage() if the message should be
   * processed as mouse wheel message.  Otherwise, dispatches a content
   * command event.
   *
   * @param aWidget     A window which receives the message.
   * @param aMessage    WM_VSCROLL or WM_HSCROLL.
   * @param aWParam     The wParam value of the message.
   * @param aLParam     The lParam value of the message.
   * @return            TRUE if the message is processed.  Otherwise, FALSE.
   */
  bool ProcessNativeScrollMessage(nsWindowBase* aWidget,
                                  UINT aMessage,
                                  WPARAM aWParam,
                                  LPARAM aLParam);

  /**
   * HandleMouseWheelMessage() processes MOZ_WM_MOUSEVWHEEL and
   * MOZ_WM_MOUSEHWHEEL which are posted when one of our windows received
   * WM_MOUSEWHEEL or WM_MOUSEHWHEEL for avoiding deadlock with OOPP.
   *
   * @param aWidget     A window which receives the wheel message.
   * @param aMessage    MOZ_WM_MOUSEWHEEL or MOZ_WM_MOUSEHWHEEL.
   * @param aWParam     The wParam value of the original message.
   * @param aLParam     The lParam value of the original message.
   */
  void HandleMouseWheelMessage(nsWindowBase* aWidget,
                               UINT aMessage,
                               WPARAM aWParam,
                               LPARAM aLParam);

  /**
   * HandleScrollMessageAsMouseWheelMessage() processes the MOZ_WM_VSCROLL and
   * MOZ_WM_HSCROLL which are posted when one of mouse windows received
   * WM_VSCROLL or WM_HSCROLL and user wants them to emulate mouse wheel
   * message's behavior.
   *
   * @param aWidget     A window which receives the scroll message.
   * @param aMessage    MOZ_WM_VSCROLL or MOZ_WM_HSCROLL.
   * @param aWParam     The wParam value of the original message.
   * @param aLParam     The lParam value of the original message.
   */
  void HandleScrollMessageAsMouseWheelMessage(nsWindowBase* aWidget,
                                              UINT aMessage,
                                              WPARAM aWParam,
                                              LPARAM aLParam);

  /**
   * ComputeMessagePos() computes the cursor position when the message was
   * added to the queue.
   *
   * @param aMessage    Handling message.
   * @param aWParam     Handling message's wParam.
   * @param aLParam     Handling message's lParam.
   * @return            Mouse cursor position when the message is added to
   *                    the queue or current cursor position if the result of
   *                    ::GetMessagePos() is broken.
   */
  POINT ComputeMessagePos(UINT aMessage,
                          WPARAM aWParam,
                          LPARAM aLParam);

  class EventInfo {
  public:
    /**
     * @param aWidget   An nsWindow which is handling the event.
     * @param aMessage  Must be WM_MOUSEWHEEL or WM_MOUSEHWHEEL.
     */
    EventInfo(nsWindowBase* aWidget, UINT aMessage, WPARAM aWParam, LPARAM aLParam);

    bool CanDispatchWheelEvent() const;

    int32_t GetNativeDelta() const { return mDelta; }
    HWND GetWindowHandle() const { return mWnd; }
    const TimeStamp& GetTimeStamp() const { return mTimeStamp; }
    bool IsVertical() const { return mIsVertical; }
    bool IsPositive() const { return (mDelta > 0); }
    bool IsPage() const { return mIsPage; }

    /**
     * @return          Number of lines or pages scrolled per WHEEL_DELTA.
     */
    int32_t GetScrollAmount() const;

  protected:
    EventInfo() :
      mIsVertical(false), mIsPage(false), mDelta(0), mWnd(nullptr)
    {
    }

    // TRUE if event is for vertical scroll.  Otherwise, FALSE.
    bool mIsVertical;
    // TRUE if event scrolls per page, otherwise, FALSE.
    bool mIsPage;
    // The native delta value.
    int32_t mDelta;
    // The window handle which is handling the event.
    HWND mWnd;
    // Timestamp of the event.
    TimeStamp mTimeStamp;
  };

  class LastEventInfo : public EventInfo {
  public:
    LastEventInfo() :
      EventInfo(), mAccumulatedDelta(0)
    {
    }

    /**
     * CanContinueTransaction() checks whether the new event can continue the
     * last transaction or not.  Note that if there is no transaction, this
     * returns true.
     */
    bool CanContinueTransaction(const EventInfo& aNewEvent);

    /**
     * ResetTransaction() resets the transaction, i.e., the instance forgets
     * the last event information.
     */
    void ResetTransaction();

    /**
     * RecordEvent() saves the information of new event.
     */
    void RecordEvent(const EventInfo& aEvent);

    /**
     * InitWheelEvent() initializes NS_WHEEL_WHEEL event and
     * recomputes the remaning detla for the event.
     * This must be called only once during handling a message and after
     * RecordEvent() is called.
     *
     * @param aWidget           A window which will dispatch the event.
     * @param aWheelEvent       An NS_WHEEL_WHEEL event, this will be
     *                          initialized.
     * @param aModKeyState      Current modifier key state.
     * @return                  TRUE if the event is ready to dispatch.
     *                          Otherwise, FALSE.
     */
    bool InitWheelEvent(nsWindowBase* aWidget,
                        WidgetWheelEvent& aWheelEvent,
                        const ModifierKeyState& aModKeyState);

  private:
    static int32_t RoundDelta(double aDelta);

    int32_t mAccumulatedDelta;
  };

  LastEventInfo mLastEventInfo;

  class SystemSettings {
  public:
    SystemSettings() : mInitialized(false) {}

    void Init();
    void MarkDirty();
    void NotifyUserPrefsMayOverrideSystemSettings();

    int32_t GetScrollAmount(bool aForVertical) const
    {
      MOZ_ASSERT(mInitialized, "SystemSettings must be initialized");
      return aForVertical ? mScrollLines : mScrollChars;
    }

    bool IsPageScroll(bool aForVertical) const
    {
      MOZ_ASSERT(mInitialized, "SystemSettings must be initialized");
      return aForVertical ? (mScrollLines == WHEEL_PAGESCROLL) :
                            (mScrollChars == WHEEL_PAGESCROLL);
    }

  private:
    bool mInitialized;
    int32_t mScrollLines;
    int32_t mScrollChars;
  };

  SystemSettings mSystemSettings;

  class UserPrefs {
  public:
    UserPrefs();
    ~UserPrefs();

    void MarkDirty();

    bool IsScrollMessageHandledAsWheelMessage()
    {
      Init();
      return mScrollMessageHandledAsWheelMessage;
    }

    int32_t GetOverriddenVerticalScrollAmout()
    {
      Init();
      return mOverriddenVerticalScrollAmount;
    }

    int32_t GetOverriddenHorizontalScrollAmout()
    {
      Init();
      return mOverriddenHorizontalScrollAmount;
    }

    int32_t GetMouseScrollTransactionTimeout()
    {
      Init();
      return mMouseScrollTransactionTimeout;
    }

  private:
    void Init();

    static void OnChange(const char* aPrefName, void* aClosure)
    {
      static_cast<UserPrefs*>(aClosure)->MarkDirty();
    }

    bool mInitialized;
    bool mScrollMessageHandledAsWheelMessage;
    int32_t mOverriddenVerticalScrollAmount;
    int32_t mOverriddenHorizontalScrollAmount;
    int32_t mMouseScrollTransactionTimeout;
  };

  UserPrefs mUserPrefs;

  class SynthesizingEvent {
  public:
    SynthesizingEvent() :
      mWnd(nullptr), mMessage(0), mWParam(0), mLParam(0),
      mStatus(NOT_SYNTHESIZING)
    {
    }

    ~SynthesizingEvent() {}

    static bool IsSynthesizing();

    nsresult Synthesize(const POINTS& aCursorPoint, HWND aWnd,
                        UINT aMessage, WPARAM aWParam, LPARAM aLParam,
                        const BYTE (&aKeyStates)[256]);

    void NativeMessageReceived(nsWindowBase* aWidget, UINT aMessage,
                               WPARAM aWParam, LPARAM aLParam);

    void NotifyNativeMessageHandlingFinished();
    void NotifyInternalMessageHandlingFinished();

    const POINTS& GetCursorPoint() const { return mCursorPoint; }

  private:
    POINTS mCursorPoint;
    HWND mWnd;
    UINT mMessage;
    WPARAM mWParam;
    LPARAM mLParam;
    BYTE mKeyState[256];
    BYTE mOriginalKeyState[256];

    enum Status {
      NOT_SYNTHESIZING,
      SENDING_MESSAGE,
      NATIVE_MESSAGE_RECEIVED,
      INTERNAL_MESSAGE_POSTED,
    };
    Status mStatus;

#ifdef PR_LOGGING
    const char* GetStatusName()
    {
      switch (mStatus) {
        case NOT_SYNTHESIZING:
          return "NOT_SYNTHESIZING";
        case SENDING_MESSAGE:
          return "SENDING_MESSAGE";
        case NATIVE_MESSAGE_RECEIVED:
          return "NATIVE_MESSAGE_RECEIVED";
        case INTERNAL_MESSAGE_POSTED:
          return "INTERNAL_MESSAGE_POSTED";
        default:
          return "Unknown";
      }
    }
#endif

    void Finish();
  }; // SynthesizingEvent

  SynthesizingEvent* mSynthesizingEvent;

public:

  class Device {
  public:
    class Elantech {
    public:
      /**
       * GetDriverMajorVersion() returns the installed driver's major version.
       * If Elantech's driver was installed, returns 0.
       */
      static int32_t GetDriverMajorVersion();

      /**
       * IsHelperWindow() checks whether aWnd is a helper window of Elantech's
       * touchpad.  Returns TRUE if so.  Otherwise, FALSE.
       */
      static bool IsHelperWindow(HWND aWnd);

      /**
       * Key message handler for Elantech's hack.  Returns TRUE if the message
       * is consumed by this handler.  Otherwise, FALSE.
       */
      static bool HandleKeyMessage(nsWindowBase* aWidget,
                                   UINT aMsg,
                                   WPARAM aWParam);

      static void UpdateZoomUntil();
      static bool IsZooming();

      static void Init();

      static bool IsPinchHackNeeded() { return sUsePinchHack; }


    private:
      // Whether to enable the Elantech swipe gesture hack.
      static bool sUseSwipeHack;
      // Whether to enable the Elantech pinch-to-zoom gesture hack.
      static bool sUsePinchHack;
      static DWORD sZoomUntil;
    }; // class Elantech

    class TrackPoint {
    public:
      /**
       * IsDriverInstalled() returns TRUE if TrackPoint's driver is installed.
       * Otherwise, returns FALSE.
       */
      static bool IsDriverInstalled();
    }; // class TrackPoint

    class UltraNav {
    public:
      /**
       * IsObsoleteDriverInstalled() checks whether obsoleted UltraNav
       * is installed on the environment.
       * Returns TRUE if it was installed.  Otherwise, FALSE.
       */
      static bool IsObsoleteDriverInstalled();
    }; // class UltraNav

    class SetPoint {
    public:
      /**
       * SetPoint, Logitech's mouse driver, may report wrong cursor position
       * for WM_MOUSEHWHEEL message.  See comment in the implementation for
       * the detail.
       */
      static bool IsGetMessagePosResponseValid(UINT aMessage,
                                               WPARAM aWParam,
                                               LPARAM aLParam);
    private:
      static bool sMightBeUsing;
    };

    static void Init();

    static bool IsFakeScrollableWindowNeeded()
    {
      return sFakeScrollableWindowNeeded;
    }

  private:
    /**
     * Gets the bool value of aPrefName used to enable or disable an input
     * workaround (like the Trackpoint hack).  The pref can take values 0 (for
     * disabled), 1 (for enabled) or -1 (to automatically detect whether to
     * enable the workaround).
     *
     * @param aPrefName The name of the pref.
     * @param aValueIfAutomatic Whether the given input workaround should be
     *                          enabled by default.
     */
    static bool GetWorkaroundPref(const char* aPrefName,
                                  bool aValueIfAutomatic);

    static bool sFakeScrollableWindowNeeded;
  }; // class Device
};

} // namespace widget
} // namespace mozilla

#endif // mozilla_widget_WinMouseScrollHandler_h__
