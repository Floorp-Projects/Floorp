/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif // MOZ_LOGGING
#include "prlog.h"

#include "WinMouseScrollHandler.h"
#include "nsWindow.h"
#include "WinUtils.h"

#include "mozilla/Preferences.h"

#include <psapi.h>

namespace mozilla {
namespace widget {

#ifdef PR_LOGGING
PRLogModuleInfo* gMouseScrollLog = nsnull;

static const char* GetBoolName(bool aBool)
{
  return aBool ? "TRUE" : "FALSE";
}

static void LogKeyStateImpl()
{
  if (!PR_LOG_TEST(gMouseScrollLog, PR_LOG_DEBUG)) {
    return;
  }
  BYTE keyboardState[256];
  if (::GetKeyboardState(keyboardState)) {
    for (size_t i = 0; i < ArrayLength(keyboardState); i++) {
      if (keyboardState[i]) {
        PR_LOG(gMouseScrollLog, PR_LOG_DEBUG,
          ("    Current key state: keyboardState[0x%02X]=0x%02X (%s)",
           i, keyboardState[i],
           ((keyboardState[i] & 0x81) == 0x81) ? "Pressed and Toggled" :
           (keyboardState[i] & 0x80) ? "Pressed" :
           (keyboardState[i] & 0x01) ? "Toggled" : "Unknown"));
      }
    }
  } else {
    PR_LOG(gMouseScrollLog, PR_LOG_DEBUG,
      ("MouseScroll::Device::Elantech::HandleKeyMessage(): Failed to print "
       "current keyboard state"));
  }
}

#define LOG_KEYSTATE() LogKeyStateImpl()
#else // PR_LOGGING
#define LOG_KEYSTATE()
#endif

MouseScrollHandler* MouseScrollHandler::sInstance = nsnull;

bool MouseScrollHandler::Device::sFakeScrollableWindowNeeded = false;

bool MouseScrollHandler::Device::Elantech::sUseSwipeHack = false;
bool MouseScrollHandler::Device::Elantech::sUsePinchHack = false;
DWORD MouseScrollHandler::Device::Elantech::sZoomUntil = 0;

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
void
MouseScrollHandler::Initialize()
{
#ifdef PR_LOGGING
  if (!gMouseScrollLog) {
    gMouseScrollLog = PR_NewLogModule("MouseScrollHandlerWidgets");
  }
#endif
  Device::Init();
}

/* static */
void
MouseScrollHandler::Shutdown()
{
  delete sInstance;
  sInstance = nsnull;
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

MouseScrollHandler::MouseScrollHandler()
{
  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScroll: Creating an instance, this=%p, sInstance=%p",
     this, sInstance));
}

MouseScrollHandler::~MouseScrollHandler()
{
  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScroll: Destroying an instance, this=%p, sInstance=%p",
     this, sInstance));
}

/* static */
bool
MouseScrollHandler::ProcessMessage(nsWindow* aWindow, UINT msg,
                                   WPARAM wParam, LPARAM lParam,
                                   LRESULT *aRetValue, bool &aEatMessage)
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

    case WM_KEYDOWN:
    case WM_KEYUP:
      PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
        ("MouseScroll::ProcessMessage(): aWindow=%p, "
         "msg=%s(0x%04X), wParam=0x%02X, ::GetMessageTime()=%d",
         aWindow, msg == WM_KEYDOWN ? "WM_KEYDOWN" :
                    msg == WM_KEYUP ? "WM_KEYUP" : "Unknown", msg, wParam,
         ::GetMessageTime()));
      LOG_KEYSTATE();
      if (Device::Elantech::HandleKeyMessage(aWindow, msg, wParam)) {
        *aRetValue = 0;
        aEatMessage = true;
        return true;
      }
      return false;

    default:
      return false;
  }
}

/* static */
bool
MouseScrollHandler::DispatchEvent(nsWindow* aWindow, nsGUIEvent& aEvent)
{
  return aWindow->DispatchWindowEvent(&aEvent);
}

/* static */
nsModifierKeyState
MouseScrollHandler::GetModifierKeyState()
{
  nsModifierKeyState result;
  // Assume the Control key is down if the Elantech touchpad has sent the
  // mis-ordered WM_KEYDOWN/WM_MOUSEWHEEL messages.  (See the comment in
  // MouseScrollHandler::Device::Elantech::HandleKeyMessage().)
  if (!result.mIsControlDown) {
    result.mIsControlDown = Device::Elantech::IsZooming();
  }
  return result;
}

MouseScrollHandler::ScrollTargetInfo
MouseScrollHandler::GetScrollTargetInfo(
                      nsWindow* aWindow,
                      const EventInfo& aEventInfo,
                      const nsModifierKeyState& aModifierKeyState)
{
  ScrollTargetInfo result;
  result.dispatchPixelScrollEvent = false;
  result.reversePixelScrollDirection = false;
  result.actualScrollAmount = aEventInfo.GetScrollAmount();
  result.actualScrollAction = nsQueryContentEvent::SCROLL_ACTION_NONE;
  result.pixelsPerUnit = 0;
  if (!mUserPrefs.IsPixelScrollingEnabled()) {
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::GetPixelScrollInfo: Succeeded, aWindow=%p, "
       "result: { dispatchPixelScrollEvent: %s, actualScrollAmount: %d }",
       aWindow, GetBoolName(result.dispatchPixelScrollEvent),
       result.actualScrollAmount));
    return result;
  }

  nsMouseScrollEvent testEvent(true, NS_MOUSE_SCROLL, aWindow);
  aWindow->InitEvent(testEvent);
  aModifierKeyState.InitInputEvent(testEvent);

  testEvent.scrollFlags = aEventInfo.GetScrollFlags();
  testEvent.delta       = result.actualScrollAmount;
  if ((aEventInfo.IsVertical() && aEventInfo.IsPositive()) ||
      (!aEventInfo.IsVertical() && !aEventInfo.IsPositive())) {
    testEvent.delta *= -1;
  }

  nsQueryContentEvent queryEvent(true, NS_QUERY_SCROLL_TARGET_INFO, aWindow);
  aWindow->InitEvent(queryEvent);
  queryEvent.InitForQueryScrollTargetInfo(&testEvent);
  DispatchEvent(aWindow, queryEvent);

  // If the necessary interger isn't larger than 0, we should assume that
  // the event failed for us.
  if (!queryEvent.mSucceeded) {
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::GetPixelScrollInfo: Failed to query the "
       "scroll target information, aWindow=%p"
       "result: { dispatchPixelScrollEvent: %s, actualScrollAmount: %d }",
       aWindow, GetBoolName(result.dispatchPixelScrollEvent),
       result.actualScrollAmount));
    return result;
  }

  result.actualScrollAction = queryEvent.mReply.mComputedScrollAction;

  if (result.actualScrollAction == nsQueryContentEvent::SCROLL_ACTION_PAGE) {
    result.pixelsPerUnit =
      aEventInfo.IsVertical() ? queryEvent.mReply.mPageHeight :
                                queryEvent.mReply.mPageWidth;
  } else {
    result.pixelsPerUnit = queryEvent.mReply.mLineHeight;
  }

  result.actualScrollAmount = queryEvent.mReply.mComputedScrollAmount;

  if (result.pixelsPerUnit > 0 && result.actualScrollAmount != 0 &&
      result.actualScrollAction != nsQueryContentEvent::SCROLL_ACTION_NONE) {
    result.dispatchPixelScrollEvent = true;
    // If original delta's sign and computed delta's one are different,
    // we need to reverse the pixel scroll direction at dispatching it.
    result.reversePixelScrollDirection =
      (testEvent.delta > 0 && result.actualScrollAmount < 0) ||
      (testEvent.delta < 0 && result.actualScrollAmount > 0);
    // scroll amount must be positive.
    result.actualScrollAmount = NS_ABS(result.actualScrollAmount);
  }

  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScroll::GetPixelScrollInfo: Succeeded, aWindow=%p, "
     "result: { dispatchPixelScrollEvent: %s, reversePixelScrollDirection: %s, "
     "actualScrollAmount: %d, actualScrollAction: 0x%01X, "
     "pixelsPerUnit: %d }",
     aWindow, GetBoolName(result.dispatchPixelScrollEvent),
     GetBoolName(result.reversePixelScrollDirection), result.actualScrollAmount,
     result.actualScrollAction, result.pixelsPerUnit));

  return result;
}

/******************************************************************************
 *
 * EventInfo
 *
 ******************************************************************************/

MouseScrollHandler::EventInfo::EventInfo(nsWindow* aWindow,
                                         UINT aMessage,
                                         WPARAM aWParam, LPARAM aLParam)
{
  NS_ABORT_IF_FALSE(aMessage == WM_MOUSEWHEEL || aMessage == WM_MOUSEHWHEEL,
    "EventInfo must be initialized with WM_MOUSEWHEEL or WM_MOUSEHWHEEL");

  MouseScrollHandler::GetInstance()->mSystemSettings.Init();

  mIsVertical = (aMessage == WM_MOUSEWHEEL);
  mIsPage = MouseScrollHandler::sInstance->
              mSystemSettings.IsPageScroll(mIsVertical);
  mDelta = (short)HIWORD(aWParam);
  mWnd = aWindow->GetWindowHandle();
  mTimeStamp = TimeStamp::Now();
}

bool
MouseScrollHandler::EventInfo::CanDispatchMouseScrollEvent() const
{
  if (!GetScrollAmount()) {
    // XXX I think that we should dispatch mouse wheel events even if the
    // operation will not scroll because the wheel operation really happened
    // and web application may want to handle the event for non-scroll action.
    return false;
  }

  return (mDelta != 0);
}

PRInt32
MouseScrollHandler::EventInfo::GetScrollAmount() const
{
  if (mIsPage) {
    return 1;
  }
  return MouseScrollHandler::sInstance->
           mSystemSettings.GetScrollAmount(mIsVertical);
}

PRInt32
MouseScrollHandler::EventInfo::GetScrollFlags() const
{
  PRInt32 result = mIsPage ? nsMouseScrollEvent::kIsFullPage : 0;
  result |= mIsVertical ? nsMouseScrollEvent::kIsVertical :
                          nsMouseScrollEvent::kIsHorizontal;
  return result;
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
  return !mWnd ||
           (mWnd == aNewEvent.GetWindowHandle() &&
            IsPositive() == aNewEvent.IsPositive() &&
            mIsVertical == aNewEvent.IsVertical() &&
            mIsPage == aNewEvent.IsPage() &&
            TimeStamp::Now() - mTimeStamp <=
              TimeDuration::FromMilliseconds(DEFAULT_TIMEOUT_DURATION));
}

void
MouseScrollHandler::LastEventInfo::ResetTransaction()
{
  if (!mWnd) {
    return;
  }

  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScroll::LastEventInfo::ResetTransaction()"));

  mWnd = nsnull;
  mRemainingDeltaForScroll = 0;
  mRemainingDeltaForPixel = 0;
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
PRInt32
MouseScrollHandler::LastEventInfo::RoundDelta(double aDelta)
{
  return (aDelta >= 0) ? (PRInt32)floor(aDelta) : (PRInt32)ceil(aDelta);
}

bool
MouseScrollHandler::LastEventInfo::InitMouseScrollEvent(
                                     nsWindow* aWindow,
                                     nsMouseScrollEvent& aMouseScrollEvent,
                                     const ScrollTargetInfo& aScrollTargetInfo,
                                     const nsModifierKeyState& aModKeyState)
{
  NS_ABORT_IF_FALSE(aMouseScrollEvent.message == NS_MOUSE_SCROLL,
    "aMouseScrollEvent must be NS_MOUSE_SCROLL");

  // XXX Why don't we use lParam value? We should use lParam value because
  //     our internal message is always posted by original message handler.
  //     So, GetMessagePos() may return different cursor position.
  aWindow->InitEvent(aMouseScrollEvent);

  aModKeyState.InitInputEvent(aMouseScrollEvent);

  // If we dispatch pixel scroll event after the line scroll event,
  // we should set kHasPixels flag to the line scroll event.
  aMouseScrollEvent.scrollFlags =
    aScrollTargetInfo.dispatchPixelScrollEvent ?
      nsMouseScrollEvent::kHasPixels : 0;
  aMouseScrollEvent.scrollFlags |= GetScrollFlags();

  // Our positive delta value means to bottom or right.
  // But positive native delta value means to top or right.
  // Use orienter for computing our delta value with native delta value.
  PRInt32 orienter = mIsVertical ? -1 : 1;

  // NOTE: Don't use aScrollTargetInfo.actualScrollAmount for computing the
  //       delta value of line/page scroll event.  The value will be recomputed
  //       in ESM.
  PRInt32 nativeDelta = mDelta + mRemainingDeltaForScroll;
  if (IsPage()) {
    aMouseScrollEvent.delta = nativeDelta * orienter / WHEEL_DELTA;
    PRInt32 recomputedNativeDelta =
      aMouseScrollEvent.delta * orienter / WHEEL_DELTA;
    mRemainingDeltaForScroll = nativeDelta - recomputedNativeDelta;
  } else {
    double deltaPerUnit = (double)WHEEL_DELTA / GetScrollAmount();
    aMouseScrollEvent.delta = 
      RoundDelta((double)nativeDelta * orienter / deltaPerUnit);
    PRInt32 recomputedNativeDelta =
      (PRInt32)(aMouseScrollEvent.delta * orienter * deltaPerUnit);
    mRemainingDeltaForScroll = nativeDelta - recomputedNativeDelta;
  }

  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScroll::LastEventInfo::InitMouseScrollEvent: aWindow=%p, "
     "aMouseScrollEvent { refPoint: { x: %d, y: %d }, delta: %d, "
     "scrollFlags: 0x%04X, isShift: %s, isControl: %s, isAlt: %s, "
     "isMeta: %s }, mRemainingDeltaForScroll: %d",
     aWindow, aMouseScrollEvent.refPoint.x, aMouseScrollEvent.refPoint.y,
     aMouseScrollEvent.delta, aMouseScrollEvent.scrollFlags,
     GetBoolName(aMouseScrollEvent.isShift),
     GetBoolName(aMouseScrollEvent.isControl),
     GetBoolName(aMouseScrollEvent.isAlt),
     GetBoolName(aMouseScrollEvent.isMeta), mRemainingDeltaForScroll));

  return (aMouseScrollEvent.delta != 0);
}

bool
MouseScrollHandler::LastEventInfo::InitMousePixelScrollEvent(
                                     nsWindow* aWindow,
                                     nsMouseScrollEvent& aPixelScrollEvent,
                                     const ScrollTargetInfo& aScrollTargetInfo,
                                     const nsModifierKeyState& aModKeyState)
{
  NS_ABORT_IF_FALSE(aPixelScrollEvent.message == NS_MOUSE_PIXEL_SCROLL,
    "aPixelScrollEvent must be NS_MOUSE_PIXEL_SCROLL");

  if (!aScrollTargetInfo.dispatchPixelScrollEvent) {
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::LastEventInfo::InitMousePixelScrollEvent: aWindow=%p, "
       "PixelScroll is disabled",
       aWindow, mRemainingDeltaForPixel));

    mRemainingDeltaForPixel = 0;
    return false;
  }

  // XXX Why don't we use lParam value? We should use lParam value because
  //     our internal message is always posted by original message handler.
  //     So, GetMessagePos() may return different cursor position.
  aWindow->InitEvent(aPixelScrollEvent);

  aModKeyState.InitInputEvent(aPixelScrollEvent);

  aPixelScrollEvent.scrollFlags = nsMouseScrollEvent::kAllowSmoothScroll;
  aPixelScrollEvent.scrollFlags |= mIsVertical ?
    nsMouseScrollEvent::kIsVertical : nsMouseScrollEvent::kIsHorizontal;
  if (aScrollTargetInfo.actualScrollAction ==
        nsQueryContentEvent::SCROLL_ACTION_PAGE) {
    aPixelScrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
  }

  // Our positive delta value means to bottom or right.
  // But positive native delta value means to top or right.
  // Use orienter for computing our delta value with native delta value.
  PRInt32 orienter = mIsVertical ? -1 : 1;
  // However, pixel scroll event won't be recomputed the scroll amout and
  // direction by ESM.  Therefore, we need to set the computed amout and
  // direction here.
  if (aScrollTargetInfo.reversePixelScrollDirection) {
    orienter *= -1;
  }

  PRInt32 nativeDelta = mDelta + mRemainingDeltaForPixel;
  double deltaPerPixel = (double)WHEEL_DELTA /
    aScrollTargetInfo.actualScrollAmount / aScrollTargetInfo.pixelsPerUnit;
  aPixelScrollEvent.delta =
    RoundDelta((double)nativeDelta * orienter / deltaPerPixel);
  PRInt32 recomputedNativeDelta =
    (PRInt32)(aPixelScrollEvent.delta * orienter * deltaPerPixel);
  mRemainingDeltaForPixel = nativeDelta - recomputedNativeDelta;

  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScroll::LastEventInfo::InitMousePixelScrollEvent: aWindow=%p, "
     "aPixelScrollEvent { refPoint: { x: %d, y: %d }, delta: %d, "
     "scrollFlags: 0x%04X, isShift: %s, isControl: %s, isAlt: %s, "
     "isMeta: %s }, mRemainingDeltaForScroll: %d",
     aWindow, aPixelScrollEvent.refPoint.x, aPixelScrollEvent.refPoint.y,
     aPixelScrollEvent.delta, aPixelScrollEvent.scrollFlags,
     GetBoolName(aPixelScrollEvent.isShift),
     GetBoolName(aPixelScrollEvent.isControl),
     GetBoolName(aPixelScrollEvent.isAlt),
     GetBoolName(aPixelScrollEvent.isMeta), mRemainingDeltaForPixel));

  return (aPixelScrollEvent.delta != 0);
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

  mInitialized = true;

  if (!::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &mScrollLines, 0)) {
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::SystemSettings::Init(): ::SystemParametersInfo("
         "SPI_GETWHEELSCROLLLINES) failed"));
    mScrollLines = 3;
  } else if (mScrollLines > WHEEL_DELTA) {
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::SystemSettings::Init(): the result of "
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

  if (!::SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &mScrollChars, 0)) {
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::SystemSettings::Init(): ::SystemParametersInfo("
         "SPI_GETWHEELSCROLLCHARS) failed, %s",
       WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION ?
         "this is unexpected on Vista or later" :
         "but on XP or earlier, this is not a problem"));
    mScrollChars = 1;
  } else if (mScrollChars > WHEEL_DELTA) {
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::SystemSettings::Init(): the result of "
         "::SystemParametersInfo(SPI_GETWHEELSCROLLCHARS) is too large: %d",
       mScrollChars));
    // See the comments for the case mScrollLines > WHEEL_DELTA.
    mScrollChars = WHEEL_PAGESCROLL;
  }

  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScroll::SystemSettings::Init(): initialized, "
       "mScrollLines=%d, mScrollChars=%d",
     mScrollLines, mScrollChars));
}

void
MouseScrollHandler::SystemSettings::MarkDirty()
{
  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScrollHandler::SystemSettings::MarkDirty(): "
       "Marking SystemSettings dirty"));
  mInitialized = false;
  // When system settings are changed, we should reset current transaction.
  MOZ_ASSERT(sInstance,
    "Must not be called at initializing MouseScrollHandler");
  MouseScrollHandler::sInstance->mLastEventInfo.ResetTransaction();
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

  mPixelScrollingEnabled =
    Preferences::GetBool("mousewheel.enable_pixel_scrolling", true);
  mScrollMessageHandledAsWheelMessage =
    Preferences::GetBool("mousewheel.emulate_at_wm_scroll", false);

  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScroll::UserPrefs::Init(): initialized, "
       "mPixelScrollingEnabled=%s, mScrollMessageHandledAsWheelMessage=%s",
     GetBoolName(mPixelScrollingEnabled),
     GetBoolName(mScrollMessageHandledAsWheelMessage)));
}

void
MouseScrollHandler::UserPrefs::MarkDirty()
{
  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScrollHandler::UserPrefs::MarkDirty(): Marking UserPrefs dirty"));
  mInitialized = false;
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
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::Device::GetWorkaroundPref(): Failed, aPrefName is NULL"));
    return aValueIfAutomatic;
  }

  PRInt32 lHackValue = 0;
  if (NS_FAILED(Preferences::GetInt(aPrefName, &lHackValue))) {
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::Device::GetWorkaroundPref(): Preferences::GetInt() failed,"
       " aPrefName=\"%s\", aValueIfAutomatic=%s",
       aPrefName, GetBoolName(aValueIfAutomatic)));
    return aValueIfAutomatic;
  }

  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
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
  sFakeScrollableWindowNeeded =
    GetWorkaroundPref("ui.trackpoint_hack.enabled",
                      (TrackPoint::IsDriverInstalled() ||
                       UltraNav::IsObsoleteDriverInstalled()));

  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScroll::Device::Init(): sFakeScrollableWindowNeeded=%s",
     GetBoolName(sFakeScrollableWindowNeeded)));

  Elantech::Init();
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
  PRInt32 version = GetDriverMajorVersion();
  bool needsHack =
    Device::GetWorkaroundPref("ui.elantech_gesture_hacks.enabled",
                              version != 0);
  sUseSwipeHack = needsHack && version <= 7;
  sUsePinchHack = needsHack && version <= 8;

  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScroll::Device::Elantech::Init(): version=%d, sUseSwipeHack=%s, "
     "sUsePinchHack=%s",
     version, GetBoolName(sUseSwipeHack), GetBoolName(sUsePinchHack)));
}

/* static */
PRInt32
MouseScrollHandler::Device::Elantech::GetDriverMajorVersion()
{
  PRUnichar buf[40];
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
  for (PRUnichar* p = buf; *p; p++) {
    if (*p >= L'0' && *p <= L'9' && (p == buf || *(p - 1) == L' ')) {
      return wcstol(p, NULL, 10);
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

  const PRUnichar* filenameSuffix = L"\\etdctrl.exe";
  const int filenameSuffixLength = 12;

  DWORD pid;
  ::GetWindowThreadProcessId(aWnd, &pid);

  HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (!hProcess) {
    return false;
  }

  bool result = false;
  PRUnichar path[256] = {L'\0'};
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
MouseScrollHandler::Device::Elantech::HandleKeyMessage(nsWindow* aWindow,
                                                       UINT aMsg,
                                                       WPARAM aWParam)
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
  //   WM_KEYDOWN virtual_key = 0xCC or 0xFF (depending on driver version)
  //   WM_KEYDOWN virtual_key = VK_NEXT
  //   WM_KEYUP   virtual_key = VK_NEXT
  //   WM_KEYUP   virtual_key = 0xCC or 0xFF
  //
  // so we use the 0xCC or 0xFF key modifier to detect whether the Page Down
  // is due to the gesture rather than a regular Page Down keypress.  We then
  // pretend that we should dispatch "Go Forward" command.  Similarly
  // for VK_PRIOR and "Go Back" command.
  if (sUseSwipeHack &&
      (aWParam == VK_NEXT || aWParam == VK_PRIOR) &&
      (IS_VK_DOWN(0xFF) || IS_VK_DOWN(0xCC))) {
    if (aMsg == WM_KEYDOWN) {
      PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
        ("MouseScroll::Device::Elantech::HandleKeyMessage(): Dispatching "
         "%s command event",
         aWParam == VK_NEXT ? "Forward" : "Back"));

      nsCommandEvent commandEvent(true, nsGkAtoms::onAppCommand,
        (aWParam == VK_NEXT) ? nsGkAtoms::Forward : nsGkAtoms::Back, aWindow);
      aWindow->InitEvent(commandEvent);
      MouseScrollHandler::DispatchEvent(aWindow, commandEvent);
    }
#ifdef PR_LOGGING
    else {
      PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
        ("MouseScroll::Device::Elantech::HandleKeyMessage(): Consumed"));
    }
#endif
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

    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
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

    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
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
 * Device::TrackPoint
 *
 ******************************************************************************/

/* static */
bool
MouseScrollHandler::Device::TrackPoint::IsDriverInstalled()
{
  if (WinUtils::HasRegistryKey(HKEY_CURRENT_USER,
                               L"Software\\Lenovo\\TrackPoint")) {
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::Device::TrackPoint::IsDriverInstalled(): "
       "Lenovo's TrackPoint driver is found"));
    return true;
  }

  if (WinUtils::HasRegistryKey(HKEY_CURRENT_USER,
                               L"Software\\Alps\\Apoint\\TrackPoint")) {
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
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
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::Device::UltraNav::IsObsoleteDriverInstalled(): "
       "Lenovo's UltraNav driver is found"));
    return true;
  }

  bool installed = false;
  if (WinUtils::HasRegistryKey(HKEY_CURRENT_USER,
        L"Software\\Synaptics\\SynTPEnh\\UltraNavUSB")) {
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::Device::UltraNav::IsObsoleteDriverInstalled(): "
       "Synaptics's UltraNav (USB) driver is found"));
    installed = true;
  } else if (WinUtils::HasRegistryKey(HKEY_CURRENT_USER,
               L"Software\\Synaptics\\SynTPEnh\\UltraNavPS2")) {
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::Device::UltraNav::IsObsoleteDriverInstalled(): "
       "Synaptics's UltraNav (PS/2) driver is found"));
    installed = true;
  }

  if (!installed) {
    return false;
  }

  PRUnichar buf[40];
  bool foundKey =
    WinUtils::GetRegistryKey(HKEY_LOCAL_MACHINE,
                             L"Software\\Synaptics\\SynTP\\Install",
                             L"DriverVersion",
                             buf, sizeof buf);
  if (!foundKey) {
    PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
      ("MouseScroll::Device::UltraNav::IsObsoleteDriverInstalled(): "
       "Failed to get UltraNav driver version"));
    return false;
  }

  int majorVersion = wcstol(buf, NULL, 10);
  int minorVersion = 0;
  PRUnichar* p = wcschr(buf, L'.');
  if (p) {
    minorVersion = wcstol(p + 1, NULL, 10);
  }
  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScroll::Device::UltraNav::IsObsoleteDriverInstalled(): "
     "found driver version = %d.%d",
     majorVersion, minorVersion));
  return majorVersion < 15 || majorVersion == 15 && minorVersion == 0;
}

} // namespace widget
} // namespace mozilla
