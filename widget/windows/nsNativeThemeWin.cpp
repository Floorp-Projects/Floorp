/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include "nsNativeThemeWin.h"
#include "nsRenderingContext.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsTransform2D.h"
#include "nsThemeConstants.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsEventStates.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsMenuFrame.h"
#include "nsGkAtoms.h"
#include <malloc.h>
#include "nsWindow.h"
#include "nsIComboboxControlFrame.h"
#include "prinrval.h"
#include "WinUtils.h"

#include "gfxPlatform.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "gfxWindowsPlatform.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsNativeDrawing.h"

#include "nsUXThemeData.h"
#include "nsUXThemeConstants.h"
#include <algorithm>

using namespace mozilla::widget;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gWindowsLog;
#endif

NS_IMPL_ISUPPORTS_INHERITED1(nsNativeThemeWin, nsNativeTheme, nsITheme)

nsNativeThemeWin::nsNativeThemeWin() :
  mProgressDeterminateTimeStamp(TimeStamp::Now()),
  mProgressIndeterminateTimeStamp(TimeStamp::Now())
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
  // Get the widget. nsIFrame's GetNearestWidget walks up the view chain
  // until it finds a real window.
  nsIWidget* widget = aFrame->GetNearestWidget();
  nsWindow * window = static_cast<nsWindow*>(widget);
  if (!window)
    return mozilla::widget::themeconst::FS_INACTIVE;
  if (widget && !window->IsTopLevelWidget() &&
      !(window = window->GetParentWindow(false)))
    return mozilla::widget::themeconst::FS_INACTIVE;

  if (window->GetWindowHandle() == ::GetActiveWindow())
    return mozilla::widget::themeconst::FS_ACTIVE;
  return mozilla::widget::themeconst::FS_INACTIVE;
}

static int32_t
GetWindowFrameButtonState(nsIFrame *aFrame, nsEventStates eventState)
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
GetClassicWindowFrameButtonState(nsEventStates eventState)
{
  if (eventState.HasState(NS_EVENT_STATE_ACTIVE) &&
      eventState.HasState(NS_EVENT_STATE_HOVER))
    return DFCS_BUTTONPUSH|DFCS_PUSHED;
  return DFCS_BUTTONPUSH;
}

static void
QueryForButtonData(nsIFrame *aFrame)
{
  if (nsUXThemeData::sTitlebarInfoPopulatedThemed && nsUXThemeData::sTitlebarInfoPopulatedAero)
    return;
  nsIWidget* widget = aFrame->GetNearestWidget();
  nsWindow * window = static_cast<nsWindow*>(widget);
  if (!window)
    return;
  if (!window->IsTopLevelWidget() &&
      !(window = window->GetParentWindow(false)))
    return;

  nsUXThemeData::UpdateTitlebarInfo(window->GetWindowHandle());
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
                    TMT_CONTENTMARGINS, NULL, &checkboxContent);
    return checkboxContent;
}

static SIZE
GetCheckboxBGSize(HANDLE theme, HDC hdc)
{
    SIZE checkboxSize;
    GetThemePartSize(theme, hdc, MENU_POPUPCHECK, MC_CHECKMARKNORMAL,
                     NULL, TS_TRUE, &checkboxSize);

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
                    TMT_SIZINGMARGINS, NULL, &checkboxBGSizing);
    GetThemeMargins(theme, hdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL,
                    TMT_CONTENTMARGINS, NULL, &checkboxBGContent);

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
    GetThemePartSize(theme, hdc, MENU_POPUPGUTTER, 0, NULL, TS_TRUE, &gutterSize);

    SIZE checkboxBGSize(GetCheckboxBGBounds(theme, hdc));

    SIZE itemSize;
    GetThemePartSize(theme, hdc, MENU_POPUPITEM, MPI_NORMAL, NULL, TS_TRUE, &itemSize);

    // Figure out how big the menuitem's icon will be (if present) at current DPI
    FLOAT scaleFactor = gfxWindowsPlatform::GetPlatform()->GetDPIScale();
    int iconDevicePixels = NSToIntRound(16 * scaleFactor);
    SIZE iconSize = {
      iconDevicePixels, iconDevicePixels
    };
    // Not really sure what margins should be used here, but this seems to work in practice...
    MARGINS margins = {0};
    GetThemeMargins(theme, hdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL,
                    TMT_CONTENTMARGINS, NULL, &margins);
    iconSize.cx += margins.cxLeftWidth + margins.cxRightWidth;
    iconSize.cy += margins.cyTopHeight + margins.cyBottomHeight;

    int width = std::max(itemSize.cx, std::max(iconSize.cx, checkboxBGSize.cx) + gutterSize.cx);
    int height = std::max(itemSize.cy, std::max(iconSize.cy, checkboxBGSize.cy));

    SIZE ret;
    ret.cx = width;
    ret.cy = height;
    return ret;
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
    RECT *newCRectPtr = NULL;

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
 *  xp theme min            0        2        0        2
 *  xp theme max            0        2        1        2
 *  xp theme close          1        2        2        2
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
 *
 *  xp theme min            0        0        1        0
 *  xp theme max            1        0        0        0
 *  xp theme close          0        0        0        0
 */

enum CaptionDesktopTheme {
  CAPTION_CLASSIC = 0,
  CAPTION_BASIC,
  CAPTION_XPTHEME,
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
AddPaddingRect(nsIntSize* aSize, CaptionButton button) {
  if (!aSize)
    return;
  RECT offset;
  if (!IsAppThemed())
    offset = buttonData[CAPTION_CLASSIC].hotPadding[button];
  else if (WinUtils::GetWindowsVersion() < WinUtils::VISTA_VERSION)
    offset = buttonData[CAPTION_XPTHEME].hotPadding[button];
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
  else if (WinUtils::GetWindowsVersion() < WinUtils::VISTA_VERSION)
    offset = buttonData[CAPTION_XPTHEME].hotPadding[button];
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
static const int32_t kProgressHorizontalVistaOverlaySize = 120;
// The width of the overlay used for the horizontal indeterminate progress bars on XP.
static const int32_t kProgressHorizontalXPOverlaySize = 55;
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
  if (aIsVertical) {
    if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION) {
      return PP_MOVEOVERLAYVERT;
    }
    return PP_CHUNKVERT;
  } else {
    if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION) {
      return PP_MOVEOVERLAY;
    }
    return PP_CHUNK;
  }
}

/*
 * GetProgressOverlaySize - returns the minimum width or height for themed
 * progress bar overlays. This includes the width of indeterminate chunks
 * and vista pulse overlays.
 */
static int32_t
GetProgressOverlaySize(bool aIsVertical, bool aIsIndeterminate)
{
  if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION) {
    if (aIsVertical) {
      return aIsIndeterminate ? kProgressVerticalIndeterminateOverlaySize
                              : kProgressVerticalOverlaySize;
    }
    return kProgressHorizontalVistaOverlaySize;
  }
  return kProgressHorizontalXPOverlaySize;
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
 * DrawChunkProgressMeter - renders an xp style chunked progress meter. Called
 * by DrawProgressMeter.
 *
 * @param aTheme       progress theme handle
 * @param aHdc         hdc returned by gfxWindowsNativeDrawing
 * @param aPart        the PP_X progress part
 * @param aState       the theme state
 * @param aFrame       the elements frame
 * @param aWidgetRect  bounding rect for the widget
 * @param aClipRect    dirty rect that needs drawing.
 * @param aAppUnits    app units per device pixel
 * @param aIsIndeterm  is an indeterminate progress?
 * @param aIsVertical  render a vertical progress?
 * @param aIsRtl       direction is rtl
 */
static void
DrawChunkProgressMeter(HTHEME aTheme, HDC aHdc, int aPart,
                       int aState, nsIFrame* aFrame, RECT* aWidgetRect,
                       RECT* aClipRect, gfxFloat aAppUnits, bool aIsIndeterm,
                       bool aIsVertical, bool aIsRtl)
{
  NS_ASSERTION(aTheme, "Bad theme.");
  NS_ASSERTION(aHdc, "Bad hdc.");
  NS_ASSERTION(aWidgetRect, "Bad rect.");
  NS_ASSERTION(aClipRect, "Bad clip rect.");
  NS_ASSERTION(aFrame, "Bad frame.");

  // For horizontal meters, the theme lib paints the right graphic but doesn't
  // paint the chunks, so we do that manually. For vertical meters, the theme
  // library draws everything correctly.
  if (aIsVertical) {
    DrawThemeBackground(aTheme, aHdc, aPart, aState, aWidgetRect, aClipRect);
    return;
  }

  // query for the proper chunk metrics
  int chunkSize, spaceSize;
  if (FAILED(GetThemeMetric(aTheme, aHdc, aPart, aState,
                            TMT_PROGRESSCHUNKSIZE, &chunkSize)) ||
      FAILED(GetThemeMetric(aTheme, aHdc, aPart, aState,
                            TMT_PROGRESSSPACESIZE, &spaceSize))) {
    DrawThemeBackground(aTheme, aHdc, aPart, aState, aWidgetRect, aClipRect);
    return;
  }

  // render chunks
  if (!aIsRtl || aIsIndeterm) {
    for (int chunk = aWidgetRect->left; chunk <= aWidgetRect->right;
         chunk += (chunkSize+spaceSize)) {
      if (!aIsIndeterm && ((chunk + chunkSize) > aWidgetRect->right)) {
        // aWidgetRect->right represents the end of the meter. Partial blocks
        // don't get rendered with one exception, so exit here if we don't have
        // a full chunk to draw.
        // The above is true *except* when the meter is at 100% fill, in which
        // case Windows renders any remaining partial block. Query the parent
        // frame to find out if we're at 100%.
        if (!IsProgressMeterFilled(aFrame)) {
          break;
        }
      }
      RECT bounds =
        { chunk, aWidgetRect->top, chunk + chunkSize, aWidgetRect->bottom };
      DrawThemeBackground(aTheme, aHdc, aPart, aState, &bounds, aClipRect);
    }
  } else {
    // rtl needs to grow in the opposite direction to look right.
    for (int chunk = aWidgetRect->right; chunk >= aWidgetRect->left;
         chunk -= (chunkSize+spaceSize)) {
      if ((chunk - chunkSize) < aWidgetRect->left) {
        if (!IsProgressMeterFilled(aFrame)) {
          break;
        }
      }
      RECT bounds =
        { chunk - chunkSize, aWidgetRect->top, chunk, aWidgetRect->bottom };
      DrawThemeBackground(aTheme, aHdc, aPart, aState, &bounds, aClipRect);
    }
  }
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
                                          RECT* aWidgetRect, RECT* aClipRect,
                                          gfxFloat aAppUnits)
{
  if (!aFrame || !aTheme || !aHdc)
    return;

  NS_ASSERTION(aWidgetRect, "bad rect pointer");
  NS_ASSERTION(aClipRect, "bad clip rect pointer");

  RECT adjWidgetRect, adjClipRect;
  adjWidgetRect = *aWidgetRect;
  adjClipRect = *aClipRect;
  if (WinUtils::GetWindowsVersion() < WinUtils::VISTA_VERSION) {
    // Adjust clipping out by one pixel. XP progress meters are inset,
    // Vista+ are not.
    InflateRect(&adjWidgetRect, 1, 1);
    InflateRect(&adjClipRect, 1, 1);
  }

  nsIFrame* parentFrame = aFrame->GetParent();
  if (!parentFrame) {
    // We have no parent to work with, just bail.
    NS_WARNING("No parent frame for progress rendering. Can't paint.");
    return;
  }

  nsEventStates eventStates = GetContentState(parentFrame, aWidgetType);
  bool vertical = IsVerticalProgress(parentFrame) ||
                  aWidgetType == NS_THEME_PROGRESSBAR_CHUNK_VERTICAL;
  bool indeterminate = IsIndeterminateProgress(parentFrame, eventStates);
  bool animate = indeterminate;

  if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION) {
    // Vista and up progress meter is fill style, rendered here. We render
    // the pulse overlay in the follow up section below.
    DrawThemeBackground(aTheme, aHdc, aPart, aState,
                        &adjWidgetRect, &adjClipRect);
    if (!IsProgressMeterFilled(aFrame)) {
      animate = true;
    }
  } else if (!indeterminate) {
    // XP progress meters are 'chunk' style.
    DrawChunkProgressMeter(aTheme, aHdc, aPart, aState, aFrame,
                           &adjWidgetRect, &adjClipRect, aAppUnits,
                           indeterminate, vertical, IsFrameRTL(aFrame));
  }    

  if (animate) {
    // Indeterminate rendering
    int32_t overlayPart = GetProgressOverlayStyle(vertical);
    RECT overlayRect =
      CalculateProgressOverlayRect(aFrame, &adjWidgetRect, vertical,
                                   indeterminate, false);
    if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION) {
      DrawThemeBackground(aTheme, aHdc, overlayPart, aState, &overlayRect,
                          &adjClipRect);
    } else {
      DrawChunkProgressMeter(aTheme, aHdc, overlayPart, aState, aFrame,
                             &overlayRect, &adjClipRect, aAppUnits,
                             indeterminate, vertical, IsFrameRTL(aFrame));
    }

    if (!QueueAnimatedContentForRefresh(aFrame->GetContent(), 60)) {
      NS_WARNING("unable to animate progress widget!");
    }
  }
}

HANDLE
nsNativeThemeWin::GetTheme(uint8_t aWidgetType)
{ 
  if (WinUtils::GetWindowsVersion() < WinUtils::VISTA_VERSION) {
    // On XP or earlier, render dropdowns as textfields;
    // doing it the right way works fine with the MS themes,
    // but breaks on a lot of custom themes (presumably because MS
    // apps do the textfield border business as well).
    if (aWidgetType == NS_THEME_DROPDOWN)
      aWidgetType = NS_THEME_TEXTFIELD;
  }

  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX:
    case NS_THEME_GROUPBOX:
      return nsUXThemeData::GetTheme(eUXButton);
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
      return nsUXThemeData::GetTheme(eUXEdit);
    case NS_THEME_TOOLTIP:
      // XP/2K3 should force a classic treatment of tooltips
      return WinUtils::GetWindowsVersion() < WinUtils::VISTA_VERSION ?
        NULL : nsUXThemeData::GetTheme(eUXTooltip);
    case NS_THEME_TOOLBOX:
      return nsUXThemeData::GetTheme(eUXRebar);
    case NS_THEME_WIN_MEDIA_TOOLBOX:
      return nsUXThemeData::GetTheme(eUXMediaRebar);
    case NS_THEME_WIN_COMMUNICATIONS_TOOLBOX:
      return nsUXThemeData::GetTheme(eUXCommunicationsRebar);
    case NS_THEME_WIN_BROWSER_TAB_BAR_TOOLBOX:
      return nsUXThemeData::GetTheme(eUXBrowserTabBarRebar);
    case NS_THEME_TOOLBAR:
    case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_TOOLBAR_SEPARATOR:
      return nsUXThemeData::GetTheme(eUXToolbar);
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
      return nsUXThemeData::GetTheme(eUXProgress);
    case NS_THEME_TAB:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS:
      return nsUXThemeData::GetTheme(eUXTab);
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
      return nsUXThemeData::GetTheme(eUXScrollbar);
    case NS_THEME_RANGE:
    case NS_THEME_RANGE_THUMB:
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:
      return nsUXThemeData::GetTheme(eUXTrackbar);
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON:
      return nsUXThemeData::GetTheme(eUXSpin);
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_RESIZER:
      return nsUXThemeData::GetTheme(eUXStatus);
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
      return nsUXThemeData::GetTheme(eUXCombobox);
    case NS_THEME_TREEVIEW_HEADER_CELL:
    case NS_THEME_TREEVIEW_HEADER_SORTARROW:
      return nsUXThemeData::GetTheme(eUXHeader);
    case NS_THEME_LISTBOX:
    case NS_THEME_LISTBOX_LISTITEM:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TREEVIEW_TWISTY_OPEN:
    case NS_THEME_TREEVIEW_TREEITEM:
      return nsUXThemeData::GetTheme(eUXListview);
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
      return nsUXThemeData::GetTheme(eUXMenu);
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
      return nsUXThemeData::GetTheme(eUXWindowFrame);
  }
  return NULL;
}

int32_t
nsNativeThemeWin::StandardGetState(nsIFrame* aFrame, uint8_t aWidgetType,
                                   bool wantFocused)
{
  nsEventStates eventState = GetContentState(aFrame, aWidgetType);
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
  if (content->IsXUL() &&
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
  if (WinUtils::GetWindowsVersion() < WinUtils::VISTA_VERSION) {
    // See GetTheme
    if (aWidgetType == NS_THEME_DROPDOWN)
      aWidgetType = NS_THEME_TEXTFIELD;
  }

  switch (aWidgetType) {
    case NS_THEME_BUTTON: {
      aPart = BP_BUTTON;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      nsEventStates eventState = GetContentState(aFrame, aWidgetType);
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

        nsEventStates eventState = GetContentState(isXULCheckboxRadio ? aFrame->GetParent()
                                                                      : aFrame,
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
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE: {
      nsEventStates eventState = GetContentState(aFrame, aWidgetType);

      if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION) {
        /* Note: the NOSCROLL type has a rounded corner in each
         * corner.  The more specific HSCROLL, VSCROLL, HVSCROLL types
         * have side and/or top/bottom edges rendered as straight
         * horizontal lines with sharp corners to accommodate a
         * scrollbar.  However, the scrollbar gets rendered on top of
         * this for us, so we don't care, and can just use NOSCROLL
         * here.
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
          if (content && content->IsXUL() && IsFocused(aFrame))
            aState = TFS_EDITBORDER_FOCUSED;
          else if (eventState.HasAtLeastOneOfStates(NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_FOCUS))
            aState = TFS_EDITBORDER_FOCUSED;
          else if (eventState.HasState(NS_EVENT_STATE_HOVER))
            aState = TFS_EDITBORDER_HOVER;
          else
            aState = TFS_EDITBORDER_NORMAL;
        }
      } else {
        aPart = TFP_TEXTFIELD;
        
        if (!aFrame)
          aState = TS_NORMAL;
        else if (IsDisabled(aFrame, eventState))
          aState = TS_DISABLED;
        else if (IsReadOnly(aFrame))
          aState = TFS_READONLY;
        else
          aState = StandardGetState(aFrame, aWidgetType, true);
      }

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
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL: {
      nsIFrame* parentFrame = aFrame->GetParent();
      nsEventStates eventStates = GetContentState(parentFrame, aWidgetType);
      if (aWidgetType == NS_THEME_PROGRESSBAR_CHUNK_VERTICAL ||
          IsVerticalProgress(parentFrame)) {
        aPart = WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION ?
          PP_FILLVERT : PP_CHUNKVERT;
      } else {
        aPart = WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION ?
          PP_FILL : PP_CHUNK;
      }

      aState = PBBVS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TOOLBAR_BUTTON: {
      aPart = BP_BUTTON;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      nsEventStates eventState = GetContentState(aFrame, aWidgetType);
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
    case NS_THEME_TOOLBAR_SEPARATOR: {
      aPart = TP_SEPARATOR;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT: {
      aPart = SP_BUTTON;
      aState = (aWidgetType - NS_THEME_SCROLLBAR_BUTTON_UP)*4;
      nsEventStates eventState = GetContentState(aFrame, aWidgetType);
      if (!aFrame)
        aState += TS_NORMAL;
      else if (IsDisabled(aFrame, eventState))
        aState += TS_DISABLED;
      else {
        nsIFrame *parent = aFrame->GetParent();
        nsEventStates parentState = GetContentState(parent, parent->StyleDisplay()->mAppearance);
        if (eventState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE))
          aState += TS_ACTIVE;
        else if (eventState.HasState(NS_EVENT_STATE_HOVER))
          aState += TS_HOVER;
        else if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION &&
                 parentState.HasState(NS_EVENT_STATE_HOVER))
          aState = (aWidgetType - NS_THEME_SCROLLBAR_BUTTON_UP) + SP_BUTTON_IMPLICIT_HOVER_BASE;
        else
          aState += TS_NORMAL;
      }
      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL: {
      aPart = (aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL) ?
              SP_TRACKSTARTHOR : SP_TRACKSTARTVERT;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL: {
      aPart = (aWidgetType == NS_THEME_SCROLLBAR_THUMB_HORIZONTAL) ?
              SP_THUMBHOR : SP_THUMBVERT;
      nsEventStates eventState = GetContentState(aFrame, aWidgetType);
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
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL: {
      if (aWidgetType == NS_THEME_RANGE_THUMB) {
        if (IsRangeHorizontal(aFrame)) {
          aPart = TKP_THUMBBOTTOM;
        } else {
          aPart = IsFrameRTL(aFrame) ? TKP_THUMBLEFT : TKP_THUMBRIGHT;
        }
      } else {
        aPart = (aWidgetType == NS_THEME_SCALE_THUMB_HORIZONTAL) ?
                TKP_THUMB : TKP_THUMBVERT;
      }
      nsEventStates eventState = GetContentState(aFrame, aWidgetType);
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
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON: {
      aPart = (aWidgetType == NS_THEME_SPINNER_UP_BUTTON) ?
              SPNP_UP : SPNP_DOWN;
      nsEventStates eventState = GetContentState(aFrame, aWidgetType);
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
    case NS_THEME_WIN_BROWSER_TAB_BAR_TOOLBOX:
    case NS_THEME_STATUSBAR:
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_SMALL: {
      aState = 0;
      if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION) {
        // On vista, they have a part
        aPart = RP_BACKGROUND;
      } else {
        // Otherwise, they don't.  (But I bet
        // RP_BACKGROUND would work here, too);
        aPart = 0;
      }
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
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_RESIZER: {
      aPart = (aWidgetType - NS_THEME_STATUSBAR_PANEL) + 1;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TREEVIEW:
    case NS_THEME_LISTBOX: {
      aPart = TREEVIEW_BODY;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TAB_PANELS: {
      aPart = TABP_PANELS;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TAB_PANEL: {
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

      nsEventStates eventState = GetContentState(aFrame, aWidgetType);
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
    case NS_THEME_TREEVIEW_HEADER_SORTARROW: {
      // XXX Probably will never work due to a bug in the Luna theme.
      aPart = 4;
      aState = 1;
      return NS_OK;
    }
    case NS_THEME_TREEVIEW_HEADER_CELL: {
      aPart = 1;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }
      
      aState = StandardGetState(aFrame, aWidgetType, true);
      
      return NS_OK;
    }
    case NS_THEME_DROPDOWN: {
      nsIContent* content = aFrame->GetContent();
      bool isHTML = content && content->IsHTML();
      bool useDropBorder = isHTML || IsMenuListEditable(aFrame);
      nsEventStates eventState = GetContentState(aFrame, aWidgetType);

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
    case NS_THEME_DROPDOWN_BUTTON: {
      bool isHTML = IsHTMLContent(aFrame);
      nsIFrame* parentFrame = aFrame->GetParent();
      bool isMenulist = !isHTML && parentFrame->GetType() == nsGkAtoms::menuFrame;
      bool isOpen = false;

      // HTML select and XUL menulist dropdown buttons get state from the parent.
      if (isHTML || isMenulist)
        aFrame = parentFrame;

      nsEventStates eventState = GetContentState(aFrame, aWidgetType);
      aPart = WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION ?
        CBP_DROPMARKER_VISTA : CBP_DROPMARKER;

      // For HTML controls with author styling, we should fall
      // back to the old dropmarker style to avoid clashes with
      // author-specified backgrounds and borders (bug #441034)
      if (isHTML && IsWidgetStyled(aFrame->PresContext(), aFrame, NS_THEME_DROPDOWN))
        aPart = CBP_DROPMARKER;

      if (IsDisabled(aFrame, eventState)) {
        aState = TS_DISABLED;
        return NS_OK;
      }

      if (isHTML) {
        nsIComboboxControlFrame* ccf = do_QueryFrame(aFrame);
        isOpen = (ccf && ccf->IsDroppedDown());
      }
      else
        isOpen = IsOpenButton(aFrame);

      if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION) {
        if (isHTML || IsMenuListEditable(aFrame)) {
          if (isOpen) {
            /* Hover is propagated, but we need to know whether we're
             * hovering just the combobox frame, not the dropdown frame.
             * But, we can't get that information, since hover is on the
             * content node, and they share the same content node.  So,
             * instead, we cheat -- if the dropdown is open, we always
             * show the hover state.  This looks fine in practice.
             */
            aState = TS_HOVER;
            return NS_OK;
          }
        } else {
          /* On Vista, the dropdown indicator on a menulist button in  
           * chrome is not given a hover effect. When the frame isn't
           * isn't HTML content, we cheat and force the dropdown state
           * to be normal. (Bug 430434)
           */
          aState = TS_NORMAL;
          return NS_OK;
        }
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
      nsEventStates eventState = GetContentState(aFrame, aWidgetType);

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
        nsEventStates eventState = GetContentState(aFrame, aWidgetType);
        aState = IsDisabled(aFrame, eventState) ? MSM_DISABLED : MSM_NORMAL;
        return NS_OK;
      }
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
      {
        bool isChecked;
        nsEventStates eventState = GetContentState(aFrame, aWidgetType);

        // NOTE: we can probably use NS_EVENT_STATE_CHECKED
        isChecked = CheckBooleanAttr(aFrame, nsGkAtoms::checked);

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
  if (aPart == MENU_POPUPITEM && aState == MBI_NORMAL) {
    return true;
  }
  return false;
}

NS_IMETHODIMP
nsNativeThemeWin::DrawWidgetBackground(nsRenderingContext* aContext,
                                       nsIFrame* aFrame,
                                       uint8_t aWidgetType,
                                       const nsRect& aRect,
                                       const nsRect& aDirtyRect)
{
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

  gfxFloat p2a = gfxFloat(aContext->AppUnitsPerDevPixel());
  RECT widgetRect;
  RECT clipRect;
  gfxRect tr(aRect.x, aRect.y, aRect.width, aRect.height),
          dr(aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);

  tr.ScaleInverse(p2a);
  dr.ScaleInverse(p2a);

  /* See GetWidgetOverflow */
  if (aWidgetType == NS_THEME_DROPDOWN_BUTTON &&
      part == CBP_DROPMARKER_VISTA && IsHTMLContent(aFrame))
  {
    tr.y -= 1.0;
    tr.width += 1.0;
    tr.height += 2.0;

    dr.y -= 1.0;
    dr.width += 1.0;
    dr.height += 2.0;

    if (IsFrameRTL(aFrame)) {
      tr.x -= 1.0;
      dr.x -= 1.0;
    }
  }

  nsRefPtr<gfxContext> ctx = aContext->ThebesContext();

  gfxWindowsNativeDrawing nativeDrawing(ctx, dr, GetWidgetNativeDrawingFlags(aWidgetType));

RENDER_AGAIN:

  HDC hdc = nativeDrawing.BeginNativeDrawing();
  if (!hdc)
    return NS_ERROR_FAILURE;

  nativeDrawing.TransformToNativeRect(tr, widgetRect);
  nativeDrawing.TransformToNativeRect(dr, clipRect);

#if 0
  {
    PR_LOG(gWindowsLog, PR_LOG_ERROR,
           (stderr, "xform: %f %f %f %f [%f %f]\n", m.xx, m.yx, m.xy, m.yy, 
            m.x0, m.y0));
    PR_LOG(gWindowsLog, PR_LOG_ERROR,
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
        nsEventStates eventState = GetContentState(aFrame, aWidgetType);

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
    GetThemePartSize(theme, hdc, MENU_POPUPBORDERS, 0, NULL, TS_TRUE, &borderSize);

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
  // The following widgets need to be RTL-aware
  else if (aWidgetType == NS_THEME_MENUARROW ||
           aWidgetType == NS_THEME_RESIZER ||
           aWidgetType == NS_THEME_DROPDOWN_BUTTON)
  {
    DrawThemeBGRTLAware(theme, hdc, part, state,
                        &widgetRect, &clipRect, IsFrameRTL(aFrame));
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
  else if (aWidgetType == NS_THEME_PROGRESSBAR_CHUNK ||
           aWidgetType == NS_THEME_PROGRESSBAR_CHUNK_VERTICAL) {
    DrawThemedProgressMeter(aFrame, aWidgetType, theme, hdc, part, state,
                            &widgetRect, &clipRect, p2a);
  }
  // If part is negative, the element wishes us to not render a themed
  // background, instead opting to be drawn specially below.
  else if (part >= 0) {
    DrawThemeBackground(theme, hdc, part, state, &widgetRect, &clipRect);
  }

  // Draw focus rectangles for XP HTML checkboxes and radio buttons
  // XXX it'd be nice to draw these outside of the frame
  if (((aWidgetType == NS_THEME_CHECKBOX || aWidgetType == NS_THEME_RADIO) &&
        aFrame->GetContent()->IsHTML()) ||
      aWidgetType == NS_THEME_RANGE ||
      aWidgetType == NS_THEME_SCALE_HORIZONTAL ||
      aWidgetType == NS_THEME_SCALE_VERTICAL) {
      nsEventStates contentState = GetContentState(aFrame, aWidgetType);

      if (contentState.HasState(NS_EVENT_STATE_FOCUS)) {
        POINT vpOrg;
        HPEN hPen = nullptr;

        uint8_t id = SaveDC(hdc);

        ::SelectClipRgn(hdc, NULL);
        ::GetViewportOrgEx(hdc, &vpOrg);
        ::SetBrushOrgEx(hdc, vpOrg.x + widgetRect.left, vpOrg.y + widgetRect.top, NULL);

        // On vista, choose our own colors and draw an XP style half focus rect
        // for focused checkboxes and a full rect when active.
        if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION &&
            aWidgetType == NS_THEME_CHECKBOX) {
          LOGBRUSH lb;
          lb.lbStyle = BS_SOLID;
          lb.lbColor = RGB(255,255,255);
          lb.lbHatch = 0;

          hPen = ::ExtCreatePen(PS_COSMETIC|PS_ALTERNATE, 1, &lb, 0, NULL);
          ::SelectObject(hdc, hPen);

          // If pressed, draw the upper left corner of the dotted rect.
          if (contentState.HasState(NS_EVENT_STATE_ACTIVE)) {
            ::MoveToEx(hdc, widgetRect.left, widgetRect.bottom-1, NULL);
            ::LineTo(hdc, widgetRect.left, widgetRect.top);
            ::LineTo(hdc, widgetRect.right-1, widgetRect.top);
          }

          // Draw the lower right corner of the dotted rect.
          ::MoveToEx(hdc, widgetRect.right-1, widgetRect.top, NULL);
          ::LineTo(hdc, widgetRect.right-1, widgetRect.bottom-1);
          ::LineTo(hdc, widgetRect.left, widgetRect.bottom-1);
        } else {
          ::SetTextColor(hdc, 0);
          ::DrawFocusRect(hdc, &widgetRect);
        }
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
    DrawThemeEdge(theme, hdc, RP_BAND, 0, &widgetRect, EDGE_ETCHED, BF_TOP, NULL);
  }
  else if (aWidgetType == NS_THEME_SCROLLBAR_THUMB_HORIZONTAL ||
           aWidgetType == NS_THEME_SCROLLBAR_THUMB_VERTICAL)
  {
    // Draw the decorative gripper for the scrollbar thumb button, if it fits

    SIZE gripSize;
    MARGINS thumbMgns;
    int gripPart = (aWidgetType == NS_THEME_SCROLLBAR_THUMB_HORIZONTAL) ?
                   SP_GRIPPERHOR : SP_GRIPPERVERT;

    if (GetThemePartSize(theme, hdc, gripPart, state, NULL, TS_TRUE, &gripSize) == S_OK &&
        GetThemeMargins(theme, hdc, part, state, TMT_CONTENTMARGINS, NULL, &thumbMgns) == S_OK &&
        gripSize.cx + thumbMgns.cxLeftWidth + thumbMgns.cxRightWidth <= widgetRect.right - widgetRect.left &&
        gripSize.cy + thumbMgns.cyTopHeight + thumbMgns.cyBottomHeight <= widgetRect.bottom - widgetRect.top)
    {
      DrawThemeBackground(theme, hdc, gripPart, state, &widgetRect, &clipRect);
    }
  }
  else if ((aWidgetType == NS_THEME_WINDOW_BUTTON_BOX ||
            aWidgetType == NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED) &&
            nsUXThemeData::CheckForCompositor())
  {
    // The caption buttons are drawn by the DWM, we just need to clear the area where they
    // are because we might have drawn something above them (like a background-image).
    ctx->Save();
    ctx->ResetClip();
    ctx->Translate(dr.TopLeft());

    // Create a rounded rectangle to follow the buttons' look.
    gfxRect buttonbox1(0.0, 0.0, dr.Width(), dr.Height() - 2.0);
    gfxRect buttonbox2(1.0, dr.Height() - 2.0, dr.Width() - 1.0, 1.0);
    gfxRect buttonbox3(2.0, dr.Height() - 1.0, dr.Width() - 3.0, 1.0);

    gfxContext::GraphicsOperator currentOp = ctx->CurrentOperator();
    ctx->SetOperator(gfxContext::OPERATOR_CLEAR);

   // Each rectangle is drawn individually because OPERATOR_CLEAR takes
   // the fallback path to cairo_d2d_acquire_dest if the area to fill
   // is a complex region.
    ctx->NewPath();
    ctx->Rectangle(buttonbox1, true);
    ctx->Fill();

    ctx->NewPath();
    ctx->Rectangle(buttonbox2, true);
    ctx->Fill();

    ctx->NewPath();
    ctx->Rectangle(buttonbox3, true);
    ctx->Fill();

    ctx->Restore();
    ctx->SetOperator(currentOp);
  }

  nativeDrawing.EndNativeDrawing();

  if (nativeDrawing.ShouldRenderAgain())
    goto RENDER_AGAIN;

  nativeDrawing.PaintToContext();

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeWin::GetWidgetBorder(nsDeviceContext* aContext, 
                                  nsIFrame* aFrame,
                                  uint8_t aWidgetType,
                                  nsIntMargin* aResult)
{
  HANDLE theme = GetTheme(aWidgetType);
  if (!theme)
    return ClassicGetWidgetBorder(aContext, aFrame, aWidgetType, aResult); 

  (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right = 0;

  if (!WidgetIsContainer(aWidgetType) ||
      aWidgetType == NS_THEME_TOOLBOX || 
      aWidgetType == NS_THEME_WIN_MEDIA_TOOLBOX ||
      aWidgetType == NS_THEME_WIN_COMMUNICATIONS_TOOLBOX ||
      aWidgetType == NS_THEME_WIN_BROWSER_TAB_BAR_TOOLBOX ||
      aWidgetType == NS_THEME_STATUSBAR || 
      aWidgetType == NS_THEME_RESIZER || aWidgetType == NS_THEME_TAB_PANEL ||
      aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL ||
      aWidgetType == NS_THEME_SCROLLBAR_TRACK_VERTICAL ||
      aWidgetType == NS_THEME_MENUITEM || aWidgetType == NS_THEME_CHECKMENUITEM ||
      aWidgetType == NS_THEME_RADIOMENUITEM || aWidgetType == NS_THEME_MENUPOPUP ||
      aWidgetType == NS_THEME_MENUIMAGE || aWidgetType == NS_THEME_MENUITEMTEXT ||
      aWidgetType == NS_THEME_TOOLBAR_SEPARATOR ||
      aWidgetType == NS_THEME_WINDOW_TITLEBAR ||
      aWidgetType == NS_THEME_WINDOW_TITLEBAR_MAXIMIZED ||
      aWidgetType == NS_THEME_WIN_GLASS || aWidgetType == NS_THEME_WIN_BORDERLESS_GLASS)
    return NS_OK; // Don't worry about it.

  int32_t part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return rv;

  if (aWidgetType == NS_THEME_TOOLBAR) {
    // make space for the separator line above all toolbars but the first
    if (state == 0)
      aResult->top = TB_SEPARATOR_HEIGHT;
    return NS_OK;
  }

  // Get our info.
  RECT outerRect; // Create a fake outer rect.
  outerRect.top = outerRect.left = 100;
  outerRect.right = outerRect.bottom = 200;
  RECT contentRect(outerRect);
  HRESULT res = GetThemeBackgroundContentRect(theme, NULL, part, state, &outerRect, &contentRect);
  
  if (FAILED(res))
    return NS_ERROR_FAILURE;

  // Now compute the delta in each direction and place it in our
  // nsIntMargin struct.
  aResult->top = contentRect.top - outerRect.top;
  aResult->bottom = outerRect.bottom - contentRect.bottom;
  aResult->left = contentRect.left - outerRect.left;
  aResult->right = outerRect.right - contentRect.right;

  // Remove the edges for tabs that are before or after the selected tab,
  if (aWidgetType == NS_THEME_TAB) {
    if (IsLeftToSelectedTab(aFrame))
      // Remove the right edge, since we won't be drawing it.
      aResult->right = 0;
    else if (IsRightToSelectedTab(aFrame))
      // Remove the left edge, since we won't be drawing it.
      aResult->left = 0;
  }

  if (aFrame && (aWidgetType == NS_THEME_TEXTFIELD || aWidgetType == NS_THEME_TEXTFIELD_MULTILINE)) {
    nsIContent* content = aFrame->GetContent();
    if (content && content->IsHTML()) {
      // We need to pad textfields by 1 pixel, since the caret will draw
      // flush against the edge by default if we don't.
      aResult->top++;
      aResult->left++;
      aResult->bottom++;
      aResult->right++;
    }
  }

  return NS_OK;
}

bool
nsNativeThemeWin::GetWidgetPadding(nsDeviceContext* aContext, 
                                   nsIFrame* aFrame,
                                   uint8_t aWidgetType,
                                   nsIntMargin* aResult)
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

  HANDLE theme = GetTheme(aWidgetType);

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
    return true;
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
    return true;
  }

  if (!theme)
    return ClassicGetWidgetPadding(aContext, aFrame, aWidgetType, aResult);

  if (aWidgetType == NS_THEME_MENUPOPUP)
  {
    SIZE popupSize;
    GetThemePartSize(theme, NULL, MENU_POPUPBORDERS, /* state */ 0, NULL, TS_TRUE, &popupSize);
    aResult->top = aResult->bottom = popupSize.cy;
    aResult->left = aResult->right = popupSize.cx;
    return true;
  }

  if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION) {
    if (aWidgetType == NS_THEME_TEXTFIELD ||
        aWidgetType == NS_THEME_TEXTFIELD_MULTILINE ||
        aWidgetType == NS_THEME_DROPDOWN)
    {
      /* If we have author-specified padding for these elements, don't do the fixups below */
      if (aFrame->PresContext()->HasAuthorSpecifiedRules(aFrame, NS_AUTHOR_SPECIFIED_PADDING))
        return false;
    }

    /* textfields need extra pixels on all sides, otherwise they
     * wrap their content too tightly.  The actual border is drawn 1px
     * inside the specified rectangle, so Gecko will end up making the
     * contents look too small.  Instead, we add 2px padding for the
     * contents and fix this. (Used to be 1px added, see bug 430212)
     */
    if (aWidgetType == NS_THEME_TEXTFIELD || aWidgetType == NS_THEME_TEXTFIELD_MULTILINE) {
      aResult->top = aResult->bottom = 2;
      aResult->left = aResult->right = 2;
      return true;
    } else if (IsHTMLContent(aFrame) && aWidgetType == NS_THEME_DROPDOWN) {
      /* For content menulist controls, we need an extra pixel so
       * that we have room to draw our focus rectangle stuff.
       * Otherwise, the focus rect might overlap the control's
       * border.
       */
      aResult->top = aResult->bottom = 1;
      aResult->left = aResult->right = 1;
      return true;
    }
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
          SIZE size(GetGutterSize(theme, NULL));
          left = size.cx + 2;
        }
        break;
    case NS_THEME_MENUSEPARATOR:
        {
          SIZE size(GetGutterSize(theme, NULL));
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
  
  return true;
}

bool
nsNativeThemeWin::GetWidgetOverflow(nsDeviceContext* aContext, 
                                    nsIFrame* aFrame,
                                    uint8_t aOverflowRect,
                                    nsRect* aResult)
{
  /* This is disabled for now, because it causes invalidation problems --
   * see bug 420381.  The effect of not updating the overflow area is that
   * for dropdown buttons in content areas, there is a 1px border on 3 sides
   * where, if invalidated, the dropdown control probably won't be repainted.
   * This is fairly minor, as by default there is nothing in that area, and
   * a border only shows up if the widget is being hovered.
   */
#if 0
  if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION) {
    /* We explicitly draw dropdown buttons in HTML content 1px bigger
     * up, right, and bottom so that they overlap the dropdown's border
     * like they're supposed to.
     */
    if (aWidgetType == NS_THEME_DROPDOWN_BUTTON &&
        IsHTMLContent(aFrame) &&
        !IsWidgetStyled(aFrame->GetParent()->PresContext(),
                        aFrame->GetParent(),
                        NS_THEME_DROPDOWN))
    {
      int32_t p2a = aContext->AppUnitsPerDevPixel();
      /* Note: no overflow on the left */
      nsMargin m(p2a, p2a, p2a, 0);
      aOverflowRect->Inflate (m);
      return true;
    }
  }
#endif

  return false;
}

NS_IMETHODIMP
nsNativeThemeWin::GetMinimumWidgetSize(nsRenderingContext* aContext, nsIFrame* aFrame,
                                       uint8_t aWidgetType,
                                       nsIntSize* aResult, bool* aIsOverridable)
{
  (*aResult).width = (*aResult).height = 0;
  *aIsOverridable = true;

  HANDLE theme = GetTheme(aWidgetType);
  if (!theme)
    return ClassicGetMinimumWidgetSize(aContext, aFrame, aWidgetType, aResult, aIsOverridable);

  switch (aWidgetType) {
    case NS_THEME_GROUPBOX:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TOOLBOX:
    case NS_THEME_WIN_MEDIA_TOOLBOX:
    case NS_THEME_WIN_COMMUNICATIONS_TOOLBOX:
    case NS_THEME_WIN_BROWSER_TAB_BAR_TOOLBOX:
    case NS_THEME_TOOLBAR:
    case NS_THEME_STATUSBAR:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TAB_PANELS:
    case NS_THEME_TAB_PANEL:
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
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_DROPDOWN_BUTTON:
      return ClassicGetMinimumWidgetSize(aContext, aFrame, aWidgetType, aResult, aIsOverridable);

    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
      if(!IsTopLevelMenu(aFrame))
      {
        SIZE gutterSize(GetGutterSize(theme, NULL));
        aResult->width = gutterSize.cx;
        aResult->height = gutterSize.cy;
        return NS_OK;
      }
      break;

    case NS_THEME_MENUIMAGE:
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
      {
        SIZE boxSize(GetGutterSize(theme, NULL));
        aResult->width = boxSize.cx+2;
        aResult->height = boxSize.cy;
        *aIsOverridable = false;
      }

    case NS_THEME_MENUITEMTEXT:
      return NS_OK;

    case NS_THEME_MENUARROW:
      aResult->width = 26;
      aResult->height = 16;
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
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:
    {
      *aIsOverridable = false;
      // on Vista, GetThemePartAndState returns odd values for
      // scale thumbs, so use a hardcoded size instead.
      if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION) {
        if (aWidgetType == NS_THEME_SCALE_THUMB_HORIZONTAL ||
            (aWidgetType == NS_THEME_RANGE_THUMB && IsRangeHorizontal(aFrame))) {
          aResult->width = 12;
          aResult->height = 20;
        }
        else {
          aResult->width = 20;
          aResult->height = 12;
        }
        return NS_OK;
      }
      break;
    }

    case NS_THEME_TOOLBAR_SEPARATOR:
      // that's 2px left margin, 2px right margin and 2px separator
      // (the margin is drawn as part of the separator, though)
      aResult->width = 6;
      return NS_OK;

    case NS_THEME_BUTTON:
      // We should let HTML buttons shrink to their min size.
      // FIXME bug 403934: We should probably really separate
      // GetPreferredWidgetSize from GetMinimumWidgetSize, so callers can
      // use the one they want.
      if (aFrame->GetContent()->IsHTML()) {
        sizeReq = TS_MIN;
      }
      break;

    case NS_THEME_WINDOW_BUTTON_MAXIMIZE:
    case NS_THEME_WINDOW_BUTTON_RESTORE:
      // The only way to get accurate titlebar button info is to query a
      // window w/buttons when it's visible. nsWindow takes care of this and
      // stores that info in nsUXThemeData.
      QueryForButtonData(aFrame);
      aResult->width = nsUXThemeData::sCommandButtons[CMDBUTTONIDX_RESTORE].cx;
      aResult->height = nsUXThemeData::sCommandButtons[CMDBUTTONIDX_RESTORE].cy;
      // For XP, subtract 4 from system metrics dimensions.
      if (WinUtils::GetWindowsVersion() == WinUtils::WINXP_VERSION) {
        aResult->width -= 4;
        aResult->height -= 4;
      }
      AddPaddingRect(aResult, CAPTIONBUTTON_RESTORE);
      *aIsOverridable = false;
      return NS_OK;

    case NS_THEME_WINDOW_BUTTON_MINIMIZE:
      QueryForButtonData(aFrame);
      aResult->width = nsUXThemeData::sCommandButtons[CMDBUTTONIDX_MINIMIZE].cx;
      aResult->height = nsUXThemeData::sCommandButtons[CMDBUTTONIDX_MINIMIZE].cy;
      if (WinUtils::GetWindowsVersion() == WinUtils::WINXP_VERSION) {
        aResult->width -= 4;
        aResult->height -= 4;
      }
      AddPaddingRect(aResult, CAPTIONBUTTON_MINIMIZE);
      *aIsOverridable = false;
      return NS_OK;

    case NS_THEME_WINDOW_BUTTON_CLOSE:
      QueryForButtonData(aFrame);
      aResult->width = nsUXThemeData::sCommandButtons[CMDBUTTONIDX_CLOSE].cx;
      aResult->height = nsUXThemeData::sCommandButtons[CMDBUTTONIDX_CLOSE].cy;
      if (WinUtils::GetWindowsVersion() == WinUtils::WINXP_VERSION) {
        aResult->width -= 4;
        aResult->height -= 4;
      }
      AddPaddingRect(aResult, CAPTIONBUTTON_CLOSE);
      *aIsOverridable = false;
      return NS_OK;

    case NS_THEME_WINDOW_TITLEBAR:
    case NS_THEME_WINDOW_TITLEBAR_MAXIMIZED:
      aResult->height = GetSystemMetrics(SM_CYCAPTION);
      aResult->height += GetSystemMetrics(SM_CYFRAME);
      aResult->height += GetSystemMetrics(SM_CXPADDEDBORDER);
      *aIsOverridable = false;
      return NS_OK;

    case NS_THEME_WINDOW_BUTTON_BOX:
    case NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED:
      if (nsUXThemeData::CheckForCompositor()) {
        QueryForButtonData(aFrame);
        aResult->width = nsUXThemeData::sCommandButtons[CMDBUTTONIDX_BUTTONBOX].cx;
        aResult->height = nsUXThemeData::sCommandButtons[CMDBUTTONIDX_BUTTONBOX].cy
                          - GetSystemMetrics(SM_CYFRAME)
                          - GetSystemMetrics(SM_CXPADDEDBORDER);
        if (aWidgetType == NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED) {
          aResult->width += 1;
          aResult->height -= 2;
        }
        *aIsOverridable = false;
        return NS_OK;
      }
      break;

    case NS_THEME_WINDOW_FRAME_LEFT:
    case NS_THEME_WINDOW_FRAME_RIGHT:
    case NS_THEME_WINDOW_FRAME_BOTTOM:
      aResult->width = GetSystemMetrics(SM_CXFRAME);
      aResult->height = GetSystemMetrics(SM_CYFRAME);
      *aIsOverridable = false;
      return NS_OK;
  }

  int32_t part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return rv;

  HDC hdc = gfxWindowsPlatform::GetPlatform()->GetScreenDC();
  if (!hdc)
    return NS_ERROR_FAILURE;

  SIZE sz;
  GetThemePartSize(theme, hdc, part, state, NULL, sizeReq, &sz);
  aResult->width = sz.cx;
  aResult->height = sz.cy;

  switch(aWidgetType) {
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON:
      aResult->width++;
      aResult->height = aResult->height / 2 + 1;
      break;

    case NS_THEME_MENUSEPARATOR:
    {
      SIZE gutterSize(GetGutterSize(theme,hdc));
      aResult->width += gutterSize.cx;
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeWin::WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType, 
                                     nsIAtom* aAttribute, bool* aShouldRepaint)
{
  // Some widget types just never change state.
  if (aWidgetType == NS_THEME_TOOLBOX ||
      aWidgetType == NS_THEME_WIN_MEDIA_TOOLBOX ||
      aWidgetType == NS_THEME_WIN_COMMUNICATIONS_TOOLBOX ||
      aWidgetType == NS_THEME_WIN_BROWSER_TAB_BAR_TOOLBOX ||
      aWidgetType == NS_THEME_TOOLBAR ||
      aWidgetType == NS_THEME_STATUSBAR || aWidgetType == NS_THEME_STATUSBAR_PANEL ||
      aWidgetType == NS_THEME_STATUSBAR_RESIZER_PANEL ||
      aWidgetType == NS_THEME_PROGRESSBAR_CHUNK ||
      aWidgetType == NS_THEME_PROGRESSBAR_CHUNK_VERTICAL ||
      aWidgetType == NS_THEME_PROGRESSBAR ||
      aWidgetType == NS_THEME_PROGRESSBAR_VERTICAL ||
      aWidgetType == NS_THEME_TOOLTIP ||
      aWidgetType == NS_THEME_TAB_PANELS ||
      aWidgetType == NS_THEME_TAB_PANEL ||
      aWidgetType == NS_THEME_TOOLBAR_SEPARATOR ||
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
      aWidgetType == NS_THEME_WINDOW_BUTTON_MINIMIZE ||
      aWidgetType == NS_THEME_WINDOW_BUTTON_RESTORE) {
    *aShouldRepaint = true;
    return NS_OK;
  }

  // On Vista, the scrollbar buttons need to change state when the track has/doesn't have hover
  if (WinUtils::GetWindowsVersion() < WinUtils::VISTA_VERSION &&
      (aWidgetType == NS_THEME_SCROLLBAR_TRACK_VERTICAL || 
      aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL)) {
    *aShouldRepaint = false;
    return NS_OK;
  }

  // We need to repaint the dropdown arrow in vista HTML combobox controls when
  // the control is closed to get rid of the hover effect.
  if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION &&
      (aWidgetType == NS_THEME_DROPDOWN || aWidgetType == NS_THEME_DROPDOWN_BUTTON) &&
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
  return NS_OK;
}

bool 
nsNativeThemeWin::ThemeSupportsWidget(nsPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      uint8_t aWidgetType)
{
  // XXXdwh We can go even further and call the API to ask if support exists for
  // specific widgets.

  if (aPresContext && !aPresContext->PresShell()->IsThemeSupportEnabled())
    return false;

  HANDLE theme = NULL;
  if (aWidgetType == NS_THEME_CHECKBOX_CONTAINER)
    theme = GetTheme(NS_THEME_CHECKBOX);
  else if (aWidgetType == NS_THEME_RADIO_CONTAINER)
    theme = GetTheme(NS_THEME_RADIO);
  else
    theme = GetTheme(aWidgetType);

  if ((theme) || (!theme && ClassicThemeSupportsWidget(aPresContext, aFrame, aWidgetType)))
    // turn off theming for some HTML widgets styled by the page
    return (!IsWidgetStyled(aPresContext, aFrame, aWidgetType));
  
  return false;
}

bool 
nsNativeThemeWin::WidgetIsContainer(uint8_t aWidgetType)
{
  // XXXdwh At some point flesh all of this out.
  if (aWidgetType == NS_THEME_DROPDOWN_BUTTON || 
      aWidgetType == NS_THEME_RADIO ||
      aWidgetType == NS_THEME_CHECKBOX)
    return false;
  return true;
}

bool
nsNativeThemeWin::ThemeDrawsFocusForWidget(nsPresContext* aPresContext, nsIFrame* aFrame, uint8_t aWidgetType)
{
  return false;
}

bool
nsNativeThemeWin::ThemeNeedsComboboxDropmarker()
{
  return true;
}

nsITheme::Transparency
nsNativeThemeWin::GetWidgetTransparency(nsIFrame* aFrame, uint8_t aWidgetType)
{
  switch (aWidgetType) {
  case NS_THEME_SCROLLBAR_SMALL:
  case NS_THEME_SCROLLBAR:
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
  case NS_THEME_PROGRESSBAR_CHUNK:
  case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
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
nsNativeThemeWin::ClassicThemeSupportsWidget(nsPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      uint8_t aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_RESIZER:
    {
      // The classic native resizer has an opaque grey background which doesn't
      // match the usually white background of the scrollable container, so
      // only support the native resizer if not in a scrollframe.
      nsIFrame* parentFrame = aFrame->GetParent();
      return (!parentFrame || parentFrame->GetType() != nsGkAtoms::scrollFrame);
    }
    case NS_THEME_MENUBAR:
    case NS_THEME_MENUPOPUP:
      // Classic non-flat menus are handled almost entirely through CSS.
      if (!nsUXThemeData::sFlatMenus)
        return false;
    case NS_THEME_BUTTON:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
    case NS_THEME_RANGE:
    case NS_THEME_RANGE_THUMB:
    case NS_THEME_GROUPBOX:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_DROPDOWN:
    case NS_THEME_TOOLTIP:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS:
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

nsresult
nsNativeThemeWin::ClassicGetWidgetBorder(nsDeviceContext* aContext, 
                                  nsIFrame* aFrame,
                                  uint8_t aWidgetType,
                                  nsIntMargin* aResult)
{
  switch (aWidgetType) {
    case NS_THEME_GROUPBOX:
    case NS_THEME_BUTTON:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 2; 
      break;
    case NS_THEME_STATUSBAR:
      (*aResult).bottom = (*aResult).left = (*aResult).right = 0;
      (*aResult).top = 2;
      break;
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_TAB:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 2;
      break;
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL: {
      (*aResult).top = 1;      
      (*aResult).left = 1;
      (*aResult).bottom = 1;
      (*aResult).right = aFrame->GetNextSibling() ? 3 : 1;
      break;
    }    
    case NS_THEME_TOOLTIP:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 1;
      break;
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 1;
      break;
    case NS_THEME_MENUBAR:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 0;
      break;
    case NS_THEME_MENUPOPUP:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 3;
      break;
    default:
      (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right = 0;
      break;
  }
  return NS_OK;
}

bool
nsNativeThemeWin::ClassicGetWidgetPadding(nsDeviceContext* aContext,
                                   nsIFrame* aFrame,
                                   uint8_t aWidgetType,
                                   nsIntMargin* aResult)
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
nsNativeThemeWin::ClassicGetMinimumWidgetSize(nsRenderingContext* aContext, nsIFrame* aFrame,
                                       uint8_t aWidgetType,
                                       nsIntSize* aResult, bool* aIsOverridable)
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
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
      (*aResult).width = ::GetSystemMetrics(SM_CXVSCROLL);
      (*aResult).height = ::GetSystemMetrics(SM_CYVSCROLL);
      *aIsOverridable = false;
      break;
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
      (*aResult).width = ::GetSystemMetrics(SM_CXHSCROLL);
      (*aResult).height = ::GetSystemMetrics(SM_CYHSCROLL);
      *aIsOverridable = false;
      break;
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
      // XXX HACK We should be able to have a minimum height for the scrollbar
      // track.  However, this causes problems when uncollapsing a scrollbar
      // inside a tree.  See bug 201379 for details.

        //      (*aResult).height = ::GetSystemMetrics(SM_CYVTHUMB) << 1;
      break;
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
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
      (*aResult).width = 12;
      (*aResult).height = 20;
      *aIsOverridable = false;
      break;
    case NS_THEME_SCALE_THUMB_VERTICAL:
      (*aResult).width = 20;
      (*aResult).height = 12;
      *aIsOverridable = false;
      break;
    case NS_THEME_DROPDOWN_BUTTON:
      (*aResult).width = ::GetSystemMetrics(SM_CXVSCROLL);
      break;
    case NS_THEME_DROPDOWN:
    case NS_THEME_BUTTON:
    case NS_THEME_GROUPBOX:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_DROPDOWN_TEXTFIELD:      
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:      
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TOOLTIP:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS:
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
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
      (*aResult).width = ::GetSystemMetrics(SM_CXVSCROLL);
      (*aResult).height = ::GetSystemMetrics(SM_CYVTHUMB);
      // Without theming, divide the thumb size by two in order to look more
      // native
      if (!GetTheme(aWidgetType))
        (*aResult).height >>= 1;
      *aIsOverridable = false;
      break;
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
      (*aResult).width = ::GetSystemMetrics(SM_CXHTHUMB);
      (*aResult).height = ::GetSystemMetrics(SM_CYHSCROLL);
      // Without theming, divide the thumb size by two in order to look more
      // native
      if (!GetTheme(aWidgetType))
        (*aResult).width >>= 1;
      *aIsOverridable = false;
      break;
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
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
      nsEventStates contentState;

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
          if (uiData->mUserFocus == NS_STYLE_USER_FOCUS_NORMAL) {
            if (!aFrame->GetContent()->IsHTML())
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
      nsEventStates contentState;
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
      if (!content->IsXUL() &&
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
      bool isContainer = false;
      nsMenuFrame *menuFrame = do_QueryFrame(aFrame);
      nsEventStates eventState = GetContentState(aFrame, aWidgetType);

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
        isContainer = menuFrame->IsMenu();
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
      nsEventStates eventState = GetContentState(aFrame, aWidgetType);

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
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_RANGE:
    case NS_THEME_RANGE_THUMB:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:     
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:      
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TOOLTIP:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS:
    case NS_THEME_MENUBAR:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_GROUPBOX:
      // these don't use DrawFrameControl
      return NS_OK;
    case NS_THEME_DROPDOWN_BUTTON: {

      aPart = DFC_SCROLL;
      aState = DFCS_SCROLLCOMBOBOX;

      nsIFrame* parentFrame = aFrame->GetParent();
      bool isHTML = IsHTMLContent(aFrame);
      bool isMenulist = !isHTML && parentFrame->GetType() == nsGkAtoms::menuFrame;
      bool isOpen = false;

      // HTML select and XUL menulist dropdown buttons get state from the parent.
      if (isHTML || isMenulist)
        aFrame = parentFrame;

      nsEventStates eventState = GetContentState(aFrame, aWidgetType);

      if (IsDisabled(aFrame, eventState)) {
        aState |= DFCS_INACTIVE;
        return NS_OK;
      }

      if (isHTML) {
        nsIComboboxControlFrame* ccf = do_QueryFrame(aFrame);
        isOpen = (ccf && ccf->IsDroppedDown());
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
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT: {
      nsEventStates contentState = GetContentState(aFrame, aWidgetType);

      aPart = DFC_SCROLL;
      switch (aWidgetType) {
        case NS_THEME_SCROLLBAR_BUTTON_UP:
          aState = DFCS_SCROLLUP;
          break;
        case NS_THEME_SCROLLBAR_BUTTON_DOWN:
          aState = DFCS_SCROLLDOWN;
          break;
        case NS_THEME_SCROLLBAR_BUTTON_LEFT:
          aState = DFCS_SCROLLLEFT;
          break;
        case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
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
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON: {
      nsEventStates contentState = GetContentState(aFrame, aWidgetType);

      aPart = DFC_SCROLL;
      switch (aWidgetType) {
        case NS_THEME_SPINNER_UP_BUTTON:
          aState = DFCS_SCROLLUP;
          break;
        case NS_THEME_SPINNER_DOWN_BUTTON:
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

    HBITMAP hMonoBitmap = ::CreateBitmap(checkW, checkH, 1, 1, NULL);
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
      ::SetBrushOrgEx(hdc, vpOrg.x + rc.left, vpOrg.y + rc.top, NULL);
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

nsresult nsNativeThemeWin::ClassicDrawWidgetBackground(nsRenderingContext* aContext,
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

  gfxFloat p2a = gfxFloat(aContext->AppUnitsPerDevPixel());
  RECT widgetRect;
  gfxRect tr(aRect.x, aRect.y, aRect.width, aRect.height),
          dr(aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);

  tr.ScaleInverse(p2a);
  dr.ScaleInverse(p2a);

  nsRefPtr<gfxContext> ctx = aContext->ThebesContext();

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
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_RESIZER: {
      int32_t oldTA;
      // setup DC to make DrawFrameControl draw correctly
      oldTA = ::SetTextAlign(hdc, TA_TOP | TA_LEFT | TA_NOUPDATECP);
      ::DrawFrameControl(hdc, &widgetRect, part, state);
      ::SetTextAlign(hdc, oldTA);

      // Draw focus rectangles for HTML checkboxes and radio buttons
      // XXX it'd be nice to draw these outside of the frame
      if (focused && (aWidgetType == NS_THEME_CHECKBOX || aWidgetType == NS_THEME_RADIO)) {
        // setup DC to make DrawFocusRect draw correctly
        POINT vpOrg;
        ::GetViewportOrgEx(hdc, &vpOrg);
        ::SetBrushOrgEx(hdc, vpOrg.x + widgetRect.left, vpOrg.y + widgetRect.top, NULL);
        int32_t oldColor;
        oldColor = ::SetTextColor(hdc, 0);
        // draw focus rectangle
        ::DrawFocusRect(hdc, &widgetRect);
        ::SetTextColor(hdc, oldColor);
      }
      break;
    }
    // Draw controls with 2px 3D inset border
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_LISTBOX:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXTFIELD: {
      // Draw inset edge
      ::DrawEdge(hdc, &widgetRect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
      nsEventStates eventState = GetContentState(aFrame, aWidgetType);

      // Fill in background
      if (IsDisabled(aFrame, eventState) ||
          (aFrame->GetContent()->IsXUL() &&
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
    case NS_THEME_TAB_PANEL:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_RESIZER_PANEL: {
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_BTNFACE+1));

      break;
    }
    // Draw 3D inset statusbar panel
    case NS_THEME_STATUSBAR_PANEL: {
      if (aFrame->GetNextSibling())
        widgetRect.right -= 2; // space between sibling status panels

      ::DrawEdge(hdc, &widgetRect, BDR_SUNKENOUTER, BF_RECT | BF_MIDDLE);

      break;
    }
    // Draw scrollbar thumb
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
      ::DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_RECT | BF_MIDDLE);

      break;
    case NS_THEME_RANGE_THUMB:
    case NS_THEME_SCALE_THUMB_VERTICAL:
    case NS_THEME_SCALE_THUMB_HORIZONTAL: {
      nsEventStates eventState = GetContentState(aFrame, aWidgetType);

      ::DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_RECT | BF_SOFT | BF_MIDDLE | BF_ADJUST);
      if (IsDisabled(aFrame, eventState)) {
        DrawCheckedRect(hdc, widgetRect, COLOR_3DFACE, COLOR_3DHILIGHT,
                        (HBRUSH) COLOR_3DHILIGHT);
      }

      break;
    }
    // Draw scrollbar track background
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL: {

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
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_HIGHLIGHT+1));
      break;

    case NS_THEME_PROGRESSBAR_CHUNK: {
      nsIFrame* stateFrame = aFrame->GetParent();
      nsEventStates eventStates = GetContentState(stateFrame, aWidgetType);

      bool indeterminate = IsIndeterminateProgress(stateFrame, eventStates);
      bool vertical = IsVerticalProgress(stateFrame) ||
                      aWidgetType == NS_THEME_PROGRESSBAR_CHUNK_VERTICAL;
      int32_t overlayPart = GetProgressOverlayStyle(vertical);

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
    case NS_THEME_TAB_PANELS:
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
      rect.bottom -= 1;

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
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:

    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXTFIELD:
      return
        gfxWindowsNativeDrawing::CANNOT_DRAW_TO_COLOR_ALPHA |
        gfxWindowsNativeDrawing::CAN_AXIS_ALIGNED_SCALE |
        gfxWindowsNativeDrawing::CANNOT_COMPLEX_TRANSFORM;

    // need to check these others
    case NS_THEME_RANGE:
    case NS_THEME_RANGE_THUMB:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TOOLTIP:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_RESIZER:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS:
    case NS_THEME_MENUBAR:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_MENUITEM:
      break;

    // the dropdown button /almost/ renders correctly with scaling,
    // except that the graphic in the dropdown button (the downward arrow)
    // doesn't get scaled up.
    case NS_THEME_DROPDOWN_BUTTON:
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
