/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/WindowsVersion.h"

#include "nsUXThemeData.h"
#include "nsDebug.h"
#include "nsToolkit.h"
#include "nsUXThemeConstants.h"

using namespace mozilla;
using namespace mozilla::widget;

const wchar_t
nsUXThemeData::kThemeLibraryName[] = L"uxtheme.dll";

HANDLE
nsUXThemeData::sThemes[eUXNumClasses];

HMODULE
nsUXThemeData::sThemeDLL = nullptr;

const int NUM_COMMAND_BUTTONS = 3;
SIZE nsUXThemeData::sCommandButtonMetrics[NUM_COMMAND_BUTTONS];
bool nsUXThemeData::sCommandButtonMetricsInitialized = false;
SIZE nsUXThemeData::sCommandButtonBoxMetrics;
bool nsUXThemeData::sCommandButtonBoxMetricsInitialized = false;

bool
nsUXThemeData::sFlatMenus = false;

bool nsUXThemeData::sTitlebarInfoPopulatedAero = false;
bool nsUXThemeData::sTitlebarInfoPopulatedThemed = false;

void
nsUXThemeData::Teardown() {
  Invalidate();
  if(sThemeDLL)
    FreeLibrary(sThemeDLL);
}

void
nsUXThemeData::Initialize()
{
  ::ZeroMemory(sThemes, sizeof(sThemes));
  NS_ASSERTION(!sThemeDLL, "nsUXThemeData being initialized twice!");

  CheckForCompositor(true);
  Invalidate();
}

void
nsUXThemeData::Invalidate() {
  for(int i = 0; i < eUXNumClasses; i++) {
    if(sThemes[i]) {
      CloseThemeData(sThemes[i]);
      sThemes[i] = nullptr;
    }
  }
  BOOL useFlat = FALSE;
  sFlatMenus = ::SystemParametersInfo(SPI_GETFLATMENU, 0, &useFlat, 0) ?
                   useFlat : false;
}

HANDLE
nsUXThemeData::GetTheme(nsUXThemeClass cls) {
  NS_ASSERTION(cls < eUXNumClasses, "Invalid theme class!");
  if(!sThemes[cls])
  {
    sThemes[cls] = OpenThemeData(nullptr, GetClassName(cls));
  }
  return sThemes[cls];
}

HMODULE
nsUXThemeData::GetThemeDLL() {
  if (!sThemeDLL)
    sThemeDLL = ::LoadLibraryW(kThemeLibraryName);
  return sThemeDLL;
}

const wchar_t *nsUXThemeData::GetClassName(nsUXThemeClass cls) {
  switch(cls) {
    case eUXButton:
      return L"Button";
    case eUXEdit:
      return L"Edit";
    case eUXTooltip:
      return L"Tooltip";
    case eUXRebar:
      return L"Rebar";
    case eUXMediaRebar:
      return L"Media::Rebar";
    case eUXCommunicationsRebar:
      return L"Communications::Rebar";
    case eUXBrowserTabBarRebar:
      return L"BrowserTabBar::Rebar";
    case eUXToolbar:
      return L"Toolbar";
    case eUXMediaToolbar:
      return L"Media::Toolbar";
    case eUXCommunicationsToolbar:
      return L"Communications::Toolbar";
    case eUXProgress:
      return L"Progress";
    case eUXTab:
      return L"Tab";
    case eUXScrollbar:
      return L"Scrollbar";
    case eUXTrackbar:
      return L"Trackbar";
    case eUXSpin:
      return L"Spin";
    case eUXStatus:
      return L"Status";
    case eUXCombobox:
      return L"Combobox";
    case eUXHeader:
      return L"Header";
    case eUXListview:
      return L"Listview";
    case eUXMenu:
      return L"Menu";
    case eUXWindowFrame:
      return L"Window";
    default:
      NS_NOTREACHED("unknown uxtheme class");
      return L"";
  }
}

// static
void
nsUXThemeData::EnsureCommandButtonMetrics()
{
  if (sCommandButtonMetricsInitialized) {
    return;
  }
  sCommandButtonMetricsInitialized = true;

  // This code should never need to be evaluated since we should
  // obtain the metrics we need when nsWindow::Create() is called.
  // The generic metrics that we set here likley will not match the
  // current theme, but they insure the buttons at least show up if
  // there is some combination of theme settings that we aren't
  // handling in nsWindow::Create().
  MOZ_ASSERT_UNREACHABLE("We should avoid expensive GetSystemMetrics calls");

  sCommandButtonMetrics[0].cx = GetSystemMetrics(SM_CXSIZE);
  sCommandButtonMetrics[0].cy = GetSystemMetrics(SM_CYSIZE);
  sCommandButtonMetrics[1].cx = sCommandButtonMetrics[2].cx = sCommandButtonMetrics[0].cx;
  sCommandButtonMetrics[1].cy = sCommandButtonMetrics[2].cy = sCommandButtonMetrics[0].cy;

  // Trigger a refresh on the next layout.
  sTitlebarInfoPopulatedAero = sTitlebarInfoPopulatedThemed = false;
}

// static
void
nsUXThemeData::EnsureCommandButtonBoxMetrics()
{
  if (sCommandButtonBoxMetricsInitialized) {
    return;
  }
  sCommandButtonBoxMetricsInitialized = true;

  EnsureCommandButtonMetrics();

  sCommandButtonBoxMetrics.cx = sCommandButtonMetrics[0].cx
                                + sCommandButtonMetrics[1].cx
                                + sCommandButtonMetrics[2].cx;
  sCommandButtonBoxMetrics.cy = sCommandButtonMetrics[0].cy
                                + sCommandButtonMetrics[1].cy
                                + sCommandButtonMetrics[2].cy;

  // Trigger a refresh on the next layout.
  sTitlebarInfoPopulatedAero = sTitlebarInfoPopulatedThemed = false;
}

// static
void
nsUXThemeData::UpdateTitlebarInfo(HWND aWnd)
{
  if (!aWnd)
    return;

  if (!sTitlebarInfoPopulatedAero && nsUXThemeData::CheckForCompositor()) {
    RECT captionButtons;
    if (SUCCEEDED(DwmGetWindowAttribute(aWnd,
                                        DWMWA_CAPTION_BUTTON_BOUNDS,
                                        &captionButtons,
                                        sizeof(captionButtons)))) {
      sCommandButtonBoxMetrics.cx = captionButtons.right - captionButtons.left - 3;
      sCommandButtonBoxMetrics.cy = (captionButtons.bottom - captionButtons.top) - 1;
      sCommandButtonBoxMetricsInitialized = true;
      MOZ_ASSERT(sCommandButtonBoxMetrics.cx > 0 &&
                 sCommandButtonBoxMetrics.cy > 0,
                 "We must not cache bad command button box dimensions");
      sTitlebarInfoPopulatedAero = true;
    }
  }

  // NB: sTitlebarInfoPopulatedThemed is always true pre-vista.
  if (sTitlebarInfoPopulatedThemed || IsWin8OrLater())
    return;

  // Query a temporary, visible window with command buttons to get
  // the right metrics. 
  WNDCLASSW wc;
  wc.style         = 0;
  wc.lpfnWndProc   = ::DefWindowProcW;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = nsToolkit::mDllInstance;
  wc.hIcon         = nullptr;
  wc.hCursor       = nullptr;
  wc.hbrBackground = nullptr;
  wc.lpszMenuName  = nullptr;
  wc.lpszClassName = kClassNameTemp;
  ::RegisterClassW(&wc);

  // Create a transparent descendant of the window passed in. This
  // keeps the window from showing up on the desktop or the taskbar.
  // Note the parent (browser) window is usually still hidden, we
  // don't want to display it, so we can't query it directly.
  HWND hWnd = CreateWindowExW(WS_EX_LAYERED,
                              kClassNameTemp, L"",
                              WS_OVERLAPPEDWINDOW,
                              0, 0, 0, 0, aWnd, nullptr,
                              nsToolkit::mDllInstance, nullptr);
  NS_ASSERTION(hWnd, "UpdateTitlebarInfo window creation failed.");

  int showType = SW_SHOWNA;
  // We try to avoid activating this window, but on Aero basic (aero without
  // compositor) and aero lite (special theme for win server 2012/2013) we may
  // get the wrong information if the window isn't activated, so we have to:
  if (sThemeId == LookAndFeel::eWindowsTheme_AeroLite ||
      (sThemeId == LookAndFeel::eWindowsTheme_Aero && !nsUXThemeData::CheckForCompositor())) {
    showType = SW_SHOW;
  }
  ShowWindow(hWnd, showType);
  TITLEBARINFOEX info = {0};
  info.cbSize = sizeof(TITLEBARINFOEX);
  SendMessage(hWnd, WM_GETTITLEBARINFOEX, 0, (LPARAM)&info); 
  DestroyWindow(hWnd);

  // Only set if we have valid data for all three buttons we use.
  if ((info.rgrect[2].right - info.rgrect[2].left) == 0 ||
      (info.rgrect[3].right - info.rgrect[3].left) == 0 ||
      (info.rgrect[5].right - info.rgrect[5].left) == 0) {
    NS_WARNING("WM_GETTITLEBARINFOEX query failed to find usable metrics.");
    return;
  }
  // minimize
  sCommandButtonMetrics[0].cx = info.rgrect[2].right - info.rgrect[2].left;
  sCommandButtonMetrics[0].cy = info.rgrect[2].bottom - info.rgrect[2].top;
  // maximize/restore
  sCommandButtonMetrics[1].cx = info.rgrect[3].right - info.rgrect[3].left;
  sCommandButtonMetrics[1].cy = info.rgrect[3].bottom - info.rgrect[3].top;
  // close
  sCommandButtonMetrics[2].cx = info.rgrect[5].right - info.rgrect[5].left;
  sCommandButtonMetrics[2].cy = info.rgrect[5].bottom - info.rgrect[5].top;
  sCommandButtonMetricsInitialized = true;

#ifdef DEBUG
  // Verify that all values for the command buttons are positive values
  // otherwise we have cached bad values for the caption buttons
  for (int i = 0; i < NUM_COMMAND_BUTTONS; i++) {
    MOZ_ASSERT(sCommandButtonMetrics[i].cx > 0);
    MOZ_ASSERT(sCommandButtonMetrics[i].cy > 0);
  }
#endif

  sTitlebarInfoPopulatedThemed = true;
}

// visual style (aero glass, aero basic)
//    theme (aero, luna, zune)
//      theme color (silver, olive, blue)
//        system colors

struct THEMELIST {
  LPCWSTR name;
  int type;
};

const THEMELIST knownThemes[] = {
  { L"aero.msstyles", WINTHEME_AERO },
  { L"aerolite.msstyles", WINTHEME_AERO_LITE },
  { L"luna.msstyles", WINTHEME_LUNA },
  { L"zune.msstyles", WINTHEME_ZUNE },
  { L"royale.msstyles", WINTHEME_ROYALE }
};

const THEMELIST knownColors[] = {
  { L"normalcolor", WINTHEMECOLOR_NORMAL },
  { L"homestead",   WINTHEMECOLOR_HOMESTEAD },
  { L"metallic",    WINTHEMECOLOR_METALLIC }
};

LookAndFeel::WindowsTheme
nsUXThemeData::sThemeId = LookAndFeel::eWindowsTheme_Generic;

bool
nsUXThemeData::sIsDefaultWindowsTheme = false;
bool
nsUXThemeData::sIsHighContrastOn = false;

// static
LookAndFeel::WindowsTheme
nsUXThemeData::GetNativeThemeId()
{
  return sThemeId;
}

// static
bool nsUXThemeData::IsDefaultWindowTheme()
{
  return sIsDefaultWindowsTheme;
}

bool nsUXThemeData::IsHighContrastOn()
{
  return sIsHighContrastOn;
}

// static
bool nsUXThemeData::CheckForCompositor(bool aUpdateCache)
{
  static BOOL sCachedValue = FALSE;
  if (aUpdateCache) {
    DwmIsCompositionEnabled(&sCachedValue);
  }
  return sCachedValue;
}

// static
void
nsUXThemeData::UpdateNativeThemeInfo()
{
  // Trigger a refresh of themed button metrics if needed
  sTitlebarInfoPopulatedThemed = false;

  sIsDefaultWindowsTheme = false;
  sThemeId = LookAndFeel::eWindowsTheme_Generic;

  HIGHCONTRAST highContrastInfo;
  highContrastInfo.cbSize = sizeof(HIGHCONTRAST);
  if (SystemParametersInfo(SPI_GETHIGHCONTRAST, 0, &highContrastInfo, 0)) {
    sIsHighContrastOn = ((highContrastInfo.dwFlags & HCF_HIGHCONTRASTON) != 0);
  } else {
    sIsHighContrastOn = false;
  }

  if (!IsAppThemed()) {
    sThemeId = LookAndFeel::eWindowsTheme_Classic;
    return;
  }

  WCHAR themeFileName[MAX_PATH + 1];
  WCHAR themeColor[MAX_PATH + 1];
  if (FAILED(GetCurrentThemeName(themeFileName,
                                 MAX_PATH,
                                 themeColor,
                                 MAX_PATH,
                                 nullptr, 0))) {
    sThemeId = LookAndFeel::eWindowsTheme_Classic;
    return;
  }

  LPCWSTR themeName = wcsrchr(themeFileName, L'\\');
  themeName = themeName ? themeName + 1 : themeFileName;

  WindowsTheme theme = WINTHEME_UNRECOGNIZED;
  for (size_t i = 0; i < ArrayLength(knownThemes); ++i) {
    if (!lstrcmpiW(themeName, knownThemes[i].name)) {
      theme = (WindowsTheme)knownThemes[i].type;
      break;
    }
  }

  if (theme == WINTHEME_UNRECOGNIZED)
    return;

  // We're using the default theme if we're using any of Aero, Aero Lite, or
  // luna. However, on Win8, GetCurrentThemeName (see above) returns
  // AeroLite.msstyles for the 4 builtin highcontrast themes as well. Those
  // themes "don't count" as default themes, so we specifically check for high
  // contrast mode in that situation.
  if (!(IsWin8OrLater() && sIsHighContrastOn) &&
      (theme == WINTHEME_AERO || theme == WINTHEME_AERO_LITE || theme == WINTHEME_LUNA)) {
    sIsDefaultWindowsTheme = true;
  }

  if (theme != WINTHEME_LUNA) {
    switch(theme) {
      case WINTHEME_AERO:
        sThemeId = LookAndFeel::eWindowsTheme_Aero;
        return;
      case WINTHEME_AERO_LITE:
        sThemeId = LookAndFeel::eWindowsTheme_AeroLite;
        return;
      case WINTHEME_ZUNE:
        sThemeId = LookAndFeel::eWindowsTheme_Zune;
        return;
      case WINTHEME_ROYALE:
        sThemeId = LookAndFeel::eWindowsTheme_Royale;
        return;
      default:
        NS_WARNING("unhandled theme type.");
        return;
    }
  }

  // calculate the luna color scheme
  WindowsThemeColor color = WINTHEMECOLOR_UNRECOGNIZED;
  for (size_t i = 0; i < ArrayLength(knownColors); ++i) {
    if (!lstrcmpiW(themeColor, knownColors[i].name)) {
      color = (WindowsThemeColor)knownColors[i].type;
      break;
    }
  }

  switch(color) {
    case WINTHEMECOLOR_NORMAL:
      sThemeId = LookAndFeel::eWindowsTheme_LunaBlue;
      return;
    case WINTHEMECOLOR_HOMESTEAD:
      sThemeId = LookAndFeel::eWindowsTheme_LunaOlive;
      return;
    case WINTHEMECOLOR_METALLIC:
      sThemeId = LookAndFeel::eWindowsTheme_LunaSilver;
      return;
    default:
      NS_WARNING("unhandled theme color.");
      return;
  }
}
