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
