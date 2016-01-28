/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "mozilla/Logging.h"

#include "WinMouseScrollHandler.h"
#include "nsWindow.h"
#include "nsWindowDefs.h"
#include "KeyboardLayout.h"
#include "WinUtils.h"
#include "nsGkAtoms.h"
#include "nsIDOMWindowUtils.h"
#include "nsIDOMWheelEvent.h"

#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/WindowsVersion.h"

#include <psapi.h>

namespace mozilla {
namespace widget {

PRLogModuleInfo* gMouseScrollLog = nullptr;

static const char* GetBoolName(bool aBool)
{
  return aBool ? "TRUE" : "FALSE";
}

static void LogKeyStateImpl()
{
  if (!MOZ_LOG_TEST(gMouseScrollLog, LogLevel::Debug)) {
    return;
  }
  BYTE keyboardState[256];
  if (::GetKeyboardState(keyboardState)) {
    for (size_t i = 0; i < ArrayLength(keyboardState); i++) {
      if (keyboardState[i]) {
        MOZ_LOG(gMouseScrollLog, LogLevel::Debug,
          ("    Current key state: keyboardState[0x%02X]=0x%02X (%s)",
           i, keyboardState[i],
           ((keyboardState[i] & 0x81) == 0x81) ? "Pressed and Toggled" :
           (keyboardState[i] & 0x80) ? "Pressed" :
           (keyboardState[i] & 0x01) ? "Toggled" : "Unknown"));
      }
    }
  } else {
    MOZ_LOG(gMouseScrollLog, LogLevel::Debug,
      ("MouseScroll::Device::Elantech::HandleKeyMessage(): Failed to print "
       "current keyboard state"));
  }
}

#define LOG_KEYSTATE() LogKeyStateImpl()

MouseScrollHandler* MouseScrollHandler::sInstance = nullptr;

bool MouseScrollHandler::Device::sFakeScrollableWindowNeeded = false;

bool MouseScrollHandler::Device::SynTP::sInitialized = false;
int32_t MouseScrollHandler::Device::SynTP::sMajorVersion = 0;
int32_t MouseScrollHandler::Device::SynTP::sMinorVersion = -1;

bool MouseScrollHandler::Device::Elantech::sUseSwipeHack = false;
bool MouseScrollHandler::Device::Elantech::sUsePinchHack = false;
DWORD MouseScrollHandler::Device::Elantech::sZoomUntil = 0;

bool MouseScrollHandler::Device::Apoint::sInitialized = false;
int32_t MouseScrollHandler::Device::Apoint::sMajorVersion = 0;
int32_t MouseScrollHandler::Device::Apoint::sMinorVersion = -1;

bool MouseScrollHandler::Device::SetPoint::sMightBeUsing = false;

// The duration until timeout of events transaction.  The value is 1.5 sec,
// it's just a magic number, it was suggested by Logitech's engineer, see
// bug 605648 comment 90.
#define DEFAULT_TIMEOUT_DURATION 1500

/******************************************************************************
 *
 * MouseScrollHandler
 *
 ******************************************************************************/

/* static */
POINTS
MouseScrollHandler::GetCurrentMessagePos()
{
  if (SynthesizingEvent::IsSynthesizing()) {
    return sInstance->mSynthesizingEvent->GetCursorPoint();
  }
  DWORD pos = ::GetMessagePos();
  return MAKEPOINTS(pos);
}

// Get rid of the GetMessagePos() API.
#define GetMessagePos()

/* static */
void
MouseScrollHandler::Initialize()
{
  if (!gMouseScrollLog) {
    gMouseScrollLog = PR_NewLogModule("MouseScrollHandlerWidgets");
  }
  Device::Init();
}

/* static */
void
MouseScrollHandler::Shutdown()
{
  delete sInstance;
  sInstance = nullptr;
}

/* static */
MouseScrollHandler*
MouseScrollHandler::GetInstance()
{
  if (!sInstance) {
    sInstance = new MouseScrollHandler();
  }
  return sInstance;
}

MouseScrollHandler::MouseScrollHandler() :
  mIsWaitingInternalMessage(false),
  mSynthesizingEvent(nullptr)
{
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll: Creating an instance, this=%p, sInstance=%p",
     this, sInstance));
}

MouseScrollHandler::~MouseScrollHandler()
{
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll: Destroying an instance, this=%p, sInstance=%p",
     this, sInstance));

  delete mSynthesizingEvent;
}

/* static */
bool
MouseScrollHandler::NeedsMessage(UINT aMsg)
{
  switch (aMsg) {
    case WM_SETTINGCHANGE:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_HSCROLL:
    case WM_VSCROLL:
    case MOZ_WM_MOUSEVWHEEL:
    case MOZ_WM_MOUSEHWHEEL:
    case MOZ_WM_HSCROLL:
    case MOZ_WM_VSCROLL:
    case WM_KEYDOWN:
    case WM_KEYUP:
      return true;
  }
  return false;
}

/* static */
bool
MouseScrollHandler::ProcessMessage(nsWindowBase* aWidget, UINT msg,
                                   WPARAM wParam, LPARAM lParam,
                                   MSGResult& aResult)
{
  Device::Elantech::UpdateZoomUntil();

  switch (msg) {
    case WM_SETTINGCHANGE:
      if (!sInstance) {
        return false;
      }
      if (wParam == SPI_SETWHEELSCROLLLINES ||
          wParam == SPI_SETWHEELSCROLLCHARS) {
        sInstance->mSystemSettings.MarkDirty();
      }
      return false;

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      GetInstance()->
        ProcessNativeMouseWheelMessage(aWidget, msg, wParam, lParam);
      sInstance->mSynthesizingEvent->NotifyNativeMessageHandlingFinished();
      // We don't need to call next wndproc for WM_MOUSEWHEEL and
      // WM_MOUSEHWHEEL.  We should consume them always.  If the messages
      // would be handled by our window again, it caused making infinite
      // message loop.
      aResult.mConsumed = true;
      aResult.mResult = (msg != WM_MOUSEHWHEEL);
      return true;

    case WM_HSCROLL:
    case WM_VSCROLL:
      aResult.mConsumed =
        GetInstance()->ProcessNativeScrollMessage(aWidget, msg, wParam, lParam);
      sInstance->mSynthesizingEvent->NotifyNativeMessageHandlingFinished();
      aResult.mResult = 0;
      return true;

    case MOZ_WM_MOUSEVWHEEL:
    case MOZ_WM_MOUSEHWHEEL:
      GetInstance()->HandleMouseWheelMessage(aWidget, msg, wParam, lParam);
      sInstance->mSynthesizingEvent->NotifyInternalMessageHandlingFinished();
      // Doesn't need to call next wndproc for internal wheel message.
      aResult.mConsumed = true;
      return true;

    case MOZ_WM_HSCROLL:
    case MOZ_WM_VSCROLL:
      GetInstance()->
        HandleScrollMessageAsMouseWheelMessage(aWidget, msg, wParam, lParam);
      sInstance->mSynthesizingEvent->NotifyInternalMessageHandlingFinished();
      // Doesn't need to call next wndproc for internal scroll message.
      aResult.mConsumed = true;
      return true;

    case WM_KEYDOWN:
    case WM_KEYUP:
      MOZ_LOG(gMouseScrollLog, LogLevel::Info,
        ("MouseScroll::ProcessMessage(): aWidget=%p, "
         "msg=%s(0x%04X), wParam=0x%02X, ::GetMessageTime()=%d",
         aWidget, msg == WM_KEYDOWN ? "WM_KEYDOWN" :
                    msg == WM_KEYUP ? "WM_KEYUP" : "Unknown", msg, wParam,
         ::GetMessageTime()));
      LOG_KEYSTATE();
      if (Device::Elantech::HandleKeyMessage(aWidget, msg, wParam, lParam)) {
        aResult.mResult = 0;
        aResult.mConsumed = true;
        return true;
      }
      return false;

    default:
      return false;
  }
}

/* static */
nsresult
MouseScrollHandler::SynthesizeNativeMouseScrollEvent(nsWindowBase* aWidget,
                                                     const LayoutDeviceIntPoint& aPoint,
                                                     uint32_t aNativeMessage,
                                                     int32_t aDelta,
                                                     uint32_t aModifierFlags,
                                                     uint32_t aAdditionalFlags)
{
  bool useFocusedWindow =
    !(aAdditionalFlags & nsIDOMWindowUtils::MOUSESCROLL_PREFER_WIDGET_AT_POINT);

  POINT pt;
  pt.x = aPoint.x;
  pt.y = aPoint.y;

  HWND target = useFocusedWindow ? ::WindowFromPoint(pt) : ::GetFocus();
  NS_ENSURE_TRUE(target, NS_ERROR_FAILURE);

  WPARAM wParam = 0;
  LPARAM lParam = 0;
  switch (aNativeMessage) {
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL: {
      lParam = MAKELPARAM(pt.x, pt.y);
      WORD mod = 0;
      if (aModifierFlags & (nsIWidget::CTRL_L | nsIWidget::CTRL_R)) {
        mod |= MK_CONTROL;
      }
      if (aModifierFlags & (nsIWidget::SHIFT_L | nsIWidget::SHIFT_R)) {
        mod |= MK_SHIFT;
      }
      wParam = MAKEWPARAM(mod, aDelta);
      break;
    }
    case WM_VSCROLL:
    case WM_HSCROLL:
      lParam = (aAdditionalFlags &
                  nsIDOMWindowUtils::MOUSESCROLL_WIN_SCROLL_LPARAM_NOT_NULL) ?
        reinterpret_cast<LPARAM>(target) : 0;
      wParam = aDelta;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  // Ensure to make the instance.
  GetInstance();

  BYTE kbdState[256];
  memset(kbdState, 0, sizeof(kbdState));

  nsAutoTArray<KeyPair,10> keySequence;
  WinUtils::SetupKeyModifiersSequence(&keySequence, aModifierFlags);

  for (uint32_t i = 0; i < keySequence.Length(); ++i) {
    uint8_t key = keySequence[i].mGeneral;
    uint8_t keySpecific = keySequence[i].mSpecific;
    kbdState[key] = 0x81; // key is down and toggled on if appropriate
    if (keySpecific) {
      kbdState[keySpecific] = 0x81;
    }
  }

  if (!sInstance->mSynthesizingEvent) {
    sInstance->mSynthesizingEvent = new SynthesizingEvent();
  }

  POINTS pts;
  pts.x = static_cast<SHORT>(pt.x);
  pts.y = static_cast<SHORT>(pt.y);
  return sInstance->mSynthesizingEvent->
           Synthesize(pts, target, aNativeMessage, wParam, lParam, kbdState);
}

/* static */
void
MouseScrollHandler::InitEvent(nsWindowBase* aWidget,
                              WidgetGUIEvent& aEvent,
                              LayoutDeviceIntPoint* aPoint)
{
  NS_ENSURE_TRUE_VOID(aWidget);
  LayoutDeviceIntPoint point;
  if (aPoint) {
    point = *aPoint;
  } else {
    POINTS pts = GetCurrentMessagePos();
    POINT pt;
    pt.x = pts.x;
    pt.y = pts.y;
    ::ScreenToClient(aWidget->GetWindowHandle(), &pt);
    point.x = pt.x;
    point.y = pt.y;
  }
  aWidget->InitEvent(aEvent, &point);
}

/* static */
ModifierKeyState
MouseScrollHandler::GetModifierKeyState(UINT aMessage)
{
  ModifierKeyState result;
  // Assume the Control key is down if the Elantech touchpad has sent the
  // mis-ordered WM_KEYDOWN/WM_MOUSEWHEEL messages.  (See the comment in
  // MouseScrollHandler::Device::Elantech::HandleKeyMessage().)
  if ((aMessage == MOZ_WM_MOUSEVWHEEL || aMessage == WM_MOUSEWHEEL) &&
      !result.IsControl() && Device::Elantech::IsZooming()) {
    result.Set(MODIFIER_CONTROL);
  }
  return result;
}

POINT
MouseScrollHandler::ComputeMessagePos(UINT aMessage,
                                      WPARAM aWParam,
                                      LPARAM aLParam)
{
  POINT point;
  if (Device::SetPoint::IsGetMessagePosResponseValid(aMessage,
                                                     aWParam, aLParam)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::ComputeMessagePos: Using ::GetCursorPos()"));
    ::GetCursorPos(&point);
  } else {
    POINTS pts = GetCurrentMessagePos();
    point.x = pts.x;
    point.y = pts.y;
  }
  return point;
}

void
MouseScrollHandler::ProcessNativeMouseWheelMessage(nsWindowBase* aWidget,
                                                   UINT aMessage,
                                                   WPARAM aWParam,
                                                   LPARAM aLParam)
{
  if (SynthesizingEvent::IsSynthesizing()) {
    mSynthesizingEvent->NativeMessageReceived(aWidget, aMessage,
                                              aWParam, aLParam);
  }

  POINT point = ComputeMessagePos(aMessage, aWParam, aLParam);

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::ProcessNativeMouseWheelMessage: aWidget=%p, "
     "aMessage=%s, wParam=0x%08X, lParam=0x%08X, point: { x=%d, y=%d }",
     aWidget, aMessage == WM_MOUSEWHEEL ? "WM_MOUSEWHEEL" :
              aMessage == WM_MOUSEHWHEEL ? "WM_MOUSEHWHEEL" :
              aMessage == WM_VSCROLL ? "WM_VSCROLL" : "WM_HSCROLL",
     aWParam, aLParam, point.x, point.y));
  LOG_KEYSTATE();

  HWND underCursorWnd = ::WindowFromPoint(point);
  if (!underCursorWnd) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::ProcessNativeMouseWheelMessage: "
       "No window is not found under the cursor"));
    return;
  }

  if (Device::Elantech::IsPinchHackNeeded() &&
      Device::Elantech::IsHelperWindow(underCursorWnd)) {
    // The Elantech driver places a window right underneath the cursor
    // when sending a WM_MOUSEWHEEL event to us as part of a pinch-to-zoom
    // gesture.  We detect that here, and search for our window that would
    // be beneath the cursor if that window wasn't there.
    underCursorWnd = WinUtils::FindOurWindowAtPoint(point);
    if (!underCursorWnd) {
      MOZ_LOG(gMouseScrollLog, LogLevel::Info,
        ("MouseScroll::ProcessNativeMouseWheelMessage: "
         "Our window is not found under the Elantech helper window"));
      return;
    }
  }

  // Handle most cases first.  If the window under mouse cursor is our window
  // except plugin window (MozillaWindowClass), we should handle the message
  // on the window.
  if (WinUtils::IsOurProcessWindow(underCursorWnd)) {
    nsWindowBase* destWindow = WinUtils::GetNSWindowBasePtr(underCursorWnd);
    if (!destWindow) {
      MOZ_LOG(gMouseScrollLog, LogLevel::Info,
        ("MouseScroll::ProcessNativeMouseWheelMessage: "
         "Found window under the cursor isn't managed by nsWindow..."));
      HWND wnd = ::GetParent(underCursorWnd);
      for (; wnd; wnd = ::GetParent(wnd)) {
        destWindow = WinUtils::GetNSWindowBasePtr(wnd);
        if (destWindow) {
          break;
        }
      }
      if (!wnd) {
        MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::ProcessNativeMouseWheelMessage: Our window which is "
           "managed by nsWindow is not found under the cursor"));
        return;
      }
    }

    MOZ_ASSERT(destWindow, "destWindow must not be NULL");

    // If the found window is our plugin window, it means that the message
    // has been handled by the plugin but not consumed.  We should handle the
    // message on its parent window.  However, note that the DOM event may
    // cause accessing the plugin.  Therefore, we should unlock the plugin
    // process by using PostMessage().
    if (destWindow->IsPlugin()) {
      destWindow = destWindow->GetParentWindowBase(false);
      if (!destWindow) {
        MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::ProcessNativeMouseWheelMessage: "
           "Our window which is a parent of a plugin window is not found"));
        return;
      }
    }
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::ProcessNativeMouseWheelMessage: Succeeded, "
       "Posting internal message to an nsWindow (%p)...",
       destWindow));
    mIsWaitingInternalMessage = true;
    UINT internalMessage = WinUtils::GetInternalMessage(aMessage);
    ::PostMessage(destWindow->GetWindowHandle(), internalMessage,
                  aWParam, aLParam);
    return;
  }

  // If the window under cursor is not in our process, it means:
  // 1. The window may be a plugin window (GeckoPluginWindow or its descendant).
  // 2. The window may be another application's window.
  HWND pluginWnd = WinUtils::FindOurProcessWindow(underCursorWnd);
  if (!pluginWnd) {
    // If there is no plugin window in ancestors of the window under cursor,
    // the window is for another applications (case 2).
    // We don't need to handle this message.
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::ProcessNativeMouseWheelMessage: "
       "Our window is not found under the cursor"));
    return;
  }

  // If we're a plugin window (MozillaWindowClass) and cursor in this window,
  // the message shouldn't go to plugin's wndproc again.  So, we should handle
  // it on parent window.  However, note that the DOM event may cause accessing
  // the plugin.  Therefore, we should unlock the plugin process by using
  // PostMessage().
  if (aWidget->IsPlugin() &&
      aWidget->GetWindowHandle() == pluginWnd) {
    nsWindowBase* destWindow = aWidget->GetParentWindowBase(false);
    if (!destWindow) {
      MOZ_LOG(gMouseScrollLog, LogLevel::Info,
        ("MouseScroll::ProcessNativeMouseWheelMessage: Our normal window which "
         "is a parent of this plugin window is not found"));
      return;
    }
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::ProcessNativeMouseWheelMessage: Succeeded, "
       "Posting internal message to an nsWindow (%p) which is parent of this "
       "plugin window...",
       destWindow));
    mIsWaitingInternalMessage = true;
    UINT internalMessage = WinUtils::GetInternalMessage(aMessage);
    ::PostMessage(destWindow->GetWindowHandle(), internalMessage,
                  aWParam, aLParam);
    return;
  }

  // If the window is a part of plugin, we should post the message to it.
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::ProcessNativeMouseWheelMessage: Succeeded, "
     "Redirecting the message to a window which is a plugin child window"));
  ::PostMessage(underCursorWnd, aMessage, aWParam, aLParam);
}

bool
MouseScrollHandler::ProcessNativeScrollMessage(nsWindowBase* aWidget,
                                               UINT aMessage,
                                               WPARAM aWParam,
                                               LPARAM aLParam)
{
  if (aLParam || mUserPrefs.IsScrollMessageHandledAsWheelMessage()) {
    // Scroll message generated by Thinkpad Trackpoint Driver or similar
    // Treat as a mousewheel message and scroll appropriately
    ProcessNativeMouseWheelMessage(aWidget, aMessage, aWParam, aLParam);
    // Always consume the scroll message if we try to emulate mouse wheel
    // action.
    return true;
  }

  if (SynthesizingEvent::IsSynthesizing()) {
    mSynthesizingEvent->NativeMessageReceived(aWidget, aMessage,
                                              aWParam, aLParam);
  }

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::ProcessNativeScrollMessage: aWidget=%p, "
     "aMessage=%s, wParam=0x%08X, lParam=0x%08X",
     aWidget, aMessage == WM_VSCROLL ? "WM_VSCROLL" : "WM_HSCROLL",
     aWParam, aLParam));

  // Scroll message generated by external application
  WidgetContentCommandEvent commandEvent(true, eContentCommandScroll, aWidget);
  commandEvent.mScroll.mIsHorizontal = (aMessage == WM_HSCROLL);

  switch (LOWORD(aWParam)) {
    case SB_LINEUP:   // SB_LINELEFT
      commandEvent.mScroll.mUnit =
        WidgetContentCommandEvent::eCmdScrollUnit_Line;
      commandEvent.mScroll.mAmount = -1;
      break;
    case SB_LINEDOWN: // SB_LINERIGHT
      commandEvent.mScroll.mUnit =
        WidgetContentCommandEvent::eCmdScrollUnit_Line;
      commandEvent.mScroll.mAmount = 1;
      break;
    case SB_PAGEUP:   // SB_PAGELEFT
      commandEvent.mScroll.mUnit =
        WidgetContentCommandEvent::eCmdScrollUnit_Page;
      commandEvent.mScroll.mAmount = -1;
      break;
    case SB_PAGEDOWN: // SB_PAGERIGHT
      commandEvent.mScroll.mUnit =
        WidgetContentCommandEvent::eCmdScrollUnit_Page;
      commandEvent.mScroll.mAmount = 1;
      break;
    case SB_TOP:      // SB_LEFT
      commandEvent.mScroll.mUnit =
        WidgetContentCommandEvent::eCmdScrollUnit_Whole;
      commandEvent.mScroll.mAmount = -1;
      break;
    case SB_BOTTOM:   // SB_RIGHT
      commandEvent.mScroll.mUnit =
        WidgetContentCommandEvent::eCmdScrollUnit_Whole;
      commandEvent.mScroll.mAmount = 1;
      break;
    default:
      return false;
  }
  // XXX If this is a plugin window, we should dispatch the event from
  //     parent window.
  aWidget->DispatchContentCommandEvent(&commandEvent);
  return true;
}

void
MouseScrollHandler::HandleMouseWheelMessage(nsWindowBase* aWidget,
                                            UINT aMessage,
                                            WPARAM aWParam,
                                            LPARAM aLParam)
{
  MOZ_ASSERT(
    (aMessage == MOZ_WM_MOUSEVWHEEL || aMessage == MOZ_WM_MOUSEHWHEEL),
    "HandleMouseWheelMessage must be called with "
    "MOZ_WM_MOUSEVWHEEL or MOZ_WM_MOUSEHWHEEL");

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::HandleMouseWheelMessage: aWidget=%p, "
     "aMessage=MOZ_WM_MOUSE%sWHEEL, aWParam=0x%08X, aLParam=0x%08X",
     aWidget, aMessage == MOZ_WM_MOUSEVWHEEL ? "V" : "H",
     aWParam, aLParam));

  mIsWaitingInternalMessage = false;

  // If it's not allowed to cache system settings, we need to reset the cache
  // before handling the mouse wheel message.
  mSystemSettings.TrustedScrollSettingsDriver();

  EventInfo eventInfo(aWidget, WinUtils::GetNativeMessage(aMessage),
                      aWParam, aLParam);
  if (!eventInfo.CanDispatchWheelEvent()) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::HandleMouseWheelMessage: Cannot dispatch the events"));
    mLastEventInfo.ResetTransaction();
    return;
  }

  // Discard the remaining delta if current wheel message and last one are
  // received by different window or to scroll different direction or
  // different unit scroll.  Furthermore, if the last event was too old.
  if (!mLastEventInfo.CanContinueTransaction(eventInfo)) {
    mLastEventInfo.ResetTransaction();
  }

  mLastEventInfo.RecordEvent(eventInfo);

  ModifierKeyState modKeyState = GetModifierKeyState(aMessage);

  // Grab the widget, it might be destroyed by a DOM event handler.
  RefPtr<nsWindowBase> kungFuDethGrip(aWidget);

  WidgetWheelEvent wheelEvent(true, eWheel, aWidget);
  if (mLastEventInfo.InitWheelEvent(aWidget, wheelEvent, modKeyState)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::HandleMouseWheelMessage: dispatching "
       "eWheel event"));
    aWidget->DispatchWheelEvent(&wheelEvent);
    if (aWidget->Destroyed()) {
      MOZ_LOG(gMouseScrollLog, LogLevel::Info,
        ("MouseScroll::HandleMouseWheelMessage: The window was destroyed "
         "by eWheel event"));
      mLastEventInfo.ResetTransaction();
      return;
    }
  }
  else {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::HandleMouseWheelMessage: eWheel event is not "
       "dispatched"));
  }
}

void
MouseScrollHandler::HandleScrollMessageAsMouseWheelMessage(nsWindowBase* aWidget,
                                                           UINT aMessage,
                                                           WPARAM aWParam,
                                                           LPARAM aLParam)
{
  MOZ_ASSERT(
    (aMessage == MOZ_WM_VSCROLL || aMessage == MOZ_WM_HSCROLL),
    "HandleScrollMessageAsMouseWheelMessage must be called with "
    "MOZ_WM_VSCROLL or MOZ_WM_HSCROLL");

  mIsWaitingInternalMessage = false;

  ModifierKeyState modKeyState = GetModifierKeyState(aMessage);

  WidgetWheelEvent wheelEvent(true, eWheel, aWidget);
  double& delta =
   (aMessage == MOZ_WM_VSCROLL) ? wheelEvent.deltaY : wheelEvent.deltaX;
  int32_t& lineOrPageDelta =
   (aMessage == MOZ_WM_VSCROLL) ? wheelEvent.lineOrPageDeltaY :
                                  wheelEvent.lineOrPageDeltaX;

  delta = 1.0;
  lineOrPageDelta = 1;

  switch (LOWORD(aWParam)) {
    case SB_PAGEUP:
      delta = -1.0;
      lineOrPageDelta = -1;
    case SB_PAGEDOWN:
      wheelEvent.deltaMode = nsIDOMWheelEvent::DOM_DELTA_PAGE;
      break;

    case SB_LINEUP:
      delta = -1.0;
      lineOrPageDelta = -1;
    case SB_LINEDOWN:
      wheelEvent.deltaMode = nsIDOMWheelEvent::DOM_DELTA_LINE;
      break;

    default:
      return;
  }
  modKeyState.InitInputEvent(wheelEvent);
  // XXX Current mouse position may not be same as when the original message
  //     is received.  We need to know the actual mouse cursor position when
  //     the original message was received.
  InitEvent(aWidget, wheelEvent);

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::HandleScrollMessageAsMouseWheelMessage: aWidget=%p, "
     "aMessage=MOZ_WM_%sSCROLL, aWParam=0x%08X, aLParam=0x%08X, "
     "wheelEvent { refPoint: { x: %d, y: %d }, deltaX: %f, deltaY: %f, "
     "lineOrPageDeltaX: %d, lineOrPageDeltaY: %d, "
     "isShift: %s, isControl: %s, isAlt: %s, isMeta: %s }",
     aWidget, (aMessage == MOZ_WM_VSCROLL) ? "V" : "H", aWParam, aLParam,
     wheelEvent.refPoint.x, wheelEvent.refPoint.y,
     wheelEvent.deltaX, wheelEvent.deltaY,
     wheelEvent.lineOrPageDeltaX, wheelEvent.lineOrPageDeltaY,
     GetBoolName(wheelEvent.IsShift()),
     GetBoolName(wheelEvent.IsControl()),
     GetBoolName(wheelEvent.IsAlt()),
     GetBoolName(wheelEvent.IsMeta())));

  aWidget->DispatchWheelEvent(&wheelEvent);
}

/******************************************************************************
 *
 * EventInfo
 *
 ******************************************************************************/

MouseScrollHandler::EventInfo::EventInfo(nsWindowBase* aWidget,
                                         UINT aMessage,
                                         WPARAM aWParam, LPARAM aLParam)
{
  MOZ_ASSERT(aMessage == WM_MOUSEWHEEL || aMessage == WM_MOUSEHWHEEL,
    "EventInfo must be initialized with WM_MOUSEWHEEL or WM_MOUSEHWHEEL");

  MouseScrollHandler::GetInstance()->mSystemSettings.Init();

  mIsVertical = (aMessage == WM_MOUSEWHEEL);
  mIsPage = MouseScrollHandler::sInstance->
              mSystemSettings.IsPageScroll(mIsVertical);
  mDelta = (short)HIWORD(aWParam);
  mWnd = aWidget->GetWindowHandle();
  mTimeStamp = TimeStamp::Now();
}

bool
MouseScrollHandler::EventInfo::CanDispatchWheelEvent() const
{
  if (!GetScrollAmount()) {
    // XXX I think that we should dispatch mouse wheel events even if the
    // operation will not scroll because the wheel operation really happened
    // and web application may want to handle the event for non-scroll action.
    return false;
  }

  return (mDelta != 0);
}

int32_t
MouseScrollHandler::EventInfo::GetScrollAmount() const
{
  if (mIsPage) {
    return 1;
  }
  return MouseScrollHandler::sInstance->
           mSystemSettings.GetScrollAmount(mIsVertical);
}

/******************************************************************************
 *
 * LastEventInfo
 *
 ******************************************************************************/

bool
MouseScrollHandler::LastEventInfo::CanContinueTransaction(
                                     const EventInfo& aNewEvent)
{
  int32_t timeout = MouseScrollHandler::sInstance->
                      mUserPrefs.GetMouseScrollTransactionTimeout();
  return !mWnd ||
           (mWnd == aNewEvent.GetWindowHandle() &&
            IsPositive() == aNewEvent.IsPositive() &&
            mIsVertical == aNewEvent.IsVertical() &&
            mIsPage == aNewEvent.IsPage() &&
            (timeout < 0 ||
             TimeStamp::Now() - mTimeStamp <=
               TimeDuration::FromMilliseconds(timeout)));
}

void
MouseScrollHandler::LastEventInfo::ResetTransaction()
{
  if (!mWnd) {
    return;
  }

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::LastEventInfo::ResetTransaction()"));

  mWnd = nullptr;
  mAccumulatedDelta = 0;
}

void
MouseScrollHandler::LastEventInfo::RecordEvent(const EventInfo& aEvent)
{
  mWnd = aEvent.GetWindowHandle();
  mDelta = aEvent.GetNativeDelta();
  mIsVertical = aEvent.IsVertical();
  mIsPage = aEvent.IsPage();
  mTimeStamp = TimeStamp::Now();
}

/* static */
int32_t
MouseScrollHandler::LastEventInfo::RoundDelta(double aDelta)
{
  return (aDelta >= 0) ? (int32_t)floor(aDelta) : (int32_t)ceil(aDelta);
}

bool
MouseScrollHandler::LastEventInfo::InitWheelEvent(
                                     nsWindowBase* aWidget,
                                     WidgetWheelEvent& aWheelEvent,
                                     const ModifierKeyState& aModKeyState)
{
  MOZ_ASSERT(aWheelEvent.mMessage == eWheel);

  // XXX Why don't we use lParam value? We should use lParam value because
  //     our internal message is always posted by original message handler.
  //     So, GetMessagePos() may return different cursor position.
  InitEvent(aWidget, aWheelEvent);

  aModKeyState.InitInputEvent(aWheelEvent);

  // Our positive delta value means to bottom or right.
  // But positive native delta value means to top or right.
  // Use orienter for computing our delta value with native delta value.
  int32_t orienter = mIsVertical ? -1 : 1;

  aWheelEvent.deltaMode = mIsPage ? nsIDOMWheelEvent::DOM_DELTA_PAGE :
                                    nsIDOMWheelEvent::DOM_DELTA_LINE;

  double& delta = mIsVertical ? aWheelEvent.deltaY : aWheelEvent.deltaX;
  int32_t& lineOrPageDelta = mIsVertical ? aWheelEvent.lineOrPageDeltaY :
                                           aWheelEvent.lineOrPageDeltaX;

  double nativeDeltaPerUnit =
    mIsPage ? static_cast<double>(WHEEL_DELTA) :
              static_cast<double>(WHEEL_DELTA) / GetScrollAmount();

  delta = static_cast<double>(mDelta) * orienter / nativeDeltaPerUnit;
  mAccumulatedDelta += mDelta;
  lineOrPageDelta =
    mAccumulatedDelta * orienter / RoundDelta(nativeDeltaPerUnit);
  mAccumulatedDelta -=
    lineOrPageDelta * orienter * RoundDelta(nativeDeltaPerUnit);

  if (aWheelEvent.deltaMode != nsIDOMWheelEvent::DOM_DELTA_LINE) {
    // If the scroll delta mode isn't per line scroll, we shouldn't allow to
    // override the system scroll speed setting.
    aWheelEvent.mAllowToOverrideSystemScrollSpeed = false;
  } else if (!MouseScrollHandler::sInstance->
                mSystemSettings.IsOverridingSystemScrollSpeedAllowed()) {
    // If the system settings are customized by either the user or
    // the mouse utility, we shouldn't allow to override the system scroll
    // speed setting.
    aWheelEvent.mAllowToOverrideSystemScrollSpeed = false;
  } else {
    // For suppressing too fast scroll, we should ensure that the maximum
    // overridden delta value should be less than overridden scroll speed
    // with default scroll amount.
    double defaultScrollAmount =
      mIsVertical ? SystemSettings::DefaultScrollLines() :
                    SystemSettings::DefaultScrollChars();
    double maxDelta =
      WidgetWheelEvent::ComputeOverriddenDelta(defaultScrollAmount,
                                               mIsVertical);
    if (maxDelta != defaultScrollAmount) {
      double overriddenDelta =
        WidgetWheelEvent::ComputeOverriddenDelta(Abs(delta), mIsVertical);
      if (overriddenDelta > maxDelta) {
        // Suppress to fast scroll since overriding system scroll speed with
        // current delta value causes too big delta value.
        aWheelEvent.mAllowToOverrideSystemScrollSpeed = false;
      }
    }
  }

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::LastEventInfo::InitWheelEvent: aWidget=%p, "
     "aWheelEvent { refPoint: { x: %d, y: %d }, deltaX: %f, deltaY: %f, "
     "lineOrPageDeltaX: %d, lineOrPageDeltaY: %d, "
     "isShift: %s, isControl: %s, isAlt: %s, isMeta: %s, "
     "mAllowToOverrideSystemScrollSpeed: %s }, "
     "mAccumulatedDelta: %d",
     aWidget, aWheelEvent.refPoint.x, aWheelEvent.refPoint.y,
     aWheelEvent.deltaX, aWheelEvent.deltaY,
     aWheelEvent.lineOrPageDeltaX, aWheelEvent.lineOrPageDeltaY,
     GetBoolName(aWheelEvent.IsShift()),
     GetBoolName(aWheelEvent.IsControl()),
     GetBoolName(aWheelEvent.IsAlt()),
     GetBoolName(aWheelEvent.IsMeta()),
     GetBoolName(aWheelEvent.mAllowToOverrideSystemScrollSpeed),
     mAccumulatedDelta));

  return (delta != 0);
}

/******************************************************************************
 *
 * SystemSettings
 *
 ******************************************************************************/

void
MouseScrollHandler::SystemSettings::Init()
{
  if (mInitialized) {
    return;
  }

  InitScrollLines();
  InitScrollChars();

  mInitialized = true;

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::SystemSettings::Init(): initialized, "
       "mScrollLines=%d, mScrollChars=%d",
     mScrollLines, mScrollChars));
}

bool
MouseScrollHandler::SystemSettings::InitScrollLines()
{
  int32_t oldValue = mInitialized ? mScrollLines : 0;
  mIsReliableScrollLines = false;
  mScrollLines = MouseScrollHandler::sInstance->
                   mUserPrefs.GetOverriddenVerticalScrollAmout();
  if (mScrollLines >= 0) {
    // overridden by the pref.
    mIsReliableScrollLines = true;
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::SystemSettings::InitScrollLines(): mScrollLines is "
       "overridden by the pref: %d",
       mScrollLines));
  } else if (!::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0,
                                     &mScrollLines, 0)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::SystemSettings::InitScrollLines(): ::SystemParametersInfo("
       "SPI_GETWHEELSCROLLLINES) failed"));
    mScrollLines = DefaultScrollLines();
  }

  if (mScrollLines > WHEEL_DELTA) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::SystemSettings::InitScrollLines(): the result of "
       "::SystemParametersInfo(SPI_GETWHEELSCROLLLINES) is too large: %d",
       mScrollLines));
    // sScrollLines usually equals 3 or 0 (for no scrolling)
    // However, if sScrollLines > WHEEL_DELTA, we assume that
    // the mouse driver wants a page scroll.  The docs state that
    // sScrollLines should explicitly equal WHEEL_PAGESCROLL, but
    // since some mouse drivers use an arbitrary large number instead,
    // we have to handle that as well.
    mScrollLines = WHEEL_PAGESCROLL;
  }

  return oldValue != mScrollLines;
}

bool
MouseScrollHandler::SystemSettings::InitScrollChars()
{
  int32_t oldValue = mInitialized ? mScrollChars : 0;
  mIsReliableScrollChars = false;
  mScrollChars = MouseScrollHandler::sInstance->
                   mUserPrefs.GetOverriddenHorizontalScrollAmout();
  if (mScrollChars >= 0) {
    // overridden by the pref.
    mIsReliableScrollChars = true;
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::SystemSettings::InitScrollChars(): mScrollChars is "
       "overridden by the pref: %d",
       mScrollChars));
  } else if (!::SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0,
                                     &mScrollChars, 0)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::SystemSettings::InitScrollChars(): ::SystemParametersInfo("
       "SPI_GETWHEELSCROLLCHARS) failed, %s",
       IsVistaOrLater() ?
         "this is unexpected on Vista or later" :
         "but on XP or earlier, this is not a problem"));
    // XXX Should we use DefaultScrollChars()?
    mScrollChars = 1;
  }

  if (mScrollChars > WHEEL_DELTA) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::SystemSettings::InitScrollChars(): the result of "
       "::SystemParametersInfo(SPI_GETWHEELSCROLLCHARS) is too large: %d",
       mScrollChars));
    // See the comments for the case mScrollLines > WHEEL_DELTA.
    mScrollChars = WHEEL_PAGESCROLL;
  }

  return oldValue != mScrollChars;
}

void
MouseScrollHandler::SystemSettings::MarkDirty()
{
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScrollHandler::SystemSettings::MarkDirty(): "
       "Marking SystemSettings dirty"));
  mInitialized = false;
  // When system settings are changed, we should reset current transaction.
  MOZ_ASSERT(sInstance,
    "Must not be called at initializing MouseScrollHandler");
  MouseScrollHandler::sInstance->mLastEventInfo.ResetTransaction();
}

void
MouseScrollHandler::SystemSettings::RefreshCache()
{
  bool isChanged = InitScrollLines();
  isChanged = InitScrollChars() || isChanged;
  if (!isChanged) {
    return;
  }
  // If the scroll amount is changed, we should reset current transaction.
  MOZ_ASSERT(sInstance,
    "Must not be called at initializing MouseScrollHandler");
  MouseScrollHandler::sInstance->mLastEventInfo.ResetTransaction();
}

void
MouseScrollHandler::SystemSettings::TrustedScrollSettingsDriver()
{
  if (!mInitialized) {
    return;
  }

  // if the cache is initialized with prefs, we don't need to refresh it.
  if (mIsReliableScrollLines && mIsReliableScrollChars) {
    return;
  }

  MouseScrollHandler::UserPrefs& userPrefs =
    MouseScrollHandler::sInstance->mUserPrefs;

  // If system settings cache is disabled, we should always refresh them.
  if (!userPrefs.IsSystemSettingCacheEnabled()) {
    RefreshCache();
    return;
  }

  // If pref is set to as "always trust the cache", we shouldn't refresh them
  // in any environments.
  if (userPrefs.IsSystemSettingCacheForciblyEnabled()) {
    return;
  }

  // If SynTP of Synaptics or Apoint of Alps is installed, it may hook
  // ::SystemParametersInfo() and returns different value from system settings.
  if (Device::SynTP::IsDriverInstalled() ||
      Device::Apoint::IsDriverInstalled()) {
    RefreshCache();
    return;
  }

  // XXX We're not sure about other touchpad drivers...
}

bool
MouseScrollHandler::SystemSettings::IsOverridingSystemScrollSpeedAllowed()
{
  return mScrollLines == DefaultScrollLines() &&
         (!IsVistaOrLater() || mScrollChars == DefaultScrollChars());
}

/******************************************************************************
 *
 * UserPrefs
 *
 ******************************************************************************/

MouseScrollHandler::UserPrefs::UserPrefs() :
  mInitialized(false)
{
  // We need to reset mouse wheel transaction when all of mousewheel related
  // prefs are changed.
  DebugOnly<nsresult> rv =
    Preferences::RegisterCallback(OnChange, "mousewheel.", this);
  MOZ_ASSERT(NS_SUCCEEDED(rv),
    "Failed to register callback for mousewheel.");
}

MouseScrollHandler::UserPrefs::~UserPrefs()
{
  DebugOnly<nsresult> rv =
    Preferences::UnregisterCallback(OnChange, "mousewheel.", this);
  MOZ_ASSERT(NS_SUCCEEDED(rv),
    "Failed to unregister callback for mousewheel.");
}

void
MouseScrollHandler::UserPrefs::Init()
{
  if (mInitialized) {
    return;
  }

  mInitialized = true;

  mScrollMessageHandledAsWheelMessage =
    Preferences::GetBool("mousewheel.emulate_at_wm_scroll", false);
  mEnableSystemSettingCache =
    Preferences::GetBool("mousewheel.system_settings_cache.enabled", true);
  mForceEnableSystemSettingCache =
    Preferences::GetBool("mousewheel.system_settings_cache.force_enabled",
                         false);
  mOverriddenVerticalScrollAmount =
    Preferences::GetInt("mousewheel.windows.vertical_amount_override", -1);
  mOverriddenHorizontalScrollAmount =
    Preferences::GetInt("mousewheel.windows.horizontal_amount_override", -1);
  mMouseScrollTransactionTimeout =
    Preferences::GetInt("mousewheel.windows.transaction.timeout",
                        DEFAULT_TIMEOUT_DURATION);

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::UserPrefs::Init(): initialized, "
       "mScrollMessageHandledAsWheelMessage=%s, "
       "mEnableSystemSettingCache=%s, "
       "mForceEnableSystemSettingCache=%s, "
       "mOverriddenVerticalScrollAmount=%d, "
       "mOverriddenHorizontalScrollAmount=%d, "
       "mMouseScrollTransactionTimeout=%d",
     GetBoolName(mScrollMessageHandledAsWheelMessage),
     GetBoolName(mEnableSystemSettingCache),
     GetBoolName(mForceEnableSystemSettingCache),
     mOverriddenVerticalScrollAmount, mOverriddenHorizontalScrollAmount,
     mMouseScrollTransactionTimeout));
}

void
MouseScrollHandler::UserPrefs::MarkDirty()
{
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScrollHandler::UserPrefs::MarkDirty(): Marking UserPrefs dirty"));
  mInitialized = false;
  // Some prefs might override system settings, so, we should mark them dirty.
  MouseScrollHandler::sInstance->mSystemSettings.MarkDirty();
  // When user prefs for mousewheel are changed, we should reset current
  // transaction.
  MOZ_ASSERT(sInstance,
    "Must not be called at initializing MouseScrollHandler");
  MouseScrollHandler::sInstance->mLastEventInfo.ResetTransaction();
}

/******************************************************************************
 *
 * Device
 *
 ******************************************************************************/

/* static */
bool
MouseScrollHandler::Device::GetWorkaroundPref(const char* aPrefName,
                                              bool aValueIfAutomatic)
{
  if (!aPrefName) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::GetWorkaroundPref(): Failed, aPrefName is NULL"));
    return aValueIfAutomatic;
  }

  int32_t lHackValue = 0;
  if (NS_FAILED(Preferences::GetInt(aPrefName, &lHackValue))) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::GetWorkaroundPref(): Preferences::GetInt() failed,"
       " aPrefName=\"%s\", aValueIfAutomatic=%s",
       aPrefName, GetBoolName(aValueIfAutomatic)));
    return aValueIfAutomatic;
  }

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::Device::GetWorkaroundPref(): Succeeded, "
     "aPrefName=\"%s\", aValueIfAutomatic=%s, lHackValue=%d",
     aPrefName, GetBoolName(aValueIfAutomatic), lHackValue));

  switch (lHackValue) {
    case 0: // disabled
      return false;
    case 1: // enabled
      return true;
    default: // -1: autodetect
      return aValueIfAutomatic;
  }
}

/* static */
void
MouseScrollHandler::Device::Init()
{
  // FYI: Thinkpad's TrackPoint is Apoint of Alps and UltraNav is SynTP of
  //      Synaptics.  So, those drivers' information should be initialized
  //      before calling methods of TrackPoint and UltraNav.
  SynTP::Init();
  Elantech::Init();
  Apoint::Init();

  sFakeScrollableWindowNeeded =
    GetWorkaroundPref("ui.trackpoint_hack.enabled",
                      (TrackPoint::IsDriverInstalled() ||
                       UltraNav::IsObsoleteDriverInstalled()));

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::Device::Init(): sFakeScrollableWindowNeeded=%s",
     GetBoolName(sFakeScrollableWindowNeeded)));
}

/******************************************************************************
 *
 * Device::SynTP
 *
 ******************************************************************************/

/* static */
void
MouseScrollHandler::Device::SynTP::Init()
{
  if (sInitialized) {
    return;
  }

  sInitialized = true;
  sMajorVersion = 0;
  sMinorVersion = -1;

  wchar_t buf[40];
  bool foundKey =
    WinUtils::GetRegistryKey(HKEY_LOCAL_MACHINE,
                             L"Software\\Synaptics\\SynTP\\Install",
                             L"DriverVersion",
                             buf, sizeof buf);
  if (!foundKey) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::SynTP::Init(): "
       "SynTP driver is not found"));
    return;
  }

  sMajorVersion = wcstol(buf, nullptr, 10);
  sMinorVersion = 0;
  wchar_t* p = wcschr(buf, L'.');
  if (p) {
    sMinorVersion = wcstol(p + 1, nullptr, 10);
  }
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::Device::SynTP::Init(): "
     "found driver version = %d.%d",
     sMajorVersion, sMinorVersion));
}

/******************************************************************************
 *
 * Device::Elantech
 *
 ******************************************************************************/

/* static */
void
MouseScrollHandler::Device::Elantech::Init()
{
  int32_t version = GetDriverMajorVersion();
  bool needsHack =
    Device::GetWorkaroundPref("ui.elantech_gesture_hacks.enabled",
                              version != 0);
  sUseSwipeHack = needsHack && version <= 7;
  sUsePinchHack = needsHack && version <= 8;

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::Device::Elantech::Init(): version=%d, sUseSwipeHack=%s, "
     "sUsePinchHack=%s",
     version, GetBoolName(sUseSwipeHack), GetBoolName(sUsePinchHack)));
}

/* static */
int32_t
MouseScrollHandler::Device::Elantech::GetDriverMajorVersion()
{
  wchar_t buf[40];
  // The driver version is found in one of these two registry keys.
  bool foundKey =
    WinUtils::GetRegistryKey(HKEY_CURRENT_USER,
                             L"Software\\Elantech\\MainOption",
                             L"DriverVersion",
                             buf, sizeof buf);
  if (!foundKey) {
    foundKey =
      WinUtils::GetRegistryKey(HKEY_CURRENT_USER,
                               L"Software\\Elantech",
                               L"DriverVersion",
                               buf, sizeof buf);
  }

  if (!foundKey) {
    return 0;
  }

  // Assume that the major version number can be found just after a space
  // or at the start of the string.
  for (wchar_t* p = buf; *p; p++) {
    if (*p >= L'0' && *p <= L'9' && (p == buf || *(p - 1) == L' ')) {
      return wcstol(p, nullptr, 10);
    }
  }

  return 0;
}

/* static */
bool
MouseScrollHandler::Device::Elantech::IsHelperWindow(HWND aWnd)
{
  // The helper window cannot be distinguished based on its window class, so we
  // need to check if it is owned by the helper process, ETDCtrl.exe.

  const wchar_t* filenameSuffix = L"\\etdctrl.exe";
  const int filenameSuffixLength = 12;

  DWORD pid;
  ::GetWindowThreadProcessId(aWnd, &pid);

  HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (!hProcess) {
    return false;
  }

  bool result = false;
  wchar_t path[256] = {L'\0'};
  if (::GetProcessImageFileNameW(hProcess, path, ArrayLength(path))) {
    int pathLength = lstrlenW(path);
    if (pathLength >= filenameSuffixLength) {
      if (lstrcmpiW(path + pathLength - filenameSuffixLength,
                    filenameSuffix) == 0) {
        result = true;
      }
    }
  }
  ::CloseHandle(hProcess);

  return result;
}

/* static */
bool
MouseScrollHandler::Device::Elantech::HandleKeyMessage(nsWindowBase* aWidget,
                                                       UINT aMsg,
                                                       WPARAM aWParam,
                                                       LPARAM aLParam)
{
  // The Elantech touchpad driver understands three-finger swipe left and
  // right gestures, and translates them into Page Up and Page Down key
  // events for most applications.  For Firefox 3.6, it instead sends
  // Alt+Left and Alt+Right to trigger browser back/forward actions.  As
  // with the Thinkpad Driver hack in nsWindow::Create, the change in
  // HWND structure makes Firefox not trigger the driver's heuristics
  // any longer.
  //
  // The Elantech driver actually sends these messages for a three-finger
  // swipe right:
  //
  //   WM_KEYDOWN virtual_key = 0xCC or 0xFF ScanCode = 00
  //   WM_KEYDOWN virtual_key = VK_NEXT      ScanCode = 00
  //   WM_KEYUP   virtual_key = VK_NEXT      ScanCode = 00
  //   WM_KEYUP   virtual_key = 0xCC or 0xFF ScanCode = 00
  //
  // Whether 0xCC or 0xFF is sent is suspected to depend on the driver
  // version.  7.0.4.12_14Jul09_WHQL, 7.0.5.10, and 7.0.6.0 generate 0xCC.
  // 7.0.4.3 from Asus on EeePC generates 0xFF.
  //
  // On some hardware, IS_VK_DOWN(0xFF) returns true even when Elantech
  // messages are not involved, meaning that alone is not enough to
  // distinguish the gesture from a regular Page Up or Page Down key press.
  // The ScanCode is therefore also tested to detect the gesture.
  // We then pretend that we should dispatch "Go Forward" command.  Similarly
  // for VK_PRIOR and "Go Back" command.
  if (sUseSwipeHack &&
      (aWParam == VK_NEXT || aWParam == VK_PRIOR) &&
      WinUtils::GetScanCode(aLParam) == 0 &&
      (IS_VK_DOWN(0xFF) || IS_VK_DOWN(0xCC))) {
    if (aMsg == WM_KEYDOWN) {
      MOZ_LOG(gMouseScrollLog, LogLevel::Info,
        ("MouseScroll::Device::Elantech::HandleKeyMessage(): Dispatching "
         "%s command event",
         aWParam == VK_NEXT ? "Forward" : "Back"));

      WidgetCommandEvent commandEvent(true, nsGkAtoms::onAppCommand,
        (aWParam == VK_NEXT) ? nsGkAtoms::Forward : nsGkAtoms::Back, aWidget);
      InitEvent(aWidget, commandEvent);
      aWidget->DispatchWindowEvent(&commandEvent);
    }
    else {
      MOZ_LOG(gMouseScrollLog, LogLevel::Info,
        ("MouseScroll::Device::Elantech::HandleKeyMessage(): Consumed"));
    }
    return true; // consume the message (doesn't need to dispatch key events)
  }

  // Version 8 of the Elantech touchpad driver sends these messages for
  // zoom gestures:
  //
  //   WM_KEYDOWN    virtual_key = 0xCC        time = 10
  //   WM_KEYDOWN    virtual_key = VK_CONTROL  time = 10
  //   WM_MOUSEWHEEL                           time = ::GetTickCount()
  //   WM_KEYUP      virtual_key = VK_CONTROL  time = 10
  //   WM_KEYUP      virtual_key = 0xCC        time = 10
  //
  // The result of this is that we process all of the WM_KEYDOWN/WM_KEYUP
  // messages first because their timestamps make them appear to have
  // been sent before the WM_MOUSEWHEEL message.  To work around this,
  // we store the current time when we process the WM_KEYUP message and
  // assume that any WM_MOUSEWHEEL message with a timestamp before that
  // time is one that should be processed as if the Control key was down.
  if (sUsePinchHack && aMsg == WM_KEYUP &&
      aWParam == VK_CONTROL && ::GetMessageTime() == 10) {
    // We look only at the bottom 31 bits of the system tick count since
    // GetMessageTime returns a LONG, which is signed, so we want values
    // that are more easily comparable.
    sZoomUntil = ::GetTickCount() & 0x7FFFFFFF;

    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::Elantech::HandleKeyMessage(): sZoomUntil=%d",
       sZoomUntil));
  }

  return false;
}

/* static */
void
MouseScrollHandler::Device::Elantech::UpdateZoomUntil()
{
  if (!sZoomUntil) {
    return;
  }

  // For the Elantech Touchpad Zoom Gesture Hack, we should check that the
  // system time (32-bit milliseconds) hasn't wrapped around.  Otherwise we
  // might get into the situation where wheel events for the next 50 days of
  // system uptime are assumed to be Ctrl+Wheel events.  (It is unlikely that
  // we would get into that state, because the system would already need to be
  // up for 50 days and the Control key message would need to be processed just
  // before the system time overflow and the wheel message just after.)
  //
  // We also take the chance to reset sZoomUntil if we simply have passed that
  // time.
  LONG msgTime = ::GetMessageTime();
  if ((sZoomUntil >= 0x3fffffffu && DWORD(msgTime) < 0x40000000u) ||
      (sZoomUntil < DWORD(msgTime))) {
    sZoomUntil = 0;

    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::Elantech::UpdateZoomUntil(): "
       "sZoomUntil was reset"));
  }
}

/* static */
bool
MouseScrollHandler::Device::Elantech::IsZooming()
{
  // Assume the Control key is down if the Elantech touchpad has sent the
  // mis-ordered WM_KEYDOWN/WM_MOUSEWHEEL messages.  (See the comment in
  // OnKeyUp.)
  return (sZoomUntil && static_cast<DWORD>(::GetMessageTime()) < sZoomUntil);
}

/******************************************************************************
 *
 * Device::Apoint
 *
 ******************************************************************************/

/* static */
void
MouseScrollHandler::Device::Apoint::Init()
{
  if (sInitialized) {
    return;
  }

  sInitialized = true;
  sMajorVersion = 0;
  sMinorVersion = -1;

  wchar_t buf[40];
  bool foundKey =
    WinUtils::GetRegistryKey(HKEY_LOCAL_MACHINE,
                             L"Software\\Alps\\Apoint",
                             L"ProductVer",
                             buf, sizeof buf);
  if (!foundKey) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::Apoint::Init(): "
       "Apoint driver is not found"));
    return;
  }

  sMajorVersion = wcstol(buf, nullptr, 10);
  sMinorVersion = 0;
  wchar_t* p = wcschr(buf, L'.');
  if (p) {
    sMinorVersion = wcstol(p + 1, nullptr, 10);
  }
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScroll::Device::Apoint::Init(): "
     "found driver version = %d.%d",
     sMajorVersion, sMinorVersion));
}

/******************************************************************************
 *
 * Device::TrackPoint
 *
 ******************************************************************************/

/* static */
bool
MouseScrollHandler::Device::TrackPoint::IsDriverInstalled()
{
  if (WinUtils::HasRegistryKey(HKEY_CURRENT_USER,
                               L"Software\\Lenovo\\TrackPoint")) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::TrackPoint::IsDriverInstalled(): "
       "Lenovo's TrackPoint driver is found"));
    return true;
  }

  if (WinUtils::HasRegistryKey(HKEY_CURRENT_USER,
                               L"Software\\Alps\\Apoint\\TrackPoint")) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::TrackPoint::IsDriverInstalled(): "
       "Alps's TrackPoint driver is found"));
  }

  return false;
}

/******************************************************************************
 *
 * Device::UltraNav
 *
 ******************************************************************************/

/* static */
bool
MouseScrollHandler::Device::UltraNav::IsObsoleteDriverInstalled()
{
  if (WinUtils::HasRegistryKey(HKEY_CURRENT_USER,
                               L"Software\\Lenovo\\UltraNav")) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::UltraNav::IsObsoleteDriverInstalled(): "
       "Lenovo's UltraNav driver is found"));
    return true;
  }

  bool installed = false;
  if (WinUtils::HasRegistryKey(HKEY_CURRENT_USER,
        L"Software\\Synaptics\\SynTPEnh\\UltraNavUSB")) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::UltraNav::IsObsoleteDriverInstalled(): "
       "Synaptics's UltraNav (USB) driver is found"));
    installed = true;
  } else if (WinUtils::HasRegistryKey(HKEY_CURRENT_USER,
               L"Software\\Synaptics\\SynTPEnh\\UltraNavPS2")) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::UltraNav::IsObsoleteDriverInstalled(): "
       "Synaptics's UltraNav (PS/2) driver is found"));
    installed = true;
  }

  if (!installed) {
    return false;
  }

  int32_t majorVersion = Device::SynTP::GetDriverMajorVersion();
  if (!majorVersion) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::UltraNav::IsObsoleteDriverInstalled(): "
       "Failed to get UltraNav driver version"));
    return false;
  }
  int32_t minorVersion = Device::SynTP::GetDriverMinorVersion();
  return majorVersion < 15 || (majorVersion == 15 && minorVersion == 0);
}

/******************************************************************************
 *
 * Device::SetPoint
 *
 ******************************************************************************/

/* static */
bool
MouseScrollHandler::Device::SetPoint::IsGetMessagePosResponseValid(
                                        UINT aMessage,
                                        WPARAM aWParam,
                                        LPARAM aLParam)
{
  if (aMessage != WM_MOUSEHWHEEL) {
    return false;
  }

  POINTS pts = MouseScrollHandler::GetCurrentMessagePos();
  LPARAM messagePos = MAKELPARAM(pts.x, pts.y);

  // XXX We should check whether SetPoint is installed or not by registry.

  // SetPoint, Logitech (Logicool) mouse driver, (confirmed with 4.82.11 and
  // MX-1100) always sets 0 to the lParam of WM_MOUSEHWHEEL.  The driver SENDs
  // one message at first time, this time, ::GetMessagePos() works fine.
  // Then, we will return 0 (0 means we process it) to the message. Then, the
  // driver will POST the same messages continuously during the wheel tilted.
  // But ::GetMessagePos() API always returns (0, 0) for them, even if the
  // actual mouse cursor isn't 0,0.  Therefore, we cannot trust the result of
  // ::GetMessagePos API if the sender is SetPoint.
  if (!sMightBeUsing && !aLParam && aLParam != messagePos &&
      ::InSendMessage()) {
    sMightBeUsing = true;
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::SetPoint::IsGetMessagePosResponseValid(): "
       "Might using SetPoint"));
  } else if (sMightBeUsing && aLParam != 0 && ::InSendMessage()) {
    // The user has changed the mouse from Logitech's to another one (e.g.,
    // the user has changed to the touchpad of the notebook.
    sMightBeUsing = false;
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::SetPoint::IsGetMessagePosResponseValid(): "
       "Might stop using SetPoint"));
  }
  return (sMightBeUsing && !aLParam && !messagePos);
}

/******************************************************************************
 *
 * SynthesizingEvent
 *
 ******************************************************************************/

/* static */
bool
MouseScrollHandler::SynthesizingEvent::IsSynthesizing()
{
  return MouseScrollHandler::sInstance &&
    MouseScrollHandler::sInstance->mSynthesizingEvent &&
    MouseScrollHandler::sInstance->mSynthesizingEvent->mStatus !=
      NOT_SYNTHESIZING;
}

nsresult
MouseScrollHandler::SynthesizingEvent::Synthesize(const POINTS& aCursorPoint,
                                                  HWND aWnd,
                                                  UINT aMessage,
                                                  WPARAM aWParam,
                                                  LPARAM aLParam,
                                                  const BYTE (&aKeyStates)[256])
{
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScrollHandler::SynthesizingEvent::Synthesize(): aCursorPoint: { "
     "x: %d, y: %d }, aWnd=0x%X, aMessage=0x%04X, aWParam=0x%08X, "
     "aLParam=0x%08X, IsSynthesized()=%s, mStatus=%s",
     aCursorPoint.x, aCursorPoint.y, aWnd, aMessage, aWParam, aLParam,
     GetBoolName(IsSynthesizing()), GetStatusName()));

  if (IsSynthesizing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ::GetKeyboardState(mOriginalKeyState);

  // Note that we cannot use ::SetCursorPos() because it works asynchronously.
  // We should SEND the message for reducing the possibility of receiving
  // unexpected message which were not sent from here.
  mCursorPoint = aCursorPoint;

  mWnd = aWnd;
  mMessage = aMessage;
  mWParam = aWParam;
  mLParam = aLParam;

  memcpy(mKeyState, aKeyStates, sizeof(mKeyState));
  ::SetKeyboardState(mKeyState);

  mStatus = SENDING_MESSAGE;

  // Don't assume that aWnd is always managed by nsWindow.  It might be
  // a plugin window.
  ::SendMessage(aWnd, aMessage, aWParam, aLParam);

  return NS_OK;
}

void
MouseScrollHandler::SynthesizingEvent::NativeMessageReceived(nsWindowBase* aWidget,
                                                             UINT aMessage,
                                                             WPARAM aWParam,
                                                             LPARAM aLParam)
{
  if (mStatus == SENDING_MESSAGE && mMessage == aMessage &&
      mWParam == aWParam && mLParam == aLParam) {
    mStatus = NATIVE_MESSAGE_RECEIVED;
    if (aWidget && aWidget->GetWindowHandle() == mWnd) {
      return;
    }
    // If the target window is not ours and received window is our plugin
    // window, it comes from child window of the plugin.
    if (aWidget && aWidget->IsPlugin() &&
        !WinUtils::GetNSWindowBasePtr(mWnd)) {
      return;
    }
    // Otherwise, the message may not be sent by us.
  }

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScrollHandler::SynthesizingEvent::NativeMessageReceived(): "
     "aWidget=%p, aWidget->GetWindowHandle()=0x%X, mWnd=0x%X, "
     "aMessage=0x%04X, aWParam=0x%08X, aLParam=0x%08X, mStatus=%s",
     aWidget, aWidget ? aWidget->GetWindowHandle() : 0, mWnd,
     aMessage, aWParam, aLParam, GetStatusName()));

  // We failed to receive our sent message, we failed to do the job.
  Finish();

  return;
}

void
MouseScrollHandler::SynthesizingEvent::NotifyNativeMessageHandlingFinished()
{
  if (!IsSynthesizing()) {
    return;
  }

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScrollHandler::SynthesizingEvent::"
     "NotifyNativeMessageHandlingFinished(): IsWaitingInternalMessage=%s",
     GetBoolName(MouseScrollHandler::IsWaitingInternalMessage())));

  if (MouseScrollHandler::IsWaitingInternalMessage()) {
    mStatus = INTERNAL_MESSAGE_POSTED;
    return;
  }

  // If the native message handler didn't post our internal message,
  // we our job is finished.
  // TODO: When we post the message to plugin window, there is remaning job.
  Finish();
}

void
MouseScrollHandler::SynthesizingEvent::NotifyInternalMessageHandlingFinished()
{
  if (!IsSynthesizing()) {
    return;
  }

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScrollHandler::SynthesizingEvent::"
     "NotifyInternalMessageHandlingFinished()"));

  Finish();
}

void
MouseScrollHandler::SynthesizingEvent::Finish()
{
  if (!IsSynthesizing()) {
    return;
  }

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
    ("MouseScrollHandler::SynthesizingEvent::Finish()"));

  // Restore the original key state.
  ::SetKeyboardState(mOriginalKeyState);

  mStatus = NOT_SYNTHESIZING;
}

} // namespace widget
} // namespace mozilla
