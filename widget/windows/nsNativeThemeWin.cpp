/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeWin.h"

#include "mozilla/EventStates.h"
#include "mozilla/Logging.h"
#include "mozilla/WindowsVersion.h"
#include "nsColor.h"
#include "nsDeviceContext.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsTransform2D.h"
#include "nsThemeConstants.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsNameSpaceManager.h"
#include "nsLookAndFeel.h"
#include "nsMenuFrame.h"
#include "nsGkAtoms.h"
#include <malloc.h>
#include "nsWindow.h"
#include "nsIComboboxControlFrame.h"
#include "prinrval.h"
#include "WinUtils.h"

#include "gfxPlatform.h"
#include "gfxContext.h"
#include "gfxWindowsPlatform.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsNativeDrawing.h"

#include "nsUXThemeData.h"
#include "nsUXThemeConstants.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::widget;

extern mozilla::LazyLogModule gWindowsLog;

NS_IMPL_ISUPPORTS_INHERITED(nsNativeThemeWin, nsNativeTheme, nsITheme)

nsNativeThemeWin::nsNativeThemeWin() :
  mProgressDeterminateTimeStamp(TimeStamp::Now()),
  mProgressIndeterminateTimeStamp(TimeStamp::Now()),
  mBorderCacheValid(),
  mMinimumWidgetSizeCacheValid(),
  mGutterSizeCacheValid(false)
{
  // If there is a relevant change in forms.css for windows platform,
  // static widget style variables (e.g. sButtonBorderSize) should be 
  // reinitialized here.
}

nsNativeThemeWin::~nsNativeThemeWin()
{
  nsUXThemeData::Invalidate();
}

static int32_t
GetTopLevelWindowActiveState(nsIFrame *aFrame)
{
  // Used by window frame and button box rendering. We can end up in here in
  // the content process when rendering one of these moz styles freely in a
  // page. Bail in this case, there is no applicable window focus state.
  if (!XRE_IsParentProcess()) {
    return mozilla::widget::themeconst::FS_INACTIVE;
  }
  // All headless windows are considered active so they are painted.
  if (gfxPlatform::IsHeadless()) {
    return mozilla::widget::themeconst::FS_ACTIVE;
  }
  // Get the widget. nsIFrame's GetNearestWidget walks up the view chain
  // until it finds a real window.
  nsIWidget* widget = aFrame->GetNearestWidget();
  nsWindowBase * window = static_cast<nsWindowBase*>(widget);
  if (!window)
    return mozilla::widget::themeconst::FS_INACTIVE;
  if (widget && !window->IsTopLevelWidget() &&
      !(window = window->GetParentWindowBase(false)))
    return mozilla::widget::themeconst::FS_INACTIVE;

  if (window->GetWindowHandle() == ::GetActiveWindow())
    return mozilla::widget::themeconst::FS_ACTIVE;
  return mozilla::widget::themeconst::FS_INACTIVE;
}

static int32_t
GetWindowFrameButtonState(nsIFrame* aFrame, EventStates eventState)
{
  if (GetTopLevelWindowActiveState(aFrame) ==
      mozilla::widget::themeconst::FS_INACTIVE) {
    if (eventState.HasState(NS_EVENT_STATE_HOVER))
      return mozilla::widget::themeconst::BS_HOT;
    return mozilla::widget::themeconst::BS_INACTIVE;
  }

  if (eventState.HasState(NS_EVENT_STATE_HOVER)) {
    if (eventState.HasState(NS_EVENT_STATE_ACTIVE))
      return mozilla::widget::themeconst::BS_PUSHED;
    return mozilla::widget::themeconst::BS_HOT;
  }
  return mozilla::widget::themeconst::BS_NORMAL;
}

static int32_t
GetClassicWindowFrameButtonState(EventStates eventState)
{
  if (eventState.HasState(NS_EVENT_STATE_ACTIVE) &&
      eventState.HasState(NS_EVENT_STATE_HOVER))
    return DFCS_BUTTONPUSH|DFCS_PUSHED;
  return DFCS_BUTTONPUSH;
}

static bool
IsTopLevelMenu(nsIFrame *aFrame)
{
  bool isTopLevel(false);
  nsMenuFrame *menuFrame = do_QueryFrame(aFrame);
  if (menuFrame) {
    isTopLevel = menuFrame->IsOnMenuBar();
  }
  return isTopLevel;
}

static MARGINS
GetCheckboxMargins(HANDLE theme, HDC hdc)
{
    MARGINS checkboxContent = {0};
    GetThemeMargins(theme, hdc, MENU_POPUPCHECK, MCB_NORMAL,
                    TMT_CONTENTMARGINS, nullptr, &checkboxContent);
    return checkboxContent;
}

static SIZE
GetCheckboxBGSize(HANDLE theme, HDC hdc)
{
    SIZE checkboxSize;
    GetThemePartSize(theme, hdc, MENU_POPUPCHECK, MC_CHECKMARKNORMAL,
                     nullptr, TS_TRUE, &checkboxSize);

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

static SIZE
GetCheckboxBGBounds(HANDLE theme, HDC hdc)
{
    MARGINS checkboxBGSizing = {0};
    MARGINS checkboxBGContent = {0};
    GetThemeMargins(theme, hdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL,
                    TMT_SIZINGMARGINS, nullptr, &checkboxBGSizing);
    GetThemeMargins(theme, hdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL,
                    TMT_CONTENTMARGINS, nullptr, &checkboxBGContent);

#define posdx(d) ((d) > 0 ? d : 0)

    int dx = posdx(checkboxBGContent.cxRightWidth -
                   checkboxBGSizing.cxRightWidth) +
             posdx(checkboxBGContent.cxLeftWidth -
                   checkboxBGSizing.cxLeftWidth);
    int dy = posdx(checkboxBGContent.cyTopHeight -
                   checkboxBGSizing.cyTopHeight) +
             posdx(checkboxBGContent.cyBottomHeight -
                   checkboxBGSizing.cyBottomHeight);

#undef posdx

    SIZE ret(GetCheckboxBGSize(theme, hdc));
    ret.cx += dx;
    ret.cy += dy;
    return ret;
}

static SIZE
GetGutterSize(HANDLE theme, HDC hdc)
{
    SIZE gutterSize;
    GetThemePartSize(theme, hdc, MENU_POPUPGUTTER, 0, nullptr, TS_TRUE, &gutterSize);

    SIZE checkboxBGSize(GetCheckboxBGBounds(theme, hdc));

    SIZE itemSize;
    GetThemePartSize(theme, hdc, MENU_POPUPITEM, MPI_NORMAL, nullptr, TS_TRUE, &itemSize);

    // Figure out how big the menuitem's icon will be (if present) at current DPI
    // Needs the system scale for consistency with Windows Theme API.
    double scaleFactor = WinUtils::SystemScaleFactor();
    int iconDevicePixels = NSToIntRound(16 * scaleFactor);
    SIZE iconSize = {
      iconDevicePixels, iconDevicePixels
    };
    // Not really sure what margins should be used here, but this seems to work in practice...
    MARGINS margins = {0};
    GetThemeMargins(theme, hdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL,
                    TMT_CONTENTMARGINS, nullptr, &margins);
    iconSize.cx += margins.cxLeftWidth + margins.cxRightWidth;
    iconSize.cy += margins.cyTopHeight + margins.cyBottomHeight;

    int width = std::max(itemSize.cx, std::max(iconSize.cx, checkboxBGSize.cx) + gutterSize.cx);
    int height = std::max(itemSize.cy, std::max(iconSize.cy, checkboxBGSize.cy));

    SIZE ret;
    ret.cx = width;
    ret.cy = height;
    return ret;
}

SIZE
nsNativeThemeWin::GetCachedGutterSize(HANDLE theme)
{
  if (mGutterSizeCacheValid) {
    return mGutterSizeCache;
  }

  mGutterSizeCache = GetGutterSize(theme, nullptr);
  mGutterSizeCacheValid = true;

  return mGutterSizeCache;
}

/* DrawThemeBGRTLAware - render a theme part based on rtl state.
 * Some widgets are not direction-neutral and need to be drawn reversed for
 * RTL.  Windows provides a way to do this with SetLayout, but this reverses
 * the entire drawing area of a given device context, which means that its
 * use will also affect the positioning of the widget.  There are two ways
 * to work around this:
 *
 * Option 1: Alter the position of the rect that we send so that we cancel
 *           out the positioning effects of SetLayout
 * Option 2: Create a memory DC with the widgetRect's dimensions, draw onto
 *           that, and then transfer the results back to our DC
 *
 * This function tries to implement option 1, under the assumption that the
 * correct way to reverse the effects of SetLayout is to translate the rect
 * such that the offset from the DC bitmap's left edge to the old rect's
 * left edge is equal to the offset from the DC bitmap's right edge to the
 * new rect's right edge.  In other words,
 * (oldRect.left + vpOrg.x) == ((dcBMP.width - vpOrg.x) - newRect.right)
 */
static HRESULT
DrawThemeBGRTLAware(HANDLE aTheme, HDC aHdc, int aPart, int aState,
                    const RECT *aWidgetRect, const RECT *aClipRect,
                    bool aIsRtl)
{
  NS_ASSERTION(aTheme, "Bad theme handle.");
  NS_ASSERTION(aHdc, "Bad hdc.");
  NS_ASSERTION(aWidgetRect, "Bad rect.");
  NS_ASSERTION(aClipRect, "Bad clip rect.");

  if (!aIsRtl) {
    return DrawThemeBackground(aTheme, aHdc, aPart, aState,
                               aWidgetRect, aClipRect);
  }

  HGDIOBJ hObj = GetCurrentObject(aHdc, OBJ_BITMAP);
  BITMAP bitmap;
  POINT vpOrg;

  if (hObj && GetObject(hObj, sizeof(bitmap), &bitmap) &&
      GetViewportOrgEx(aHdc, &vpOrg)) {
    RECT newWRect(*aWidgetRect);
    newWRect.left = bitmap.bmWidth - (aWidgetRect->right + 2*vpOrg.x);
    newWRect.right = bitmap.bmWidth - (aWidgetRect->left + 2*vpOrg.x);

    RECT newCRect;
    RECT *newCRectPtr = nullptr;

    if (aClipRect) {
      newCRect.top = aClipRect->top;
      newCRect.bottom = aClipRect->bottom;
      newCRect.left = bitmap.bmWidth - (aClipRect->right + 2*vpOrg.x);
      newCRect.right = bitmap.bmWidth - (aClipRect->left + 2*vpOrg.x);
      newCRectPtr = &newCRect;
    }

    SetLayout(aHdc, LAYOUT_RTL);
    HRESULT hr = DrawThemeBackground(aTheme, aHdc, aPart, aState, &newWRect,
                                     newCRectPtr);
    SetLayout(aHdc, 0);
    if (SUCCEEDED(hr)) {
      return hr;
    }
  }
  return DrawThemeBackground(aTheme, aHdc, aPart, aState,
                             aWidgetRect, aClipRect);
}

/*
 *  Caption button padding data - 'hot' button padding.
 *  These areas are considered hot, in that they activate
 *  a button when hovered or clicked. The button graphic
 *  is drawn inside the padding border. Unrecognized themes
 *  are treated as their recognized counterparts for now.
 *                       left      top    right   bottom
 *  classic min             1        2        0        1
 *  classic max             0        2        1        1
 *  classic close           1        2        2        1
 *
 *  aero basic min          1        2        0        2
 *  aero basic max          0        2        1        2
 *  aero basic close        1        2        1        2
 *
 *  'cold' button padding - generic button padding, should
 *  be handled in css.
 *                       left      top    right   bottom
 *  classic min             0        0        0        0
 *  classic max             0        0        0        0
 *  classic close           0        0        0        0
 *
 *  aero basic min          0        0        1        0
 *  aero basic max          1        0        0        0
 *  aero basic close        0        0        0        0
 */

enum CaptionDesktopTheme {
  CAPTION_CLASSIC = 0,
  CAPTION_BASIC,
};

enum CaptionButton {
  CAPTIONBUTTON_MINIMIZE = 0,
  CAPTIONBUTTON_RESTORE,
  CAPTIONBUTTON_CLOSE,
};

struct CaptionButtonPadding {
  RECT hotPadding[3];
};

// RECT: left, top, right, bottom
static CaptionButtonPadding buttonData[3] = {
  { 
    { { 1, 2, 0, 1 }, { 0, 2, 1, 1 }, { 1, 2, 2, 1 } }
  },
  { 
    { { 1, 2, 0, 2 }, { 0, 2, 1, 2 }, { 1, 2, 2, 2 } }
  },
  { 
    { { 0, 2, 0, 2 }, { 0, 2, 1, 2 }, { 1, 2, 2, 2 } }
  }
};

// Adds "hot" caption button padding to minimum widget size.
static void
AddPaddingRect(LayoutDeviceIntSize* aSize, CaptionButton button) {
  if (!aSize)
    return;
  RECT offset;
  if (!IsAppThemed())
    offset = buttonData[CAPTION_CLASSIC].hotPadding[button];
  else
    offset = buttonData[CAPTION_BASIC].hotPadding[button];
  aSize->width += offset.left + offset.right;
  aSize->height += offset.top + offset.bottom;
}

// If we've added padding to the minimum widget size, offset
// the area we draw into to compensate.
static void
OffsetBackgroundRect(RECT& rect, CaptionButton button) {
  RECT offset;
  if (!IsAppThemed())
    offset = buttonData[CAPTION_CLASSIC].hotPadding[button];
  else
    offset = buttonData[CAPTION_BASIC].hotPadding[button];
  rect.left += offset.left;
  rect.top += offset.top;
  rect.right -= offset.right;
  rect.bottom -= offset.bottom;
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
// The width of the overlay used to animate the horizontal progress bar (Vista and later).
static const int32_t kProgressHorizontalOverlaySize = 120;
// The height of the overlay used to animate the vertical progress bar (Vista and later).
static const int32_t kProgressVerticalOverlaySize = 45;
// The height of the overlay used for the vertical indeterminate progress bar (Vista and later).
static const int32_t kProgressVerticalIndeterminateOverlaySize = 60;
// The width of the overlay used to animate the indeterminate progress bar (Windows Classic).
static const int32_t kProgressClassicOverlaySize = 40;

/*
 * GetProgressOverlayStyle - returns the proper overlay part for themed
 * progress bars based on os and orientation.
 */
static int32_t
GetProgressOverlayStyle(bool aIsVertical)
{
  return aIsVertical ? PP_MOVEOVERLAYVERT : PP_MOVEOVERLAY;
}

/*
 * GetProgressOverlaySize - returns the minimum width or height for themed
 * progress bar overlays. This includes the width of indeterminate chunks
 * and vista pulse overlays.
 */
static int32_t
GetProgressOverlaySize(bool aIsVertical, bool aIsIndeterminate)
{
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
static bool
IsProgressMeterFilled(nsIFrame* aFrame)
{
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
RECT
nsNativeThemeWin::CalculateProgressOverlayRect(nsIFrame* aFrame,
                                               RECT* aWidgetRect,
                                               bool aIsVertical,
                                               bool aIsIndeterminate,
                                               bool aIsClassic)
{
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
    if (TimeStamp::Now() > (mProgressDeterminateTimeStamp +
                            TimeDuration::FromSeconds(span))) {
      mProgressDeterminateTimeStamp = TimeStamp::Now();
    }
    period = TimeStamp::Now() - mProgressDeterminateTimeStamp;
  } else {
    if (TimeStamp::Now() > (mProgressIndeterminateTimeStamp +
                            TimeDuration::FromSeconds(span))) {
      mProgressIndeterminateTimeStamp = TimeStamp::Now();
    }
    period = TimeStamp::Now() - mProgressIndeterminateTimeStamp;
  }

  double percent = period / TimeDuration::FromSeconds(span);

  if (!aIsVertical && IsFrameRTL(aFrame))
    percent = 1 - percent;

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
    xPos += (int)ceil(((double)(trackWidth*2) * percent));
    overlayRect.left = xPos;
    overlayRect.right = xPos + overlaySize;
  } else {
    int yPos = aWidgetRect->bottom + trackWidth;
    yPos -= (int)ceil(((double)(trackWidth*2) * percent));
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
 * @param aWidgetType  type of widget
 * @param aTheme       progress theme handle
 * @param aHdc         hdc returned by gfxWindowsNativeDrawing
 * @param aPart        the PP_X progress part
 * @param aState       the theme state
 * @param aWidgetRect  bounding rect for the widget
 * @param aClipRect    dirty rect that needs drawing.
 * @param aAppUnits    app units per device pixel
 */
void
nsNativeThemeWin::DrawThemedProgressMeter(nsIFrame* aFrame, int aWidgetType,
                                          HANDLE aTheme, HDC aHdc,
                                          int aPart, int aState,
                                          RECT* aWidgetRect, RECT* aClipRect)
{
  if (!aFrame || !aTheme || !aHdc)
    return;

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

  EventStates eventStates = GetContentState(parentFrame, aWidgetType);
  bool vertical = IsVerticalProgress(parentFrame) ||
                  aWidgetType == NS_THEME_PROGRESSCHUNK_VERTICAL;
  bool indeterminate = IsIndeterminateProgress(parentFrame, eventStates);
  bool animate = indeterminate;

  // Vista and up progress meter is fill style, rendered here. We render
  // the pulse overlay in the follow up section below.
  DrawThemeBackground(aTheme, aHdc, aPart, aState,
                      &adjWidgetRect, &adjClipRect);
  if (!IsProgressMeterFilled(aFrame)) {
    animate = true;
  }

  if (animate) {
    // Indeterminate rendering
    int32_t overlayPart = GetProgressOverlayStyle(vertical);
    RECT overlayRect =
      CalculateProgressOverlayRect(aFrame, &adjWidgetRect, vertical,
                                   indeterminate, false);
    DrawThemeBackground(aTheme, aHdc, overlayPart, aState, &overlayRect,
                        &adjClipRect);

    if (!QueueAnimatedContentForRefresh(aFrame->GetContent(), 60)) {
      NS_WARNING("unable to animate progress widget!");
    }
  }
}

LayoutDeviceIntMargin
nsNativeThemeWin::GetCachedWidgetBorder(HTHEME aTheme,
                                        nsUXThemeClass aThemeClass,
                                        uint8_t aWidgetType,
                                        int32_t aPart,
                                        int32_t aState)
{
  int32_t cacheIndex = aThemeClass * THEME_PART_DISTINCT_VALUE_COUNT + aPart;
  int32_t cacheBitIndex = cacheIndex / 8;
  uint8_t cacheBit = 1u << (cacheIndex % 8);

  if (mBorderCacheValid[cacheBitIndex] & cacheBit) {
    return mBorderCache[cacheIndex];
  }

  // Get our info.
  RECT outerRect; // Create a fake outer rect.
  outerRect.top = outerRect.left = 100;
  outerRect.right = outerRect.bottom = 200;
  RECT contentRect(outerRect);
  HRESULT res = GetThemeBackgroundContentRect(aTheme, nullptr, aPart, aState, &outerRect, &contentRect);

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

nsresult nsNativeThemeWin::GetCachedMinimumWidgetSize(nsIFrame * aFrame, HANDLE aTheme,
                                                      nsUXThemeClass aThemeClass, uint8_t aWidgetType,
                                                      int32_t aPart, int32_t aState, THEMESIZE aSizeReq,
                                                      mozilla::LayoutDeviceIntSize * aResult)
{
  int32_t cachePart = aPart;

  if (aWidgetType == NS_THEME_BUTTON && aSizeReq == TS_MIN) {
    // In practice, NS_THEME_BUTTON is the only widget type which has an aSizeReq
    // that varies for us, and it can only be TS_MIN or TS_TRUE. Just stuff that
    // extra bit into the aPart part of the cache, since BP_Count is well below
    // THEME_PART_DISTINCT_VALUE_COUNT anyway.
    cachePart = BP_Count;
  }

  MOZ_ASSERT(aPart < THEME_PART_DISTINCT_VALUE_COUNT);
  int32_t cacheIndex = aThemeClass * THEME_PART_DISTINCT_VALUE_COUNT + cachePart;
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

  switch (aWidgetType) {
    case NS_THEME_INNER_SPIN_BUTTON:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON:
      aResult->width++;
      aResult->height = aResult->height / 2 + 1;
      break;

    case NS_THEME_MENUSEPARATOR:
    {
      SIZE gutterSize(GetGutterSize(aTheme, hdc));
      aResult->width += gutterSize.cx;
      break;
    }

    case NS_THEME_MENUARROW:
    {
      // Use the width of the arrow glyph as padding. See the drawing
      // code for details.
      aResult->width *= 2;
      break;
    }
  }

  ::ReleaseDC(nullptr, hdc);

  mMinimumWidgetSizeCacheValid[cacheBitIndex] |= cacheBit;
  mMinimumWidgetSizeCache[cacheIndex] = *aResult;

  return NS_OK;
}

mozilla::Maybe<nsUXThemeClass> nsNativeThemeWin::GetThemeClass(uint8_t aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX:
    case NS_THEME_GROUPBOX:
      return Some(eUXButton);
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_FOCUS_OUTLINE:
      return Some(eUXEdit);
    case NS_THEME_TOOLTIP:
      return Some(eUXTooltip);
    case NS_THEME_TOOLBOX:
      return Some(eUXRebar);
    case NS_THEME_WIN_MEDIA_TOOLBOX:
      return Some(eUXMediaRebar);
    case NS_THEME_WIN_COMMUNICATIONS_TOOLBOX:
      return Some(eUXCommunicationsRebar);
    case NS_THEME_WIN_BROWSERTABBAR_TOOLBOX:
      return Some(eUXBrowserTabBarRebar);
    case NS_THEME_TOOLBAR:
    case NS_THEME_TOOLBARBUTTON:
    case NS_THEME_SEPARATOR:
      return Some(eUXToolbar);
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSCHUNK:
    case NS_THEME_PROGRESSCHUNK_VERTICAL:
      return Some(eUXProgress);
    case NS_THEME_TAB:
    case NS_THEME_TABPANEL:
    case NS_THEME_TABPANELS:
      return Some(eUXTab);
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLBAR_VERTICAL:
    case NS_THEME_SCROLLBAR_HORIZONTAL:
    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT:
    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
    case NS_THEME_SCROLLCORNER:
      return Some(eUXScrollbar);
    case NS_THEME_RANGE:
    case NS_THEME_RANGE_THUMB:
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALETHUMB_HORIZONTAL:
    case NS_THEME_SCALETHUMB_VERTICAL:
      return Some(eUXTrackbar);
    case NS_THEME_INNER_SPIN_BUTTON:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON:
      return Some(eUXSpin);
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBARPANEL:
    case NS_THEME_RESIZERPANEL:
    case NS_THEME_RESIZER:
      return Some(eUXStatus);
    case NS_THEME_MENULIST:
    case NS_THEME_MENULIST_BUTTON:
      return Some(eUXCombobox);
    case NS_THEME_TREEHEADERCELL:
    case NS_THEME_TREEHEADERSORTARROW:
      return Some(eUXHeader);
    case NS_THEME_LISTBOX:
    case NS_THEME_LISTITEM:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TREETWISTYOPEN:
    case NS_THEME_TREEITEM:
      return Some(eUXListview);
    case NS_THEME_MENUBAR:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
    case NS_THEME_MENUSEPARATOR:
    case NS_THEME_MENUARROW:
    case NS_THEME_MENUIMAGE:
    case NS_THEME_MENUITEMTEXT:
      return Some(eUXMenu);
    case NS_THEME_WINDOW_TITLEBAR:
    case NS_THEME_WINDOW_TITLEBAR_MAXIMIZED:
    case NS_THEME_WINDOW_FRAME_LEFT:
    case NS_THEME_WINDOW_FRAME_RIGHT:
    case NS_THEME_WINDOW_FRAME_BOTTOM:
    case NS_THEME_WINDOW_BUTTON_CLOSE:
    case NS_THEME_WINDOW_BUTTON_MINIMIZE:
    case NS_THEME_WINDOW_BUTTON_MAXIMIZE:
    case NS_THEME_WINDOW_BUTTON_RESTORE:
    case NS_THEME_WINDOW_BUTTON_BOX:
    case NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED:
    case NS_THEME_WIN_GLASS:
    case NS_THEME_WIN_BORDERLESS_GLASS:
      return Some(eUXWindowFrame);
  }
  return Nothing();
}

HANDLE
nsNativeThemeWin::GetTheme(uint8_t aWidgetType)
{
  mozilla::Maybe<nsUXThemeClass> themeClass = GetThemeClass(aWidgetType);
  if (themeClass.isNothing()) {
    return nullptr;
  }
  return nsUXThemeData::GetTheme(themeClass.value());
}

int32_t
nsNativeThemeWin::StandardGetState(nsIFrame* aFrame, uint8_t aWidgetType,
                                   bool wantFocused)
{
  EventStates eventState = GetContentState(aFrame, aWidgetType);
  if (eventState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE))
    return TS_ACTIVE;
  if (eventState.HasState(NS_EVENT_STATE_HOVER))
    return TS_HOVER;
  if (wantFocused && eventState.HasState(NS_EVENT_STATE_FOCUS))
    return TS_FOCUSED;

  return TS_NORMAL;
}

bool
nsNativeThemeWin::IsMenuActive(nsIFrame *aFrame, uint8_t aWidgetType)
{
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
nsresult 
nsNativeThemeWin::GetThemePartAndState(nsIFrame* aFrame, uint8_t aWidgetType, 
                                       int32_t& aPart, int32_t& aState)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON: {
      aPart = BP_BUTTON;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      EventStates eventState = GetContentState(aFrame, aWidgetType);
      if (IsDisabled(aFrame, eventState)) {
        aState = TS_DISABLED;
        return NS_OK;
      } else if (IsOpenButton(aFrame) ||
                 IsCheckedButton(aFrame)) {
        aState = TS_ACTIVE;
        return NS_OK;
      }

      aState = StandardGetState(aFrame, aWidgetType, true);
      
      // Check for default dialog buttons.  These buttons should always look
      // focused.
      if (aState == TS_NORMAL && IsDefaultButton(aFrame))
        aState = TS_FOCUSED;
      return NS_OK;
    }
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO: {
      bool isCheckbox = (aWidgetType == NS_THEME_CHECKBOX);
      aPart = isCheckbox ? BP_CHECKBOX : BP_RADIO;

      enum InputState {
        UNCHECKED = 0, CHECKED, INDETERMINATE
      };
      InputState inputState = UNCHECKED;
      bool isXULCheckboxRadio = false;

      if (!aFrame) {
        aState = TS_NORMAL;
      } else {
        if (GetCheckedOrSelected(aFrame, !isCheckbox)) {
          inputState = CHECKED;
        } if (isCheckbox && GetIndeterminate(aFrame)) {
          inputState = INDETERMINATE;
        }

        EventStates eventState =
          GetContentState(isXULCheckboxRadio ? aFrame->GetParent() : aFrame,
                          aWidgetType);
        if (IsDisabled(aFrame, eventState)) {
          aState = TS_DISABLED;
        } else {
          aState = StandardGetState(aFrame, aWidgetType, false);
        }
      }

      // 4 unchecked states, 4 checked states, 4 indeterminate states.
      aState += inputState * 4;
      return NS_OK;
    }
    case NS_THEME_GROUPBOX: {
      aPart = BP_GROUPBOX;
      aState = TS_NORMAL;
      // Since we don't support groupbox disabled and GBS_DISABLED looks the
      // same as GBS_NORMAL don't bother supporting GBS_DISABLED.
      return NS_OK;
    }
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE: {
      EventStates eventState = GetContentState(aFrame, aWidgetType);

      /* Note: the NOSCROLL type has a rounded corner in each corner.  The more
       * specific HSCROLL, VSCROLL, HVSCROLL types have side and/or top/bottom
       * edges rendered as straight horizontal lines with sharp corners to
       * accommodate a scrollbar.  However, the scrollbar gets rendered on top
       * of this for us, so we don't care, and can just use NOSCROLL here.
       */
      aPart = TFP_EDITBORDER_NOSCROLL;

      if (!aFrame) {
        aState = TFS_EDITBORDER_NORMAL;
      } else if (IsDisabled(aFrame, eventState)) {
        aState = TFS_EDITBORDER_DISABLED;
      } else if (IsReadOnly(aFrame)) {
        /* no special read-only state */
        aState = TFS_EDITBORDER_NORMAL;
      } else {
        nsIContent* content = aFrame->GetContent();

        /* XUL textboxes don't get focused themselves, because they have child
         * html:input.. but we can check the XUL focused attributes on them
         */
        if (content && content->IsXULElement() && IsFocused(aFrame))
          aState = TFS_EDITBORDER_FOCUSED;
        else if (eventState.HasAtLeastOneOfStates(NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_FOCUS))
          aState = TFS_EDITBORDER_FOCUSED;
        else if (eventState.HasState(NS_EVENT_STATE_HOVER))
          aState = TFS_EDITBORDER_HOVER;
        else
          aState = TFS_EDITBORDER_NORMAL;
      }

      return NS_OK;
    }
    case NS_THEME_FOCUS_OUTLINE: {
      // XXX the EDITBORDER values don't respect DTBG_OMITCONTENT
      aPart = TFP_TEXTFIELD; //TFP_EDITBORDER_NOSCROLL;
      aState = TS_FOCUSED; //TFS_EDITBORDER_FOCUSED;
      return NS_OK;
    }
    case NS_THEME_TOOLTIP: {
      aPart = TTP_STANDARD;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL: {
      // Note IsVerticalProgress only tests for orient css attrribute,
      // NS_THEME_PROGRESSBAR_VERTICAL is dedicated to -moz-appearance:
      // progressbar-vertical.
      bool vertical = IsVerticalProgress(aFrame) ||
                      aWidgetType == NS_THEME_PROGRESSBAR_VERTICAL;
      aPart = vertical ? PP_BARVERT : PP_BAR;
      aState = PBBS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_PROGRESSCHUNK:
    case NS_THEME_PROGRESSCHUNK_VERTICAL: {
      nsIFrame* parentFrame = aFrame->GetParent();
      if (aWidgetType == NS_THEME_PROGRESSCHUNK_VERTICAL ||
          IsVerticalProgress(parentFrame)) {
        aPart = PP_FILLVERT;
      } else {
        aPart = PP_FILL;
      }

      aState = PBBVS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TOOLBARBUTTON: {
      aPart = BP_BUTTON;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      EventStates eventState = GetContentState(aFrame, aWidgetType);
      if (IsDisabled(aFrame, eventState)) {
        aState = TS_DISABLED;
        return NS_OK;
      }
      if (IsOpenButton(aFrame)) {
        aState = TS_ACTIVE;
        return NS_OK;
      }

      if (eventState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE))
        aState = TS_ACTIVE;
      else if (eventState.HasState(NS_EVENT_STATE_HOVER)) {
        if (IsCheckedButton(aFrame))
          aState = TB_HOVER_CHECKED;
        else
          aState = TS_HOVER;
      }
      else {
        if (IsCheckedButton(aFrame))
          aState = TB_CHECKED;
        else
          aState = TS_NORMAL;
      }
     
      return NS_OK;
    }
    case NS_THEME_SEPARATOR: {
      aPart = TP_SEPARATOR;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT: {
      aPart = SP_BUTTON;
      aState = (aWidgetType - NS_THEME_SCROLLBARBUTTON_UP)*4;
      EventStates eventState = GetContentState(aFrame, aWidgetType);
      if (!aFrame)
        aState += TS_NORMAL;
      else if (IsDisabled(aFrame, eventState))
        aState += TS_DISABLED;
      else {
        nsIFrame *parent = aFrame->GetParent();
        EventStates parentState =
          GetContentState(parent, parent->StyleDisplay()->mAppearance);
        if (eventState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE))
          aState += TS_ACTIVE;
        else if (eventState.HasState(NS_EVENT_STATE_HOVER))
          aState += TS_HOVER;
        else if (parentState.HasState(NS_EVENT_STATE_HOVER))
          aState = (aWidgetType - NS_THEME_SCROLLBARBUTTON_UP) + SP_BUTTON_IMPLICIT_HOVER_BASE;
        else
          aState += TS_NORMAL;
      }
      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_HORIZONTAL:
    case NS_THEME_SCROLLBAR_VERTICAL: {
      aPart = (aWidgetType == NS_THEME_SCROLLBAR_HORIZONTAL) ?
              SP_TRACKSTARTHOR : SP_TRACKSTARTVERT;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
    case NS_THEME_SCROLLBARTHUMB_VERTICAL: {
      aPart = (aWidgetType == NS_THEME_SCROLLBARTHUMB_HORIZONTAL) ?
              SP_THUMBHOR : SP_THUMBVERT;
      EventStates eventState = GetContentState(aFrame, aWidgetType);
      if (!aFrame)
        aState = TS_NORMAL;
      else if (IsDisabled(aFrame, eventState))
        aState = TS_DISABLED;
      else {
        if (eventState.HasState(NS_EVENT_STATE_ACTIVE)) // Hover is not also a requirement for
                                                        // the thumb, since the drag is not canceled
                                                        // when you move outside the thumb.
          aState = TS_ACTIVE;
        else if (eventState.HasState(NS_EVENT_STATE_HOVER))
          aState = TS_HOVER;
        else 
          aState = TS_NORMAL;
      }
      return NS_OK;
    }
    case NS_THEME_RANGE:
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL: {
      if (aWidgetType == NS_THEME_SCALE_HORIZONTAL ||
          (aWidgetType == NS_THEME_RANGE &&
           IsRangeHorizontal(aFrame))) {
        aPart = TKP_TRACK;
        aState = TRS_NORMAL;
      } else {
        aPart = TKP_TRACKVERT;
        aState = TRVS_NORMAL;
      }
      return NS_OK;
    }
    case NS_THEME_RANGE_THUMB:
    case NS_THEME_SCALETHUMB_HORIZONTAL:
    case NS_THEME_SCALETHUMB_VERTICAL: {
      if (aWidgetType == NS_THEME_RANGE_THUMB) {
        if (IsRangeHorizontal(aFrame)) {
          aPart = TKP_THUMBBOTTOM;
        } else {
          aPart = IsFrameRTL(aFrame) ? TKP_THUMBLEFT : TKP_THUMBRIGHT;
        }
      } else {
        aPart = (aWidgetType == NS_THEME_SCALETHUMB_HORIZONTAL) ?
                TKP_THUMB : TKP_THUMBVERT;
      }
      EventStates eventState = GetContentState(aFrame, aWidgetType);
      if (!aFrame)
        aState = TS_NORMAL;
      else if (IsDisabled(aFrame, eventState)) {
        aState = TKP_DISABLED;
      }
      else {
        if (eventState.HasState(NS_EVENT_STATE_ACTIVE)) // Hover is not also a requirement for
                                                        // the thumb, since the drag is not canceled
                                                        // when you move outside the thumb.
          aState = TS_ACTIVE;
        else if (eventState.HasState(NS_EVENT_STATE_FOCUS))
          aState = TKP_FOCUSED;
        else if (eventState.HasState(NS_EVENT_STATE_HOVER))
          aState = TS_HOVER;
        else
          aState = TS_NORMAL;
      }
      return NS_OK;
    }
    case NS_THEME_INNER_SPIN_BUTTON:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON: {
      aPart = (aWidgetType == NS_THEME_SPINNER_UPBUTTON) ?
              SPNP_UP : SPNP_DOWN;
      EventStates eventState = GetContentState(aFrame, aWidgetType);
      if (!aFrame)
        aState = TS_NORMAL;
      else if (IsDisabled(aFrame, eventState))
        aState = TS_DISABLED;
      else
        aState = StandardGetState(aFrame, aWidgetType, false);
      return NS_OK;    
    }
    case NS_THEME_TOOLBOX:
    case NS_THEME_WIN_MEDIA_TOOLBOX:
    case NS_THEME_WIN_COMMUNICATIONS_TOOLBOX:
    case NS_THEME_WIN_BROWSERTABBAR_TOOLBOX:
    case NS_THEME_STATUSBAR:
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLCORNER: {
      aState = 0;
      aPart = RP_BACKGROUND;
      return NS_OK;
    }
    case NS_THEME_TOOLBAR: {
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
    case NS_THEME_STATUSBARPANEL:
    case NS_THEME_RESIZERPANEL:
    case NS_THEME_RESIZER: {
      aPart = (aWidgetType - NS_THEME_STATUSBARPANEL) + 1;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TREEVIEW:
    case NS_THEME_LISTBOX: {
      aPart = TREEVIEW_BODY;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TABPANELS: {
      aPart = TABP_PANELS;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TABPANEL: {
      aPart = TABP_PANEL;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TAB: {
      aPart = TABP_TAB;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      EventStates eventState = GetContentState(aFrame, aWidgetType);
      if (IsDisabled(aFrame, eventState)) {
        aState = TS_DISABLED;
        return NS_OK;
      }

      if (IsSelectedTab(aFrame)) {
        aPart = TABP_TAB_SELECTED;
        aState = TS_ACTIVE; // The selected tab is always "pressed".
      }
      else
        aState = StandardGetState(aFrame, aWidgetType, true);
      
      return NS_OK;
    }
    case NS_THEME_TREEHEADERSORTARROW: {
      // XXX Probably will never work due to a bug in the Luna theme.
      aPart = 4;
      aState = 1;
      return NS_OK;
    }
    case NS_THEME_TREEHEADERCELL: {
      aPart = 1;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }
      
      aState = StandardGetState(aFrame, aWidgetType, true);
      
      return NS_OK;
    }
    case NS_THEME_MENULIST: {
      nsIContent* content = aFrame->GetContent();
      bool isHTML = content && content->IsHTMLElement();
      bool isChrome = aFrame->GetContent()->IsInChromeDocument();
      bool useDropBorder = isHTML || (isChrome && IsMenuListEditable(aFrame));
      EventStates eventState = GetContentState(aFrame, aWidgetType);

      /* On Vista/Win7, we use CBP_DROPBORDER instead of DROPFRAME for HTML
       * content or for editable menulists; this gives us the thin outline,
       * instead of the gradient-filled background */
      if (useDropBorder)
        aPart = CBP_DROPBORDER;
      else
        aPart = CBP_DROPFRAME;

      if (IsDisabled(aFrame, eventState)) {
        aState = TS_DISABLED;
      } else if (IsReadOnly(aFrame)) {
        aState = TS_NORMAL;
      } else if (IsOpenButton(aFrame)) {
        aState = TS_ACTIVE;
      } else {
        if (useDropBorder && (eventState.HasState(NS_EVENT_STATE_FOCUS) || IsFocused(aFrame)))
          aState = TS_ACTIVE;
        else if (eventState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE))
          aState = TS_ACTIVE;
        else if (eventState.HasState(NS_EVENT_STATE_HOVER))
          aState = TS_HOVER;
        else
          aState = TS_NORMAL;
      }

      return NS_OK;
    }
    case NS_THEME_MENULIST_BUTTON: {
      bool isHTML = IsHTMLContent(aFrame);
      nsIFrame* parentFrame = aFrame->GetParent();
      bool isMenulist = !isHTML && parentFrame->IsMenuFrame();
      bool isOpen = false;

      // HTML select and XUL menulist dropdown buttons get state from the parent.
      if (isHTML || isMenulist)
        aFrame = parentFrame;

      EventStates eventState = GetContentState(aFrame, aWidgetType);
      aPart = CBP_DROPMARKER_VISTA;

      // For HTML controls with author styling, we should fall
      // back to the old dropmarker style to avoid clashes with
      // author-specified backgrounds and borders (bug #441034)
      if (isHTML && IsWidgetStyled(aFrame->PresContext(), aFrame, NS_THEME_MENULIST))
        aPart = CBP_DROPMARKER;

      if (IsDisabled(aFrame, eventState)) {
        aState = TS_DISABLED;
        return NS_OK;
      }

      if (isHTML) {
        nsIComboboxControlFrame* ccf = do_QueryFrame(aFrame);
        isOpen = (ccf && ccf->IsDroppedDownOrHasParentPopup());
      }
      else
        isOpen = IsOpenButton(aFrame);

      bool isChrome = aFrame->GetContent()->IsInChromeDocument();
      if (isHTML || (isChrome && IsMenuListEditable(aFrame))) {
        if (isOpen) {
          /* Hover is propagated, but we need to know whether we're hovering
           * just the combobox frame, not the dropdown frame. But, we can't get
           * that information, since hover is on the content node, and they
           * share the same content node.  So, instead, we cheat -- if the
           * dropdown is open, we always show the hover state.  This looks fine
           * in practice.
           */
          aState = TS_HOVER;
          return NS_OK;
        }
      } else {
        /* The dropdown indicator on a menulist button in chrome is not given a
         * hover effect. When the frame isn't isn't HTML content, we cheat and
         * force the dropdown state to be normal. (Bug 430434)
         */
        aState = TS_NORMAL;
        return NS_OK;
      }

      aState = TS_NORMAL;

      // Dropdown button active state doesn't need :hover.
      if (eventState.HasState(NS_EVENT_STATE_ACTIVE)) {
        if (isOpen && (isHTML || isMenulist)) {
          // XXX Button should look active until the mouse is released, but
          //     without making it look active when the popup is clicked.
          return NS_OK;
        }
        aState = TS_ACTIVE;
      }
      else if (eventState.HasState(NS_EVENT_STATE_HOVER)) {
        // No hover effect for XUL menulists and autocomplete dropdown buttons
        // while the dropdown menu is open.
        if (isOpen) {
          // XXX HTML select dropdown buttons should have the hover effect when
          //     hovering the combobox frame, but not the popup frame.
          return NS_OK;
        }
        aState = TS_HOVER;
      }
      return NS_OK;
    }
    case NS_THEME_MENUPOPUP: {
      aPart = MENU_POPUPBACKGROUND;
      aState = MB_ACTIVE;
      return NS_OK;
    }
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM: 
    case NS_THEME_RADIOMENUITEM: {
      bool isTopLevel = false;
      bool isOpen = false;
      bool isHover = false;
      nsMenuFrame *menuFrame = do_QueryFrame(aFrame);
      EventStates eventState = GetContentState(aFrame, aWidgetType);

      isTopLevel = IsTopLevelMenu(aFrame);

      if (menuFrame)
        isOpen = menuFrame->IsOpen();

      isHover = IsMenuActive(aFrame, aWidgetType);

      if (isTopLevel) {
        aPart = MENU_BARITEM;

        if (isOpen)
          aState = MBI_PUSHED;
        else if (isHover)
          aState = MBI_HOT;
        else
          aState = MBI_NORMAL;

        // the disabled states are offset by 3
        if (IsDisabled(aFrame, eventState))
          aState += 3;
      } else {
        aPart = MENU_POPUPITEM;

        if (isHover)
          aState = MPI_HOT;
        else
          aState = MPI_NORMAL;

        // the disabled states are offset by 2
        if (IsDisabled(aFrame, eventState))
          aState += 2;
      }

      return NS_OK;
    }
    case NS_THEME_MENUSEPARATOR:
      aPart = MENU_POPUPSEPARATOR;
      aState = 0;
      return NS_OK;
    case NS_THEME_MENUARROW:
      {
        aPart = MENU_POPUPSUBMENU;
        EventStates eventState = GetContentState(aFrame, aWidgetType);
        aState = IsDisabled(aFrame, eventState) ? MSM_DISABLED : MSM_NORMAL;
        return NS_OK;
      }
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
      {
        EventStates eventState = GetContentState(aFrame, aWidgetType);

        aPart = MENU_POPUPCHECK;
        aState = MC_CHECKMARKNORMAL;

        // Radio states are offset by 2
        if (aWidgetType == NS_THEME_MENURADIO)
          aState += 2;

        // the disabled states are offset by 1
        if (IsDisabled(aFrame, eventState))
          aState += 1;

        return NS_OK;
      }
    case NS_THEME_MENUITEMTEXT:
    case NS_THEME_MENUIMAGE:
      aPart = -1;
      aState = 0;
      return NS_OK;

    case NS_THEME_WINDOW_TITLEBAR:
      aPart = mozilla::widget::themeconst::WP_CAPTION;
      aState = GetTopLevelWindowActiveState(aFrame);
      return NS_OK;
    case NS_THEME_WINDOW_TITLEBAR_MAXIMIZED:
      aPart = mozilla::widget::themeconst::WP_MAXCAPTION;
      aState = GetTopLevelWindowActiveState(aFrame);
      return NS_OK;
    case NS_THEME_WINDOW_FRAME_LEFT:
      aPart = mozilla::widget::themeconst::WP_FRAMELEFT;
      aState = GetTopLevelWindowActiveState(aFrame);
      return NS_OK;
    case NS_THEME_WINDOW_FRAME_RIGHT:
      aPart = mozilla::widget::themeconst::WP_FRAMERIGHT;
      aState = GetTopLevelWindowActiveState(aFrame);
      return NS_OK;
    case NS_THEME_WINDOW_FRAME_BOTTOM:
      aPart = mozilla::widget::themeconst::WP_FRAMEBOTTOM;
      aState = GetTopLevelWindowActiveState(aFrame);
      return NS_OK;
    case NS_THEME_WINDOW_BUTTON_CLOSE:
      aPart = mozilla::widget::themeconst::WP_CLOSEBUTTON;
      aState = GetWindowFrameButtonState(aFrame, GetContentState(aFrame, aWidgetType));
      return NS_OK;
    case NS_THEME_WINDOW_BUTTON_MINIMIZE:
      aPart = mozilla::widget::themeconst::WP_MINBUTTON;
      aState = GetWindowFrameButtonState(aFrame, GetContentState(aFrame, aWidgetType));
      return NS_OK;
    case NS_THEME_WINDOW_BUTTON_MAXIMIZE:
      aPart = mozilla::widget::themeconst::WP_MAXBUTTON;
      aState = GetWindowFrameButtonState(aFrame, GetContentState(aFrame, aWidgetType));
      return NS_OK;
    case NS_THEME_WINDOW_BUTTON_RESTORE:
      aPart = mozilla::widget::themeconst::WP_RESTOREBUTTON;
      aState = GetWindowFrameButtonState(aFrame, GetContentState(aFrame, aWidgetType));
      return NS_OK;
    case NS_THEME_WINDOW_BUTTON_BOX:
    case NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED:
    case NS_THEME_WIN_GLASS:
    case NS_THEME_WIN_BORDERLESS_GLASS:
      aPart = -1;
      aState = 0;
      return NS_OK;
  }

  aPart = 0;
  aState = 0;
  return NS_ERROR_FAILURE;
}

static bool
AssumeThemePartAndStateAreTransparent(int32_t aPart, int32_t aState)
{
  if (!(IsWin8Point1OrLater() && nsUXThemeData::IsHighContrastOn()) &&
      aPart == MENU_POPUPITEM && aState == MBI_NORMAL) {
    return true;
  }
  return false;
}

// When running with per-monitor DPI (on Win8.1+), and rendering on a display
// with a different DPI setting from the system's default scaling, we need to
// apply scaling to native-themed elements as the Windows theme APIs assume
// the system default resolution.
static inline double
GetThemeDpiScaleFactor(nsIFrame* aFrame)
{
  if (WinUtils::IsPerMonitorDPIAware() ||
      nsIWidget::DefaultScaleOverride() > 0.0) {
    nsIWidget* rootWidget = aFrame->PresContext()->GetRootWidget();
    if (rootWidget) {
      double systemScale = WinUtils::SystemScaleFactor();
      return rootWidget->GetDefaultScale().scale / systemScale;
    }
  }
  return 1.0;
}

NS_IMETHODIMP
nsNativeThemeWin::DrawWidgetBackground(gfxContext* aContext,
                                       nsIFrame* aFrame,
                                       uint8_t aWidgetType,
                                       const nsRect& aRect,
                                       const nsRect& aDirtyRect)
{
  if (IsWidgetScrollbarPart(aWidgetType)) {
    ComputedStyle* style = nsLayoutUtils::StyleForScrollbar(aFrame);
    if (style->StyleUserInterface()->HasCustomScrollbars()) {
      return DrawCustomScrollbarPart(aContext, aFrame, style,
                                     aWidgetType, aRect, aDirtyRect);
    }
  }

  HANDLE theme = GetTheme(aWidgetType);
  if (!theme)
    return ClassicDrawWidgetBackground(aContext, aFrame, aWidgetType, aRect, aDirtyRect); 

  // ^^ without the right sdk, assume xp theming and fall through.
  if (nsUXThemeData::CheckForCompositor()) {
    switch (aWidgetType) {
      case NS_THEME_WINDOW_TITLEBAR:
      case NS_THEME_WINDOW_TITLEBAR_MAXIMIZED:
      case NS_THEME_WINDOW_FRAME_LEFT:
      case NS_THEME_WINDOW_FRAME_RIGHT:
      case NS_THEME_WINDOW_FRAME_BOTTOM:
        // Nothing to draw, these areas are glass. Minimum dimensions
        // should be set, so xul content should be layed out correctly.
        return NS_OK;
      break;
      case NS_THEME_WINDOW_BUTTON_CLOSE:
      case NS_THEME_WINDOW_BUTTON_MINIMIZE:
      case NS_THEME_WINDOW_BUTTON_MAXIMIZE:
      case NS_THEME_WINDOW_BUTTON_RESTORE:
        // Not conventional bitmaps, can't be retrieved. If we fall
        // through here and call the theme library we'll get aero
        // basic bitmaps. 
        return NS_OK;
      break;
      case NS_THEME_WIN_GLASS:
      case NS_THEME_WIN_BORDERLESS_GLASS:
        // Nothing to draw, this is the glass background.
        return NS_OK;
      case NS_THEME_WINDOW_BUTTON_BOX:
      case NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED:
        // We handle these through nsIWidget::UpdateThemeGeometries
        return NS_OK;
      break;
    }
  }

  int32_t part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return rv;

  if (AssumeThemePartAndStateAreTransparent(part, state)) {
    return NS_OK;
  }

  RefPtr<gfxContext> ctx = aContext;
  gfxContextMatrixAutoSaveRestore save(ctx);

  double themeScale = GetThemeDpiScaleFactor(aFrame);
  if (themeScale != 1.0) {
    ctx->SetMatrix(ctx->CurrentMatrix().PreScale(themeScale, themeScale));
  }

  gfxFloat p2a = gfxFloat(aFrame->PresContext()->AppUnitsPerDevPixel());
  RECT widgetRect;
  RECT clipRect;
  gfxRect tr(aRect.X(), aRect.Y(), aRect.Width(), aRect.Height()),
          dr(aDirtyRect.X(), aDirtyRect.Y(), aDirtyRect.Width(), aDirtyRect.Height());

  tr.Scale(1.0 / (p2a * themeScale));
  dr.Scale(1.0 / (p2a * themeScale));

  gfxWindowsNativeDrawing nativeDrawing(ctx, dr, GetWidgetNativeDrawingFlags(aWidgetType));

RENDER_AGAIN:

  HDC hdc = nativeDrawing.BeginNativeDrawing();
  if (!hdc)
    return NS_ERROR_FAILURE;

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

  if (aWidgetType == NS_THEME_WINDOW_TITLEBAR) {
    // Clip out the left and right corners of the frame, all we want in
    // is the middle section.
    widgetRect.left -= GetSystemMetrics(SM_CXFRAME);
    widgetRect.right += GetSystemMetrics(SM_CXFRAME);
  } else if (aWidgetType == NS_THEME_WINDOW_TITLEBAR_MAXIMIZED) {
    // The origin of the window is off screen when maximized and windows
    // doesn't compensate for this in rendering the background. Push the
    // top of the bitmap down by SM_CYFRAME so we get the full graphic.
    widgetRect.top += GetSystemMetrics(SM_CYFRAME);
  } else if (aWidgetType == NS_THEME_TAB) {
    // For left edge and right edge tabs, we need to adjust the widget
    // rects and clip rects so that the edges don't get drawn.
    bool isLeft = IsLeftToSelectedTab(aFrame);
    bool isRight = !isLeft && IsRightToSelectedTab(aFrame);

    if (isLeft || isRight) {
      // HACK ALERT: There appears to be no way to really obtain this value, so we're forced
      // to just use the default value for Luna (which also happens to be correct for
      // all the other skins I've tried).
      int32_t edgeSize = 2;
    
      // Armed with the size of the edge, we now need to either shift to the left or to the
      // right.  The clip rect won't include this extra area, so we know that we're
      // effectively shifting the edge out of view (such that it won't be painted).
      if (isLeft)
        // The right edge should not be drawn.  Extend our rect by the edge size.
        widgetRect.right += edgeSize;
      else
        // The left edge should not be drawn.  Move the widget rect's left coord back.
        widgetRect.left -= edgeSize;
    }
  }
  else if (aWidgetType == NS_THEME_WINDOW_BUTTON_MINIMIZE) {
    OffsetBackgroundRect(widgetRect, CAPTIONBUTTON_MINIMIZE);
  }
  else if (aWidgetType == NS_THEME_WINDOW_BUTTON_MAXIMIZE ||
           aWidgetType == NS_THEME_WINDOW_BUTTON_RESTORE) {
    OffsetBackgroundRect(widgetRect, CAPTIONBUTTON_RESTORE);
  }
  else if (aWidgetType == NS_THEME_WINDOW_BUTTON_CLOSE) {
    OffsetBackgroundRect(widgetRect, CAPTIONBUTTON_CLOSE);
  }

  // widgetRect is the bounding box for a widget, yet the scale track is only
  // a small portion of this size, so the edges of the scale need to be
  // adjusted to the real size of the track.
  if (aWidgetType == NS_THEME_RANGE ||
      aWidgetType == NS_THEME_SCALE_HORIZONTAL ||
      aWidgetType == NS_THEME_SCALE_VERTICAL) {
    RECT contentRect;
    GetThemeBackgroundContentRect(theme, hdc, part, state, &widgetRect, &contentRect);

    SIZE siz;
    GetThemePartSize(theme, hdc, part, state, &widgetRect, TS_TRUE, &siz);

    // When rounding is necessary, we round the position of the track
    // away from the chevron of the thumb to make it look better.
    if (aWidgetType == NS_THEME_SCALE_HORIZONTAL ||
        (aWidgetType == NS_THEME_RANGE && IsRangeHorizontal(aFrame))) {
      contentRect.top += (contentRect.bottom - contentRect.top - siz.cy) / 2;
      contentRect.bottom = contentRect.top + siz.cy;
    }
    else {
      if (!IsFrameRTL(aFrame)) {
        contentRect.left += (contentRect.right - contentRect.left - siz.cx) / 2;
        contentRect.right = contentRect.left + siz.cx;
      } else {
        contentRect.right -= (contentRect.right - contentRect.left - siz.cx) / 2;
        contentRect.left = contentRect.right - siz.cx;
      }
    }

    DrawThemeBackground(theme, hdc, part, state, &contentRect, &clipRect);
  }
  else if (aWidgetType == NS_THEME_MENUCHECKBOX || aWidgetType == NS_THEME_MENURADIO)
  {
      bool isChecked = false;
      isChecked = CheckBooleanAttr(aFrame, nsGkAtoms::checked);

      if (isChecked)
      {
        int bgState = MCB_NORMAL;
        EventStates eventState = GetContentState(aFrame, aWidgetType);

        // the disabled states are offset by 1
        if (IsDisabled(aFrame, eventState))
          bgState += 1;

        SIZE checkboxBGSize(GetCheckboxBGSize(theme, hdc));

        RECT checkBGRect = widgetRect;
        if (IsFrameRTL(aFrame)) {
          checkBGRect.left = checkBGRect.right-checkboxBGSize.cx;
        } else {
          checkBGRect.right = checkBGRect.left+checkboxBGSize.cx;
        }

        // Center the checkbox background vertically in the menuitem
        checkBGRect.top += (checkBGRect.bottom - checkBGRect.top)/2 - checkboxBGSize.cy/2;
        checkBGRect.bottom = checkBGRect.top + checkboxBGSize.cy;

        DrawThemeBackground(theme, hdc, MENU_POPUPCHECKBACKGROUND, bgState, &checkBGRect, &clipRect);

        MARGINS checkMargins = GetCheckboxMargins(theme, hdc);
        RECT checkRect = checkBGRect;
        checkRect.left += checkMargins.cxLeftWidth;
        checkRect.right -= checkMargins.cxRightWidth;
        checkRect.top += checkMargins.cyTopHeight;
        checkRect.bottom -= checkMargins.cyBottomHeight;
        DrawThemeBackground(theme, hdc, MENU_POPUPCHECK, state, &checkRect, &clipRect);
      }
  }
  else if (aWidgetType == NS_THEME_MENUPOPUP)
  {
    DrawThemeBackground(theme, hdc, MENU_POPUPBORDERS, /* state */ 0, &widgetRect, &clipRect);
    SIZE borderSize;
    GetThemePartSize(theme, hdc, MENU_POPUPBORDERS, 0, nullptr, TS_TRUE, &borderSize);

    RECT bgRect = widgetRect;
    bgRect.top += borderSize.cy;
    bgRect.bottom -= borderSize.cy;
    bgRect.left += borderSize.cx;
    bgRect.right -= borderSize.cx;

    DrawThemeBackground(theme, hdc, MENU_POPUPBACKGROUND, /* state */ 0, &bgRect, &clipRect);

    SIZE gutterSize(GetGutterSize(theme, hdc));

    RECT gutterRect;
    gutterRect.top = bgRect.top;
    gutterRect.bottom = bgRect.bottom;
    if (IsFrameRTL(aFrame)) {
      gutterRect.right = bgRect.right;
      gutterRect.left = gutterRect.right-gutterSize.cx;
    } else {
      gutterRect.left = bgRect.left;
      gutterRect.right = gutterRect.left+gutterSize.cx;
    }

    DrawThemeBGRTLAware(theme, hdc, MENU_POPUPGUTTER, /* state */ 0,
                        &gutterRect, &clipRect, IsFrameRTL(aFrame));
  }
  else if (aWidgetType == NS_THEME_MENUSEPARATOR)
  {
    SIZE gutterSize(GetGutterSize(theme,hdc));

    RECT sepRect = widgetRect;
    if (IsFrameRTL(aFrame))
      sepRect.right -= gutterSize.cx;
    else
      sepRect.left += gutterSize.cx;

    DrawThemeBackground(theme, hdc, MENU_POPUPSEPARATOR, /* state */ 0, &sepRect, &clipRect);
  }
  else if (aWidgetType == NS_THEME_MENUARROW)
  {
    // We're dpi aware and as such on systems that have dpi > 96 set, the
    // theme library expects us to do proper positioning and scaling of glyphs.
    // For NS_THEME_MENUARROW, layout may hand us a widget rect larger than the
    // glyph rect we request in GetMinimumWidgetSize. To prevent distortion we
    // have to position and scale what we draw.

    SIZE glyphSize;
    GetThemePartSize(theme, hdc, part, state, nullptr, TS_TRUE, &glyphSize);

    int32_t widgetHeight = widgetRect.bottom - widgetRect.top;

    RECT renderRect = widgetRect;

    // We request (glyph width * 2, glyph height) in GetMinimumWidgetSize. In
    // Firefox some menu items provide the full height of the item to us, in
    // others our widget rect is the exact dims of our arrow glyph. Adjust the
    // vertical position by the added space, if any exists.
    renderRect.top += ((widgetHeight - glyphSize.cy) / 2);
    renderRect.bottom = renderRect.top + glyphSize.cy;
    // I'm using the width of the arrow glyph for the arrow-side padding.
    // AFAICT there doesn't appear to be a theme constant we can query
    // for this value. Generally this looks correct, and has the added
    // benefit of being a dpi adjusted value.
    if (!IsFrameRTL(aFrame)) {
      renderRect.right = widgetRect.right - glyphSize.cx;
      renderRect.left = renderRect.right - glyphSize.cx;
    } else {
      renderRect.left = glyphSize.cx;
      renderRect.right = renderRect.left + glyphSize.cx;
    }
    DrawThemeBGRTLAware(theme, hdc, part, state, &renderRect, &clipRect,
                        IsFrameRTL(aFrame));
  }
  // The following widgets need to be RTL-aware
  else if (aWidgetType == NS_THEME_RESIZER ||
           aWidgetType == NS_THEME_MENULIST_BUTTON)
  {
    DrawThemeBGRTLAware(theme, hdc, part, state,
                        &widgetRect, &clipRect, IsFrameRTL(aFrame));
  }
  else if (aWidgetType == NS_THEME_NUMBER_INPUT ||
           aWidgetType == NS_THEME_TEXTFIELD ||
           aWidgetType == NS_THEME_TEXTFIELD_MULTILINE) {
    DrawThemeBackground(theme, hdc, part, state, &widgetRect, &clipRect);
     if (state == TFS_EDITBORDER_DISABLED) {
      InflateRect(&widgetRect, -1, -1);
      ::FillRect(hdc, &widgetRect, reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1));
    }
  }
  else if (aWidgetType == NS_THEME_PROGRESSBAR ||
           aWidgetType == NS_THEME_PROGRESSBAR_VERTICAL) {
    // DrawThemeBackground renders each corner with a solid white pixel.
    // Restore these pixels to the underlying color. Tracks are rendered
    // using alpha recovery, so this makes the corners transparent.
    COLORREF color;
    color = GetPixel(hdc, widgetRect.left, widgetRect.top);
    DrawThemeBackground(theme, hdc, part, state, &widgetRect, &clipRect);
    SetPixel(hdc, widgetRect.left, widgetRect.top, color);
    SetPixel(hdc, widgetRect.right-1, widgetRect.top, color);
    SetPixel(hdc, widgetRect.right-1, widgetRect.bottom-1, color);
    SetPixel(hdc, widgetRect.left, widgetRect.bottom-1, color);
  }
  else if (aWidgetType == NS_THEME_PROGRESSCHUNK ||
           aWidgetType == NS_THEME_PROGRESSCHUNK_VERTICAL) {
    DrawThemedProgressMeter(aFrame, aWidgetType, theme, hdc, part, state,
                            &widgetRect, &clipRect);
  }
  else if (aWidgetType == NS_THEME_FOCUS_OUTLINE) {
    // Inflate 'widgetRect' with the focus outline size.
    LayoutDeviceIntMargin border =
      GetWidgetBorder(aFrame->PresContext()->DeviceContext(),
                      aFrame, aWidgetType);
    widgetRect.left -= border.left;
    widgetRect.right += border.right;
    widgetRect.top -= border.top;
    widgetRect.bottom += border.bottom;

    DTBGOPTS opts = {
      sizeof(DTBGOPTS),
      DTBG_OMITCONTENT | DTBG_CLIPRECT,
      clipRect
    };
    DrawThemeBackgroundEx(theme, hdc, part, state, &widgetRect, &opts);
  }
  // If part is negative, the element wishes us to not render a themed
  // background, instead opting to be drawn specially below.
  else if (part >= 0) {
    DrawThemeBackground(theme, hdc, part, state, &widgetRect, &clipRect);
  }

  // Draw focus rectangles for range and scale elements
  // XXX it'd be nice to draw these outside of the frame
  if (aWidgetType == NS_THEME_RANGE ||
      aWidgetType == NS_THEME_SCALE_HORIZONTAL ||
      aWidgetType == NS_THEME_SCALE_VERTICAL) {
      EventStates contentState = GetContentState(aFrame, aWidgetType);

      if (contentState.HasState(NS_EVENT_STATE_FOCUS)) {
        POINT vpOrg;
        HPEN hPen = nullptr;

        uint8_t id = SaveDC(hdc);

        ::SelectClipRgn(hdc, nullptr);
        ::GetViewportOrgEx(hdc, &vpOrg);
        ::SetBrushOrgEx(hdc, vpOrg.x + widgetRect.left, vpOrg.y + widgetRect.top, nullptr);
        ::SetTextColor(hdc, 0);
        ::DrawFocusRect(hdc, &widgetRect);
        ::RestoreDC(hdc, id);
        if (hPen) {
          ::DeleteObject(hPen);
        }
      }
  }
  else if (aWidgetType == NS_THEME_TOOLBAR && state == 0) {
    // Draw toolbar separator lines above all toolbars except the first one.
    // The lines are part of the Rebar theme, which is loaded for NS_THEME_TOOLBOX.
    theme = GetTheme(NS_THEME_TOOLBOX);
    if (!theme)
      return NS_ERROR_FAILURE;

    widgetRect.bottom = widgetRect.top + TB_SEPARATOR_HEIGHT;
    DrawThemeEdge(theme, hdc, RP_BAND, 0, &widgetRect, EDGE_ETCHED, BF_TOP, nullptr);
  }
  else if (aWidgetType == NS_THEME_SCROLLBARTHUMB_HORIZONTAL ||
           aWidgetType == NS_THEME_SCROLLBARTHUMB_VERTICAL)
  {
    // Draw the decorative gripper for the scrollbar thumb button, if it fits

    SIZE gripSize;
    MARGINS thumbMgns;
    int gripPart = (aWidgetType == NS_THEME_SCROLLBARTHUMB_HORIZONTAL) ?
                   SP_GRIPPERHOR : SP_GRIPPERVERT;

    if (GetThemePartSize(theme, hdc, gripPart, state, nullptr, TS_TRUE, &gripSize) == S_OK &&
        GetThemeMargins(theme, hdc, part, state, TMT_CONTENTMARGINS, nullptr, &thumbMgns) == S_OK &&
        gripSize.cx + thumbMgns.cxLeftWidth + thumbMgns.cxRightWidth <= widgetRect.right - widgetRect.left &&
        gripSize.cy + thumbMgns.cyTopHeight + thumbMgns.cyBottomHeight <= widgetRect.bottom - widgetRect.top)
    {
      DrawThemeBackground(theme, hdc, gripPart, state, &widgetRect, &clipRect);
    }
  }

  nativeDrawing.EndNativeDrawing();

  if (nativeDrawing.ShouldRenderAgain())
    goto RENDER_AGAIN;

  nativeDrawing.PaintToContext();

  return NS_OK;
}

static nscolor
GetScrollbarFaceColorForAuto(ComputedStyle* aStyle)
{
  // Do we want to derive from scrollbar-track-color when possible?
  DWORD sysColor = ::GetSysColor(COLOR_SCROLLBAR);
  return NS_RGB(GetRValue(sysColor),
                GetGValue(sysColor),
                GetBValue(sysColor));
}

static nscolor
GetScrollbarTrackColorForAuto(ComputedStyle* aStyle)
{
  // Fallback to background color for now. Do we want to derive from
  // scrollbar-face-color somehow?
  return aStyle->StyleBackground()->BackgroundColor(aStyle);
}

nscolor
nsNativeThemeWin::GetWidgetAutoColor(ComputedStyle* aStyle, uint8_t aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLBAR_VERTICAL:
    case NS_THEME_SCROLLBAR_HORIZONTAL:
    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT:
      return GetScrollbarTrackColorForAuto(aStyle);

    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
      return GetScrollbarFaceColorForAuto(aStyle);

    default:
      return nsITheme::GetWidgetAutoColor(aStyle, aWidgetType);
  }
}

static void
ScaleForFrameDPI(LayoutDeviceIntMargin* aMargin, nsIFrame* aFrame)
{
  double themeScale = GetThemeDpiScaleFactor(aFrame);
  if (themeScale != 1.0) {
    aMargin->top = NSToIntRound(aMargin->top * themeScale);
    aMargin->left = NSToIntRound(aMargin->left * themeScale);
    aMargin->bottom = NSToIntRound(aMargin->bottom * themeScale);
    aMargin->right = NSToIntRound(aMargin->right * themeScale);
  }
}

static void
ScaleForFrameDPI(nsIntMargin* aMargin, nsIFrame* aFrame)
{
  double themeScale = GetThemeDpiScaleFactor(aFrame);
  if (themeScale != 1.0) {
    aMargin->top = NSToIntRound(aMargin->top * themeScale);
    aMargin->left = NSToIntRound(aMargin->left * themeScale);
    aMargin->bottom = NSToIntRound(aMargin->bottom * themeScale);
    aMargin->right = NSToIntRound(aMargin->right * themeScale);
  }
}

static void
ScaleForFrameDPI(LayoutDeviceIntSize* aSize, nsIFrame* aFrame)
{
  double themeScale = GetThemeDpiScaleFactor(aFrame);
  if (themeScale != 1.0) {
    aSize->width = NSToIntRound(aSize->width * themeScale);
    aSize->height = NSToIntRound(aSize->height * themeScale);
  }
}

LayoutDeviceIntMargin
nsNativeThemeWin::GetWidgetBorder(nsDeviceContext* aContext, 
                                  nsIFrame* aFrame,
                                  uint8_t aWidgetType)
{
  LayoutDeviceIntMargin result;
  mozilla::Maybe<nsUXThemeClass> themeClass = GetThemeClass(aWidgetType);
  HTHEME theme = NULL;
  if (!themeClass.isNothing()) {
    theme = nsUXThemeData::GetTheme(themeClass.value());
  }
  if (!theme) {
    result = ClassicGetWidgetBorder(aContext, aFrame, aWidgetType);
    ScaleForFrameDPI(&result, aFrame);
    return result;
  }

  if (!WidgetIsContainer(aWidgetType) ||
      aWidgetType == NS_THEME_TOOLBOX || 
      aWidgetType == NS_THEME_WIN_MEDIA_TOOLBOX ||
      aWidgetType == NS_THEME_WIN_COMMUNICATIONS_TOOLBOX ||
      aWidgetType == NS_THEME_WIN_BROWSERTABBAR_TOOLBOX ||
      aWidgetType == NS_THEME_STATUSBAR || 
      aWidgetType == NS_THEME_RESIZER || aWidgetType == NS_THEME_TABPANEL ||
      aWidgetType == NS_THEME_SCROLLBAR_HORIZONTAL ||
      aWidgetType == NS_THEME_SCROLLBAR_VERTICAL ||
      aWidgetType == NS_THEME_SCROLLCORNER ||
      aWidgetType == NS_THEME_MENUITEM || aWidgetType == NS_THEME_CHECKMENUITEM ||
      aWidgetType == NS_THEME_RADIOMENUITEM || aWidgetType == NS_THEME_MENUPOPUP ||
      aWidgetType == NS_THEME_MENUIMAGE || aWidgetType == NS_THEME_MENUITEMTEXT ||
      aWidgetType == NS_THEME_SEPARATOR ||
      aWidgetType == NS_THEME_WINDOW_TITLEBAR ||
      aWidgetType == NS_THEME_WINDOW_TITLEBAR_MAXIMIZED ||
      aWidgetType == NS_THEME_WIN_GLASS || aWidgetType == NS_THEME_WIN_BORDERLESS_GLASS)
    return result; // Don't worry about it.

  int32_t part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return result;

  if (aWidgetType == NS_THEME_TOOLBAR) {
    // make space for the separator line above all toolbars but the first
    if (state == 0)
      result.top = TB_SEPARATOR_HEIGHT;
    return result;
  }

  result = GetCachedWidgetBorder(theme, themeClass.value(), aWidgetType, part, state);

  // Remove the edges for tabs that are before or after the selected tab,
  if (aWidgetType == NS_THEME_TAB) {
    if (IsLeftToSelectedTab(aFrame))
      // Remove the right edge, since we won't be drawing it.
      result.right = 0;
    else if (IsRightToSelectedTab(aFrame))
      // Remove the left edge, since we won't be drawing it.
      result.left = 0;
  }

  if (aFrame && (aWidgetType == NS_THEME_NUMBER_INPUT ||
                 aWidgetType == NS_THEME_TEXTFIELD ||
                 aWidgetType == NS_THEME_TEXTFIELD_MULTILINE)) {
    nsIContent* content = aFrame->GetContent();
    if (content && content->IsHTMLElement()) {
      // We need to pad textfields by 1 pixel, since the caret will draw
      // flush against the edge by default if we don't.
      result.top++;
      result.left++;
      result.bottom++;
      result.right++;
    }
  }

  ScaleForFrameDPI(&result, aFrame);
  return result;
}

bool
nsNativeThemeWin::GetWidgetPadding(nsDeviceContext* aContext, 
                                   nsIFrame* aFrame,
                                   uint8_t aWidgetType,
                                   LayoutDeviceIntMargin* aResult)
{
  switch (aWidgetType) {
    // Radios and checkboxes return a fixed size in GetMinimumWidgetSize
    // and have a meaningful baseline, so they can't have
    // author-specified padding.
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
      aResult->SizeTo(0, 0, 0, 0);
      return true;
  }

  bool ok = true;

  if (aWidgetType == NS_THEME_WINDOW_BUTTON_BOX ||
      aWidgetType == NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED) {
    aResult->SizeTo(0, 0, 0, 0);

    // aero glass doesn't display custom buttons
    if (nsUXThemeData::CheckForCompositor())
      return true;

    // button padding for standard windows
    if (aWidgetType == NS_THEME_WINDOW_BUTTON_BOX) {
      aResult->top = GetSystemMetrics(SM_CXFRAME);
    }
    ScaleForFrameDPI(aResult, aFrame);
    return ok;
  }

  // Content padding
  if (aWidgetType == NS_THEME_WINDOW_TITLEBAR ||
      aWidgetType == NS_THEME_WINDOW_TITLEBAR_MAXIMIZED) {
    aResult->SizeTo(0, 0, 0, 0);
    // XXX Maximized windows have an offscreen offset equal to
    // the border padding. This should be addressed in nsWindow,
    // but currently can't be, see UpdateNonClientMargins.
    if (aWidgetType == NS_THEME_WINDOW_TITLEBAR_MAXIMIZED)
      aResult->top = GetSystemMetrics(SM_CXFRAME)
                   + GetSystemMetrics(SM_CXPADDEDBORDER);
    return ok;
  }

  HANDLE theme = GetTheme(aWidgetType);
  if (!theme) {
    ok = ClassicGetWidgetPadding(aContext, aFrame, aWidgetType, aResult);
    ScaleForFrameDPI(aResult, aFrame);
    return ok;
  }

  if (aWidgetType == NS_THEME_MENUPOPUP)
  {
    SIZE popupSize;
    GetThemePartSize(theme, nullptr, MENU_POPUPBORDERS, /* state */ 0, nullptr, TS_TRUE, &popupSize);
    aResult->top = aResult->bottom = popupSize.cy;
    aResult->left = aResult->right = popupSize.cx;
    ScaleForFrameDPI(aResult, aFrame);
    return ok;
  }

  if (aWidgetType == NS_THEME_NUMBER_INPUT ||
      aWidgetType == NS_THEME_TEXTFIELD ||
      aWidgetType == NS_THEME_TEXTFIELD_MULTILINE ||
      aWidgetType == NS_THEME_MENULIST)
  {
    // If we have author-specified padding for these elements, don't do the
    // fixups below.
    if (aFrame->PresContext()->HasAuthorSpecifiedRules(aFrame, NS_AUTHOR_SPECIFIED_PADDING))
      return false;
  }

  /* textfields need extra pixels on all sides, otherwise they wrap their
   * content too tightly.  The actual border is drawn 1px inside the specified
   * rectangle, so Gecko will end up making the contents look too small.
   * Instead, we add 2px padding for the contents and fix this. (Used to be 1px
   * added, see bug 430212)
   */
  if (aWidgetType == NS_THEME_NUMBER_INPUT ||
      aWidgetType == NS_THEME_TEXTFIELD ||
      aWidgetType == NS_THEME_TEXTFIELD_MULTILINE) {
    aResult->top = aResult->bottom = 2;
    aResult->left = aResult->right = 2;
    ScaleForFrameDPI(aResult, aFrame);
    return ok;
  } else if (IsHTMLContent(aFrame) && aWidgetType == NS_THEME_MENULIST) {
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
  switch (aWidgetType)
  {
    case NS_THEME_MENUIMAGE:
        right = 8;
        left = 3;
        break;
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
        right = 8;
        left = 0;
        break;
    case NS_THEME_MENUITEMTEXT:
        // There seem to be exactly 4 pixels from the edge
        // of the gutter to the text: 2px margin (CSS) + 2px padding (here)
        {
          SIZE size(GetGutterSize(theme, nullptr));
          left = size.cx + 2;
        }
        break;
    case NS_THEME_MENUSEPARATOR:
        {
          SIZE size(GetGutterSize(theme, nullptr));
          left = size.cx + 5;
          top = 10;
          bottom = 7;
        }
        break;
    default:
        return false;
  }

  if (IsFrameRTL(aFrame))
  {
    aResult->right = left;
    aResult->left = right;
  }
  else
  {
    aResult->right = right;
    aResult->left = left;
  }

  ScaleForFrameDPI(aResult, aFrame);
  return ok;
}

bool
nsNativeThemeWin::GetWidgetOverflow(nsDeviceContext* aContext, 
                                    nsIFrame* aFrame,
                                    uint8_t aWidgetType,
                                    nsRect* aOverflowRect)
{
  /* This is disabled for now, because it causes invalidation problems --
   * see bug 420381.  The effect of not updating the overflow area is that
   * for dropdown buttons in content areas, there is a 1px border on 3 sides
   * where, if invalidated, the dropdown control probably won't be repainted.
   * This is fairly minor, as by default there is nothing in that area, and
   * a border only shows up if the widget is being hovered.
   */
#if 0
  /* We explicitly draw dropdown buttons in HTML content 1px bigger up, right,
   * and bottom so that they overlap the dropdown's border like they're
   * supposed to.
   */
  if (aWidgetType == NS_THEME_MENULIST_BUTTON &&
      IsHTMLContent(aFrame) &&
      !IsWidgetStyled(aFrame->GetParent()->PresContext(),
                      aFrame->GetParent(),
                      NS_THEME_MENULIST))
  {
    int32_t p2a = aContext->AppUnitsPerDevPixel();
    /* Note: no overflow on the left */
    nsMargin m(p2a, p2a, p2a, 0);
    aOverflowRect->Inflate (m);
    return true;
  }
#endif

  if (aWidgetType == NS_THEME_FOCUS_OUTLINE) {
    LayoutDeviceIntMargin border =
      GetWidgetBorder(aContext, aFrame, aWidgetType);
    int32_t p2a = aContext->AppUnitsPerDevPixel();
    nsMargin m(NSIntPixelsToAppUnits(border.top, p2a),
               NSIntPixelsToAppUnits(border.right, p2a),
               NSIntPixelsToAppUnits(border.bottom, p2a),
               NSIntPixelsToAppUnits(border.left, p2a));
    aOverflowRect->Inflate(m);
    return true;
  }

  return false;
}

NS_IMETHODIMP
nsNativeThemeWin::GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame* aFrame,
                                       uint8_t aWidgetType,
                                       LayoutDeviceIntSize* aResult, bool* aIsOverridable)
{
  aResult->width = aResult->height = 0;
  *aIsOverridable = true;
  nsresult rv = NS_OK;

  mozilla::Maybe<nsUXThemeClass> themeClass = GetThemeClass(aWidgetType);
  HTHEME theme = NULL;
  if (!themeClass.isNothing()) {
    theme = nsUXThemeData::GetTheme(themeClass.value());
  }
  if (!theme) {
    rv = ClassicGetMinimumWidgetSize(aFrame, aWidgetType, aResult, aIsOverridable);
    ScaleForFrameDPI(aResult, aFrame);
    return rv;
  }

  switch (aWidgetType) {
    case NS_THEME_GROUPBOX:
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TOOLBOX:
    case NS_THEME_WIN_MEDIA_TOOLBOX:
    case NS_THEME_WIN_COMMUNICATIONS_TOOLBOX:
    case NS_THEME_WIN_BROWSERTABBAR_TOOLBOX:
    case NS_THEME_TOOLBAR:
    case NS_THEME_STATUSBAR:
    case NS_THEME_PROGRESSCHUNK:
    case NS_THEME_PROGRESSCHUNK_VERTICAL:
    case NS_THEME_TABPANELS:
    case NS_THEME_TABPANEL:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_MENUITEMTEXT:
    case NS_THEME_WIN_GLASS:
    case NS_THEME_WIN_BORDERLESS_GLASS:
      return NS_OK; // Don't worry about it.
  }

  if (aWidgetType == NS_THEME_MENUITEM && IsTopLevelMenu(aFrame))
      return NS_OK; // Don't worry about it for top level menus

  // Call GetSystemMetrics to determine size for WinXP scrollbars
  // (GetThemeSysSize API returns the optimal size for the theme, but 
  //  Windows appears to always use metrics when drawing standard scrollbars)
  THEMESIZE sizeReq = TS_TRUE; // Best-fit size
  switch (aWidgetType) {
    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_HORIZONTAL:
    case NS_THEME_SCROLLBAR_VERTICAL:
    case NS_THEME_MENULIST_BUTTON: {
      rv = ClassicGetMinimumWidgetSize(aFrame, aWidgetType, aResult, aIsOverridable);
      ScaleForFrameDPI(aResult, aFrame);
      return rv;
    }
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
      if(!IsTopLevelMenu(aFrame))
      {
        SIZE gutterSize(GetCachedGutterSize(theme));
        aResult->width = gutterSize.cx;
        aResult->height = gutterSize.cy;
        ScaleForFrameDPI(aResult, aFrame);
        return rv;
      }
      break;

    case NS_THEME_MENUIMAGE:
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
      {
        SIZE boxSize(GetCachedGutterSize(theme));
        aResult->width = boxSize.cx+2;
        aResult->height = boxSize.cy;
        *aIsOverridable = false;
        ScaleForFrameDPI(aResult, aFrame);
        return rv;
      }

    case NS_THEME_MENUITEMTEXT:
      return NS_OK;

    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
      // Best-fit size for progress meters is too large for most 
      // themes. We want these widgets to be able to really shrink
      // down, so use the min-size request value (of 0).
      sizeReq = TS_MIN; 
      break;

    case NS_THEME_RESIZER:
      *aIsOverridable = false;
      break;

    case NS_THEME_RANGE_THUMB:
    case NS_THEME_SCALETHUMB_HORIZONTAL:
    case NS_THEME_SCALETHUMB_VERTICAL:
    {
      *aIsOverridable = false;
      // On Vista, GetThemePartAndState returns odd values for
      // scale thumbs, so use a hardcoded size instead.
      if (aWidgetType == NS_THEME_SCALETHUMB_HORIZONTAL ||
          (aWidgetType == NS_THEME_RANGE_THUMB && IsRangeHorizontal(aFrame))) {
        aResult->width = 12;
        aResult->height = 20;
      } else {
        aResult->width = 20;
        aResult->height = 12;
      }
      ScaleForFrameDPI(aResult, aFrame);
      return rv;
    }

    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLCORNER:
    {
      if (nsLookAndFeel::GetInt(
            nsLookAndFeel::eIntID_UseOverlayScrollbars) != 0) {
        aResult->SizeTo(::GetSystemMetrics(SM_CXHSCROLL),
                        ::GetSystemMetrics(SM_CYVSCROLL));
        ScaleForFrameDPI(aResult, aFrame);
        return rv;
      }
      break;
    }

    case NS_THEME_SEPARATOR:
      // that's 2px left margin, 2px right margin and 2px separator
      // (the margin is drawn as part of the separator, though)
      aResult->width = 6;
      ScaleForFrameDPI(aResult, aFrame);
      return rv;

    case NS_THEME_BUTTON:
      // We should let HTML buttons shrink to their min size.
      // FIXME bug 403934: We should probably really separate
      // GetPreferredWidgetSize from GetMinimumWidgetSize, so callers can
      // use the one they want.
      if (aFrame->GetContent()->IsHTMLElement()) {
        sizeReq = TS_MIN;
      }
      break;

    case NS_THEME_WINDOW_BUTTON_MAXIMIZE:
    case NS_THEME_WINDOW_BUTTON_RESTORE:
      // The only way to get accurate titlebar button info is to query a
      // window w/buttons when it's visible. nsWindow takes care of this and
      // stores that info in nsUXThemeData.
      aResult->width = nsUXThemeData::GetCommandButtonMetrics(CMDBUTTONIDX_RESTORE).cx;
      aResult->height = nsUXThemeData::GetCommandButtonMetrics(CMDBUTTONIDX_RESTORE).cy;
      AddPaddingRect(aResult, CAPTIONBUTTON_RESTORE);
      *aIsOverridable = false;
      return rv;

    case NS_THEME_WINDOW_BUTTON_MINIMIZE:
      aResult->width = nsUXThemeData::GetCommandButtonMetrics(CMDBUTTONIDX_MINIMIZE).cx;
      aResult->height = nsUXThemeData::GetCommandButtonMetrics(CMDBUTTONIDX_MINIMIZE).cy;
      AddPaddingRect(aResult, CAPTIONBUTTON_MINIMIZE);
      *aIsOverridable = false;
      return rv;

    case NS_THEME_WINDOW_BUTTON_CLOSE:
      aResult->width = nsUXThemeData::GetCommandButtonMetrics(CMDBUTTONIDX_CLOSE).cx;
      aResult->height = nsUXThemeData::GetCommandButtonMetrics(CMDBUTTONIDX_CLOSE).cy;
      AddPaddingRect(aResult, CAPTIONBUTTON_CLOSE);
      *aIsOverridable = false;
      return rv;

    case NS_THEME_WINDOW_TITLEBAR:
    case NS_THEME_WINDOW_TITLEBAR_MAXIMIZED:
      aResult->height = GetSystemMetrics(SM_CYCAPTION);
      aResult->height += GetSystemMetrics(SM_CYFRAME);
      aResult->height += GetSystemMetrics(SM_CXPADDEDBORDER);
      // On Win8.1, we don't want this scaling, because Windows doesn't scale
      // the non-client area of the window, and we can end up with ugly overlap
      // of the window frame controls into the tab bar or content area. But on
      // Win10, we render the window controls ourselves, and the result looks
      // better if we do apply this scaling (particularly with themes such as
      // DevEdition; see bug 1267636).
      if (IsWin10OrLater()) {
        ScaleForFrameDPI(aResult, aFrame);
      }
      *aIsOverridable = false;
      return rv;

    case NS_THEME_WINDOW_BUTTON_BOX:
    case NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED:
      if (nsUXThemeData::CheckForCompositor()) {
        aResult->width = nsUXThemeData::GetCommandButtonBoxMetrics().cx;
        aResult->height = nsUXThemeData::GetCommandButtonBoxMetrics().cy
                          - GetSystemMetrics(SM_CYFRAME)
                          - GetSystemMetrics(SM_CXPADDEDBORDER);
        if (aWidgetType == NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED) {
          aResult->width += 1;
          aResult->height -= 2;
        }
        *aIsOverridable = false;
        return rv;
      }
      break;

    case NS_THEME_WINDOW_FRAME_LEFT:
    case NS_THEME_WINDOW_FRAME_RIGHT:
    case NS_THEME_WINDOW_FRAME_BOTTOM:
      aResult->width = GetSystemMetrics(SM_CXFRAME);
      aResult->height = GetSystemMetrics(SM_CYFRAME);
      *aIsOverridable = false;
      return rv;
  }

  int32_t part, state;
  rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return rv;

  rv = GetCachedMinimumWidgetSize(aFrame, theme, themeClass.value(), aWidgetType, part,
                                  state, sizeReq, aResult);

  ScaleForFrameDPI(aResult, aFrame);
  return rv;
}

NS_IMETHODIMP
nsNativeThemeWin::WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType, 
                                     nsAtom* aAttribute, bool* aShouldRepaint,
                                     const nsAttrValue* aOldValue)
{
  // Some widget types just never change state.
  if (aWidgetType == NS_THEME_TOOLBOX ||
      aWidgetType == NS_THEME_WIN_MEDIA_TOOLBOX ||
      aWidgetType == NS_THEME_WIN_COMMUNICATIONS_TOOLBOX ||
      aWidgetType == NS_THEME_WIN_BROWSERTABBAR_TOOLBOX ||
      aWidgetType == NS_THEME_TOOLBAR ||
      aWidgetType == NS_THEME_STATUSBAR || aWidgetType == NS_THEME_STATUSBARPANEL ||
      aWidgetType == NS_THEME_RESIZERPANEL ||
      aWidgetType == NS_THEME_PROGRESSCHUNK ||
      aWidgetType == NS_THEME_PROGRESSCHUNK_VERTICAL ||
      aWidgetType == NS_THEME_PROGRESSBAR ||
      aWidgetType == NS_THEME_PROGRESSBAR_VERTICAL ||
      aWidgetType == NS_THEME_TOOLTIP ||
      aWidgetType == NS_THEME_TABPANELS ||
      aWidgetType == NS_THEME_TABPANEL ||
      aWidgetType == NS_THEME_SEPARATOR ||
      aWidgetType == NS_THEME_WIN_GLASS ||
      aWidgetType == NS_THEME_WIN_BORDERLESS_GLASS) {
    *aShouldRepaint = false;
    return NS_OK;
  }

  if (aWidgetType == NS_THEME_WINDOW_TITLEBAR ||
      aWidgetType == NS_THEME_WINDOW_TITLEBAR_MAXIMIZED ||
      aWidgetType == NS_THEME_WINDOW_FRAME_LEFT ||
      aWidgetType == NS_THEME_WINDOW_FRAME_RIGHT ||
      aWidgetType == NS_THEME_WINDOW_FRAME_BOTTOM ||
      aWidgetType == NS_THEME_WINDOW_BUTTON_CLOSE ||
      aWidgetType == NS_THEME_WINDOW_BUTTON_MINIMIZE ||
      aWidgetType == NS_THEME_WINDOW_BUTTON_MAXIMIZE ||
      aWidgetType == NS_THEME_WINDOW_BUTTON_RESTORE) {
    *aShouldRepaint = true;
    return NS_OK;
  }

  // We need to repaint the dropdown arrow in vista HTML combobox controls when
  // the control is closed to get rid of the hover effect.
  if ((aWidgetType == NS_THEME_MENULIST || aWidgetType == NS_THEME_MENULIST_BUTTON) &&
      IsHTMLContent(aFrame))
  {
    *aShouldRepaint = true;
    return NS_OK;
  }

  // XXXdwh Not sure what can really be done here.  Can at least guess for
  // specific widgets that they're highly unlikely to have certain states.
  // For example, a toolbar doesn't care about any states.
  if (!aAttribute) {
    // Hover/focus/active changed.  Always repaint.
    *aShouldRepaint = true;
  }
  else {
    // Check the attribute to see if it's relevant.  
    // disabled, checked, dlgtype, default, etc.
    *aShouldRepaint = false;
    if (aAttribute == nsGkAtoms::disabled ||
        aAttribute == nsGkAtoms::checked ||
        aAttribute == nsGkAtoms::selected ||
        aAttribute == nsGkAtoms::visuallyselected ||
        aAttribute == nsGkAtoms::readonly ||
        aAttribute == nsGkAtoms::open ||
        aAttribute == nsGkAtoms::menuactive ||
        aAttribute == nsGkAtoms::focused)
      *aShouldRepaint = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeWin::ThemeChanged()
{
  nsUXThemeData::Invalidate();
  memset(mBorderCacheValid, 0, sizeof(mBorderCacheValid));
  memset(mMinimumWidgetSizeCacheValid, 0, sizeof(mMinimumWidgetSizeCacheValid));
  mGutterSizeCacheValid = false;
  return NS_OK;
}

bool 
nsNativeThemeWin::ThemeSupportsWidget(nsPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      uint8_t aWidgetType)
{
  // XXXdwh We can go even further and call the API to ask if support exists for
  // specific widgets.

  if (aWidgetType == NS_THEME_FOCUS_OUTLINE) {
    return true;
  }

  HANDLE theme = nullptr;
  if (aWidgetType == NS_THEME_CHECKBOX_CONTAINER)
    theme = GetTheme(NS_THEME_CHECKBOX);
  else if (aWidgetType == NS_THEME_RADIO_CONTAINER)
    theme = GetTheme(NS_THEME_RADIO);
  else
    theme = GetTheme(aWidgetType);

  if (theme && aWidgetType == NS_THEME_RESIZER)
    return true;

  if ((theme) || (!theme && ClassicThemeSupportsWidget(aFrame, aWidgetType)))
    // turn off theming for some HTML widgets styled by the page
    return (!IsWidgetStyled(aPresContext, aFrame, aWidgetType));

  return false;
}

bool 
nsNativeThemeWin::WidgetIsContainer(uint8_t aWidgetType)
{
  // XXXdwh At some point flesh all of this out.
  if (aWidgetType == NS_THEME_MENULIST_BUTTON || 
      aWidgetType == NS_THEME_RADIO ||
      aWidgetType == NS_THEME_CHECKBOX)
    return false;
  return true;
}

bool
nsNativeThemeWin::ThemeDrawsFocusForWidget(uint8_t aWidgetType)
{
  return false;
}

bool
nsNativeThemeWin::ThemeNeedsComboboxDropmarker()
{
  return true;
}

bool
nsNativeThemeWin::WidgetAppearanceDependsOnWindowFocus(uint8_t aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_WINDOW_TITLEBAR:
    case NS_THEME_WINDOW_TITLEBAR_MAXIMIZED:
    case NS_THEME_WINDOW_FRAME_LEFT:
    case NS_THEME_WINDOW_FRAME_RIGHT:
    case NS_THEME_WINDOW_FRAME_BOTTOM:
    case NS_THEME_WINDOW_BUTTON_CLOSE:
    case NS_THEME_WINDOW_BUTTON_MINIMIZE:
    case NS_THEME_WINDOW_BUTTON_MAXIMIZE:
    case NS_THEME_WINDOW_BUTTON_RESTORE:
      return true;
    default:
      return false;
  }
}

nsITheme::ThemeGeometryType
nsNativeThemeWin::ThemeGeometryTypeForWidget(nsIFrame* aFrame,
                                             uint8_t aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_WINDOW_BUTTON_BOX:
    case NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED:
      return eThemeGeometryTypeWindowButtons;
    default:
      return eThemeGeometryTypeUnknown;
  }
}

nsITheme::Transparency
nsNativeThemeWin::GetWidgetTransparency(nsIFrame* aFrame, uint8_t aWidgetType)
{
  switch (aWidgetType) {
  case NS_THEME_SCROLLBAR_SMALL:
  case NS_THEME_SCROLLBAR:
  case NS_THEME_SCROLLCORNER:
  case NS_THEME_STATUSBAR:
    // Knowing that scrollbars and statusbars are opaque improves
    // performance, because we create layers for them. This better be
    // true across all Windows themes! If it's not true, we should
    // paint an opaque background for them to make it true!
    return eOpaque;
  case NS_THEME_WIN_GLASS:
  case NS_THEME_WIN_BORDERLESS_GLASS:
  case NS_THEME_SCALE_HORIZONTAL:
  case NS_THEME_SCALE_VERTICAL:
  case NS_THEME_PROGRESSBAR:
  case NS_THEME_PROGRESSBAR_VERTICAL:
  case NS_THEME_PROGRESSCHUNK:
  case NS_THEME_PROGRESSCHUNK_VERTICAL:
  case NS_THEME_RANGE:
    return eTransparent;
  }

  HANDLE theme = GetTheme(aWidgetType);
  // For the classic theme we don't really have a way of knowing
  if (!theme) {
    // menu backgrounds and tooltips which can't be themed are opaque
    if (aWidgetType == NS_THEME_MENUPOPUP || aWidgetType == NS_THEME_TOOLTIP) {
      return eOpaque;
    }
    return eUnknownTransparency;
  }

  int32_t part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
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

bool
nsNativeThemeWin::ClassicThemeSupportsWidget(nsIFrame* aFrame,
                                             uint8_t aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_RESIZER:
    {
      // The classic native resizer has an opaque grey background which doesn't
      // match the usually white background of the scrollable container, so
      // only support the native resizer if not in a scrollframe.
      nsIFrame* parentFrame = aFrame->GetParent();
      return !parentFrame || !parentFrame->IsScrollFrame();
    }
    case NS_THEME_MENUBAR:
    case NS_THEME_MENUPOPUP:
      // Classic non-flat menus are handled almost entirely through CSS.
      if (!nsUXThemeData::sFlatMenus)
        return false;
    case NS_THEME_BUTTON:
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
    case NS_THEME_RANGE:
    case NS_THEME_RANGE_THUMB:
    case NS_THEME_GROUPBOX:
    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT:
    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_VERTICAL:
    case NS_THEME_SCROLLBAR_HORIZONTAL:
    case NS_THEME_SCROLLBAR_NON_DISAPPEARING:
    case NS_THEME_SCROLLCORNER:
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALETHUMB_HORIZONTAL:
    case NS_THEME_SCALETHUMB_VERTICAL:
    case NS_THEME_MENULIST_BUTTON:
    case NS_THEME_INNER_SPIN_BUTTON:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_MENULIST_TEXTFIELD:
    case NS_THEME_MENULIST:
    case NS_THEME_TOOLTIP:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBARPANEL:
    case NS_THEME_RESIZERPANEL:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSCHUNK:
    case NS_THEME_PROGRESSCHUNK_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TABPANEL:
    case NS_THEME_TABPANELS:
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
    case NS_THEME_MENUARROW:
    case NS_THEME_MENUSEPARATOR:
    case NS_THEME_MENUITEMTEXT:
    case NS_THEME_WINDOW_TITLEBAR:
    case NS_THEME_WINDOW_TITLEBAR_MAXIMIZED:
    case NS_THEME_WINDOW_FRAME_LEFT:
    case NS_THEME_WINDOW_FRAME_RIGHT:
    case NS_THEME_WINDOW_FRAME_BOTTOM:
    case NS_THEME_WINDOW_BUTTON_CLOSE:
    case NS_THEME_WINDOW_BUTTON_MINIMIZE:
    case NS_THEME_WINDOW_BUTTON_MAXIMIZE:
    case NS_THEME_WINDOW_BUTTON_RESTORE:
    case NS_THEME_WINDOW_BUTTON_BOX:
    case NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED:
      return true;
  }
  return false;
}

LayoutDeviceIntMargin
nsNativeThemeWin::ClassicGetWidgetBorder(nsDeviceContext* aContext, 
                                         nsIFrame* aFrame,
                                         uint8_t aWidgetType)
{
  LayoutDeviceIntMargin result;
  switch (aWidgetType) {
    case NS_THEME_GROUPBOX:
    case NS_THEME_BUTTON:
      result.top = result.left = result.bottom = result.right = 2;
      break;
    case NS_THEME_STATUSBAR:
      result.bottom = result.left = result.right = 0;
      result.top = 2;
      break;
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_MENULIST:
    case NS_THEME_MENULIST_TEXTFIELD:
    case NS_THEME_TAB:
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_FOCUS_OUTLINE:
      result.top = result.left = result.bottom = result.right = 2;
      break;
    case NS_THEME_STATUSBARPANEL:
    case NS_THEME_RESIZERPANEL: {
      result.top = 1;
      result.left = 1;
      result.bottom = 1;
      result.right = aFrame->GetNextSibling() ? 3 : 1;
      break;
    }
    case NS_THEME_TOOLTIP:
      result.top = result.left = result.bottom = result.right = 1;
      break;
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
      result.top = result.left = result.bottom = result.right = 1;
      break;
    case NS_THEME_MENUBAR:
      result.top = result.left = result.bottom = result.right = 0;
      break;
    case NS_THEME_MENUPOPUP:
      result.top = result.left = result.bottom = result.right = 3;
      break;
    default:
      result.top = result.bottom = result.left = result.right = 0;
      break;
  }
  return result;
}

bool
nsNativeThemeWin::ClassicGetWidgetPadding(nsDeviceContext* aContext,
                                   nsIFrame* aFrame,
                                   uint8_t aWidgetType,
                                   LayoutDeviceIntMargin* aResult)
{
  switch (aWidgetType) {
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM: {
      int32_t part, state;
      bool focused;

      if (NS_FAILED(ClassicGetThemePartAndState(aFrame, aWidgetType, part, state, focused)))
        return false;

      if (part == 1) { // top-level menu
        if (nsUXThemeData::sFlatMenus || !(state & DFCS_PUSHED)) {
          (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right = 2;
        }
        else {
          // make top-level menus look sunken when pushed in the Classic look
          (*aResult).top = (*aResult).left = 3;
          (*aResult).bottom = (*aResult).right = 1;
        }
      }
      else {
        (*aResult).top = 0;
        (*aResult).bottom = (*aResult).left = (*aResult).right = 2;
      }
      return true;
    }
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 1;
      return true;
    default:
      return false;
  }
}

nsresult
nsNativeThemeWin::ClassicGetMinimumWidgetSize(nsIFrame* aFrame,
                                              uint8_t aWidgetType,
                                              LayoutDeviceIntSize* aResult,
                                              bool* aIsOverridable)
{
  (*aResult).width = (*aResult).height = 0;
  *aIsOverridable = true;
  switch (aWidgetType) {
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX:
      (*aResult).width = (*aResult).height = 13;
      break;
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
    case NS_THEME_MENUARROW:
      (*aResult).width = ::GetSystemMetrics(SM_CXMENUCHECK);
      (*aResult).height = ::GetSystemMetrics(SM_CYMENUCHECK);
      break;
    case NS_THEME_INNER_SPIN_BUTTON:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON:
      (*aResult).width = ::GetSystemMetrics(SM_CXVSCROLL);
      (*aResult).height = 8; // No good metrics available for this
      *aIsOverridable = false;
      break;
    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
      (*aResult).width = ::GetSystemMetrics(SM_CXVSCROLL);
      (*aResult).height = ::GetSystemMetrics(SM_CYVSCROLL);
      *aIsOverridable = false;
      break;
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT:
      (*aResult).width = ::GetSystemMetrics(SM_CXHSCROLL);
      (*aResult).height = ::GetSystemMetrics(SM_CYHSCROLL);
      *aIsOverridable = false;
      break;
    case NS_THEME_SCROLLBAR_VERTICAL:
      // XXX HACK We should be able to have a minimum height for the scrollbar
      // track.  However, this causes problems when uncollapsing a scrollbar
      // inside a tree.  See bug 201379 for details.

        //      (*aResult).height = ::GetSystemMetrics(SM_CYVTHUMB) << 1;
      break;
    case NS_THEME_SCROLLBAR_NON_DISAPPEARING:
    {
      aResult->SizeTo(::GetSystemMetrics(SM_CXHSCROLL),
                      ::GetSystemMetrics(SM_CYVSCROLL));
      break;
    }
    case NS_THEME_RANGE_THUMB: {
      if (IsRangeHorizontal(aFrame)) {
        (*aResult).width = 12;
        (*aResult).height = 20;
      } else {
        (*aResult).width = 20;
        (*aResult).height = 12;
      }
      *aIsOverridable = false;
      break;
    }
    case NS_THEME_SCALETHUMB_HORIZONTAL:
      (*aResult).width = 12;
      (*aResult).height = 20;
      *aIsOverridable = false;
      break;
    case NS_THEME_SCALETHUMB_VERTICAL:
      (*aResult).width = 20;
      (*aResult).height = 12;
      *aIsOverridable = false;
      break;
    case NS_THEME_MENULIST_BUTTON:
      (*aResult).width = ::GetSystemMetrics(SM_CXVSCROLL);
      break;
    case NS_THEME_MENULIST:
    case NS_THEME_BUTTON:
    case NS_THEME_GROUPBOX:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_MENULIST_TEXTFIELD:      
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBARPANEL:      
    case NS_THEME_RESIZERPANEL:
    case NS_THEME_PROGRESSCHUNK:
    case NS_THEME_PROGRESSCHUNK_VERTICAL:
    case NS_THEME_TOOLTIP:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TABPANEL:
    case NS_THEME_TABPANELS:
      // no minimum widget size
      break;
    case NS_THEME_RESIZER: {     
      NONCLIENTMETRICS nc;
      nc.cbSize = sizeof(nc);
      if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(nc), &nc, 0))
        (*aResult).width = (*aResult).height = abs(nc.lfStatusFont.lfHeight) + 4;
      else
        (*aResult).width = (*aResult).height = 15;
      *aIsOverridable = false;
      break;
    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
      (*aResult).width = ::GetSystemMetrics(SM_CXVSCROLL);
      (*aResult).height = ::GetSystemMetrics(SM_CYVTHUMB);
      // Without theming, divide the thumb size by two in order to look more
      // native
      if (!GetTheme(aWidgetType))
        (*aResult).height >>= 1;
      *aIsOverridable = false;
      break;
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
      (*aResult).width = ::GetSystemMetrics(SM_CXHTHUMB);
      (*aResult).height = ::GetSystemMetrics(SM_CYHSCROLL);
      // Without theming, divide the thumb size by two in order to look more
      // native
      if (!GetTheme(aWidgetType))
        (*aResult).width >>= 1;
      *aIsOverridable = false;
      break;
    case NS_THEME_SCROLLBAR_HORIZONTAL:
      (*aResult).width = ::GetSystemMetrics(SM_CXHTHUMB) << 1;
      break;
    }
    case NS_THEME_MENUSEPARATOR:
    {
      aResult->width = 0;
      aResult->height = 10;
      break;
    }

    case NS_THEME_WINDOW_TITLEBAR_MAXIMIZED:
    case NS_THEME_WINDOW_TITLEBAR:
      aResult->height = GetSystemMetrics(SM_CYCAPTION);
      aResult->height += GetSystemMetrics(SM_CYFRAME);
      aResult->width = 0;
    break;
    case NS_THEME_WINDOW_FRAME_LEFT:
    case NS_THEME_WINDOW_FRAME_RIGHT:
      aResult->width = GetSystemMetrics(SM_CXFRAME);
      aResult->height = 0;
    break;

    case NS_THEME_WINDOW_FRAME_BOTTOM:
      aResult->height = GetSystemMetrics(SM_CYFRAME);
      aResult->width = 0;
    break;

    case NS_THEME_WINDOW_BUTTON_CLOSE:
    case NS_THEME_WINDOW_BUTTON_MINIMIZE:
    case NS_THEME_WINDOW_BUTTON_MAXIMIZE:
    case NS_THEME_WINDOW_BUTTON_RESTORE:
      aResult->width = GetSystemMetrics(SM_CXSIZE);
      aResult->height = GetSystemMetrics(SM_CYSIZE);
      // XXX I have no idea why these caption metrics are always off,
      // but they are.
      aResult->width -= 2;
      aResult->height -= 4;
      if (aWidgetType == NS_THEME_WINDOW_BUTTON_MINIMIZE) {
        AddPaddingRect(aResult, CAPTIONBUTTON_MINIMIZE);
      }
      else if (aWidgetType == NS_THEME_WINDOW_BUTTON_MAXIMIZE ||
               aWidgetType == NS_THEME_WINDOW_BUTTON_RESTORE) {
        AddPaddingRect(aResult, CAPTIONBUTTON_RESTORE);
      }
      else if (aWidgetType == NS_THEME_WINDOW_BUTTON_CLOSE) {
        AddPaddingRect(aResult, CAPTIONBUTTON_CLOSE);
      }
    break;

    default:
      return NS_ERROR_FAILURE;
  }  
  return NS_OK;
}


nsresult nsNativeThemeWin::ClassicGetThemePartAndState(nsIFrame* aFrame, uint8_t aWidgetType,
                                 int32_t& aPart, int32_t& aState, bool& aFocused)
{  
  aFocused = false;
  switch (aWidgetType) {
    case NS_THEME_BUTTON: {
      EventStates contentState;

      aPart = DFC_BUTTON;
      aState = DFCS_BUTTONPUSH;
      aFocused = false;

      contentState = GetContentState(aFrame, aWidgetType);
      if (IsDisabled(aFrame, contentState))
        aState |= DFCS_INACTIVE;
      else if (IsOpenButton(aFrame))
        aState |= DFCS_PUSHED;
      else if (IsCheckedButton(aFrame))
        aState |= DFCS_CHECKED;
      else {
        if (contentState.HasAllStates(NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_HOVER)) {
          aState |= DFCS_PUSHED;
          const nsStyleUserInterface *uiData = aFrame->StyleUserInterface();
          // The down state is flat if the button is focusable
          if (uiData->mUserFocus == StyleUserFocus::Normal) {
            if (!aFrame->GetContent()->IsHTMLElement())
              aState |= DFCS_FLAT;

            aFocused = true;
          }
        }
        if (contentState.HasState(NS_EVENT_STATE_FOCUS) ||
            (aState == DFCS_BUTTONPUSH && IsDefaultButton(aFrame))) {
          aFocused = true;
        }

      }

      return NS_OK;
    }
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO: {
      EventStates contentState;
      aFocused = false;

      aPart = DFC_BUTTON;
      aState = 0;
      nsIContent* content = aFrame->GetContent();
      bool isCheckbox = (aWidgetType == NS_THEME_CHECKBOX);
      bool isChecked = GetCheckedOrSelected(aFrame, !isCheckbox);
      bool isIndeterminate = isCheckbox && GetIndeterminate(aFrame);

      if (isCheckbox) {
        // indeterminate state takes precedence over checkedness.
        if (isIndeterminate) {
          aState = DFCS_BUTTON3STATE | DFCS_CHECKED;
        } else {
          aState = DFCS_BUTTONCHECK;
        }
      } else {
        aState = DFCS_BUTTONRADIO;
      }
      if (isChecked) {
        aState |= DFCS_CHECKED;
      }

      contentState = GetContentState(aFrame, aWidgetType);
      if (!content->IsXULElement() &&
          contentState.HasState(NS_EVENT_STATE_FOCUS)) {
        aFocused = true;
      }

      if (IsDisabled(aFrame, contentState)) {
        aState |= DFCS_INACTIVE;
      } else if (contentState.HasAllStates(NS_EVENT_STATE_ACTIVE |
                                           NS_EVENT_STATE_HOVER)) {
        aState |= DFCS_PUSHED;
      }

      return NS_OK;
    }
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM: {
      bool isTopLevel = false;
      bool isOpen = false;
      nsMenuFrame *menuFrame = do_QueryFrame(aFrame);
      EventStates eventState = GetContentState(aFrame, aWidgetType);

      // We indicate top-level-ness using aPart. 0 is a normal menu item,
      // 1 is a top-level menu item. The state of the item is composed of
      // DFCS_* flags only.
      aPart = 0;
      aState = 0;

      if (menuFrame) {
        // If this is a real menu item, we should check if it is part of the
        // main menu bar or not, and if it is a container, as these affect
        // rendering.
        isTopLevel = menuFrame->IsOnMenuBar();
        isOpen = menuFrame->IsOpen();
      }

      if (IsDisabled(aFrame, eventState))
        aState |= DFCS_INACTIVE;

      if (isTopLevel) {
        aPart = 1;
        if (isOpen)
          aState |= DFCS_PUSHED;
      }

      if (IsMenuActive(aFrame, aWidgetType))
        aState |= DFCS_HOT;

      return NS_OK;
    }
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
    case NS_THEME_MENUARROW: {
      aState = 0;
      EventStates eventState = GetContentState(aFrame, aWidgetType);

      if (IsDisabled(aFrame, eventState))
        aState |= DFCS_INACTIVE;
      if (IsMenuActive(aFrame, aWidgetType))
        aState |= DFCS_HOT;

      if (aWidgetType == NS_THEME_MENUCHECKBOX || aWidgetType == NS_THEME_MENURADIO) {
        if (IsCheckedButton(aFrame))
          aState |= DFCS_CHECKED;
      } else if (IsFrameRTL(aFrame)) {
          aState |= DFCS_RTL;
      }
      return NS_OK;
    }
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_FOCUS_OUTLINE:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_MENULIST:
    case NS_THEME_MENULIST_TEXTFIELD:
    case NS_THEME_RANGE:
    case NS_THEME_RANGE_THUMB:
    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:     
    case NS_THEME_SCROLLBAR_VERTICAL:
    case NS_THEME_SCROLLBAR_HORIZONTAL:      
    case NS_THEME_SCROLLCORNER:
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALETHUMB_HORIZONTAL:
    case NS_THEME_SCALETHUMB_VERTICAL:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBARPANEL:
    case NS_THEME_RESIZERPANEL:
    case NS_THEME_PROGRESSCHUNK:
    case NS_THEME_PROGRESSCHUNK_VERTICAL:
    case NS_THEME_TOOLTIP:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TABPANEL:
    case NS_THEME_TABPANELS:
    case NS_THEME_MENUBAR:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_GROUPBOX:
      // these don't use DrawFrameControl
      return NS_OK;
    case NS_THEME_MENULIST_BUTTON: {

      aPart = DFC_SCROLL;
      aState = DFCS_SCROLLCOMBOBOX;

      nsIFrame* parentFrame = aFrame->GetParent();
      bool isHTML = IsHTMLContent(aFrame);
      bool isMenulist = !isHTML && parentFrame->IsMenuFrame();
      bool isOpen = false;

      // HTML select and XUL menulist dropdown buttons get state from the parent.
      if (isHTML || isMenulist)
        aFrame = parentFrame;

      EventStates eventState = GetContentState(aFrame, aWidgetType);

      if (IsDisabled(aFrame, eventState)) {
        aState |= DFCS_INACTIVE;
        return NS_OK;
      }

      if (isHTML) {
        nsIComboboxControlFrame* ccf = do_QueryFrame(aFrame);
        isOpen = (ccf && ccf->IsDroppedDownOrHasParentPopup());
      }
      else
        isOpen = IsOpenButton(aFrame);

      // XXX Button should look active until the mouse is released, but
      //     without making it look active when the popup is clicked.
      if (isOpen && (isHTML || isMenulist))
        return NS_OK;

      // Dropdown button active state doesn't need :hover.
      if (eventState.HasState(NS_EVENT_STATE_ACTIVE))
        aState |= DFCS_PUSHED | DFCS_FLAT;

      return NS_OK;
    }
    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT: {
      EventStates contentState = GetContentState(aFrame, aWidgetType);

      aPart = DFC_SCROLL;
      switch (aWidgetType) {
        case NS_THEME_SCROLLBARBUTTON_UP:
          aState = DFCS_SCROLLUP;
          break;
        case NS_THEME_SCROLLBARBUTTON_DOWN:
          aState = DFCS_SCROLLDOWN;
          break;
        case NS_THEME_SCROLLBARBUTTON_LEFT:
          aState = DFCS_SCROLLLEFT;
          break;
        case NS_THEME_SCROLLBARBUTTON_RIGHT:
          aState = DFCS_SCROLLRIGHT;
          break;
      }

      if (IsDisabled(aFrame, contentState))
        aState |= DFCS_INACTIVE;
      else {
        if (contentState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE))
          aState |= DFCS_PUSHED | DFCS_FLAT;
      }

      return NS_OK;
    }
    case NS_THEME_INNER_SPIN_BUTTON:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON: {
      EventStates contentState = GetContentState(aFrame, aWidgetType);

      aPart = DFC_SCROLL;
      switch (aWidgetType) {
        case NS_THEME_SPINNER_UPBUTTON:
          aState = DFCS_SCROLLUP;
          break;
        case NS_THEME_INNER_SPIN_BUTTON:
        case NS_THEME_SPINNER_DOWNBUTTON:
          aState = DFCS_SCROLLDOWN;
          break;
      }

      if (IsDisabled(aFrame, contentState))
        aState |= DFCS_INACTIVE;
      else {
        if (contentState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE))
          aState |= DFCS_PUSHED;
      }

      return NS_OK;    
    }
    case NS_THEME_RESIZER:    
      aPart = DFC_SCROLL;
      aState = (IsFrameRTL(aFrame)) ?
               DFCS_SCROLLSIZEGRIPRIGHT : DFCS_SCROLLSIZEGRIP;
      return NS_OK;
    case NS_THEME_MENUSEPARATOR:
      aPart = 0;
      aState = 0;
      return NS_OK;
    case NS_THEME_WINDOW_TITLEBAR:
      aPart = mozilla::widget::themeconst::WP_CAPTION;
      aState = GetTopLevelWindowActiveState(aFrame);
      return NS_OK;
    case NS_THEME_WINDOW_TITLEBAR_MAXIMIZED:
      aPart = mozilla::widget::themeconst::WP_MAXCAPTION;
      aState = GetTopLevelWindowActiveState(aFrame);
      return NS_OK;
    case NS_THEME_WINDOW_FRAME_LEFT:
      aPart = mozilla::widget::themeconst::WP_FRAMELEFT;
      aState = GetTopLevelWindowActiveState(aFrame);
      return NS_OK;
    case NS_THEME_WINDOW_FRAME_RIGHT:
      aPart = mozilla::widget::themeconst::WP_FRAMERIGHT;
      aState = GetTopLevelWindowActiveState(aFrame);
      return NS_OK;
    case NS_THEME_WINDOW_FRAME_BOTTOM:
      aPart = mozilla::widget::themeconst::WP_FRAMEBOTTOM;
      aState = GetTopLevelWindowActiveState(aFrame);
      return NS_OK;
    case NS_THEME_WINDOW_BUTTON_CLOSE:
      aPart = DFC_CAPTION;
      aState = DFCS_CAPTIONCLOSE |
               GetClassicWindowFrameButtonState(GetContentState(aFrame,
                                                                aWidgetType));
      return NS_OK;
    case NS_THEME_WINDOW_BUTTON_MINIMIZE:
      aPart = DFC_CAPTION;
      aState = DFCS_CAPTIONMIN |
               GetClassicWindowFrameButtonState(GetContentState(aFrame,
                                                                aWidgetType));
      return NS_OK;
    case NS_THEME_WINDOW_BUTTON_MAXIMIZE:
      aPart = DFC_CAPTION;
      aState = DFCS_CAPTIONMAX |
               GetClassicWindowFrameButtonState(GetContentState(aFrame,
                                                                aWidgetType));
      return NS_OK;
    case NS_THEME_WINDOW_BUTTON_RESTORE:
      aPart = DFC_CAPTION;
      aState = DFCS_CAPTIONRESTORE |
               GetClassicWindowFrameButtonState(GetContentState(aFrame,
                                                                aWidgetType));
      return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

// Draw classic Windows tab
// (no system API for this, but DrawEdge can draw all the parts of a tab)
static void DrawTab(HDC hdc, const RECT& R, int32_t aPosition, bool aSelected,
                    bool aDrawLeft, bool aDrawRight)
{
  int32_t leftFlag, topFlag, rightFlag, lightFlag, shadeFlag;  
  RECT topRect, sideRect, bottomRect, lightRect, shadeRect;
  int32_t selectedOffset, lOffset, rOffset;

  selectedOffset = aSelected ? 1 : 0;
  lOffset = aDrawLeft ? 2 : 0;
  rOffset = aDrawRight ? 2 : 0;

  // Get info for tab orientation/position (Left, Top, Right, Bottom)
  switch (aPosition) {
    case BF_LEFT:
      leftFlag = BF_TOP; topFlag = BF_LEFT;
      rightFlag = BF_BOTTOM;
      lightFlag = BF_DIAGONAL_ENDTOPRIGHT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMRIGHT;

      ::SetRect(&topRect, R.left, R.top+lOffset, R.right, R.bottom-rOffset);
      ::SetRect(&sideRect, R.left+2, R.top, R.right-2+selectedOffset, R.bottom);
      ::SetRect(&bottomRect, R.right-2, R.top, R.right, R.bottom);
      ::SetRect(&lightRect, R.left, R.top, R.left+3, R.top+3);
      ::SetRect(&shadeRect, R.left+1, R.bottom-2, R.left+2, R.bottom-1);
      break;
    case BF_TOP:    
      leftFlag = BF_LEFT; topFlag = BF_TOP;
      rightFlag = BF_RIGHT;
      lightFlag = BF_DIAGONAL_ENDTOPRIGHT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMRIGHT;

      ::SetRect(&topRect, R.left+lOffset, R.top, R.right-rOffset, R.bottom);
      ::SetRect(&sideRect, R.left, R.top+2, R.right, R.bottom-1+selectedOffset);
      ::SetRect(&bottomRect, R.left, R.bottom-1, R.right, R.bottom);
      ::SetRect(&lightRect, R.left, R.top, R.left+3, R.top+3);      
      ::SetRect(&shadeRect, R.right-2, R.top+1, R.right-1, R.top+2);      
      break;
    case BF_RIGHT:    
      leftFlag = BF_TOP; topFlag = BF_RIGHT;
      rightFlag = BF_BOTTOM;
      lightFlag = BF_DIAGONAL_ENDTOPLEFT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMLEFT;

      ::SetRect(&topRect, R.left, R.top+lOffset, R.right, R.bottom-rOffset);
      ::SetRect(&sideRect, R.left+2-selectedOffset, R.top, R.right-2, R.bottom);
      ::SetRect(&bottomRect, R.left, R.top, R.left+2, R.bottom);
      ::SetRect(&lightRect, R.right-3, R.top, R.right-1, R.top+2);
      ::SetRect(&shadeRect, R.right-2, R.bottom-3, R.right, R.bottom-1);
      break;
    case BF_BOTTOM:    
      leftFlag = BF_LEFT; topFlag = BF_BOTTOM;
      rightFlag = BF_RIGHT;
      lightFlag = BF_DIAGONAL_ENDTOPLEFT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMLEFT;

      ::SetRect(&topRect, R.left+lOffset, R.top, R.right-rOffset, R.bottom);
      ::SetRect(&sideRect, R.left, R.top+2-selectedOffset, R.right, R.bottom-2);
      ::SetRect(&bottomRect, R.left, R.top, R.right, R.top+2);
      ::SetRect(&lightRect, R.left, R.bottom-3, R.left+2, R.bottom-1);
      ::SetRect(&shadeRect, R.right-2, R.bottom-3, R.right, R.bottom-1);
      break;
    default:
      MOZ_CRASH();
  }

  // Background
  ::FillRect(hdc, &R, (HBRUSH) (COLOR_3DFACE+1) );

  // Tab "Top"
  ::DrawEdge(hdc, &topRect, EDGE_RAISED, BF_SOFT | topFlag);

  // Tab "Bottom"
  if (!aSelected)
    ::DrawEdge(hdc, &bottomRect, EDGE_RAISED, BF_SOFT | topFlag);

  // Tab "Sides"
  if (!aDrawLeft)
    leftFlag = 0;
  if (!aDrawRight)
    rightFlag = 0;
  ::DrawEdge(hdc, &sideRect, EDGE_RAISED, BF_SOFT | leftFlag | rightFlag);

  // Tab Diagonal Corners
  if (aDrawLeft)
    ::DrawEdge(hdc, &lightRect, EDGE_RAISED, BF_SOFT | lightFlag);

  if (aDrawRight)
    ::DrawEdge(hdc, &shadeRect, EDGE_RAISED, BF_SOFT | shadeFlag);
}

static void DrawMenuImage(HDC hdc, const RECT& rc, int32_t aComponent, uint32_t aColor)
{
  // This procedure creates a memory bitmap to contain the check mark, draws
  // it into the bitmap (it is a mask image), then composes it onto the menu
  // item in appropriate colors.
  HDC hMemoryDC = ::CreateCompatibleDC(hdc);
  if (hMemoryDC) {
    // XXXjgr We should ideally be caching these, but we wont be notified when
    // they change currently, so we can't do so easily. Same for the bitmap.
    int checkW = ::GetSystemMetrics(SM_CXMENUCHECK);
    int checkH = ::GetSystemMetrics(SM_CYMENUCHECK);

    HBITMAP hMonoBitmap = ::CreateBitmap(checkW, checkH, 1, 1, nullptr);
    if (hMonoBitmap) {

      HBITMAP hPrevBitmap = (HBITMAP) ::SelectObject(hMemoryDC, hMonoBitmap);
      if (hPrevBitmap) {

        // XXXjgr This will go pear-shaped if the image is bigger than the
        // provided rect. What should we do?
        RECT imgRect = { 0, 0, checkW, checkH };
        POINT imgPos = {
              rc.left + (rc.right  - rc.left - checkW) / 2,
              rc.top  + (rc.bottom - rc.top  - checkH) / 2
            };

        // XXXzeniko Windows renders these 1px lower than you'd expect
        if (aComponent == DFCS_MENUCHECK || aComponent == DFCS_MENUBULLET)
          imgPos.y++;

        ::DrawFrameControl(hMemoryDC, &imgRect, DFC_MENU, aComponent);
        COLORREF oldTextCol = ::SetTextColor(hdc, 0x00000000);
        COLORREF oldBackCol = ::SetBkColor(hdc, 0x00FFFFFF);
        ::BitBlt(hdc, imgPos.x, imgPos.y, checkW, checkH, hMemoryDC, 0, 0, SRCAND);
        ::SetTextColor(hdc, ::GetSysColor(aColor));
        ::SetBkColor(hdc, 0x00000000);
        ::BitBlt(hdc, imgPos.x, imgPos.y, checkW, checkH, hMemoryDC, 0, 0, SRCPAINT);
        ::SetTextColor(hdc, oldTextCol);
        ::SetBkColor(hdc, oldBackCol);
        ::SelectObject(hMemoryDC, hPrevBitmap);
      }
      ::DeleteObject(hMonoBitmap);
    }
    ::DeleteDC(hMemoryDC);
  }
}

void nsNativeThemeWin::DrawCheckedRect(HDC hdc, const RECT& rc, int32_t fore, int32_t back,
                                       HBRUSH defaultBack)
{
  static WORD patBits[8] = {
    0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55
  };
        
  HBITMAP patBmp = ::CreateBitmap(8, 8, 1, 1, patBits);
  if (patBmp) {
    HBRUSH brush = (HBRUSH) ::CreatePatternBrush(patBmp);
    if (brush) {        
      COLORREF oldForeColor = ::SetTextColor(hdc, ::GetSysColor(fore));
      COLORREF oldBackColor = ::SetBkColor(hdc, ::GetSysColor(back));
      POINT vpOrg;

      ::UnrealizeObject(brush);
      ::GetViewportOrgEx(hdc, &vpOrg);
      ::SetBrushOrgEx(hdc, vpOrg.x + rc.left, vpOrg.y + rc.top, nullptr);
      HBRUSH oldBrush = (HBRUSH) ::SelectObject(hdc, brush);
      ::FillRect(hdc, &rc, brush);
      ::SetTextColor(hdc, oldForeColor);
      ::SetBkColor(hdc, oldBackColor);
      ::SelectObject(hdc, oldBrush);
      ::DeleteObject(brush);          
    }
    else
      ::FillRect(hdc, &rc, defaultBack);
  
    ::DeleteObject(patBmp);
  }
}

nsresult nsNativeThemeWin::ClassicDrawWidgetBackground(gfxContext* aContext,
                                  nsIFrame* aFrame,
                                  uint8_t aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect)
{
  int32_t part, state;
  bool focused;
  nsresult rv;
  rv = ClassicGetThemePartAndState(aFrame, aWidgetType, part, state, focused);
  if (NS_FAILED(rv))
    return rv;

  if (AssumeThemePartAndStateAreTransparent(part, state)) {
    return NS_OK;
  }

  gfxFloat p2a = gfxFloat(aFrame->PresContext()->AppUnitsPerDevPixel());
  RECT widgetRect;
  gfxRect tr(aRect.X(), aRect.Y(), aRect.Width(), aRect.Height()),
          dr(aDirtyRect.X(), aDirtyRect.Y(), aDirtyRect.Width(), aDirtyRect.Height());

  tr.Scale(1.0 / p2a);
  dr.Scale(1.0 / p2a);

  RefPtr<gfxContext> ctx = aContext;

  gfxWindowsNativeDrawing nativeDrawing(ctx, dr, GetWidgetNativeDrawingFlags(aWidgetType));

RENDER_AGAIN:

  HDC hdc = nativeDrawing.BeginNativeDrawing();
  if (!hdc)
    return NS_ERROR_FAILURE;

  nativeDrawing.TransformToNativeRect(tr, widgetRect);

  rv = NS_OK;
  switch (aWidgetType) { 
    // Draw button
    case NS_THEME_BUTTON: {
      if (focused) {
        // draw dark button focus border first
        HBRUSH brush;        
        brush = ::GetSysColorBrush(COLOR_3DDKSHADOW);
        if (brush)
          ::FrameRect(hdc, &widgetRect, brush);
        InflateRect(&widgetRect, -1, -1);
      }
      // fall-through...
    }
    // Draw controls supported by DrawFrameControl
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT:
    case NS_THEME_INNER_SPIN_BUTTON:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON:
    case NS_THEME_MENULIST_BUTTON:
    case NS_THEME_RESIZER: {
      int32_t oldTA;
      // setup DC to make DrawFrameControl draw correctly
      oldTA = ::SetTextAlign(hdc, TA_TOP | TA_LEFT | TA_NOUPDATECP);
      ::DrawFrameControl(hdc, &widgetRect, part, state);
      ::SetTextAlign(hdc, oldTA);
      break;
    }
    // Draw controls with 2px 3D inset border
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_LISTBOX:
    case NS_THEME_MENULIST:
    case NS_THEME_MENULIST_TEXTFIELD: {
      // Draw inset edge
      ::DrawEdge(hdc, &widgetRect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
      EventStates eventState = GetContentState(aFrame, aWidgetType);

      // Fill in background
      if (IsDisabled(aFrame, eventState) ||
          (aFrame->GetContent()->IsXULElement() &&
           IsReadOnly(aFrame)))
        ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_BTNFACE+1));
      else
        ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_WINDOW+1));

      break;
    }
    case NS_THEME_TREEVIEW: {
      // Draw inset edge
      ::DrawEdge(hdc, &widgetRect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);

      // Fill in window color background
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_WINDOW+1));

      break;
    }
    // Draw ToolTip background
    case NS_THEME_TOOLTIP:
      ::FrameRect(hdc, &widgetRect, ::GetSysColorBrush(COLOR_WINDOWFRAME));
      InflateRect(&widgetRect, -1, -1);
      ::FillRect(hdc, &widgetRect, ::GetSysColorBrush(COLOR_INFOBK));

      break;
    case NS_THEME_GROUPBOX:
      ::DrawEdge(hdc, &widgetRect, EDGE_ETCHED, BF_RECT | BF_ADJUST);
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_BTNFACE+1));
      break;
    // Draw 3D face background controls
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
      // Draw 3D border
      ::DrawEdge(hdc, &widgetRect, BDR_SUNKENOUTER, BF_RECT | BF_MIDDLE);
      InflateRect(&widgetRect, -1, -1);
      // fall through
    case NS_THEME_TABPANEL:
    case NS_THEME_STATUSBAR:
    case NS_THEME_RESIZERPANEL: {
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_BTNFACE+1));

      break;
    }
    // Draw 3D inset statusbar panel
    case NS_THEME_STATUSBARPANEL: {
      if (aFrame->GetNextSibling())
        widgetRect.right -= 2; // space between sibling status panels

      ::DrawEdge(hdc, &widgetRect, BDR_SUNKENOUTER, BF_RECT | BF_MIDDLE);

      break;
    }
    // Draw scrollbar thumb
    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
      ::DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_RECT | BF_MIDDLE);

      break;
    case NS_THEME_RANGE_THUMB:
    case NS_THEME_SCALETHUMB_VERTICAL:
    case NS_THEME_SCALETHUMB_HORIZONTAL: {
      EventStates eventState = GetContentState(aFrame, aWidgetType);

      ::DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_RECT | BF_SOFT | BF_MIDDLE | BF_ADJUST);
      if (IsDisabled(aFrame, eventState)) {
        DrawCheckedRect(hdc, widgetRect, COLOR_3DFACE, COLOR_3DHILIGHT,
                        (HBRUSH) COLOR_3DHILIGHT);
      }

      break;
    }
    // Draw scrollbar track background
    case NS_THEME_SCROLLBAR_VERTICAL:
    case NS_THEME_SCROLLBAR_HORIZONTAL: {

      // Windows fills in the scrollbar track differently 
      // depending on whether these are equal
      DWORD color3D, colorScrollbar, colorWindow;

      color3D = ::GetSysColor(COLOR_3DFACE);      
      colorWindow = ::GetSysColor(COLOR_WINDOW);
      colorScrollbar = ::GetSysColor(COLOR_SCROLLBAR);
      
      if ((color3D != colorScrollbar) && (colorWindow != colorScrollbar))
        // Use solid brush
        ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_SCROLLBAR+1));
      else
      {
        DrawCheckedRect(hdc, widgetRect, COLOR_3DHILIGHT, COLOR_3DFACE,
                        (HBRUSH) COLOR_SCROLLBAR+1);
      }
      // XXX should invert the part of the track being clicked here
      // but the track is never :active

      break;
    }
    case NS_THEME_SCROLLCORNER: {
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_SCROLLBAR + 1));
    }
    // Draw scale track background
    case NS_THEME_RANGE:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALE_HORIZONTAL: { 
      const int32_t trackWidth = 4;
      // When rounding is necessary, we round the position of the track
      // away from the chevron of the thumb to make it look better.
      if (aWidgetType == NS_THEME_SCALE_HORIZONTAL ||
          (aWidgetType == NS_THEME_RANGE && IsRangeHorizontal(aFrame))) {
        widgetRect.top += (widgetRect.bottom - widgetRect.top - trackWidth) / 2;
        widgetRect.bottom = widgetRect.top + trackWidth;
      }
      else {
        if (!IsFrameRTL(aFrame)) {
          widgetRect.left += (widgetRect.right - widgetRect.left - trackWidth) / 2;
          widgetRect.right = widgetRect.left + trackWidth;
        } else {
          widgetRect.right -= (widgetRect.right - widgetRect.left - trackWidth) / 2;
          widgetRect.left = widgetRect.right - trackWidth;
        }
      }

      ::DrawEdge(hdc, &widgetRect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
      ::FillRect(hdc, &widgetRect, (HBRUSH) GetStockObject(GRAY_BRUSH));
 
      break;
    }
    case NS_THEME_PROGRESSCHUNK_VERTICAL:
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_HIGHLIGHT+1));
      break;

    case NS_THEME_PROGRESSCHUNK: {
      nsIFrame* stateFrame = aFrame->GetParent();
      EventStates eventStates = GetContentState(stateFrame, aWidgetType);

      bool indeterminate = IsIndeterminateProgress(stateFrame, eventStates);
      bool vertical = IsVerticalProgress(stateFrame) ||
                      aWidgetType == NS_THEME_PROGRESSCHUNK_VERTICAL;

      nsIContent* content = aFrame->GetContent();
      if (!indeterminate || !content) {
        ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_HIGHLIGHT+1));
        break;
      }

      RECT overlayRect =
        CalculateProgressOverlayRect(aFrame, &widgetRect, vertical,
                                     indeterminate, true);

      ::FillRect(hdc, &overlayRect, (HBRUSH) (COLOR_HIGHLIGHT+1));

      if (!QueueAnimatedContentForRefresh(aFrame->GetContent(), 30)) {
        NS_WARNING("unable to animate progress widget!");
      }
      break;
    }

    // Draw Tab
    case NS_THEME_TAB: {
      DrawTab(hdc, widgetRect,
        IsBottomTab(aFrame) ? BF_BOTTOM : BF_TOP, 
        IsSelectedTab(aFrame),
        !IsRightToSelectedTab(aFrame),
        !IsLeftToSelectedTab(aFrame));

      break;
    }
    case NS_THEME_TABPANELS:
      ::DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_SOFT | BF_MIDDLE |
          BF_LEFT | BF_RIGHT | BF_BOTTOM);

      break;
    case NS_THEME_MENUBAR:
      break;
    case NS_THEME_MENUPOPUP:
      NS_ASSERTION(nsUXThemeData::sFlatMenus, "Classic menus are styled entirely through CSS");
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_MENU+1));
      ::FrameRect(hdc, &widgetRect, ::GetSysColorBrush(COLOR_BTNSHADOW));
      break;
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
      // part == 0 for normal items
      // part == 1 for top-level menu items
      if (nsUXThemeData::sFlatMenus) {
        // Not disabled and hot/pushed.
        if ((state & (DFCS_HOT | DFCS_PUSHED)) != 0) {
          ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_MENUHILIGHT+1));
          ::FrameRect(hdc, &widgetRect, ::GetSysColorBrush(COLOR_HIGHLIGHT));
        }
      } else {
        if (part == 1) {
          if ((state & DFCS_INACTIVE) == 0) {
            if ((state & DFCS_PUSHED) != 0) {
              ::DrawEdge(hdc, &widgetRect, BDR_SUNKENOUTER, BF_RECT);
            } else if ((state & DFCS_HOT) != 0) {
              ::DrawEdge(hdc, &widgetRect, BDR_RAISEDINNER, BF_RECT);
            }
          }
        } else {
          if ((state & (DFCS_HOT | DFCS_PUSHED)) != 0) {
            ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_HIGHLIGHT+1));
          }
        }
      }
      break;
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
      if (!(state & DFCS_CHECKED))
        break; // nothin' to do
    case NS_THEME_MENUARROW: {
      uint32_t color = COLOR_MENUTEXT;
      if ((state & DFCS_INACTIVE))
        color = COLOR_GRAYTEXT;
      else if ((state & DFCS_HOT))
        color = COLOR_HIGHLIGHTTEXT;
      
      if (aWidgetType == NS_THEME_MENUCHECKBOX)
        DrawMenuImage(hdc, widgetRect, DFCS_MENUCHECK, color);
      else if (aWidgetType == NS_THEME_MENURADIO)
        DrawMenuImage(hdc, widgetRect, DFCS_MENUBULLET, color);
      else if (aWidgetType == NS_THEME_MENUARROW)
        DrawMenuImage(hdc, widgetRect, 
                      (state & DFCS_RTL) ? DFCS_MENUARROWRIGHT : DFCS_MENUARROW,
                      color);
      break;
    }
    case NS_THEME_MENUSEPARATOR: {
      // separators are offset by a bit (see menu.css)
      widgetRect.left++;
      widgetRect.right--;

      // This magic number is brought to you by the value in menu.css
      widgetRect.top += 4;
      // Our rectangles are 1 pixel high (see border size in menu.css)
      widgetRect.bottom = widgetRect.top+1;
      ::FillRect(hdc, &widgetRect, (HBRUSH)(COLOR_3DSHADOW+1));
      widgetRect.top++;
      widgetRect.bottom++;
      ::FillRect(hdc, &widgetRect, (HBRUSH)(COLOR_3DHILIGHT+1));
      break;
    }

    case NS_THEME_WINDOW_TITLEBAR:
    case NS_THEME_WINDOW_TITLEBAR_MAXIMIZED:
    {
      RECT rect = widgetRect;
      int32_t offset = GetSystemMetrics(SM_CXFRAME);

      // first fill the area to the color of the window background
      FillRect(hdc, &rect, (HBRUSH)(COLOR_3DFACE+1));

      // inset the caption area so it doesn't overflow.
      rect.top += offset;
      // if enabled, draw a gradient titlebar background, otherwise
      // fill with a solid color.
      BOOL bFlag = TRUE;
      SystemParametersInfo(SPI_GETGRADIENTCAPTIONS, 0, &bFlag, 0);
      if (!bFlag) {
        if (state == mozilla::widget::themeconst::FS_ACTIVE)
          FillRect(hdc, &rect, (HBRUSH)(COLOR_ACTIVECAPTION+1));
        else
          FillRect(hdc, &rect, (HBRUSH)(COLOR_INACTIVECAPTION+1));
      } else {
        DWORD startColor, endColor;
        if (state == mozilla::widget::themeconst::FS_ACTIVE) {
          startColor = GetSysColor(COLOR_ACTIVECAPTION);
          endColor = GetSysColor(COLOR_GRADIENTACTIVECAPTION);
        } else {
          startColor = GetSysColor(COLOR_INACTIVECAPTION);
          endColor = GetSysColor(COLOR_GRADIENTINACTIVECAPTION);
        }

        TRIVERTEX vertex[2];
        vertex[0].x     = rect.left;
        vertex[0].y     = rect.top;
        vertex[0].Red   = GetRValue(startColor) << 8;
        vertex[0].Green = GetGValue(startColor) << 8;
        vertex[0].Blue  = GetBValue(startColor) << 8;
        vertex[0].Alpha = 0;

        vertex[1].x     = rect.right;
        vertex[1].y     = rect.bottom; 
        vertex[1].Red   = GetRValue(endColor) << 8;
        vertex[1].Green = GetGValue(endColor) << 8;
        vertex[1].Blue  = GetBValue(endColor) << 8;
        vertex[1].Alpha = 0;

        GRADIENT_RECT gRect;
        gRect.UpperLeft  = 0;
        gRect.LowerRight = 1;
        // available on win2k & up
        GradientFill(hdc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_H);
      }

      if (aWidgetType == NS_THEME_WINDOW_TITLEBAR) {
        // frame things up with a top raised border.
        DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_TOP);
      }
      break;
    }

    case NS_THEME_WINDOW_FRAME_LEFT:
      DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_LEFT);
      break;

    case NS_THEME_WINDOW_FRAME_RIGHT:
      DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_RIGHT);
      break;

    case NS_THEME_WINDOW_FRAME_BOTTOM:
      DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_BOTTOM);
      break;

    case NS_THEME_WINDOW_BUTTON_CLOSE:
    case NS_THEME_WINDOW_BUTTON_MINIMIZE:
    case NS_THEME_WINDOW_BUTTON_MAXIMIZE:
    case NS_THEME_WINDOW_BUTTON_RESTORE:
    {
      if (aWidgetType == NS_THEME_WINDOW_BUTTON_MINIMIZE) {
        OffsetBackgroundRect(widgetRect, CAPTIONBUTTON_MINIMIZE);
      }
      else if (aWidgetType == NS_THEME_WINDOW_BUTTON_MAXIMIZE ||
               aWidgetType == NS_THEME_WINDOW_BUTTON_RESTORE) {
        OffsetBackgroundRect(widgetRect, CAPTIONBUTTON_RESTORE);
      }
      else if (aWidgetType == NS_THEME_WINDOW_BUTTON_CLOSE) {
        OffsetBackgroundRect(widgetRect, CAPTIONBUTTON_CLOSE);
      }
      int32_t oldTA = SetTextAlign(hdc, TA_TOP | TA_LEFT | TA_NOUPDATECP);
      DrawFrameControl(hdc, &widgetRect, part, state);
      SetTextAlign(hdc, oldTA);
      break;
    }

    default:
      rv = NS_ERROR_FAILURE;
      break;
  }

  nativeDrawing.EndNativeDrawing();

  if (NS_FAILED(rv))
    return rv;

  if (nativeDrawing.ShouldRenderAgain())
    goto RENDER_AGAIN;

  nativeDrawing.PaintToContext();

  return rv;
}

uint32_t
nsNativeThemeWin::GetWidgetNativeDrawingFlags(uint8_t aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_FOCUS_OUTLINE:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:

    case NS_THEME_MENULIST:
    case NS_THEME_MENULIST_TEXTFIELD:
      return
        gfxWindowsNativeDrawing::CANNOT_DRAW_TO_COLOR_ALPHA |
        gfxWindowsNativeDrawing::CAN_AXIS_ALIGNED_SCALE |
        gfxWindowsNativeDrawing::CANNOT_COMPLEX_TRANSFORM;

    // need to check these others
    case NS_THEME_RANGE:
    case NS_THEME_RANGE_THUMB:
    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT:
    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_VERTICAL:
    case NS_THEME_SCROLLBAR_HORIZONTAL:
    case NS_THEME_SCROLLCORNER:
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALETHUMB_HORIZONTAL:
    case NS_THEME_SCALETHUMB_VERTICAL:
    case NS_THEME_INNER_SPIN_BUTTON:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TOOLTIP:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBARPANEL:
    case NS_THEME_RESIZERPANEL:
    case NS_THEME_RESIZER:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSCHUNK:
    case NS_THEME_PROGRESSCHUNK_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TABPANEL:
    case NS_THEME_TABPANELS:
    case NS_THEME_MENUBAR:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_MENUITEM:
      break;

    // the dropdown button /almost/ renders correctly with scaling,
    // except that the graphic in the dropdown button (the downward arrow)
    // doesn't get scaled up.
    case NS_THEME_MENULIST_BUTTON:
    // these are definitely no; they're all graphics that don't get scaled up
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
    case NS_THEME_GROUPBOX:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
    case NS_THEME_MENUARROW:
      return
        gfxWindowsNativeDrawing::CANNOT_DRAW_TO_COLOR_ALPHA |
        gfxWindowsNativeDrawing::CANNOT_AXIS_ALIGNED_SCALE |
        gfxWindowsNativeDrawing::CANNOT_COMPLEX_TRANSFORM;
  }

  return
    gfxWindowsNativeDrawing::CANNOT_DRAW_TO_COLOR_ALPHA |
    gfxWindowsNativeDrawing::CANNOT_AXIS_ALIGNED_SCALE |
    gfxWindowsNativeDrawing::CANNOT_COMPLEX_TRANSFORM;
}

static COLORREF
ToColorRef(nscolor aColor)
{
  return RGB(NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor));
}

static float
ComputeColorComponentLuminance(uint8_t aComponent)
{
  float v = float(aComponent) / 255.0f;
  if (v <= 0.03928f) {
    return v / 12.92f;
  }
  return std::pow((v + 0.055f) / 1.055f, 2.4f);
}

static constexpr float
ComputeRelativeLuminanceFromComponents(float aR, float aG, float aB)
{
  return 0.2126f * aR + 0.7152f * aG + 0.0722f * aB;
}

// This function is written according to the algorithm defined in
// https://www.w3.org/TR/WCAG20/#relativeluminancedef
static float
ComputeRelativeLuminance(nscolor aColor)
{
  float r = ComputeColorComponentLuminance(NS_GET_R(aColor));
  float g = ComputeColorComponentLuminance(NS_GET_G(aColor));
  float b = ComputeColorComponentLuminance(NS_GET_B(aColor));
  return ComputeRelativeLuminanceFromComponents(r, g, b);
}

// Inverse function of ComputeColorComponentLuminance.
static uint8_t
DecomputeColorComponentLuminance(float aComponent)
{
  if (aComponent <= 0.03928f / 12.92f) {
    aComponent *= 12.92f;
  } else {
    aComponent = std::pow(aComponent, 1.0f / 2.4f) * 1.055f - 0.055f;
  }
  return ClampColor(aComponent * 255.0f);
}

// Adjust the luminance of the color to the given value.
static nscolor
AdjustColorLuminance(nscolor aColor, float aLuminance)
{
  float r = ComputeColorComponentLuminance(NS_GET_R(aColor));
  float g = ComputeColorComponentLuminance(NS_GET_G(aColor));
  float b = ComputeColorComponentLuminance(NS_GET_B(aColor));
  float luminance = ComputeRelativeLuminanceFromComponents(r, g, b);
  float factor = aLuminance / luminance;
  uint8_t r1 = DecomputeColorComponentLuminance(r * factor);
  uint8_t g1 = DecomputeColorComponentLuminance(g * factor);
  uint8_t b1 = DecomputeColorComponentLuminance(b * factor);
  return NS_RGB(r1, g1, b1);
}

static nscolor
GetScrollbarArrowColor(nscolor aTrackColor)
{
  // In Windows 10 scrollbar, there are several gray colors used:
  //
  // State  | Background (lum) | Arrow   | Contrast
  // -------+------------------+---------+---------
  // Normal | Gray 240 (87.1%) | Gray 96 |     5.5
  // Hover  | Gray 218 (70.1%) | Black   |    15.0
  // Active | Gray 96  (11.7%) | White   |     6.3
  //
  // Contrast value is computed based on the definition in
  // https://www.w3.org/TR/WCAG20/#contrast-ratiodef
  //
  // This function is written based on these values.

  float luminance = ComputeRelativeLuminance(aTrackColor);
  // Color with luminance larger than 0.72 has contrast ratio over 4.6
  // to color with luminance of gray 96, so this value is chosen for
  // this range. It is the luminance of gray 221.
  if (luminance >= 0.72) {
    // ComputeRelativeLuminanceFromComponents(96). That function cannot
    // be constexpr because of std::pow.
    const float GRAY96_LUMINANCE = 0.117f;
    return AdjustColorLuminance(aTrackColor, GRAY96_LUMINANCE);
  }
  // The contrast ratio of a color to black equals that to white when its
  // luminance is around 0.18, with a contrast ratio ~4.6 to both sides,
  // thus the value below. It's the lumanince of gray 118.
  if (luminance >= 0.18) {
    return NS_RGB(0, 0, 0);
  }
  return NS_RGB(255, 255, 255);
}

// This tries to draw a Windows 10 style scrollbar with given colors.
nsresult
nsNativeThemeWin::DrawCustomScrollbarPart(gfxContext* aContext,
                                          nsIFrame* aFrame,
                                          ComputedStyle* aStyle,
                                          uint8_t aWidgetType,
                                          const nsRect& aRect,
                                          const nsRect& aClipRect)
{
  MOZ_ASSERT(!aStyle->StyleUserInterface()->mScrollbarFaceColor.IsAuto() ||
             !aStyle->StyleUserInterface()->mScrollbarTrackColor.IsAuto());

  gfxRect tr(aRect.X(), aRect.Y(), aRect.Width(), aRect.Height()),
          dr(aClipRect.X(), aClipRect.Y(),
             aClipRect.Width(), aClipRect.Height());

  nscolor trackColor =
    GetScrollbarTrackColor(aStyle, &GetScrollbarTrackColorForAuto);
  HBRUSH dcBrush = (HBRUSH) GetStockObject(DC_BRUSH);

  gfxFloat p2a = gfxFloat(aFrame->PresContext()->AppUnitsPerDevPixel());
  tr.Scale(1.0 / p2a);
  dr.Scale(1.0 / p2a);

  RefPtr<gfxContext> ctx = aContext;

  uint32_t flags = GetWidgetNativeDrawingFlags(aWidgetType);
  gfxWindowsNativeDrawing nativeDrawing(ctx, dr, flags);

  do {
    HDC hdc = nativeDrawing.BeginNativeDrawing();
    if (!hdc) {
      return NS_ERROR_FAILURE;
    }

    RECT widgetRect;
    nativeDrawing.TransformToNativeRect(tr, widgetRect);
    ::SetDCBrushColor(hdc, ToColorRef(trackColor));
    ::SelectObject(hdc, dcBrush);
    ::FillRect(hdc, &widgetRect, dcBrush);

    switch (aWidgetType) {
      case NS_THEME_SCROLLBARTHUMB_VERTICAL:
      case NS_THEME_SCROLLBARTHUMB_HORIZONTAL: {
        // Scrollbar thumb is two CSS pixels thinner than the track.
        gfxRect tr2 = tr;
        gfxFloat dev2css = round(AppUnitsPerCSSPixel() / p2a);
        if (aWidgetType == NS_THEME_SCROLLBARTHUMB_VERTICAL) {
          tr2.Deflate(dev2css, 0);
        } else {
          tr2.Deflate(0, dev2css);
        }
        nativeDrawing.TransformToNativeRect(tr2, widgetRect);
        nscolor faceColor =
          GetScrollbarFaceColor(aStyle, &GetScrollbarFaceColorForAuto);
        ::SetDCBrushColor(hdc, ToColorRef(faceColor));
        ::FillRect(hdc, &widgetRect, dcBrush);
        break;
      }
      case NS_THEME_SCROLLBARBUTTON_UP:
      case NS_THEME_SCROLLBARBUTTON_DOWN:
      case NS_THEME_SCROLLBARBUTTON_LEFT:
      case NS_THEME_SCROLLBARBUTTON_RIGHT: {
        // kPath is the path of scrollbar up arrow on Windows 10
        // in a 17x17 area.
        const LONG kSize = 17;
        const POINT kPath[] = {
          { 5, 9 }, { 8, 6 }, { 11, 9 }, { 11, 11 }, { 8, 8 }, { 5, 11 },
        };
        const size_t kCount = ArrayLength(kPath);
        // Calculate necessary parameters for positioning the arrow.
        LONG width = widgetRect.right - widgetRect.left;
        LONG height = widgetRect.bottom - widgetRect.top;
        LONG size = std::min(width, height);
        LONG left = (width - size) / 2 + widgetRect.left;
        LONG top = (height - size) / 2 + widgetRect.top;
        float unit = float(size) / kSize;
        POINT path[kCount];
        // Flip the path for different direction, then resize and align
        // it to the middle of the area.
        for (size_t i = 0; i < kCount; i++) {
          if (aWidgetType == NS_THEME_SCROLLBARBUTTON_UP) {
            path[i] = kPath[i];
          } else if (aWidgetType == NS_THEME_SCROLLBARBUTTON_DOWN) {
            path[i].x = kPath[i].x;
            path[i].y = kSize - kPath[i].y;
          } else if (aWidgetType == NS_THEME_SCROLLBARBUTTON_LEFT) {
            path[i].x = kPath[i].y;
            path[i].y = kPath[i].x;
          } else {
            path[i].x = kSize - kPath[i].y;
            path[i].y = kPath[i].x;
          }
          path[i].x = left + (LONG) round(unit * path[i].x);
          path[i].y = top + (LONG) round(unit * path[i].y);
        }
        // Paint the arrow.
        COLORREF arrowColor = ToColorRef(GetScrollbarArrowColor(trackColor));
        // XXX Somehow we need to paint with both pen and brush to get
        //     the desired shape. Can we do so only with brush?
        ::SetDCPenColor(hdc, arrowColor);
        ::SetDCBrushColor(hdc, arrowColor);
        ::SelectObject(hdc, GetStockObject(DC_PEN));
        ::Polygon(hdc, path, kCount);
        break;
      }
      default:
        break;
    }

    nativeDrawing.EndNativeDrawing();
  } while (nativeDrawing.ShouldRenderAgain());

  nativeDrawing.PaintToContext();
  return NS_OK;
}

///////////////////////////////////////////
// Creation Routine
///////////////////////////////////////////

// from nsWindow.cpp
extern bool gDisableNativeTheme;

nsresult NS_NewNativeTheme(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (gDisableNativeTheme)
    return NS_ERROR_NO_INTERFACE;

  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsNativeThemeWin* theme = new nsNativeThemeWin();
  if (!theme)
    return NS_ERROR_OUT_OF_MEMORY;
  return theme->QueryInterface(aIID, aResult);
}
