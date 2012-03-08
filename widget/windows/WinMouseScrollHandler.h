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
#include "mozilla/TimeStamp.h"
#include <windows.h>

class nsWindow;
class nsGUIEvent;
class nsMouseScrollEvent;
struct nsModifierKeyState;

namespace mozilla {
namespace widget {

class MouseScrollHandler {
public:
  static MouseScrollHandler* GetInstance();

  static void Initialize();
  static void Shutdown();

  static bool ProcessMessage(nsWindow* aWindow,
                             UINT msg,
                             WPARAM wParam,
                             LPARAM lParam,
                             LRESULT *aRetValue,
                             bool &aEatMessage);

private:
  MouseScrollHandler();
  ~MouseScrollHandler();

  static MouseScrollHandler* sInstance;

  /**
   * DispatchEvent() dispatches aEvent on aWindow.
   *
   * @return TRUE if the event was consumed.  Otherwise, FALSE.
   */
  static bool DispatchEvent(nsWindow* aWindow, nsGUIEvent& aEvent);

  /**
   * GetModifierKeyState() returns current modifier key state.
   * Note that some devices need some hack for the modifier key state.
   * This method does it automatically.
   *
   * @param aMessage    Handling message.
   */
  static nsModifierKeyState GetModifierKeyState(UINT aMessage);

  /**
   * ProcessNativeMouseWheelMessage() processes WM_MOUSEWHEEL and
   * WM_MOUSEHWHEEL.  Additionally, processes WM_VSCROLL and WM_HSCROLL if they
   * should be processed as mouse wheel message.
   * This method posts MOZ_WM_MOUSEVWHEEL, MOZ_WM_MOUSEHWHEEL,
   * MOZ_WM_VSCROLL or MOZ_WM_HSCROLL if we need to dispatch mouse scroll
   * events.  That avoids deadlock with plugin process.
   *
   * @param aWindow     A window which receives the message.
   * @param aMessage    WM_MOUSEWHEEL, WM_MOUSEHWHEEL, WM_VSCROLL or
   *                    WM_HSCROLL.
   * @param aWParam     The wParam value of the message.
   * @param aLParam     The lParam value of the message.
   */
  void ProcessNativeMouseWheelMessage(nsWindow* aWindow,
                                      UINT aMessage,
                                      WPARAM aWParam,
                                      LPARAM aLParam);

  /**
   * ProcessNativeScrollMessage() processes WM_VSCROLL and WM_HSCROLL.
   * This method just call ProcessMouseWheelMessage() if the message should be
   * processed as mouse wheel message.  Otherwise, dispatches a content
   * command event.
   *
   * @param aWindow     A window which receives the message.
   * @param aMessage    WM_VSCROLL or WM_HSCROLL.
   * @param aWParam     The wParam value of the message.
   * @param aLParam     The lParam value of the message.
   * @return            TRUE if the message is processed.  Otherwise, FALSE.
   */
  bool ProcessNativeScrollMessage(nsWindow* aWindow,
                                  UINT aMessage,
                                  WPARAM aWParam,
                                  LPARAM aLParam);

  /**
   * HandleMouseWheelMessage() processes MOZ_WM_MOUSEVWHEEL and
   * MOZ_WM_MOUSEHWHEEL which are posted when one of our windows received
   * WM_MOUSEWHEEL or WM_MOUSEHWHEEL for avoiding deadlock with OOPP.
   *
   * @param aWindow     A window which receives the wheel message.
   * @param aMessage    MOZ_WM_MOUSEWHEEL or MOZ_WM_MOUSEHWHEEL.
   * @param aWParam     The wParam value of the original message.
   * @param aLParam     The lParam value of the original message.
   */
  void HandleMouseWheelMessage(nsWindow* aWindow,
                               UINT aMessage,
                               WPARAM aWParam,
                               LPARAM aLParam);

  /**
   * HandleScrollMessageAsMouseWheelMessage() processes the MOZ_WM_VSCROLL and
   * MOZ_WM_HSCROLL which are posted when one of mouse windows received
   * WM_VSCROLL or WM_HSCROLL and user wants them to emulate mouse wheel
   * message's behavior.
   *
   * @param aWindow     A window which receives the scroll message.
   * @param aMessage    MOZ_WM_VSCROLL or MOZ_WM_HSCROLL.
   * @param aWParam     The wParam value of the original message.
   * @param aLParam     The lParam value of the original message.
   */
  void HandleScrollMessageAsMouseWheelMessage(nsWindow* aWindow,
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

  class EventInfo;
  /**
   * GetScrollTargetInfo() returns scroll target information which is
   * computed from the result of NS_QUERY_SCROLL_TARGET_INFO event.
   *
   * @param aWindow           An nsWindow which is handling the event.
   * @param aEventInfo        The EventInfo which is being handled.
   * @param aModifierKeyState The modifier key state.
   */
  struct ScrollTargetInfo {
    // TRUE if pixel scroll event is needed.  Otherwise, FALSE.
    bool dispatchPixelScrollEvent;
    // TRUE if pixel scroll event's delta value should be reversed.
    // Otherwise, FALSE.
    bool reversePixelScrollDirection;
    // Actual scroll amount.  It might be computed with user prefs.
    PRInt32 actualScrollAmount;
    // Actual scroll action.  It might be computed with user prefs.
    // The value is one of nsQueryContentEvent::SCROLL_ACTION_*.
    PRInt32 actualScrollAction;
    // Pixels per unit (line or page, depends on the action).
    PRInt32 pixelsPerUnit;
  };
  ScrollTargetInfo GetScrollTargetInfo(
                     nsWindow* aWindow,
                     const EventInfo& aEvent,
                     const nsModifierKeyState& aModiferKeyState);

  class EventInfo {
  public:
    /**
     * @param aWindow   An nsWindow which is handling the event.
     * @param aMessage  Must be WM_MOUSEWHEEL or WM_MOUSEHWHEEL.
     */
    EventInfo(nsWindow* aWindow, UINT aMessage, WPARAM aWParam, LPARAM aLParam);

    bool CanDispatchMouseScrollEvent() const;

    PRInt32 GetNativeDelta() const { return mDelta; }
    HWND GetWindowHandle() const { return mWnd; }
    const TimeStamp& GetTimeStamp() const { return mTimeStamp; }
    bool IsVertical() const { return mIsVertical; }
    bool IsPositive() const { return (mDelta > 0); }
    bool IsPage() const { return mIsPage; }

    /**
     * @return          Number of lines or pages scrolled per WHEEL_DELTA.
     */
    PRInt32 GetScrollAmount() const;

    /**
     * @return          One or more values of
     *                  nsMouseScrollEvent::nsMouseScrollFlags.
     */
    PRInt32 GetScrollFlags() const;

  protected:
    EventInfo() :
      mIsVertical(false), mIsPage(false), mDelta(0), mWnd(nsnull)
    {
    }

    // TRUE if event is for vertical scroll.  Otherwise, FALSE.
    bool mIsVertical;
    // TRUE if event scrolls per page, otherwise, FALSE.
    bool mIsPage;
    // The native delta value.
    PRInt32 mDelta;
    // The window handle which is handling the event.
    HWND mWnd;
    // Timestamp of the event.
    TimeStamp mTimeStamp;
  };

  class LastEventInfo : public EventInfo {
  public:
    LastEventInfo() :
      EventInfo(), mRemainingDeltaForScroll(0), mRemainingDeltaForPixel(0)
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
     * InitMouseScrollEvent() initializes NS_MOUSE_SCROLL event and
     * recomputes the remaning detla for the event.
     * This must be called only once during handling a message and after
     * RecordEvent() is called.
     *
     * @param aWindow           A window which will dispatch the event.
     * @param aMouseScrollEvent An NS_MOUSE_SCROLL event, this will be
     *                          initialized.
     * @param aScrollTargetInfo The result of GetScrollTargetInfo().
     * @param aModKeyState      Current modifier key state.
     * @return                  TRUE if the event is ready to dispatch.
     *                          Otherwise, FALSE.
     */
    bool InitMouseScrollEvent(nsWindow* aWindow,
                              nsMouseScrollEvent& aMouseScrollEvent,
                              const ScrollTargetInfo& aScrollTargetInfo,
                              const nsModifierKeyState& aModKeyState);

    /**
     * InitMousePixelScrollEvent() initializes NS_MOUSE_PIXEL_SCROLL event and
     * recomputes the remaning detla for the event.
     * This must be called only once during handling a message and after
     * RecordEvent() is called.
     *
     * @param aWindow           A window which will dispatch the event.
     * @param aMouseScrollEvent An NS_MOUSE_PIXEL_SCROLL event, this will be
     *                          initialized.
     * @param aScrollTargetInfo The result of GetScrollTargetInfo().
     * @param aModKeyState      Current modifier key state.
     * @return                  TRUE if the event is ready to dispatch.
     *                          Otherwise, FALSE.
     */
    bool InitMousePixelScrollEvent(nsWindow* aWindow,
                                   nsMouseScrollEvent& aPixelScrollEvent,
                                   const ScrollTargetInfo& aScrollTargetInfo,
                                   const nsModifierKeyState& aModKeyState);

  private:
    static PRInt32 RoundDelta(double aDelta);

    // The remaining native delta value (i.e., not handled by previous
    // message handler).
    PRInt32 mRemainingDeltaForScroll;
    PRInt32 mRemainingDeltaForPixel;
  };

  LastEventInfo mLastEventInfo;

  class SystemSettings {
  public:
    SystemSettings() : mInitialized(false) {}

    void Init();
    void MarkDirty();

    PRInt32 GetScrollAmount(bool aForVertical) const
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
    PRInt32 mScrollLines;
    PRInt32 mScrollChars;
  };

  SystemSettings mSystemSettings;

  class UserPrefs {
  public:
    UserPrefs();
    ~UserPrefs();

    void MarkDirty();

    bool IsPixelScrollingEnabled()
    {
      Init();
      return mPixelScrollingEnabled;
    }

    bool IsScrollMessageHandledAsWheelMessage()
    {
      Init();
      return mScrollMessageHandledAsWheelMessage;
    }

  private:
    void Init();

    static int OnChange(const char* aPrefName, void* aClosure)
    {
      static_cast<UserPrefs*>(aClosure)->MarkDirty();
      return 0;
    }

    bool mInitialized;
    bool mPixelScrollingEnabled;
    bool mScrollMessageHandledAsWheelMessage;
  };

  UserPrefs mUserPrefs;

public:

  class Device {
  public:
    class Elantech {
    public:
      /**
       * GetDriverMajorVersion() returns the installed driver's major version.
       * If Elantech's driver was installed, returns 0.
       */
      static PRInt32 GetDriverMajorVersion();

      /**
       * IsHelperWindow() checks whether aWnd is a helper window of Elantech's
       * touchpad.  Returns TRUE if so.  Otherwise, FALSE.
       */
      static bool IsHelperWindow(HWND aWnd);

      /**
       * Key message handler for Elantech's hack.  Returns TRUE if the message
       * is consumed by this handler.  Otherwise, FALSE.
       */
      static bool HandleKeyMessage(nsWindow* aWindow,
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