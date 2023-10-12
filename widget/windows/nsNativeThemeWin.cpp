/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeWin.h"

#include <algorithm>
#include <malloc.h>

#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxWindowsNativeDrawing.h"
#include "gfxWindowsPlatform.h"
#include "gfxWindowsSurface.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/gfx/Types.h"  // for Color::FromABGR
#include "mozilla/Logging.h"
#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/dom/XULButtonElement.h"
#include "nsColor.h"
#include "nsComboboxControlFrame.h"
#include "nsDeviceContext.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsLookAndFeel.h"
#include "nsNameSpaceManager.h"
#include "Theme.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsStyleConsts.h"
#include "nsTransform2D.h"
#include "nsWindow.h"
#include "prinrval.h"
#include "WinUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;

using ElementState = dom::ElementState;

extern mozilla::LazyLogModule gWindowsLog;

namespace mozilla::widget {

nsNativeThemeWin::nsNativeThemeWin()
    : Theme(ScrollbarStyle()),
      mProgressDeterminateTimeStamp(TimeStamp::Now()),
      mProgressIndeterminateTimeStamp(TimeStamp::Now()),
      mBorderCacheValid(),
      mMinimumWidgetSizeCacheValid(),
      mGutterSizeCacheValid(false) {}

nsNativeThemeWin::~nsNativeThemeWin() { nsUXThemeData::Invalidate(); }

bool nsNativeThemeWin::IsWidgetAlwaysNonNative(nsIFrame* aFrame,
                                               StyleAppearance aAppearance) {
  return Theme::IsWidgetAlwaysNonNative(aFrame, aAppearance) ||
         aAppearance == StyleAppearance::Checkbox ||
         aAppearance == StyleAppearance::Radio ||
         aAppearance == StyleAppearance::MozMenulistArrowButton ||
         aAppearance == StyleAppearance::SpinnerUpbutton ||
         aAppearance == StyleAppearance::SpinnerDownbutton;
}

auto nsNativeThemeWin::IsWidgetNonNative(nsIFrame* aFrame,
                                         StyleAppearance aAppearance)
    -> NonNative {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return NonNative::Always;
  }

  // We only know how to draw light widgets, so we defer to the non-native
  // theme when appropriate.
  if (Theme::ThemeSupportsWidget(aFrame->PresContext(), aFrame, aAppearance) &&
      LookAndFeel::ColorSchemeForFrame(aFrame) ==
          LookAndFeel::ColorScheme::Dark) {
    return NonNative::BecauseColorMismatch;
  }
  return NonNative::No;
}

static MARGINS GetCheckboxMargins(HANDLE theme, HDC hdc) {
  MARGINS checkboxContent = {0};
  GetThemeMargins(theme, hdc, MENU_POPUPCHECK, MCB_NORMAL, TMT_CONTENTMARGINS,
                  nullptr, &checkboxContent);
  return checkboxContent;
}

static SIZE GetCheckboxBGSize(HANDLE theme, HDC hdc) {
  SIZE checkboxSize;
  GetThemePartSize(theme, hdc, MENU_POPUPCHECK, MC_CHECKMARKNORMAL, nullptr,
                   TS_TRUE, &checkboxSize);

  MARGINS checkboxMargins = GetCheckboxMargins(theme, hdc);

  int leftMargin = checkboxMargins.cxLeftWidth;
  int rightMargin = checkboxMargins.cxRightWidth;
  int topMargin = checkboxMargins.cyTopHeight;
  int bottomMargin = checkboxMargins.cyBottomHeight;

  int width = leftMargin + checkboxSize.cx + rightMargin;
  int height = topMargin + checkboxSize.cy + bottomMargin;
  SIZE ret;
  ret.cx = width;
  ret.cy = height;
  return ret;
}

static SIZE GetCheckboxBGBounds(HANDLE theme, HDC hdc) {
  MARGINS checkboxBGSizing = {0};
  MARGINS checkboxBGContent = {0};
  GetThemeMargins(theme, hdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL,
                  TMT_SIZINGMARGINS, nullptr, &checkboxBGSizing);
  GetThemeMargins(theme, hdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL,
                  TMT_CONTENTMARGINS, nullptr, &checkboxBGContent);

#define posdx(d) ((d) > 0 ? d : 0)

  int dx =
      posdx(checkboxBGContent.cxRightWidth - checkboxBGSizing.cxRightWidth) +
      posdx(checkboxBGContent.cxLeftWidth - checkboxBGSizing.cxLeftWidth);
  int dy =
      posdx(checkboxBGContent.cyTopHeight - checkboxBGSizing.cyTopHeight) +
      posdx(checkboxBGContent.cyBottomHeight - checkboxBGSizing.cyBottomHeight);

#undef posdx

  SIZE ret(GetCheckboxBGSize(theme, hdc));
  ret.cx += dx;
  ret.cy += dy;
  return ret;
}

static SIZE GetGutterSize(HANDLE theme, HDC hdc) {
  SIZE gutterSize;
  GetThemePartSize(theme, hdc, MENU_POPUPGUTTER, 0, nullptr, TS_TRUE,
                   &gutterSize);

  SIZE checkboxBGSize(GetCheckboxBGBounds(theme, hdc));

  SIZE itemSize;
  GetThemePartSize(theme, hdc, MENU_POPUPITEM, MPI_NORMAL, nullptr, TS_TRUE,
                   &itemSize);

  // Figure out how big the menuitem's icon will be (if present) at current DPI
  // Needs the system scale for consistency with Windows Theme API.
  double scaleFactor = WinUtils::SystemScaleFactor();
  int iconDevicePixels = NSToIntRound(16 * scaleFactor);
  SIZE iconSize = {iconDevicePixels, iconDevicePixels};
  // Not really sure what margins should be used here, but this seems to work in
  // practice...
  MARGINS margins = {0};
  GetThemeMargins(theme, hdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL,
                  TMT_CONTENTMARGINS, nullptr, &margins);
  iconSize.cx += margins.cxLeftWidth + margins.cxRightWidth;
  iconSize.cy += margins.cyTopHeight + margins.cyBottomHeight;

  int width = std::max(
      itemSize.cx, std::max(iconSize.cx, checkboxBGSize.cx) + gutterSize.cx);
  int height = std::max(itemSize.cy, std::max(iconSize.cy, checkboxBGSize.cy));

  SIZE ret;
  ret.cx = width;
  ret.cy = height;
  return ret;
}

SIZE nsNativeThemeWin::GetCachedGutterSize(HANDLE theme) {
  if (mGutterSizeCacheValid) {
    return mGutterSizeCache;
  }

  mGutterSizeCache = GetGutterSize(theme, nullptr);
  mGutterSizeCacheValid = true;

  return mGutterSizeCache;
}

/*
 * Notes on progress track and meter part constants:
 * xp and up:
 * PP_BAR(_VERT)            - base progress track
 * PP_TRANSPARENTBAR(_VERT) - transparent progress track. this only works if
 *                            the underlying surface supports alpha. otherwise
 *                            theme lib's DrawThemeBackground falls back on
 *                            opaque PP_BAR. we currently don't use this.
 * PP_CHUNK(_VERT)          - xp progress meter. this does not draw an xp style
 *                            progress w/chunks, it draws fill using the chunk
 *                            graphic.
 * vista and up:
 * PP_FILL(_VERT)           - progress meter. these have four states/colors.
 * PP_PULSEOVERLAY(_VERT)   - white reflection - an overlay, not sure what this
 *                            is used for.
 * PP_MOVEOVERLAY(_VERT)    - green pulse - the pulse effect overlay on
 *                            determined progress bars. we also use this for
 *                            indeterminate chunk.
 *
 * Notes on state constants:
 * PBBS_NORMAL               - green progress
 * PBBVS_PARTIAL/PBFVS_ERROR - red error progress
 * PBFS_PAUSED               - yellow paused progress
 *
 * There is no common controls style indeterminate part on vista and up.
 */

/*
 * Progress bar related constants. These values are found by experimenting and
 * comparing against native widgets used by the system. They are very unlikely
 * exact but try to not be too wrong.
 */
// The amount of time we animate progress meters parts across the frame.
static const double kProgressDeterminateTimeSpan = 3.0;
static const double kProgressIndeterminateTimeSpan = 5.0;
// The width of the overlay used to animate the horizontal progress bar (Vista
// and later).
static const int32_t kProgressHorizontalOverlaySize = 120;
// The height of the overlay used to animate the vertical progress bar (Vista
// and later).
static const int32_t kProgressVerticalOverlaySize = 45;
// The height of the overlay used for the vertical indeterminate progress bar
// (Vista and later).
static const int32_t kProgressVerticalIndeterminateOverlaySize = 60;
// The width of the overlay used to animate the indeterminate progress bar
// (Windows Classic).
static const int32_t kProgressClassicOverlaySize = 40;

/*
 * GetProgressOverlayStyle - returns the proper overlay part for themed
 * progress bars based on os and orientation.
 */
static int32_t GetProgressOverlayStyle(bool aIsVertical) {
  return aIsVertical ? PP_MOVEOVERLAYVERT : PP_MOVEOVERLAY;
}

/*
 * GetProgressOverlaySize - returns the minimum width or height for themed
 * progress bar overlays. This includes the width of indeterminate chunks
 * and vista pulse overlays.
 */
static int32_t GetProgressOverlaySize(bool aIsVertical, bool aIsIndeterminate) {
  if (aIsVertical) {
    return aIsIndeterminate ? kProgressVerticalIndeterminateOverlaySize
                            : kProgressVerticalOverlaySize;
  }
  return kProgressHorizontalOverlaySize;
}

/*
 * IsProgressMeterFilled - Determines if a progress meter is at 100% fill based
 * on a comparison of the current value and maximum.
 */
static bool IsProgressMeterFilled(nsIFrame* aFrame) {
  NS_ENSURE_TRUE(aFrame, false);
  nsIFrame* parentFrame = aFrame->GetParent();
  NS_ENSURE_TRUE(parentFrame, false);
  return nsNativeTheme::GetProgressValue(parentFrame) ==
         nsNativeTheme::GetProgressMaxValue(parentFrame);
}

/*
 * CalculateProgressOverlayRect - returns the padded overlay animation rect
 * used in rendering progress bars. Resulting rects are used in rendering
 * vista+ pulse overlays and indeterminate progress meters. Graphics should
 * be rendered at the origin.
 */
RECT nsNativeThemeWin::CalculateProgressOverlayRect(nsIFrame* aFrame,
                                                    RECT* aWidgetRect,
                                                    bool aIsVertical,
                                                    bool aIsIndeterminate,
                                                    bool aIsClassic) {
  NS_ASSERTION(aFrame, "bad frame pointer");
  NS_ASSERTION(aWidgetRect, "bad rect pointer");

  int32_t frameSize = aIsVertical ? aWidgetRect->bottom - aWidgetRect->top
                                  : aWidgetRect->right - aWidgetRect->left;

  // Recycle a set of progress pulse timers - these timers control the position
  // of all progress overlays and indeterminate chunks that get rendered.
  double span = aIsIndeterminate ? kProgressIndeterminateTimeSpan
                                 : kProgressDeterminateTimeSpan;
  TimeDuration period;
  if (!aIsIndeterminate) {
    if (TimeStamp::Now() >
        (mProgressDeterminateTimeStamp + TimeDuration::FromSeconds(span))) {
      mProgressDeterminateTimeStamp = TimeStamp::Now();
    }
    period = TimeStamp::Now() - mProgressDeterminateTimeStamp;
  } else {
    if (TimeStamp::Now() >
        (mProgressIndeterminateTimeStamp + TimeDuration::FromSeconds(span))) {
      mProgressIndeterminateTimeStamp = TimeStamp::Now();
    }
    period = TimeStamp::Now() - mProgressIndeterminateTimeStamp;
  }

  double percent = period / TimeDuration::FromSeconds(span);

  if (!aIsVertical && IsFrameRTL(aFrame)) percent = 1 - percent;

  RECT overlayRect = *aWidgetRect;
  int32_t overlaySize;
  if (!aIsClassic) {
    overlaySize = GetProgressOverlaySize(aIsVertical, aIsIndeterminate);
  } else {
    overlaySize = kProgressClassicOverlaySize;
  }

  // Calculate a bounds that is larger than the meters frame such that the
  // overlay starts and ends completely off the edge of the frame:
  // [overlay][frame][overlay]
  // This also yields a nice delay on rotation. Use overlaySize as the minimum
  // size for [overlay] based on the graphics dims. If [frame] is larger, use
  // the frame size instead.
  int trackWidth = frameSize > overlaySize ? frameSize : overlaySize;
  if (!aIsVertical) {
    int xPos = aWidgetRect->left - trackWidth;
    xPos += (int)ceil(((double)(trackWidth * 2) * percent));
    overlayRect.left = xPos;
    overlayRect.right = xPos + overlaySize;
  } else {
    int yPos = aWidgetRect->bottom + trackWidth;
    yPos -= (int)ceil(((double)(trackWidth * 2) * percent));
    overlayRect.bottom = yPos;
    overlayRect.top = yPos - overlaySize;
  }
  return overlayRect;
}

/*
 * DrawProgressMeter - render an appropriate progress meter based on progress
 * meter style, orientation, and os. Note, this does not render the underlying
 * progress track.
 *
 * @param aFrame       the widget frame
 * @param aAppearance  type of widget
 * @param aTheme       progress theme handle
 * @param aHdc         hdc returned by gfxWindowsNativeDrawing
 * @param aPart        the PP_X progress part
 * @param aState       the theme state
 * @param aWidgetRect  bounding rect for the widget
 * @param aClipRect    dirty rect that needs drawing.
 * @param aAppUnits    app units per device pixel
 */
void nsNativeThemeWin::DrawThemedProgressMeter(
    nsIFrame* aFrame, StyleAppearance aAppearance, HANDLE aTheme, HDC aHdc,
    int aPart, int aState, RECT* aWidgetRect, RECT* aClipRect) {
  if (!aFrame || !aTheme || !aHdc) return;

  NS_ASSERTION(aWidgetRect, "bad rect pointer");
  NS_ASSERTION(aClipRect, "bad clip rect pointer");

  RECT adjWidgetRect, adjClipRect;
  adjWidgetRect = *aWidgetRect;
  adjClipRect = *aClipRect;

  nsIFrame* parentFrame = aFrame->GetParent();
  if (!parentFrame) {
    // We have no parent to work with, just bail.
    NS_WARNING("No parent frame for progress rendering. Can't paint.");
    return;
  }

  ElementState elementState = GetContentState(parentFrame, aAppearance);
  bool vertical = IsVerticalProgress(parentFrame);
  bool indeterminate = elementState.HasState(ElementState::INDETERMINATE);
  bool animate = indeterminate;

  // Vista and up progress meter is fill style, rendered here. We render
  // the pulse overlay in the follow up section below.
  DrawThemeBackground(aTheme, aHdc, aPart, aState, &adjWidgetRect,
                      &adjClipRect);
  if (!IsProgressMeterFilled(aFrame)) {
    animate = true;
  }

  if (animate) {
    // Indeterminate rendering
    int32_t overlayPart = GetProgressOverlayStyle(vertical);
    RECT overlayRect = CalculateProgressOverlayRect(
        aFrame, &adjWidgetRect, vertical, indeterminate, false);
    DrawThemeBackground(aTheme, aHdc, overlayPart, aState, &overlayRect,
                        &adjClipRect);

    if (!QueueAnimatedContentForRefresh(aFrame->GetContent(), 60)) {
      NS_WARNING("unable to animate progress widget!");
    }
  }
}

LayoutDeviceIntMargin nsNativeThemeWin::GetCachedWidgetBorder(
    HTHEME aTheme, nsUXThemeClass aThemeClass, StyleAppearance aAppearance,
    int32_t aPart, int32_t aState) {
  int32_t cacheIndex = aThemeClass * THEME_PART_DISTINCT_VALUE_COUNT + aPart;
  int32_t cacheBitIndex = cacheIndex / 8;
  uint8_t cacheBit = 1u << (cacheIndex % 8);

  if (mBorderCacheValid[cacheBitIndex] & cacheBit) {
    return mBorderCache[cacheIndex];
  }

  // Get our info.
  RECT outerRect;  // Create a fake outer rect.
  outerRect.top = outerRect.left = 100;
  outerRect.right = outerRect.bottom = 200;
  RECT contentRect(outerRect);
  HRESULT res = GetThemeBackgroundContentRect(aTheme, nullptr, aPart, aState,
                                              &outerRect, &contentRect);

  if (FAILED(res)) {
    return LayoutDeviceIntMargin();
  }

  // Now compute the delta in each direction and place it in our
  // nsIntMargin struct.
  LayoutDeviceIntMargin result;
  result.top = contentRect.top - outerRect.top;
  result.bottom = outerRect.bottom - contentRect.bottom;
  result.left = contentRect.left - outerRect.left;
  result.right = outerRect.right - contentRect.right;

  mBorderCacheValid[cacheBitIndex] |= cacheBit;
  mBorderCache[cacheIndex] = result;

  return result;
}

nsresult nsNativeThemeWin::GetCachedMinimumWidgetSize(
    nsIFrame* aFrame, HANDLE aTheme, nsUXThemeClass aThemeClass,
    StyleAppearance aAppearance, int32_t aPart, int32_t aState,
    THEMESIZE aSizeReq, mozilla::LayoutDeviceIntSize* aResult) {
  int32_t cachePart = aPart;

  if (aAppearance == StyleAppearance::Button && aSizeReq == TS_MIN) {
    // In practice, StyleAppearance::Button is the only widget type which has an
    // aSizeReq that varies for us, and it can only be TS_MIN or TS_TRUE. Just
    // stuff that extra bit into the aPart part of the cache, since BP_Count is
    // well below THEME_PART_DISTINCT_VALUE_COUNT anyway.
    cachePart = BP_Count;
  }

  MOZ_ASSERT(aPart < THEME_PART_DISTINCT_VALUE_COUNT);
  int32_t cacheIndex =
      aThemeClass * THEME_PART_DISTINCT_VALUE_COUNT + cachePart;
  int32_t cacheBitIndex = cacheIndex / 8;
  uint8_t cacheBit = 1u << (cacheIndex % 8);

  if (mMinimumWidgetSizeCacheValid[cacheBitIndex] & cacheBit) {
    *aResult = mMinimumWidgetSizeCache[cacheIndex];
    return NS_OK;
  }

  HDC hdc = ::GetDC(NULL);
  if (!hdc) {
    return NS_ERROR_FAILURE;
  }

  SIZE sz;
  GetThemePartSize(aTheme, hdc, aPart, aState, nullptr, aSizeReq, &sz);
  aResult->width = sz.cx;
  aResult->height = sz.cy;

  ::ReleaseDC(nullptr, hdc);

  mMinimumWidgetSizeCacheValid[cacheBitIndex] |= cacheBit;
  mMinimumWidgetSizeCache[cacheIndex] = *aResult;

  return NS_OK;
}

mozilla::Maybe<nsUXThemeClass> nsNativeThemeWin::GetThemeClass(
    StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::Button:
      return Some(eUXButton);
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
      return Some(eUXEdit);
    case StyleAppearance::Toolbox:
      return Some(eUXRebar);
    case StyleAppearance::Toolbar:
    case StyleAppearance::Toolbarbutton:
    case StyleAppearance::Separator:
      return Some(eUXToolbar);
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Progresschunk:
      return Some(eUXProgress);
    case StyleAppearance::Tab:
    case StyleAppearance::Tabpanel:
    case StyleAppearance::Tabpanels:
      return Some(eUXTab);
    case StyleAppearance::Range:
    case StyleAppearance::RangeThumb:
      return Some(eUXTrackbar);
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
      return Some(eUXCombobox);
    case StyleAppearance::Treeheadercell:
    case StyleAppearance::Treeheadersortarrow:
      return Some(eUXHeader);
    case StyleAppearance::Listbox:
    case StyleAppearance::Treeview:
    case StyleAppearance::Treetwistyopen:
    case StyleAppearance::Treeitem:
      return Some(eUXListview);
    default:
      return Nothing();
  }
}

HANDLE
nsNativeThemeWin::GetTheme(StyleAppearance aAppearance) {
  mozilla::Maybe<nsUXThemeClass> themeClass = GetThemeClass(aAppearance);
  if (themeClass.isNothing()) {
    return nullptr;
  }
  return nsUXThemeData::GetTheme(themeClass.value());
}

int32_t nsNativeThemeWin::StandardGetState(nsIFrame* aFrame,
                                           StyleAppearance aAppearance,
                                           bool wantFocused) {
  ElementState elementState = GetContentState(aFrame, aAppearance);
  if (elementState.HasAllStates(ElementState::HOVER | ElementState::ACTIVE)) {
    return TS_ACTIVE;
  }
  if (elementState.HasState(ElementState::HOVER)) {
    return TS_HOVER;
  }
  if (wantFocused) {
    if (elementState.HasState(ElementState::FOCUSRING)) {
      return TS_FOCUSED;
    }
    // On Windows, focused buttons are always drawn as such by the native
    // theme, that's why we check ElementState::FOCUS instead of
    // ElementState::FOCUSRING.
    if (aAppearance == StyleAppearance::Button &&
        elementState.HasState(ElementState::FOCUS)) {
      return TS_FOCUSED;
    }
  }

  return TS_NORMAL;
}

bool nsNativeThemeWin::IsMenuActive(nsIFrame* aFrame,
                                    StyleAppearance aAppearance) {
  nsIContent* content = aFrame->GetContent();
  if (content->IsXULElement() &&
      content->NodeInfo()->Equals(nsGkAtoms::richlistitem))
    return CheckBooleanAttr(aFrame, nsGkAtoms::selected);

  return CheckBooleanAttr(aFrame, nsGkAtoms::menuactive);
}

/**
 * aPart is filled in with the UXTheme part code. On return, values > 0
 * are the actual UXTheme part code; -1 means the widget will be drawn by
 * us; 0 means that we should use part code 0, which isn't a real part code
 * but elicits some kind of default behaviour from UXTheme when drawing
 * (but isThemeBackgroundPartiallyTransparent may not work).
 */
nsresult nsNativeThemeWin::GetThemePartAndState(nsIFrame* aFrame,
                                                StyleAppearance aAppearance,
                                                int32_t& aPart,
                                                int32_t& aState) {
  switch (aAppearance) {
    case StyleAppearance::Button: {
      aPart = BP_BUTTON;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      ElementState elementState = GetContentState(aFrame, aAppearance);
      if (elementState.HasState(ElementState::DISABLED)) {
        aState = TS_DISABLED;
        return NS_OK;
      }
      if (IsOpenButton(aFrame) || IsCheckedButton(aFrame)) {
        aState = TS_ACTIVE;
        return NS_OK;
      }

      aState = StandardGetState(aFrame, aAppearance, true);

      // Check for default dialog buttons.  These buttons should always look
      // focused.
      if (aState == TS_NORMAL && IsDefaultButton(aFrame)) aState = TS_FOCUSED;
      return NS_OK;
    }
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea: {
      ElementState elementState = GetContentState(aFrame, aAppearance);

      /* Note: the NOSCROLL type has a rounded corner in each corner.  The more
       * specific HSCROLL, VSCROLL, HVSCROLL types have side and/or top/bottom
       * edges rendered as straight horizontal lines with sharp corners to
       * accommodate a scrollbar.  However, the scrollbar gets rendered on top
       * of this for us, so we don't care, and can just use NOSCROLL here.
       */
      aPart = TFP_EDITBORDER_NOSCROLL;

      if (!aFrame) {
        aState = TFS_EDITBORDER_NORMAL;
      } else if (elementState.HasState(ElementState::DISABLED)) {
        aState = TFS_EDITBORDER_DISABLED;
      } else if (IsReadOnly(aFrame)) {
        /* no special read-only state */
        aState = TFS_EDITBORDER_NORMAL;
      } else if (elementState.HasAtLeastOneOfStates(ElementState::ACTIVE |
                                                    ElementState::FOCUSRING)) {
        aState = TFS_EDITBORDER_FOCUSED;
      } else if (elementState.HasState(ElementState::HOVER)) {
        aState = TFS_EDITBORDER_HOVER;
      } else {
        aState = TFS_EDITBORDER_NORMAL;
      }

      return NS_OK;
    }
    case StyleAppearance::ProgressBar: {
      bool vertical = IsVerticalProgress(aFrame);
      aPart = vertical ? PP_BARVERT : PP_BAR;
      aState = PBBS_NORMAL;
      return NS_OK;
    }
    case StyleAppearance::Progresschunk: {
      nsIFrame* parentFrame = aFrame->GetParent();
      if (IsVerticalProgress(parentFrame)) {
        aPart = PP_FILLVERT;
      } else {
        aPart = PP_FILL;
      }

      aState = PBBVS_NORMAL;
      return NS_OK;
    }
    case StyleAppearance::Toolbarbutton: {
      aPart = BP_BUTTON;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      ElementState elementState = GetContentState(aFrame, aAppearance);
      if (elementState.HasState(ElementState::DISABLED)) {
        aState = TS_DISABLED;
        return NS_OK;
      }
      if (IsOpenButton(aFrame)) {
        aState = TS_ACTIVE;
        return NS_OK;
      }

      if (elementState.HasAllStates(ElementState::HOVER | ElementState::ACTIVE))
        aState = TS_ACTIVE;
      else if (elementState.HasState(ElementState::HOVER)) {
        if (IsCheckedButton(aFrame))
          aState = TB_HOVER_CHECKED;
        else
          aState = TS_HOVER;
      } else {
        if (IsCheckedButton(aFrame))
          aState = TB_CHECKED;
        else
          aState = TS_NORMAL;
      }

      return NS_OK;
    }
    case StyleAppearance::Separator: {
      aPart = TP_SEPARATOR;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case StyleAppearance::Range: {
      if (IsRangeHorizontal(aFrame)) {
        aPart = TKP_TRACK;
        aState = TRS_NORMAL;
      } else {
        aPart = TKP_TRACKVERT;
        aState = TRVS_NORMAL;
      }
      return NS_OK;
    }
    case StyleAppearance::RangeThumb: {
      if (IsRangeHorizontal(aFrame)) {
        aPart = TKP_THUMBBOTTOM;
      } else {
        aPart = IsFrameRTL(aFrame) ? TKP_THUMBLEFT : TKP_THUMBRIGHT;
      }
      ElementState elementState = GetContentState(aFrame, aAppearance);
      if (!aFrame) {
        aState = TS_NORMAL;
      } else if (elementState.HasState(ElementState::DISABLED)) {
        aState = TKP_DISABLED;
      } else {
        if (elementState.HasState(
                ElementState::ACTIVE))  // Hover is not also a requirement for
                                        // the thumb, since the drag is not
                                        // canceled when you move outside the
                                        // thumb.
          aState = TS_ACTIVE;
        else if (elementState.HasState(ElementState::FOCUSRING))
          aState = TKP_FOCUSED;
        else if (elementState.HasState(ElementState::HOVER))
          aState = TS_HOVER;
        else
          aState = TS_NORMAL;
      }
      return NS_OK;
    }
    case StyleAppearance::Toolbox: {
      aState = 0;
      aPart = RP_BACKGROUND;
      return NS_OK;
    }
    case StyleAppearance::Toolbar: {
      // Use -1 to indicate we don't wish to have the theme background drawn
      // for this item. We will pass any nessessary information via aState,
      // and will render the item using separate code.
      aPart = -1;
      aState = 0;
      if (aFrame) {
        nsIContent* content = aFrame->GetContent();
        nsIContent* parent = content->GetParent();
        // XXXzeniko hiding the first toolbar will result in an unwanted margin
        if (parent && parent->GetFirstChild() == content) {
          aState = 1;
        }
      }
      return NS_OK;
    }
    case StyleAppearance::Treeview:
    case StyleAppearance::Listbox: {
      aPart = TREEVIEW_BODY;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case StyleAppearance::Tabpanels: {
      aPart = TABP_PANELS;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case StyleAppearance::Tabpanel: {
      aPart = TABP_PANEL;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case StyleAppearance::Tab: {
      aPart = TABP_TAB;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      ElementState elementState = GetContentState(aFrame, aAppearance);
      if (elementState.HasState(ElementState::DISABLED)) {
        aState = TS_DISABLED;
        return NS_OK;
      }

      if (IsSelectedTab(aFrame)) {
        aPart = TABP_TAB_SELECTED;
        aState = TS_ACTIVE;  // The selected tab is always "pressed".
      } else
        aState = StandardGetState(aFrame, aAppearance, true);

      return NS_OK;
    }
    case StyleAppearance::Treeheadersortarrow: {
      // XXX Probably will never work due to a bug in the Luna theme.
      aPart = 4;
      aState = 1;
      return NS_OK;
    }
    case StyleAppearance::Treeheadercell: {
      aPart = 1;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      aState = StandardGetState(aFrame, aAppearance, true);

      return NS_OK;
    }
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Menulist: {
      nsIContent* content = aFrame->GetContent();
      bool useDropBorder = content && content->IsHTMLElement();
      ElementState elementState = GetContentState(aFrame, aAppearance);

      /* On Vista/Win7, we use CBP_DROPBORDER instead of DROPFRAME for HTML
       * content or for editable menulists; this gives us the thin outline,
       * instead of the gradient-filled background */
      if (useDropBorder)
        aPart = CBP_DROPBORDER;
      else
        aPart = CBP_DROPFRAME;

      if (elementState.HasState(ElementState::DISABLED)) {
        aState = TS_DISABLED;
      } else if (IsReadOnly(aFrame)) {
        aState = TS_NORMAL;
      } else if (IsOpenButton(aFrame)) {
        aState = TS_ACTIVE;
      } else if (useDropBorder &&
                 elementState.HasState(ElementState::FOCUSRING)) {
        aState = TS_ACTIVE;
      } else if (elementState.HasAllStates(ElementState::HOVER |
                                           ElementState::ACTIVE)) {
        aState = TS_ACTIVE;
      } else if (elementState.HasState(ElementState::HOVER)) {
        aState = TS_HOVER;
      } else {
        aState = TS_NORMAL;
      }

      return NS_OK;
    }
    default:
      aPart = 0;
      aState = 0;
      return NS_ERROR_FAILURE;
  }
}

static bool AssumeThemePartAndStateAreTransparent(int32_t aPart,
                                                  int32_t aState) {
  if (!nsUXThemeData::IsHighContrastOn() && aPart == MENU_POPUPITEM &&
      aState == MBI_NORMAL) {
    return true;
  }
  return false;
}

// When running with per-monitor DPI (on Win8.1+), and rendering on a display
// with a different DPI setting from the system's default scaling, we need to
// apply scaling to native-themed elements as the Windows theme APIs assume
// the system default resolution.
static inline double GetThemeDpiScaleFactor(nsPresContext* aPresContext) {
  if (WinUtils::IsPerMonitorDPIAware() ||
      StaticPrefs::layout_css_devPixelsPerPx() > 0.0) {
    nsCOMPtr<nsIWidget> rootWidget = aPresContext->GetRootWidget();
    if (rootWidget) {
      double systemScale = WinUtils::SystemScaleFactor();
      return rootWidget->GetDefaultScale().scale / systemScale;
    }
  }
  return 1.0;
}

static inline double GetThemeDpiScaleFactor(nsIFrame* aFrame) {
  return GetThemeDpiScaleFactor(aFrame->PresContext());
}

NS_IMETHODIMP
nsNativeThemeWin::DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                       StyleAppearance aAppearance,
                                       const nsRect& aRect,
                                       const nsRect& aDirtyRect,
                                       DrawOverflow aDrawOverflow) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::DrawWidgetBackground(aContext, aFrame, aAppearance, aRect,
                                       aDirtyRect, aDrawOverflow);
  }

  HANDLE theme = GetTheme(aAppearance);
  if (!theme)
    return ClassicDrawWidgetBackground(aContext, aFrame, aAppearance, aRect,
                                       aDirtyRect);

  // ^^ without the right sdk, assume xp theming and fall through.
  int32_t part, state;
  nsresult rv = GetThemePartAndState(aFrame, aAppearance, part, state);
  if (NS_FAILED(rv)) return rv;

  if (AssumeThemePartAndStateAreTransparent(part, state)) {
    return NS_OK;
  }

  gfxContextMatrixAutoSaveRestore save(aContext);

  double themeScale = GetThemeDpiScaleFactor(aFrame);
  if (themeScale != 1.0) {
    aContext->SetMatrix(
        aContext->CurrentMatrix().PreScale(themeScale, themeScale));
  }

  gfxFloat p2a = gfxFloat(aFrame->PresContext()->AppUnitsPerDevPixel());
  RECT widgetRect;
  RECT clipRect;
  gfxRect tr(aRect.X(), aRect.Y(), aRect.Width(), aRect.Height()),
      dr(aDirtyRect.X(), aDirtyRect.Y(), aDirtyRect.Width(),
         aDirtyRect.Height());

  tr.Scale(1.0 / (p2a * themeScale));
  dr.Scale(1.0 / (p2a * themeScale));

  gfxWindowsNativeDrawing nativeDrawing(
      aContext, dr, GetWidgetNativeDrawingFlags(aAppearance));

RENDER_AGAIN:

  HDC hdc = nativeDrawing.BeginNativeDrawing();
  if (!hdc) return NS_ERROR_FAILURE;

  nativeDrawing.TransformToNativeRect(tr, widgetRect);
  nativeDrawing.TransformToNativeRect(dr, clipRect);

#if 0
  {
    MOZ_LOG(gWindowsLog, LogLevel::Error,
           (stderr, "xform: %f %f %f %f [%f %f]\n", m._11, m._21, m._12, m._22,
            m._31, m._32));
    MOZ_LOG(gWindowsLog, LogLevel::Error,
           (stderr, "tr: [%d %d %d %d]\ndr: [%d %d %d %d]\noff: [%f %f]\n",
            tr.x, tr.y, tr.width, tr.height, dr.x, dr.y, dr.width, dr.height,
            offset.x, offset.y));
  }
#endif

  if (aAppearance == StyleAppearance::Tab) {
    // For left edge and right edge tabs, we need to adjust the widget
    // rects and clip rects so that the edges don't get drawn.
    bool isLeft = IsLeftToSelectedTab(aFrame);
    bool isRight = !isLeft && IsRightToSelectedTab(aFrame);

    if (isLeft || isRight) {
      // HACK ALERT: There appears to be no way to really obtain this value, so
      // we're forced to just use the default value for Luna (which also happens
      // to be correct for all the other skins I've tried).
      int32_t edgeSize = 2;

      // Armed with the size of the edge, we now need to either shift to the
      // left or to the right.  The clip rect won't include this extra area, so
      // we know that we're effectively shifting the edge out of view (such that
      // it won't be painted).
      if (isLeft)
        // The right edge should not be drawn.  Extend our rect by the edge
        // size.
        widgetRect.right += edgeSize;
      else
        // The left edge should not be drawn.  Move the widget rect's left coord
        // back.
        widgetRect.left -= edgeSize;
    }
  }

  // widgetRect is the bounding box for a widget, yet the scale track is only
  // a small portion of this size, so the edges of the scale need to be
  // adjusted to the real size of the track.
  if (aAppearance == StyleAppearance::Range) {
    RECT contentRect;
    GetThemeBackgroundContentRect(theme, hdc, part, state, &widgetRect,
                                  &contentRect);

    SIZE siz;
    GetThemePartSize(theme, hdc, part, state, &widgetRect, TS_TRUE, &siz);

    // When rounding is necessary, we round the position of the track
    // away from the chevron of the thumb to make it look better.
    if (IsRangeHorizontal(aFrame)) {
      contentRect.top += (contentRect.bottom - contentRect.top - siz.cy) / 2;
      contentRect.bottom = contentRect.top + siz.cy;
    } else {
      if (!IsFrameRTL(aFrame)) {
        contentRect.left += (contentRect.right - contentRect.left - siz.cx) / 2;
        contentRect.right = contentRect.left + siz.cx;
      } else {
        contentRect.right -=
            (contentRect.right - contentRect.left - siz.cx) / 2;
        contentRect.left = contentRect.right - siz.cx;
      }
    }

    DrawThemeBackground(theme, hdc, part, state, &contentRect, &clipRect);
  } else if (aAppearance == StyleAppearance::NumberInput ||
             aAppearance == StyleAppearance::Textfield ||
             aAppearance == StyleAppearance::Textarea) {
    DrawThemeBackground(theme, hdc, part, state, &widgetRect, &clipRect);

    if (state == TFS_EDITBORDER_DISABLED) {
      InflateRect(&widgetRect, -1, -1);
      ::FillRect(hdc, &widgetRect, reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1));
    }
  } else if (aAppearance == StyleAppearance::ProgressBar) {
    // DrawThemeBackground renders each corner with a solid white pixel.
    // Restore these pixels to the underlying color. Tracks are rendered
    // using alpha recovery, so this makes the corners transparent.
    COLORREF color;
    color = GetPixel(hdc, widgetRect.left, widgetRect.top);
    DrawThemeBackground(theme, hdc, part, state, &widgetRect, &clipRect);
    SetPixel(hdc, widgetRect.left, widgetRect.top, color);
    SetPixel(hdc, widgetRect.right - 1, widgetRect.top, color);
    SetPixel(hdc, widgetRect.right - 1, widgetRect.bottom - 1, color);
    SetPixel(hdc, widgetRect.left, widgetRect.bottom - 1, color);
  } else if (aAppearance == StyleAppearance::Progresschunk) {
    DrawThemedProgressMeter(aFrame, aAppearance, theme, hdc, part, state,
                            &widgetRect, &clipRect);
  }
  // If part is negative, the element wishes us to not render a themed
  // background, instead opting to be drawn specially below.
  else if (part >= 0) {
    DrawThemeBackground(theme, hdc, part, state, &widgetRect, &clipRect);
  }

  // Draw focus rectangles for range elements
  // XXX it'd be nice to draw these outside of the frame
  if (aAppearance == StyleAppearance::Range) {
    ElementState contentState = GetContentState(aFrame, aAppearance);

    if (contentState.HasState(ElementState::FOCUSRING)) {
      POINT vpOrg;
      HPEN hPen = nullptr;

      uint8_t id = SaveDC(hdc);

      ::SelectClipRgn(hdc, nullptr);
      ::GetViewportOrgEx(hdc, &vpOrg);
      ::SetBrushOrgEx(hdc, vpOrg.x + widgetRect.left, vpOrg.y + widgetRect.top,
                      nullptr);
      ::SetTextColor(hdc, 0);
      ::DrawFocusRect(hdc, &widgetRect);
      ::RestoreDC(hdc, id);
      if (hPen) {
        ::DeleteObject(hPen);
      }
    }
  } else if (aAppearance == StyleAppearance::Toolbar && state == 0) {
    // Draw toolbar separator lines above all toolbars except the first one.
    // The lines are part of the Rebar theme, which is loaded for
    // StyleAppearance::Toolbox.
    theme = GetTheme(StyleAppearance::Toolbox);
    if (!theme) return NS_ERROR_FAILURE;

    widgetRect.bottom = widgetRect.top + TB_SEPARATOR_HEIGHT;
    DrawThemeEdge(theme, hdc, RP_BAND, 0, &widgetRect, EDGE_ETCHED, BF_TOP,
                  nullptr);
  }

  nativeDrawing.EndNativeDrawing();

  if (nativeDrawing.ShouldRenderAgain()) goto RENDER_AGAIN;

  nativeDrawing.PaintToContext();

  return NS_OK;
}

bool nsNativeThemeWin::CreateWebRenderCommandsForWidget(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const layers::StackingContextHelper& aSc,
    layers::RenderRootStateManager* aManager, nsIFrame* aFrame,
    StyleAppearance aAppearance, const nsRect& aRect) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::CreateWebRenderCommandsForWidget(
        aBuilder, aResources, aSc, aManager, aFrame, aAppearance, aRect);
  }
  return false;
}

static void ScaleForFrameDPI(LayoutDeviceIntMargin* aMargin, nsIFrame* aFrame) {
  double themeScale = GetThemeDpiScaleFactor(aFrame);
  if (themeScale != 1.0) {
    aMargin->top = NSToIntRound(aMargin->top * themeScale);
    aMargin->left = NSToIntRound(aMargin->left * themeScale);
    aMargin->bottom = NSToIntRound(aMargin->bottom * themeScale);
    aMargin->right = NSToIntRound(aMargin->right * themeScale);
  }
}

static void ScaleForFrameDPI(LayoutDeviceIntSize* aSize, nsIFrame* aFrame) {
  double themeScale = GetThemeDpiScaleFactor(aFrame);
  if (themeScale != 1.0) {
    aSize->width = NSToIntRound(aSize->width * themeScale);
    aSize->height = NSToIntRound(aSize->height * themeScale);
  }
}

LayoutDeviceIntMargin nsNativeThemeWin::GetWidgetBorder(
    nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::GetWidgetBorder(aContext, aFrame, aAppearance);
  }

  LayoutDeviceIntMargin result;
  mozilla::Maybe<nsUXThemeClass> themeClass = GetThemeClass(aAppearance);
  HTHEME theme = NULL;
  if (!themeClass.isNothing()) {
    theme = nsUXThemeData::GetTheme(themeClass.value());
  }
  if (!theme) {
    result = ClassicGetWidgetBorder(aContext, aFrame, aAppearance);
    ScaleForFrameDPI(&result, aFrame);
    return result;
  }

  if (!WidgetIsContainer(aAppearance) ||
      aAppearance == StyleAppearance::Toolbox ||
      aAppearance == StyleAppearance::Tabpanel)
    return result;  // Don't worry about it.

  int32_t part, state;
  nsresult rv = GetThemePartAndState(aFrame, aAppearance, part, state);
  if (NS_FAILED(rv)) return result;

  if (aAppearance == StyleAppearance::Toolbar) {
    // make space for the separator line above all toolbars but the first
    if (state == 0) result.top = TB_SEPARATOR_HEIGHT;
    return result;
  }

  result = GetCachedWidgetBorder(theme, themeClass.value(), aAppearance, part,
                                 state);

  // Remove the edges for tabs that are before or after the selected tab,
  if (aAppearance == StyleAppearance::Tab) {
    if (IsLeftToSelectedTab(aFrame))
      // Remove the right edge, since we won't be drawing it.
      result.right = 0;
    else if (IsRightToSelectedTab(aFrame))
      // Remove the left edge, since we won't be drawing it.
      result.left = 0;
  }

  if (aFrame && (aAppearance == StyleAppearance::NumberInput ||
                 aAppearance == StyleAppearance::Textfield ||
                 aAppearance == StyleAppearance::Textarea)) {
    nsIContent* content = aFrame->GetContent();
    if (content && content->IsHTMLElement()) {
      // We need to pad textfields by 1 pixel, since the caret will draw
      // flush against the edge by default if we don't.
      result.top.value++;
      result.left.value++;
      result.bottom.value++;
      result.right.value++;
    }
  }

  ScaleForFrameDPI(&result, aFrame);
  return result;
}

bool nsNativeThemeWin::GetWidgetPadding(nsDeviceContext* aContext,
                                        nsIFrame* aFrame,
                                        StyleAppearance aAppearance,
                                        LayoutDeviceIntMargin* aResult) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::GetWidgetPadding(aContext, aFrame, aAppearance, aResult);
  }

  bool ok = true;
  HANDLE theme = GetTheme(aAppearance);
  if (!theme) {
    ok = ClassicGetWidgetPadding(aContext, aFrame, aAppearance, aResult);
    ScaleForFrameDPI(aResult, aFrame);
    return ok;
  }

  /* textfields need extra pixels on all sides, otherwise they wrap their
   * content too tightly.  The actual border is drawn 1px inside the specified
   * rectangle, so Gecko will end up making the contents look too small.
   * Instead, we add 2px padding for the contents and fix this. (Used to be 1px
   * added, see bug 430212)
   */
  if (aAppearance == StyleAppearance::NumberInput ||
      aAppearance == StyleAppearance::Textfield ||
      aAppearance == StyleAppearance::Textarea) {
    aResult->top = aResult->bottom = 2;
    aResult->left = aResult->right = 2;
    ScaleForFrameDPI(aResult, aFrame);
    return ok;
  } else if (IsHTMLContent(aFrame) &&
             (aAppearance == StyleAppearance::Menulist ||
              aAppearance == StyleAppearance::MenulistButton)) {
    /* For content menulist controls, we need an extra pixel so that we have
     * room to draw our focus rectangle stuff. Otherwise, the focus rect might
     * overlap the control's border.
     */
    aResult->top = aResult->bottom = 1;
    aResult->left = aResult->right = 1;
    ScaleForFrameDPI(aResult, aFrame);
    return ok;
  }

  int32_t right, left, top, bottom;
  right = left = top = bottom = 0;
  switch (aAppearance) {
    case StyleAppearance::Button:
      if (aFrame->GetContent()->IsXULElement()) {
        top = 2;
        bottom = 3;
      }
      left = right = 5;
      break;
    default:
      return false;
  }

  if (IsFrameRTL(aFrame)) {
    aResult->right = left;
    aResult->left = right;
  } else {
    aResult->right = right;
    aResult->left = left;
  }
  aResult->top = top;
  aResult->bottom = bottom;

  ScaleForFrameDPI(aResult, aFrame);
  return ok;
}

bool nsNativeThemeWin::GetWidgetOverflow(nsDeviceContext* aContext,
                                         nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         nsRect* aOverflowRect) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::GetWidgetOverflow(aContext, aFrame, aAppearance,
                                    aOverflowRect);
  }

  /* This is disabled for now, because it causes invalidation problems --
   * see bug 420381.  The effect of not updating the overflow area is that
   * for dropdown buttons in content areas, there is a 1px border on 3 sides
   * where, if invalidated, the dropdown control probably won't be repainted.
   * This is fairly minor, as by default there is nothing in that area, and
   * a border only shows up if the widget is being hovered.
   *
   * TODO(jwatt): Figure out what do to about
   * StyleAppearance::MozMenulistArrowButton too.
   */
#if 0
  /* We explicitly draw dropdown buttons in HTML content 1px bigger up, right,
   * and bottom so that they overlap the dropdown's border like they're
   * supposed to.
   */
  if (aAppearance == StyleAppearance::MenulistButton &&
      IsHTMLContent(aFrame) &&
      !IsWidgetStyled(aFrame->GetParent()->PresContext(),
                      aFrame->GetParent(),
                      StyleAppearance::Menulist))
  {
    int32_t p2a = aContext->AppUnitsPerDevPixel();
    /* Note: no overflow on the left */
    nsMargin m(p2a, p2a, p2a, 0);
    aOverflowRect->Inflate (m);
    return true;
  }
#endif

  return false;
}

LayoutDeviceIntSize nsNativeThemeWin::GetMinimumWidgetSize(
    nsPresContext* aPresContext, nsIFrame* aFrame,
    StyleAppearance aAppearance) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::GetMinimumWidgetSize(aPresContext, aFrame, aAppearance);
  }

  mozilla::Maybe<nsUXThemeClass> themeClass = GetThemeClass(aAppearance);
  HTHEME theme = NULL;
  if (!themeClass.isNothing()) {
    theme = nsUXThemeData::GetTheme(themeClass.value());
  }
  if (!theme) {
    auto result = ClassicGetMinimumWidgetSize(aFrame, aAppearance);
    ScaleForFrameDPI(&result, aFrame);
    return result;
  }

  switch (aAppearance) {
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Toolbox:
    case StyleAppearance::Toolbar:
    case StyleAppearance::Progresschunk:
    case StyleAppearance::Tabpanels:
    case StyleAppearance::Tabpanel:
    case StyleAppearance::Listbox:
    case StyleAppearance::Treeview:
      return {};  // Don't worry about it.
    default:
      break;
  }

  // Call GetSystemMetrics to determine size for WinXP scrollbars
  // (GetThemeSysSize API returns the optimal size for the theme, but
  //  Windows appears to always use metrics when drawing standard scrollbars)
  THEMESIZE sizeReq = TS_TRUE;  // Best-fit size
  switch (aAppearance) {
    case StyleAppearance::ProgressBar:
      // Best-fit size for progress meters is too large for most
      // themes. We want these widgets to be able to really shrink
      // down, so use the min-size request value (of 0).
      sizeReq = TS_MIN;
      break;

    case StyleAppearance::RangeThumb: {
      LayoutDeviceIntSize result(12, 20);
      if (!IsRangeHorizontal(aFrame)) {
        std::swap(result.width, result.height);
      }
      ScaleForFrameDPI(&result, aFrame);
      return result;
    }

    case StyleAppearance::Separator: {
      // that's 2px left margin, 2px right margin and 2px separator
      // (the margin is drawn as part of the separator, though)
      LayoutDeviceIntSize result(6, 0);
      ScaleForFrameDPI(&result, aFrame);
      return result;
    }

    case StyleAppearance::Button:
      // We should let HTML buttons shrink to their min size.
      // FIXME bug 403934: We should probably really separate
      // GetPreferredWidgetSize from GetMinimumWidgetSize, so callers can
      // use the one they want.
      if (aFrame->GetContent()->IsHTMLElement()) {
        sizeReq = TS_MIN;
      }
      break;

    default:
      break;
  }

  int32_t part, state;
  nsresult rv = GetThemePartAndState(aFrame, aAppearance, part, state);
  if (NS_FAILED(rv)) {
    return {};
  }

  LayoutDeviceIntSize result;
  rv = GetCachedMinimumWidgetSize(aFrame, theme, themeClass.value(),
                                  aAppearance, part, state, sizeReq, &result);
  ScaleForFrameDPI(&result, aFrame);
  return result;
}

NS_IMETHODIMP
nsNativeThemeWin::WidgetStateChanged(nsIFrame* aFrame,
                                     StyleAppearance aAppearance,
                                     nsAtom* aAttribute, bool* aShouldRepaint,
                                     const nsAttrValue* aOldValue) {
  // Some widget types just never change state.
  if (aAppearance == StyleAppearance::Toolbox ||
      aAppearance == StyleAppearance::Toolbar ||
      aAppearance == StyleAppearance::Progresschunk ||
      aAppearance == StyleAppearance::ProgressBar ||
      aAppearance == StyleAppearance::Tabpanels ||
      aAppearance == StyleAppearance::Tabpanel ||
      aAppearance == StyleAppearance::Separator) {
    *aShouldRepaint = false;
    return NS_OK;
  }

  // We need to repaint the dropdown arrow in vista HTML combobox controls when
  // the control is closed to get rid of the hover effect.
  if ((aAppearance == StyleAppearance::Menulist ||
       aAppearance == StyleAppearance::MenulistButton) &&
      nsNativeTheme::IsHTMLContent(aFrame)) {
    *aShouldRepaint = true;
    return NS_OK;
  }

  // XXXdwh Not sure what can really be done here.  Can at least guess for
  // specific widgets that they're highly unlikely to have certain states.
  // For example, a toolbar doesn't care about any states.
  if (!aAttribute) {
    // Hover/focus/active changed.  Always repaint.
    *aShouldRepaint = true;
  } else {
    // Check the attribute to see if it's relevant.
    // disabled, checked, dlgtype, default, etc.
    *aShouldRepaint = false;
    if (aAttribute == nsGkAtoms::disabled || aAttribute == nsGkAtoms::checked ||
        aAttribute == nsGkAtoms::selected ||
        aAttribute == nsGkAtoms::visuallyselected ||
        aAttribute == nsGkAtoms::readonly || aAttribute == nsGkAtoms::open ||
        aAttribute == nsGkAtoms::menuactive || aAttribute == nsGkAtoms::focused)
      *aShouldRepaint = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeWin::ThemeChanged() {
  nsUXThemeData::Invalidate();
  memset(mBorderCacheValid, 0, sizeof(mBorderCacheValid));
  memset(mMinimumWidgetSizeCacheValid, 0, sizeof(mMinimumWidgetSizeCacheValid));
  mGutterSizeCacheValid = false;
  return NS_OK;
}

bool nsNativeThemeWin::ThemeSupportsWidget(nsPresContext* aPresContext,
                                           nsIFrame* aFrame,
                                           StyleAppearance aAppearance) {
  // XXXdwh We can go even further and call the API to ask if support exists for
  // specific widgets.

  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::ThemeSupportsWidget(aPresContext, aFrame, aAppearance);
  }

  HANDLE theme = GetTheme(aAppearance);
  if (theme || ClassicThemeSupportsWidget(aFrame, aAppearance))
    // turn off theming for some HTML widgets styled by the page
    return !IsWidgetStyled(aPresContext, aFrame, aAppearance);

  return false;
}

bool nsNativeThemeWin::ThemeDrawsFocusForWidget(nsIFrame* aFrame,
                                                StyleAppearance aAppearance) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::ThemeDrawsFocusForWidget(aFrame, aAppearance);
  }
  switch (aAppearance) {
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Textarea:
    case StyleAppearance::Textfield:
    case StyleAppearance::NumberInput:
      return true;
    default:
      return false;
  }
}

bool nsNativeThemeWin::ThemeNeedsComboboxDropmarker() { return true; }

nsITheme::Transparency nsNativeThemeWin::GetWidgetTransparency(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::GetWidgetTransparency(aFrame, aAppearance);
  }

  switch (aAppearance) {
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Progresschunk:
    case StyleAppearance::Range:
      return eTransparent;
    default:
      break;
  }

  HANDLE theme = GetTheme(aAppearance);
  // For the classic theme we don't really have a way of knowing
  if (!theme) {
    return eUnknownTransparency;
  }

  int32_t part, state;
  nsresult rv = GetThemePartAndState(aFrame, aAppearance, part, state);
  // Fail conservatively
  NS_ENSURE_SUCCESS(rv, eUnknownTransparency);

  if (part <= 0) {
    // Not a real part code, so IsThemeBackgroundPartiallyTransparent may
    // not work, so don't call it.
    return eUnknownTransparency;
  }

  if (IsThemeBackgroundPartiallyTransparent(theme, part, state))
    return eTransparent;
  return eOpaque;
}

/* Windows 9x/NT/2000/Classic XP Theme Support */

bool nsNativeThemeWin::ClassicThemeSupportsWidget(nsIFrame* aFrame,
                                                  StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::Button:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Range:
    case StyleAppearance::RangeThumb:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Listbox:
    case StyleAppearance::Treeview:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Progresschunk:
    case StyleAppearance::Tab:
    case StyleAppearance::Tabpanel:
    case StyleAppearance::Tabpanels:
      return true;
    default:
      return false;
  }
}

LayoutDeviceIntMargin nsNativeThemeWin::ClassicGetWidgetBorder(
    nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance) {
  LayoutDeviceIntMargin result;
  switch (aAppearance) {
    case StyleAppearance::Button:
      result.top = result.left = result.bottom = result.right = 2;
      break;
    case StyleAppearance::Listbox:
    case StyleAppearance::Treeview:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Tab:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
      result.top = result.left = result.bottom = result.right = 2;
      break;
    case StyleAppearance::ProgressBar:
      result.top = result.left = result.bottom = result.right = 1;
      break;
    default:
      result.top = result.bottom = result.left = result.right = 0;
      break;
  }
  return result;
}

bool nsNativeThemeWin::ClassicGetWidgetPadding(nsDeviceContext* aContext,
                                               nsIFrame* aFrame,
                                               StyleAppearance aAppearance,
                                               LayoutDeviceIntMargin* aResult) {
  switch (aAppearance) {
    case StyleAppearance::ProgressBar:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right =
          1;
      return true;
    default:
      return false;
  }
}

LayoutDeviceIntSize nsNativeThemeWin::ClassicGetMinimumWidgetSize(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  LayoutDeviceIntSize result;
  switch (aAppearance) {
    case StyleAppearance::RangeThumb: {
      if (IsRangeHorizontal(aFrame)) {
        result.width = 12;
        result.height = 20;
      } else {
        result.width = 20;
        result.height = 12;
      }
      break;
    }
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Button:
    case StyleAppearance::Listbox:
    case StyleAppearance::Treeview:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Progresschunk:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Tab:
    case StyleAppearance::Tabpanel:
    case StyleAppearance::Tabpanels:
      // no minimum widget size
      break;

    default:
      break;
  }
  return result;
}

nsresult nsNativeThemeWin::ClassicGetThemePartAndState(
    nsIFrame* aFrame, StyleAppearance aAppearance, int32_t& aPart,
    int32_t& aState, bool& aFocused) {
  aFocused = false;
  switch (aAppearance) {
    case StyleAppearance::Button: {
      aPart = DFC_BUTTON;
      aState = DFCS_BUTTONPUSH;
      aFocused = false;

      ElementState contentState = GetContentState(aFrame, aAppearance);
      if (contentState.HasState(ElementState::DISABLED)) {
        aState |= DFCS_INACTIVE;
      } else if (IsOpenButton(aFrame)) {
        aState |= DFCS_PUSHED;
      } else if (IsCheckedButton(aFrame)) {
        aState |= DFCS_CHECKED;
      } else {
        if (contentState.HasAllStates(ElementState::ACTIVE |
                                      ElementState::HOVER)) {
          aState |= DFCS_PUSHED;
          // The down state is flat if the button is focusable
          if (aFrame->StyleUI()->UserFocus() == StyleUserFocus::Normal) {
            if (!aFrame->GetContent()->IsHTMLElement()) aState |= DFCS_FLAT;

            aFocused = true;
          }
        }
        // On Windows, focused buttons are always drawn as such by the native
        // theme, that's why we check ElementState::FOCUS instead of
        // ElementState::FOCUSRING.
        if (contentState.HasState(ElementState::FOCUS) ||
            (aState == DFCS_BUTTONPUSH && IsDefaultButton(aFrame))) {
          aFocused = true;
        }
      }

      return NS_OK;
    }
    case StyleAppearance::Listbox:
    case StyleAppearance::Treeview:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Range:
    case StyleAppearance::RangeThumb:
    case StyleAppearance::Progresschunk:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Tab:
    case StyleAppearance::Tabpanel:
    case StyleAppearance::Tabpanels:
      // these don't use DrawFrameControl
      return NS_OK;
    default:
      return NS_ERROR_FAILURE;
  }
}

// Draw classic Windows tab
// (no system API for this, but DrawEdge can draw all the parts of a tab)
static void DrawTab(HDC hdc, const RECT& R, int32_t aPosition, bool aSelected,
                    bool aDrawLeft, bool aDrawRight) {
  int32_t leftFlag, topFlag, rightFlag, lightFlag, shadeFlag;
  RECT topRect, sideRect, bottomRect, lightRect, shadeRect;
  int32_t selectedOffset, lOffset, rOffset;

  selectedOffset = aSelected ? 1 : 0;
  lOffset = aDrawLeft ? 2 : 0;
  rOffset = aDrawRight ? 2 : 0;

  // Get info for tab orientation/position (Left, Top, Right, Bottom)
  switch (aPosition) {
    case BF_LEFT:
      leftFlag = BF_TOP;
      topFlag = BF_LEFT;
      rightFlag = BF_BOTTOM;
      lightFlag = BF_DIAGONAL_ENDTOPRIGHT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMRIGHT;

      ::SetRect(&topRect, R.left, R.top + lOffset, R.right, R.bottom - rOffset);
      ::SetRect(&sideRect, R.left + 2, R.top, R.right - 2 + selectedOffset,
                R.bottom);
      ::SetRect(&bottomRect, R.right - 2, R.top, R.right, R.bottom);
      ::SetRect(&lightRect, R.left, R.top, R.left + 3, R.top + 3);
      ::SetRect(&shadeRect, R.left + 1, R.bottom - 2, R.left + 2, R.bottom - 1);
      break;
    case BF_TOP:
      leftFlag = BF_LEFT;
      topFlag = BF_TOP;
      rightFlag = BF_RIGHT;
      lightFlag = BF_DIAGONAL_ENDTOPRIGHT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMRIGHT;

      ::SetRect(&topRect, R.left + lOffset, R.top, R.right - rOffset, R.bottom);
      ::SetRect(&sideRect, R.left, R.top + 2, R.right,
                R.bottom - 1 + selectedOffset);
      ::SetRect(&bottomRect, R.left, R.bottom - 1, R.right, R.bottom);
      ::SetRect(&lightRect, R.left, R.top, R.left + 3, R.top + 3);
      ::SetRect(&shadeRect, R.right - 2, R.top + 1, R.right - 1, R.top + 2);
      break;
    case BF_RIGHT:
      leftFlag = BF_TOP;
      topFlag = BF_RIGHT;
      rightFlag = BF_BOTTOM;
      lightFlag = BF_DIAGONAL_ENDTOPLEFT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMLEFT;

      ::SetRect(&topRect, R.left, R.top + lOffset, R.right, R.bottom - rOffset);
      ::SetRect(&sideRect, R.left + 2 - selectedOffset, R.top, R.right - 2,
                R.bottom);
      ::SetRect(&bottomRect, R.left, R.top, R.left + 2, R.bottom);
      ::SetRect(&lightRect, R.right - 3, R.top, R.right - 1, R.top + 2);
      ::SetRect(&shadeRect, R.right - 2, R.bottom - 3, R.right, R.bottom - 1);
      break;
    case BF_BOTTOM:
      leftFlag = BF_LEFT;
      topFlag = BF_BOTTOM;
      rightFlag = BF_RIGHT;
      lightFlag = BF_DIAGONAL_ENDTOPLEFT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMLEFT;

      ::SetRect(&topRect, R.left + lOffset, R.top, R.right - rOffset, R.bottom);
      ::SetRect(&sideRect, R.left, R.top + 2 - selectedOffset, R.right,
                R.bottom - 2);
      ::SetRect(&bottomRect, R.left, R.top, R.right, R.top + 2);
      ::SetRect(&lightRect, R.left, R.bottom - 3, R.left + 2, R.bottom - 1);
      ::SetRect(&shadeRect, R.right - 2, R.bottom - 3, R.right, R.bottom - 1);
      break;
    default:
      MOZ_CRASH();
  }

  // Background
  ::FillRect(hdc, &R, (HBRUSH)(COLOR_3DFACE + 1));

  // Tab "Top"
  ::DrawEdge(hdc, &topRect, EDGE_RAISED, BF_SOFT | topFlag);

  // Tab "Bottom"
  if (!aSelected) ::DrawEdge(hdc, &bottomRect, EDGE_RAISED, BF_SOFT | topFlag);

  // Tab "Sides"
  if (!aDrawLeft) leftFlag = 0;
  if (!aDrawRight) rightFlag = 0;
  ::DrawEdge(hdc, &sideRect, EDGE_RAISED, BF_SOFT | leftFlag | rightFlag);

  // Tab Diagonal Corners
  if (aDrawLeft) ::DrawEdge(hdc, &lightRect, EDGE_RAISED, BF_SOFT | lightFlag);

  if (aDrawRight) ::DrawEdge(hdc, &shadeRect, EDGE_RAISED, BF_SOFT | shadeFlag);
}

void nsNativeThemeWin::DrawCheckedRect(HDC hdc, const RECT& rc, int32_t fore,
                                       int32_t back, HBRUSH defaultBack) {
  static WORD patBits[8] = {0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55};

  HBITMAP patBmp = ::CreateBitmap(8, 8, 1, 1, patBits);
  if (patBmp) {
    HBRUSH brush = (HBRUSH)::CreatePatternBrush(patBmp);
    if (brush) {
      COLORREF oldForeColor = ::SetTextColor(hdc, ::GetSysColor(fore));
      COLORREF oldBackColor = ::SetBkColor(hdc, ::GetSysColor(back));
      POINT vpOrg;

      ::UnrealizeObject(brush);
      ::GetViewportOrgEx(hdc, &vpOrg);
      ::SetBrushOrgEx(hdc, vpOrg.x + rc.left, vpOrg.y + rc.top, nullptr);
      HBRUSH oldBrush = (HBRUSH)::SelectObject(hdc, brush);
      ::FillRect(hdc, &rc, brush);
      ::SetTextColor(hdc, oldForeColor);
      ::SetBkColor(hdc, oldBackColor);
      ::SelectObject(hdc, oldBrush);
      ::DeleteObject(brush);
    } else
      ::FillRect(hdc, &rc, defaultBack);

    ::DeleteObject(patBmp);
  }
}

nsresult nsNativeThemeWin::ClassicDrawWidgetBackground(
    gfxContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance,
    const nsRect& aRect, const nsRect& aDirtyRect) {
  int32_t part, state;
  bool focused;
  nsresult rv;
  rv = ClassicGetThemePartAndState(aFrame, aAppearance, part, state, focused);
  if (NS_FAILED(rv)) return rv;

  if (AssumeThemePartAndStateAreTransparent(part, state)) {
    return NS_OK;
  }

  gfxFloat p2a = gfxFloat(aFrame->PresContext()->AppUnitsPerDevPixel());
  RECT widgetRect;
  gfxRect tr(aRect.X(), aRect.Y(), aRect.Width(), aRect.Height()),
      dr(aDirtyRect.X(), aDirtyRect.Y(), aDirtyRect.Width(),
         aDirtyRect.Height());

  tr.Scale(1.0 / p2a);
  dr.Scale(1.0 / p2a);

  gfxWindowsNativeDrawing nativeDrawing(
      aContext, dr, GetWidgetNativeDrawingFlags(aAppearance));

RENDER_AGAIN:

  HDC hdc = nativeDrawing.BeginNativeDrawing();
  if (!hdc) return NS_ERROR_FAILURE;

  nativeDrawing.TransformToNativeRect(tr, widgetRect);

  rv = NS_OK;
  switch (aAppearance) {
    // Draw button
    case StyleAppearance::Button: {
      if (focused) {
        // draw dark button focus border first
        if (HBRUSH brush = ::GetSysColorBrush(COLOR_3DDKSHADOW)) {
          ::FrameRect(hdc, &widgetRect, brush);
        }
        InflateRect(&widgetRect, -1, -1);
      }
      // setup DC to make DrawFrameControl draw correctly
      int32_t oldTA = ::SetTextAlign(hdc, TA_TOP | TA_LEFT | TA_NOUPDATECP);
      ::DrawFrameControl(hdc, &widgetRect, part, state);
      ::SetTextAlign(hdc, oldTA);
      break;
    }
    // Draw controls with 2px 3D inset border
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton: {
      // Draw inset edge
      ::DrawEdge(hdc, &widgetRect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);

      ElementState elementState = GetContentState(aFrame, aAppearance);

      // Fill in background

      if (elementState.HasState(ElementState::DISABLED) ||
          (aFrame->GetContent()->IsXULElement() && IsReadOnly(aFrame)))
        ::FillRect(hdc, &widgetRect, (HBRUSH)(COLOR_BTNFACE + 1));
      else
        ::FillRect(hdc, &widgetRect, (HBRUSH)(COLOR_WINDOW + 1));

      break;
    }
    case StyleAppearance::Treeview: {
      // Draw inset edge
      ::DrawEdge(hdc, &widgetRect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);

      // Fill in window color background
      ::FillRect(hdc, &widgetRect, (HBRUSH)(COLOR_WINDOW + 1));

      break;
    }
    // Draw 3D face background controls
    case StyleAppearance::ProgressBar:
      // Draw 3D border
      ::DrawEdge(hdc, &widgetRect, BDR_SUNKENOUTER, BF_RECT | BF_MIDDLE);
      InflateRect(&widgetRect, -1, -1);
      [[fallthrough]];
    case StyleAppearance::Tabpanel: {
      ::FillRect(hdc, &widgetRect, (HBRUSH)(COLOR_BTNFACE + 1));
      break;
    }
    case StyleAppearance::RangeThumb: {
      ElementState elementState = GetContentState(aFrame, aAppearance);

      ::DrawEdge(hdc, &widgetRect, EDGE_RAISED,
                 BF_RECT | BF_SOFT | BF_MIDDLE | BF_ADJUST);
      if (elementState.HasState(ElementState::DISABLED)) {
        DrawCheckedRect(hdc, widgetRect, COLOR_3DFACE, COLOR_3DHILIGHT,
                        (HBRUSH)COLOR_3DHILIGHT);
      }

      break;
    }
    // Draw scale track background
    case StyleAppearance::Range: {
      const int32_t trackWidth = 4;
      // When rounding is necessary, we round the position of the track
      // away from the chevron of the thumb to make it look better.
      if (IsRangeHorizontal(aFrame)) {
        widgetRect.top += (widgetRect.bottom - widgetRect.top - trackWidth) / 2;
        widgetRect.bottom = widgetRect.top + trackWidth;
      } else {
        if (!IsFrameRTL(aFrame)) {
          widgetRect.left +=
              (widgetRect.right - widgetRect.left - trackWidth) / 2;
          widgetRect.right = widgetRect.left + trackWidth;
        } else {
          widgetRect.right -=
              (widgetRect.right - widgetRect.left - trackWidth) / 2;
          widgetRect.left = widgetRect.right - trackWidth;
        }
      }

      ::DrawEdge(hdc, &widgetRect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
      ::FillRect(hdc, &widgetRect, (HBRUSH)GetStockObject(GRAY_BRUSH));

      break;
    }
    case StyleAppearance::Progresschunk: {
      nsIFrame* stateFrame = aFrame->GetParent();
      ElementState elementState = GetContentState(stateFrame, aAppearance);

      const bool indeterminate =
          elementState.HasState(ElementState::INDETERMINATE);
      bool vertical = IsVerticalProgress(stateFrame);

      nsIContent* content = aFrame->GetContent();
      if (!indeterminate || !content) {
        ::FillRect(hdc, &widgetRect, (HBRUSH)(COLOR_HIGHLIGHT + 1));
        break;
      }

      RECT overlayRect = CalculateProgressOverlayRect(
          aFrame, &widgetRect, vertical, indeterminate, true);

      ::FillRect(hdc, &overlayRect, (HBRUSH)(COLOR_HIGHLIGHT + 1));

      if (!QueueAnimatedContentForRefresh(aFrame->GetContent(), 30)) {
        NS_WARNING("unable to animate progress widget!");
      }
      break;
    }

    // Draw Tab
    case StyleAppearance::Tab: {
      DrawTab(hdc, widgetRect, IsBottomTab(aFrame) ? BF_BOTTOM : BF_TOP,
              IsSelectedTab(aFrame), !IsRightToSelectedTab(aFrame),
              !IsLeftToSelectedTab(aFrame));

      break;
    }
    case StyleAppearance::Tabpanels:
      ::DrawEdge(hdc, &widgetRect, EDGE_RAISED,
                 BF_SOFT | BF_MIDDLE | BF_LEFT | BF_RIGHT | BF_BOTTOM);

      break;

    default:
      rv = NS_ERROR_FAILURE;
      break;
  }

  nativeDrawing.EndNativeDrawing();

  if (NS_FAILED(rv)) return rv;

  if (nativeDrawing.ShouldRenderAgain()) goto RENDER_AGAIN;

  nativeDrawing.PaintToContext();

  return rv;
}

uint32_t nsNativeThemeWin::GetWidgetNativeDrawingFlags(
    StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::Button:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
      return gfxWindowsNativeDrawing::CANNOT_DRAW_TO_COLOR_ALPHA |
             gfxWindowsNativeDrawing::CAN_AXIS_ALIGNED_SCALE |
             gfxWindowsNativeDrawing::CANNOT_COMPLEX_TRANSFORM;

    // need to check these others
    default:
      return gfxWindowsNativeDrawing::CANNOT_DRAW_TO_COLOR_ALPHA |
             gfxWindowsNativeDrawing::CANNOT_AXIS_ALIGNED_SCALE |
             gfxWindowsNativeDrawing::CANNOT_COMPLEX_TRANSFORM;
  }
}

}  // namespace mozilla::widget

///////////////////////////////////////////
// Creation Routine
///////////////////////////////////////////

already_AddRefed<Theme> do_CreateNativeThemeDoNotUseDirectly() {
  return do_AddRef(new nsNativeThemeWin());
}
