/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mike Pinkerton (pinkerton@netscape.com).
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Vladimir Vukicevic <vladimir@pobox.com> (HITheme rewrite)
 *    Josh Aas <josh@mozilla.com>
 *    Colin Barrett <cbarrett@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsNativeThemeCocoa.h"
#include "nsIRenderingContext.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsThemeConstants.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIAtom.h"
#include "nsIEventStateManager.h"
#include "nsINameSpaceManager.h"
#include "nsPresContext.h"
#include "nsILookAndFeel.h"
#include "nsWidgetAtoms.h"

#include "gfxContext.h"
#include "gfxQuartzSurface.h"

#define DRAW_IN_FRAME_DEBUG 0
#define SCROLLBARS_VISUAL_DEBUG 0

extern "C" {
  CG_EXTERN void CGContextSetCTM(CGContextRef, CGAffineTransform);
}

// Copied from nsLookAndFeel.h
// Apple hasn't defined a constant for scollbars with two arrows on each end, so we'll use this one.
static const int kThemeScrollBarArrowsBoth = 2;

#define HITHEME_ORIENTATION kHIThemeOrientationNormal

NS_IMPL_ISUPPORTS1(nsNativeThemeCocoa, nsITheme)


nsNativeThemeCocoa::nsNativeThemeCocoa()
{
  sListboxBGTransparent = PR_TRUE;
}

nsNativeThemeCocoa::~nsNativeThemeCocoa()
{
}


void
nsNativeThemeCocoa::DrawCheckboxRadio(CGContextRef cgContext, ThemeButtonKind inKind,
                                      const HIRect& inBoxRect, PRBool inChecked,
                                      PRBool inDisabled, PRInt32 inState)
{
  HIThemeButtonDrawInfo bdi;
  bdi.version = 0;
  bdi.kind = inKind;

  if (inDisabled)
    bdi.state = kThemeStateUnavailable;
  else if ((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER))
    bdi.state = kThemeStatePressed;
  else
    bdi.state = kThemeStateActive;

  bdi.value = inChecked ? kThemeButtonOn : kThemeButtonOff;
  bdi.adornment = (inState & NS_EVENT_STATE_FOCUS) ? kThemeAdornmentFocus : kThemeAdornmentNone;

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.8);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  HIThemeDrawButton(&inBoxRect, &bdi, cgContext, HITHEME_ORIENTATION, NULL);
}


void
nsNativeThemeCocoa::DrawButton(CGContextRef cgContext, ThemeButtonKind inKind,
                               const HIRect& inBoxRect, PRBool inIsDefault, PRBool inDisabled,
                               ThemeButtonValue inValue, ThemeButtonAdornment inAdornment,
                               PRInt32 inState)
{
  HIThemeButtonDrawInfo bdi;

  bdi.version = 0;
  bdi.kind = inKind;
  bdi.value = inValue;
  bdi.adornment = inAdornment;
  
  if (inDisabled)
    bdi.state = kThemeStateUnavailable;
  else if ((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER))
    bdi.state = kThemeStatePressed;
  else
    bdi.state = (inKind == kThemeArrowButton) ? kThemeStateUnavailable : kThemeStateActive;

  if (inState & NS_EVENT_STATE_FOCUS)
    bdi.adornment |= kThemeAdornmentFocus;

  if (inIsDefault && !inDisabled)
    bdi.adornment |= kThemeAdornmentDefault;

  // Certain buttons draw outside their frame with nsITheme, we adjust for that here.
  HIRect drawRect = inBoxRect;
  if (inKind == kThemePushButton ||
      inKind == kThemePopupButton) {
    // These kinds of buttons draw 2 pixels too tall.
    drawRect.size.height -= 2;
  }
  else if (inKind == kThemePushButtonSmall) {
    // These kinds of buttons draw 2 pixels too wide, one pixel too far down, and
    // two pixels too tall.
    drawRect.origin.x += 1;
    drawRect.origin.y -= 1;
    drawRect.size.width -= 2;
  }

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.8);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  HIThemeDrawButton(&drawRect, &bdi, cgContext, HITHEME_ORIENTATION, NULL);
}


void
nsNativeThemeCocoa::DrawSpinButtons(CGContextRef cgContext, ThemeButtonKind inKind,
                                    const HIRect& inBoxRect, PRBool inDisabled,
                                    ThemeDrawState inDrawState,
                                    ThemeButtonAdornment inAdornment,
                                    PRInt32 inState)
{
  HIThemeButtonDrawInfo bdi;
  bdi.version = 0;
  bdi.kind = inKind;
  bdi.state = inDrawState;
  bdi.value = kThemeButtonOff;
  bdi.adornment = inAdornment;

  if (inDisabled)
    bdi.state = kThemeStateUnavailable;

  HIThemeDrawButton(&inBoxRect, &bdi, cgContext, HITHEME_ORIENTATION, NULL);
}


void
nsNativeThemeCocoa::DrawFrame(CGContextRef cgContext, HIThemeFrameKind inKind,
                              const HIRect& inBoxRect, PRBool inIsDisabled, PRInt32 inState)
{
  HIThemeFrameDrawInfo fdi;
  fdi.version = 0;
  fdi.kind = inKind;
  fdi.state = inIsDisabled ? kThemeStateUnavailable : kThemeStateActive;
  // We do not draw focus rings for frame widgets because their complex layout has nasty
  // drawing bugs and it looks terrible.
  // fdi.isFocused = (inState & NS_EVENT_STATE_FOCUS) != 0;
  fdi.isFocused = 0;

  // HIThemeDrawFrame takes the rect for the content area of the frame, not
  // the bounding rect for the frame. Here we reduce the size of the rect we
  // will pass to make it the size of the content.
  HIRect drawRect = inBoxRect;
  if (inKind == kHIThemeFrameTextFieldSquare) {
    SInt32 frameOutset = 0;
    ::GetThemeMetric(kThemeMetricEditTextFrameOutset, &frameOutset);
    drawRect.origin.x += frameOutset;
    drawRect.origin.y += frameOutset;
    drawRect.size.width -= frameOutset * 2;
    drawRect.size.height -= frameOutset * 2;
  }

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.8);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  HIThemeDrawFrame(&drawRect, &fdi, cgContext, HITHEME_ORIENTATION);
}


void
nsNativeThemeCocoa::DrawProgress(CGContextRef cgContext,
                                 const HIRect& inBoxRect, PRBool inIsIndeterminate, 
                                 PRBool inIsHorizontal, PRInt32 inValue)
{
  HIThemeTrackDrawInfo tdi;
  static SInt32 sPhase = 0;

  tdi.version = 0;
  tdi.kind = inIsIndeterminate ? kThemeMediumIndeterminateBar: kThemeMediumProgressBar;
  tdi.bounds = inBoxRect;
  tdi.min = 0;
  tdi.max = 100;
  tdi.value = inValue;
  tdi.attributes = inIsHorizontal ? kThemeTrackHorizontal : 0;
  tdi.enableState = kThemeTrackActive;
  tdi.trackInfo.progress.phase = sPhase++; // animate for the next time we're called

  HIThemeDrawTrack(&tdi, NULL, cgContext, HITHEME_ORIENTATION);
}


void
nsNativeThemeCocoa::DrawTabPanel(CGContextRef cgContext, const HIRect& inBoxRect)
{
  HIThemeTabPaneDrawInfo tpdi;

  tpdi.version = 0;
  tpdi.state = kThemeStateActive;
  tpdi.direction = kThemeTabNorth;
  tpdi.size = kHIThemeTabSizeNormal;

  HIThemeDrawTabPane(&inBoxRect, &tpdi, cgContext, HITHEME_ORIENTATION);
}


void
nsNativeThemeCocoa::DrawScale(CGContextRef cgContext, const HIRect& inBoxRect,
                              PRBool inIsDisabled, PRInt32 inState,
                              PRBool inIsVertical, PRBool inIsReverse,
                              PRInt32 inCurrentValue,
                              PRInt32 inMinValue, PRInt32 inMaxValue)
{
  HIThemeTrackDrawInfo tdi;

  tdi.version = 0;
  tdi.kind = kThemeMediumSlider;
  tdi.bounds = inBoxRect;
  tdi.min = inMinValue;
  tdi.max = inMaxValue;
  tdi.value = inCurrentValue;
  tdi.attributes = kThemeTrackShowThumb;
  if (!inIsVertical)
    tdi.attributes |= kThemeTrackHorizontal;
  if (inIsReverse)
    tdi.attributes |= kThemeTrackRightToLeft;
  if (inState & NS_EVENT_STATE_FOCUS)
    tdi.attributes |= kThemeTrackHasFocus;
  tdi.enableState = inIsDisabled ? kThemeTrackDisabled : kThemeTrackActive;
  tdi.trackInfo.slider.thumbDir = kThemeThumbPlain;
  tdi.trackInfo.slider.pressState = 0;

  HIThemeDrawTrack(&tdi, NULL, cgContext, HITHEME_ORIENTATION);
}


void
nsNativeThemeCocoa::DrawTab(CGContextRef cgContext, const HIRect& inBoxRect,
                            PRBool inIsDisabled, PRBool inIsFrontmost,
                            PRBool inIsHorizontal, PRBool inTabBottom,
                            PRInt32 inState)
{
  HIThemeTabDrawInfo tdi;

  tdi.version = 0;

  if (inIsFrontmost) {
    if (inIsDisabled) 
      tdi.style = kThemeTabFrontUnavailable;
    else
      tdi.style = kThemeTabFront;
  } else {
    if (inIsDisabled)
      tdi.style = kThemeTabNonFrontUnavailable;
    else if ((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER))
      tdi.style = kThemeTabNonFrontPressed;
    else
      tdi.style = kThemeTabNonFront;  
  }

  // don't yet support vertical tabs
  tdi.direction = inTabBottom ? kThemeTabSouth : kThemeTabNorth;
  tdi.size = kHIThemeTabSizeNormal;
  tdi.adornment = kThemeAdornmentNone;

  HIThemeDrawTab(&inBoxRect, &tdi, cgContext, HITHEME_ORIENTATION, NULL);
}


static UInt8
ConvertToPressState(PRInt32 aButtonState, UInt8 aPressState)
{
  // If the button is pressed, return the press state passed in. Otherwise, return 0.
  return ((aButtonState & NS_EVENT_STATE_ACTIVE) && (aButtonState & NS_EVENT_STATE_HOVER)) ? aPressState : 0;
}


void 
nsNativeThemeCocoa::GetScrollbarPressStates(nsIFrame *aFrame, PRInt32 aButtonStates[])
{
  static nsIContent::AttrValuesArray attributeValues[] = {
    &nsWidgetAtoms::scrollbarUpTop,
    &nsWidgetAtoms::scrollbarDownTop,
    &nsWidgetAtoms::scrollbarUpBottom,
    &nsWidgetAtoms::scrollbarDownBottom,
    nsnull
  };

  // Get the state of any scrollbar buttons in our child frames
  for (nsIFrame *childFrame = aFrame->GetFirstChild(nsnull); 
       childFrame;
       childFrame = childFrame->GetNextSibling()) {

    nsIContent *childContent = childFrame->GetContent();
    if (!childContent) continue;
    PRInt32 attrIndex = childContent->FindAttrValueIn(kNameSpaceID_None, nsWidgetAtoms::sbattr, 
                                                      attributeValues, eCaseMatters);
    if (attrIndex < 0) continue;

    PRInt32 currentState = GetContentState(childFrame, NS_THEME_BUTTON);
    aButtonStates[attrIndex] = currentState;
  }
}


void
nsNativeThemeCocoa::GetScrollbarDrawInfo(HIThemeTrackDrawInfo& aTdi, nsIFrame *aFrame, 
                                         const HIRect& aRect, PRBool aShouldGetButtonStates)
{
  PRInt32 curpos = CheckIntAttr(aFrame, nsWidgetAtoms::curpos);
  PRInt32 minpos = CheckIntAttr(aFrame, nsWidgetAtoms::minpos);
  PRInt32 maxpos = CheckIntAttr(aFrame, nsWidgetAtoms::maxpos);

  PRBool isHorizontal = aFrame->GetContent()->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::orient, 
                                                          nsWidgetAtoms::horizontal, eCaseMatters);
  PRBool isSmall = aFrame->GetStyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL;

  aTdi.version = 0;
  aTdi.kind = isSmall ? kThemeSmallScrollBar : kThemeMediumScrollBar;
  aTdi.bounds = aRect;
  aTdi.min = minpos;
  aTdi.max = maxpos;
  aTdi.value = curpos;
  aTdi.attributes = kThemeTrackShowThumb;
  if (isHorizontal)
    aTdi.attributes |= kThemeTrackHorizontal;

  aTdi.trackInfo.scrollbar.viewsize = (SInt32)(isHorizontal ? (aRect.size.width) : (aRect.size.height));

  // Only go get these scrollbar button states if we need it. For example, there's no reaon to look up scrollbar button 
  // states when we're only creating a TrackDrawInfo to determine the size of the thumb.
  if (aShouldGetButtonStates) {
    PRInt32 buttonStates[] = {0, 0, 0, 0};
    GetScrollbarPressStates(aFrame, buttonStates);
    ThemeScrollBarArrowStyle arrowStyle;
    ::GetThemeScrollBarArrowStyle(&arrowStyle);
    // If all four buttons are visible
    if (arrowStyle == kThemeScrollBarArrowsBoth) {
      aTdi.trackInfo.scrollbar.pressState = ConvertToPressState(buttonStates[0], kThemeTopOutsideArrowPressed) |
                                            ConvertToPressState(buttonStates[1], kThemeTopInsideArrowPressed) |
                                            ConvertToPressState(buttonStates[2], kThemeBottomInsideArrowPressed) |
                                            ConvertToPressState(buttonStates[3], kThemeBottomOutsideArrowPressed);
    } else {
      // It seems that unless all four buttons are showing, kThemeTopOutsideArrowPressed is the correct constant for
      // the up scrollbar button.
      aTdi.trackInfo.scrollbar.pressState = ConvertToPressState(buttonStates[0], kThemeTopOutsideArrowPressed) |
                                            ConvertToPressState(buttonStates[2], kThemeTopOutsideArrowPressed) |
                                            ConvertToPressState(buttonStates[3], kThemeBottomOutsideArrowPressed);
    }
  }
}


void
nsNativeThemeCocoa::DrawScrollbar(CGContextRef aCGContext, const HIRect& aBoxRect, nsIFrame *aFrame)
{
  HIThemeTrackDrawInfo tdi;
  GetScrollbarDrawInfo(tdi, aFrame, aBoxRect, PR_TRUE); //True means we want the press states
  ::HIThemeDrawTrack(&tdi, NULL, aCGContext, HITHEME_ORIENTATION);
}


nsIFrame*
nsNativeThemeCocoa::GetParentScrollbarFrame(nsIFrame *aFrame)
{
  // Walk our parents to find a scrollbar frame
  nsIFrame *scrollbarFrame = aFrame;
  do {
    if (scrollbarFrame->GetType() == nsWidgetAtoms::scrollbarFrame) break;
  } while ((scrollbarFrame = scrollbarFrame->GetParent()));
  
  // We return null if we can't find a parent scrollbar frame
  return scrollbarFrame;
}


NS_IMETHODIMP
nsNativeThemeCocoa::DrawWidgetBackground(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                         PRUint8 aWidgetType, const nsRect& aRect,
                                         const nsRect& aClipRect)
{
  // setup to draw into the correct port
  nsCOMPtr<nsIDeviceContext> dctx;
  aContext->GetDeviceContext(*getter_AddRefs(dctx));
  PRInt32 p2a = dctx->AppUnitsPerDevPixel();

  nsRefPtr<gfxContext> thebesCtx = (gfxContext*)
    aContext->GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT);
  if (!thebesCtx)
    return NS_ERROR_FAILURE;

  thebesCtx->UpdateSurfaceClip();

  double offsetX = 0.0, offsetY = 0.0;
  nsRefPtr<gfxASurface> thebesSurface = thebesCtx->CurrentSurface(&offsetX, &offsetY);
  if (thebesSurface->GetType() != gfxASurface::SurfaceTypeQuartz) {
    fprintf(stderr, "Expected surface of type Quartz, got %d\n",
            thebesSurface->GetType());
    return NS_ERROR_FAILURE;
  }

  gfxMatrix mat = thebesCtx->CurrentMatrix();
  gfxQuartzSurface* quartzSurf = (gfxQuartzSurface*) (thebesSurface.get());
  CGContextRef cgContext = quartzSurf->GetCGContext();

  //fprintf (stderr, "surface: %p cgContext: %p\n", quartzSurf, cgContext);

  if (cgContext == nsnull ||
      ((((unsigned long)cgContext) & 0xffff) == 0x3f3f)) {
    fprintf (stderr, "********** Invalid CGContext!\n");
  }

  // Eventually we can just do a GetCTM and restore it with SetCTM,
  // but for now do a full save/restore
  CGAffineTransform mm0 = CGContextGetCTM(cgContext);

  CGContextSaveGState(cgContext);
  //CGContextSetCTM(cgContext, CGAffineTransformIdentity);

  // I -think- that this context will always have an identity
  // transform (since we don't maintain a transform on it in
  // cairo-land, and instead push/pop as needed)
  if (mat.HasNonTranslationOrFlip()) {
    // If we have a scale, I think we've already lost; so don't round
    // anything here
    CGContextConcatCTM(cgContext,
                       CGAffineTransformMake(mat.xx, mat.xy,
                                             mat.yx, mat.yy,
                                             mat.x0, mat.y0));
  } else {
    // Otherwise, we round the x0/y0, because otherwise things get rendered badly
    // XXX how should we be rounding the x0/y0?
    CGContextConcatCTM(cgContext,
                       CGAffineTransformMake(mat.xx, mat.xy,
                                             mat.yx, mat.yy,
                                             floor(mat.x0 + 0.5),
                                             floor(mat.y0 + 0.5)));
  }

#if 0
  if (1 /*aWidgetType == NS_THEME_TEXTFIELD*/) {
    fprintf(stderr, "Native theme drawing widget %d [%p] dis:%d in rect [%d %d %d %d]\n",
            aWidgetType, aFrame, IsDisabled(aFrame), aRect.x, aRect.y, aRect.width, aRect.height);
    fprintf(stderr, "Native theme xform[0]: [%f %f %f %f %f %f]\n",
            mm0.a, mm0.b, mm0.c, mm0.d, mm0.tx, mm0.ty);
    CGAffineTransform mm = CGContextGetCTM(cgContext);
    fprintf(stderr, "Native theme xform[1]: [%f %f %f %f %f %f]\n",
            mm.a, mm.b, mm.c, mm.d, mm.tx, mm.ty);
  }
#endif

  CGRect macRect = CGRectMake(NSAppUnitsToIntPixels(aRect.x, p2a),
                              NSAppUnitsToIntPixels(aRect.y, p2a),
                              NSAppUnitsToIntPixels(aRect.width, p2a),
                              NSAppUnitsToIntPixels(aRect.height, p2a));
  macRect.origin.x -= offsetX;
  macRect.origin.y -= offsetY;

#if 0
  fprintf(stderr, "    --> macRect %f %f %f %f\n",
          macRect.origin.x, macRect.origin.y, macRect.size.width, macRect.size.height);
  CGRect bounds = CGContextGetClipBoundingBox(cgContext);
  fprintf(stderr, "    --> clip bounds: %f %f %f %f\n",
          bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height);

  //CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 1.0, 0.1);
  //CGContextFillRect(cgContext, bounds);
#endif

  PRInt32 eventState = GetContentState(aFrame, aWidgetType);

  switch (aWidgetType) {
    case NS_THEME_DIALOG: {
      // XXXvlad -- I don't think we can just pass macRect here,
      // due to pattern phase, but it seems like it works correctly
      HIThemeBackgroundDrawInfo bdi = {
        version: 0,
        state: kThemeStateActive,
        kind: kThemeBackgroundPlacard
      };

      HIThemeApplyBackground(&macRect, &bdi, cgContext, HITHEME_ORIENTATION);
      CGContextFillRect(cgContext, macRect);
    }
      break;

    case NS_THEME_MENUPOPUP: {
      HIThemeMenuDrawInfo mdi = {
        version: 0,
        menuType: IsDisabled(aFrame) ? kThemeMenuTypeInactive : kThemeMenuTypePopUp
      };

      HIThemeDrawMenuBackground(&macRect, &mdi, cgContext, HITHEME_ORIENTATION);
    }
      break;

    case NS_THEME_MENUITEM: {
      // maybe use kThemeMenuItemHierBackground or PopUpBackground instead of just Plain?
      HIThemeMenuItemDrawInfo drawInfo = {
        version: 0,
        itemType: kThemeMenuItemPlain,
        state: (IsDisabled(aFrame) ? kThemeMenuDisabled :
                CheckBooleanAttr(aFrame, nsWidgetAtoms::mozmenuactive) ? kThemeMenuSelected :
                kThemeMenuActive)
      };

      // XXX pass in the menu rect instead of always using the item rect
      HIRect ignored;
      HIThemeDrawMenuItem(&macRect, &macRect, &drawInfo, cgContext, HITHEME_ORIENTATION, &ignored);
    }
      break;

    case NS_THEME_TOOLTIP:
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 0.78, 1.0);
      CGContextFillRect(cgContext, macRect);
      break;

    case NS_THEME_CHECKBOX:
      DrawCheckboxRadio(cgContext, kThemeCheckBox, macRect, IsChecked(aFrame), IsDisabled(aFrame), eventState);
      break;

    case NS_THEME_CHECKBOX_SMALL:
      if (aRect.height == 15) {
      	// draw at 14x16, see comment in GetMinimumWidgetSize
        // XXX this should probably happen earlier, before transform; this is very fragile
        macRect.size.height += 1.0;
      }
      DrawCheckboxRadio(cgContext, kThemeSmallCheckBox, macRect, IsChecked(aFrame), IsDisabled(aFrame), eventState);
      break;

    case NS_THEME_RADIO:
      DrawCheckboxRadio(cgContext, kThemeRadioButton, macRect, IsSelected(aFrame), IsDisabled(aFrame), eventState);
      break;

    case NS_THEME_RADIO_SMALL:
      if (aRect.height == 14) {
        // draw at 14x15, see comment in GetMinimumWidgetSize
        // XXX this should probably happen earlier, before transform; this is very fragile
        macRect.size.height += 1.0;
      }
      DrawCheckboxRadio(cgContext, kThemeSmallRadioButton, macRect,
                        IsSelected(aFrame), IsDisabled(aFrame), eventState);
      break;

    case NS_THEME_BUTTON:
    case NS_THEME_BUTTON_SMALL: {
      // Normal push buttons can only draw with a height of 20+ pixels. Small push
      // buttons can only draw at a height of 17 pixels. We can't draw buttons with
      // a height of 18 or 19 pixels, at least not with HITheme. So, we go down to
      // 17 pixel buttons when asked to draw 18 or 19 so that we don't draw outside
      // the frame. We just have to live with this until we switch to another API
      // for control rendering. Remember that the frame for a 20 pixel tall button
      // is 22 pixels because of the border and shadow.
      ThemeButtonKind buttonKind = kThemePushButton;
      if (macRect.size.height < 22)
        buttonKind = kThemePushButtonSmall;
      DrawButton(cgContext, buttonKind, macRect,
                 IsDefaultButton(aFrame), IsDisabled(aFrame),
                 kThemeButtonOn, kThemeAdornmentNone, eventState);
    }
      break;

    case NS_THEME_BUTTON_BEVEL:
      DrawButton(cgContext, kThemeMediumBevelButton, macRect,
                 IsDefaultButton(aFrame), IsDisabled(aFrame), 
                 kThemeButtonOff, kThemeAdornmentNone, eventState);
      break;

    case NS_THEME_SPINNER: {
      ThemeDrawState state = kThemeStateActive;
      nsIContent* content = aFrame->GetContent();
      if (content->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::state,
                               NS_LITERAL_STRING("up"), eCaseMatters)) {
        state = kThemeStatePressedUp;
      }
      else if (content->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::state,
                                    NS_LITERAL_STRING("down"), eCaseMatters)) {
        state = kThemeStatePressedDown;
      }

      DrawSpinButtons(cgContext, kThemeIncDecButton, macRect, IsDisabled(aFrame),
                      state, kThemeAdornmentNone, eventState);
    }
      break;

    case NS_THEME_TOOLBAR_BUTTON:
      DrawButton(cgContext, kThemePushButton, macRect,
                 IsDefaultButton(aFrame), IsDisabled(aFrame),
                 kThemeButtonOn, kThemeAdornmentNone, eventState);
      break;

    case NS_THEME_TOOLBAR_SEPARATOR: {
      HIThemeSeparatorDrawInfo sdi = { 0, kThemeStateActive };
      HIThemeDrawSeparator(&macRect, &sdi, cgContext, HITHEME_ORIENTATION);
    }
      break;

    case NS_THEME_TOOLBAR:
    case NS_THEME_TOOLBOX:
    case NS_THEME_STATUSBAR: {
      HIThemeHeaderDrawInfo hdi = { 0, kThemeStateActive, kHIThemeHeaderKindWindow };
      HIThemeDrawHeader(&macRect, &hdi, cgContext, HITHEME_ORIENTATION);
    }
      break;
      
    case NS_THEME_DROPDOWN:
      DrawButton(cgContext, kThemePopupButton, macRect,
                 IsDefaultButton(aFrame), IsDisabled(aFrame), 
                 kThemeButtonOn, kThemeAdornmentNone, eventState);
      break;

    case NS_THEME_DROPDOWN_BUTTON:
      DrawButton (cgContext, kThemeArrowButton, macRect, PR_FALSE,
                  IsDisabled(aFrame), kThemeButtonOn,
                  kThemeAdornmentArrowDownArrow, eventState);
      break;

    case NS_THEME_TEXTFIELD:
      // HIThemeSetFill is not available on 10.3
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);
      CGContextFillRect(cgContext, macRect);
      DrawFrame(cgContext, kHIThemeFrameTextFieldSquare,
                macRect, (IsDisabled(aFrame) || IsReadOnly(aFrame)), eventState);
      break;
      
    case NS_THEME_PROGRESSBAR:
      DrawProgress(cgContext, macRect, IsIndeterminateProgress(aFrame),
                   PR_TRUE, GetProgressValue(aFrame));
      break;

    case NS_THEME_PROGRESSBAR_VERTICAL:
      DrawProgress(cgContext, macRect, IsIndeterminateProgress(aFrame),
                   PR_FALSE, GetProgressValue(aFrame));
      break;

    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
      // do nothing, covered by the progress bar cases above
      break;

    case NS_THEME_TREEVIEW_TWISTY:
      DrawButton(cgContext, kThemeDisclosureButton, macRect, PR_FALSE, IsDisabled(aFrame), 
                 kThemeDisclosureRight, kThemeAdornmentNone, eventState);
      break;

    case NS_THEME_TREEVIEW_TWISTY_OPEN:
      DrawButton(cgContext, kThemeDisclosureButton, macRect, PR_FALSE, IsDisabled(aFrame), 
                 kThemeDisclosureDown, kThemeAdornmentNone, eventState);
      break;

    case NS_THEME_TREEVIEW_HEADER_CELL: {
      TreeSortDirection sortDirection = GetTreeSortDirection(aFrame);
      DrawButton(cgContext, kThemeListHeaderButton, macRect, PR_FALSE, IsDisabled(aFrame), 
                 sortDirection == eTreeSortDirection_Natural ? kThemeButtonOff : kThemeButtonOn,
                 sortDirection == eTreeSortDirection_Descending ?
                 kThemeAdornmentHeaderButtonSortUp : kThemeAdornmentNone, eventState);      
    }
      break;

    case NS_THEME_TREEVIEW_TREEITEM:
    case NS_THEME_TREEVIEW:
      // HIThemeSetFill is not available on 10.3
      // HIThemeSetFill(kThemeBrushWhite, NULL, cgContext, HITHEME_ORIENTATION);
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);
      CGContextFillRect(cgContext, macRect);
      break;

    case NS_THEME_TREEVIEW_HEADER:
      // do nothing, taken care of by individual header cells
    case NS_THEME_TREEVIEW_HEADER_SORTARROW:
      // do nothing, taken care of by treeview header
    case NS_THEME_TREEVIEW_LINE:
      // do nothing, these lines don't exist on macos
      break;

    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL: {
      PRInt32 curpos = CheckIntAttr(aFrame, nsWidgetAtoms::curpos);
      PRInt32 minpos = CheckIntAttr(aFrame, nsWidgetAtoms::minpos);
      PRInt32 maxpos = CheckIntAttr(aFrame, nsWidgetAtoms::maxpos);
      if (!maxpos)
        maxpos = 100;

      PRBool reverse = aFrame->GetContent()->
        AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::dir,
                    NS_LITERAL_STRING("reverse"), eCaseMatters);
      DrawScale(cgContext, macRect, IsDisabled(aFrame), eventState,
                (aWidgetType == NS_THEME_SCALE_VERTICAL), reverse,
                curpos, minpos, maxpos);
    }
      break;

    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:
      // do nothing, drawn by scale
      break;

    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLBAR: {
      DrawScrollbar(cgContext, macRect, aFrame);
    }
      break;
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
#if SCROLLBARS_VISUAL_DEBUG
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 0, 0.6);
      CGContextFillRect(cgContext, macRect);
    break;
#endif
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
#if SCROLLBARS_VISUAL_DEBUG
      CGContextSetRGBFillColor(cgContext, 1.0, 0, 0, 0.6);
      CGContextFillRect(cgContext, macRect);
    break;
#endif
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
#if SCROLLBARS_VISUAL_DEBUG
      CGContextSetRGBFillColor(cgContext, 0, 1.0, 0, 0.6);
      CGContextFillRect(cgContext, macRect);
    break;      
#endif
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
      // do nothing, drawn by scrollbar
      break;
    
    case NS_THEME_LISTBOX:
      // HIThemeSetFill is not available on 10.3
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);
      CGContextFillRect(cgContext, macRect);
      DrawFrame(cgContext, kHIThemeFrameListBox,
                macRect, (IsDisabled(aFrame) || IsReadOnly(aFrame)), eventState);
      break;
    
    case NS_THEME_TAB: {
      DrawTab(cgContext, macRect,
              IsDisabled(aFrame), IsSelectedTab(aFrame),
              PR_TRUE, IsBottomTab(aFrame),
              eventState);
    }
      break;

    case NS_THEME_TAB_PANELS:
      DrawTabPanel(cgContext, macRect);
      break;
  }

  CGContextRestoreGState(cgContext);

  return NS_OK;
}


NS_IMETHODIMP
nsNativeThemeCocoa::GetWidgetBorder(nsIDeviceContext* aContext, 
                                    nsIFrame* aFrame,
                                    PRUint8 aWidgetType,
                                    nsMargin* aResult)
{
  aResult->SizeTo(0, 0, 0, 0);

  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_BUTTON_SMALL:
      // Top has a single pixel line, bottom has a single pixel line plus a single
      // pixel shadow
      aResult->SizeTo(kAquaPushButtonEndcaps, 1, 
                      kAquaPushButtonEndcaps, 2);
      break;

    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
      // Top has a single pixel line, bottom has a single pixel line plus a single
      // pixel shadow
      aResult->SizeTo(kAquaDropdownLeftEndcap, 1, 
                      kAquaDropwdonRightEndcap, 2);
      break;
    
    case NS_THEME_TEXTFIELD: {
      SInt32 frameOutset = 0;
      ::GetThemeMetric(kThemeMetricEditTextFrameOutset, &frameOutset);
      aResult->SizeTo(frameOutset, frameOutset, frameOutset, frameOutset);
    }
      break;

    case NS_THEME_LISTBOX: {
      SInt32 frameOutset = 0;
      ::GetThemeMetric(kThemeMetricListBoxFrameOutset, &frameOutset);
      aResult->SizeTo(frameOutset, frameOutset, frameOutset, frameOutset);
    }
      break;
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    {
      // There's only an endcap to worry about when both arrows are on the bottom
      ThemeScrollBarArrowStyle arrowStyle;
      ::GetThemeScrollBarArrowStyle(&arrowStyle);
      if (arrowStyle == kThemeScrollBarArrowsLowerRight) {
        PRBool isHorizontal = (aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL);

        nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
        if (!scrollbarFrame) return NS_ERROR_FAILURE;
        PRBool isSmall = (scrollbarFrame->GetStyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL);

        // There isn't a metric for this, so just hardcode a best guess at the value.
        // This value is even less exact due to the fact that the endcap is partially concave.
        PRInt32 endcapSize = isSmall ? 5 : 6;

        if (isHorizontal)
          aResult->SizeTo(endcapSize, 0, 0, 0);
        else
          aResult->SizeTo(0, endcapSize, 0, 0);
      }
    }
      break;
  }
  
  return NS_OK;
}


// return false here to indicate that CSS padding values should be used
PRBool
nsNativeThemeCocoa::GetWidgetPadding(nsIDeviceContext* aContext, 
                                     nsIFrame* aFrame,
                                     PRUint8 aWidgetType,
                                     nsMargin* aResult)
{
  switch (aWidgetType)
  {
    case NS_THEME_TEXTFIELD:
    {
      SInt32 nativePadding = 0;
      ::GetThemeMetric(kThemeMetricEditTextWhitespace, &nativePadding);
      aResult->SizeTo(nativePadding, nativePadding, nativePadding, nativePadding);
      return PR_TRUE;
    }
    case NS_THEME_BUTTON:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    {
      // The border/shadow on the bottom of the button means we have to
      // draw the text a little higher than normal.
      aResult->SizeTo(0, -1, 0, 1);
      return PR_TRUE;
    }
    case NS_THEME_BUTTON_SMALL:
    {
      aResult->SizeTo(0, 0, 0, 0);
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}


PRBool
nsNativeThemeCocoa::GetWidgetOverflow(nsIDeviceContext* aContext, nsIFrame* aFrame,
                                      PRUint8 aWidgetType, nsRect* aResult)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_BUTTON_SMALL:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_LISTBOX:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_CHECKBOX:
    case NS_THEME_CHECKBOX_SMALL:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_SMALL:
    {
      // We assume that the above widgets can draw a focus ring that will be less than
      // or equal to 4 pixels thick.
      nsIntMargin extraSize = nsIntMargin(4, 4, 4, 4);
      PRInt32 p2a = aContext->AppUnitsPerDevPixel();
      nsMargin m(NSIntPixelsToAppUnits(extraSize.left, p2a),
                 NSIntPixelsToAppUnits(extraSize.top, p2a),
                 NSIntPixelsToAppUnits(extraSize.right, p2a),
                 NSIntPixelsToAppUnits(extraSize.bottom, p2a));
      nsRect r(nsPoint(0, 0), aFrame->GetSize());
      r.Inflate(m);
      *aResult = r;
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}


NS_IMETHODIMP
nsNativeThemeCocoa::GetMinimumWidgetSize(nsIRenderingContext* aContext,
                                         nsIFrame* aFrame,
                                         PRUint8 aWidgetType,
                                         nsSize* aResult,
                                         PRBool* aIsOverridable)
{
  aResult->SizeTo(0,0);
  *aIsOverridable = PR_TRUE;

  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    {
      // Height value is adjusted for shadow and border.
      SInt32 buttonHeight = 0;
      ::GetThemeMetric(kThemeMetricPushButtonHeight, &buttonHeight);
      aResult->SizeTo(kAquaMinButtonWidth, buttonHeight + 2);
      break;
    }

    case NS_THEME_BUTTON_SMALL:
    {
      // Height value is adjusted for shadow and border.
      SInt32 buttonHeight = 0;
      ::GetThemeMetric(kThemeMetricSmallPushButtonHeight, &buttonHeight);
      aResult->SizeTo(kAquaMinButtonWidth, buttonHeight + 2);
      break;
    }

    case NS_THEME_SPINNER:
    {
      SInt32 buttonHeight = 0;
      ::GetThemeMetric(kThemeMetricPushButtonHeight, &buttonHeight);
      aResult->SizeTo(kAquaPushButtonEndcaps, buttonHeight);
      break;
    }

    case NS_THEME_CHECKBOX:
    {
      SInt32 boxHeight = 0, boxWidth = 0;
      ::GetThemeMetric(kThemeMetricCheckBoxWidth, &boxWidth);
      ::GetThemeMetric(kThemeMetricCheckBoxHeight, &boxHeight);
      aResult->SizeTo(boxWidth, boxHeight);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_CHECKBOX_SMALL:
    {
      // Appearance manager (and the Aqua HIG) will tell us that a small
      // checkbox is 14x16.  This includes a transparent row at the bottom
      // of the image.  In order to allow the baseline for text to be aligned
      // with the bottom of the checkbox, we report the size as 14x15, but
      // we'll always tell appearance manager to draw it at 14x16.  This
      // will result in Gecko aligning text with the real bottom of the
      // checkbox.

      aResult->SizeTo(14, 15);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_RADIO:
    {
      SInt32 radioHeight = 0, radioWidth = 0;
      ::GetThemeMetric(kThemeMetricRadioButtonWidth, &radioWidth);
      ::GetThemeMetric(kThemeMetricRadioButtonHeight, &radioHeight);
      aResult->SizeTo(radioWidth, radioHeight);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_RADIO_SMALL:
    {
      // Same as above, but appearance manager reports 14x15, and we
      // tell gecko 14x14.

      aResult->SizeTo(14, 14);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    {
      SInt32 popupHeight = 0;
      ::GetThemeMetric(kThemeMetricPopupButtonHeight, &popupHeight);
      aResult->SizeTo(0, popupHeight);
      break;
    }
 
    case NS_THEME_TEXTFIELD:
    {
      // at minimum, we should be tall enough for 9pt text.
      // I'm using hardcoded values here because the appearance manager
      // values for the frame size are incorrect.
      aResult->SizeTo(0, (2 + 2) /* top */ + 9 + (1 + 1) /* bottom */);
      break;
    }
      
    case NS_THEME_PROGRESSBAR:
    {
      SInt32 barHeight = 0;
      ::GetThemeMetric(kThemeMetricNormalProgressBarThickness, &barHeight);
      aResult->SizeTo(0, barHeight);
      break;
    }

    case NS_THEME_TREEVIEW_TWISTY:
    case NS_THEME_TREEVIEW_TWISTY_OPEN:   
    {
      SInt32 twistyHeight = 0, twistyWidth = 0;
      ::GetThemeMetric(kThemeMetricDisclosureButtonWidth, &twistyWidth);
      ::GetThemeMetric(kThemeMetricDisclosureButtonHeight, &twistyHeight);
      aResult->SizeTo(twistyWidth, twistyHeight);
      *aIsOverridable = PR_FALSE;
      break;
    }
    
    case NS_THEME_TREEVIEW_HEADER:
    case NS_THEME_TREEVIEW_HEADER_CELL:
    {
      SInt32 headerHeight = 0;
      ::GetThemeMetric(kThemeMetricListHeaderHeight, &headerHeight);
      aResult->SizeTo(0, headerHeight);
      break;
    }

    case NS_THEME_SCALE_HORIZONTAL:
    {
      SInt32 scaleHeight = 0;
      ::GetThemeMetric(kThemeMetricHSliderHeight, &scaleHeight);
      aResult->SizeTo(scaleHeight, scaleHeight);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_SCALE_VERTICAL:
    {
      SInt32 scaleWidth = 0;
      ::GetThemeMetric(kThemeMetricVSliderWidth, &scaleWidth);
      aResult->SizeTo(scaleWidth, scaleWidth);
      *aIsOverridable = PR_FALSE;
      break;
    }
      
    case NS_THEME_SCROLLBAR_SMALL:
    {
      SInt32 scrollbarWidth = 0;
      ::GetThemeMetric(kThemeMetricSmallScrollBarWidth, &scrollbarWidth);
      aResult->SizeTo(scrollbarWidth, scrollbarWidth);
      *aIsOverridable = PR_FALSE;
      break;
    }

    // Get the rect of the thumb from HITheme, so we can return it to Gecko, which has different ideas about
    // how big the thumb should be. This is kind of a hack.
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    {
      // Find our parent scrollbar frame. If we can't, abort.
      nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
      if (!scrollbarFrame) return NS_ERROR_FAILURE;

      // We need to get the device context to convert from app units :(
      nsRect scrollbarRect = scrollbarFrame->GetRect();
      nsCOMPtr<nsIDeviceContext> dctx;
      aContext->GetDeviceContext(*getter_AddRefs(dctx));
      PRInt32 p2a = dctx->AppUnitsPerDevPixel();
      CGRect macRect = CGRectMake(NSAppUnitsToIntPixels(scrollbarRect.x, p2a),
                                  NSAppUnitsToIntPixels(scrollbarRect.y, p2a),
                                  NSAppUnitsToIntPixels(scrollbarRect.width, p2a),
                                  NSAppUnitsToIntPixels(scrollbarRect.height, p2a));

      // False here means not to get scrollbar button state information.
      HIThemeTrackDrawInfo tdi;
      GetScrollbarDrawInfo(tdi, scrollbarFrame, macRect, PR_FALSE);

      HIRect thumbRect;
      ::HIThemeGetTrackPartBounds(&tdi, kControlIndicatorPart, &thumbRect);

      // HITheme is just lying to us, I guess...
      PRInt32 thumbAdjust = ((scrollbarFrame->GetStyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL) ?
                             2 : 4);

      if (aWidgetType == NS_THEME_SCROLLBAR_THUMB_VERTICAL)
        aResult->SizeTo(nscoord(thumbRect.size.width), nscoord(thumbRect.size.height - thumbAdjust));
      else
        aResult->SizeTo(nscoord(thumbRect.size.width - thumbAdjust), nscoord(thumbRect.size.height));
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    {
      // yeah, i know i'm cheating a little here, but i figure that it
      // really doesn't matter if the scrollbar is vertical or horizontal
      // and the width metric is a really good metric for every piece
      // of the scrollbar.

      nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
      if (!scrollbarFrame) return NS_ERROR_FAILURE;

      PRInt32 themeMetric = (scrollbarFrame->GetStyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL) ?
                            kThemeMetricSmallScrollBarWidth :
                            kThemeMetricScrollBarWidth;
      SInt32 scrollbarWidth = 0;
      ::GetThemeMetric(themeMetric, &scrollbarWidth);
      aResult->SizeTo(scrollbarWidth, scrollbarWidth);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    {
      nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
      if (!scrollbarFrame) return NS_ERROR_FAILURE;

      // Since there is no NS_THEME_SCROLLBAR_BUTTON_UP_SMALL we need to ask the parent what appearance style it has.
      PRInt32 themeMetric = (scrollbarFrame->GetStyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL) ?
                            kThemeMetricSmallScrollBarWidth :
                            kThemeMetricScrollBarWidth;
      SInt32 scrollbarWidth = 0;
      ::GetThemeMetric(themeMetric, &scrollbarWidth);

      // It seems that for both sizes of scrollbar, the buttons are one pixel "longer".
      if (aWidgetType == NS_THEME_SCROLLBAR_BUTTON_LEFT || aWidgetType == NS_THEME_SCROLLBAR_BUTTON_RIGHT)
        aResult->SizeTo(scrollbarWidth+1, scrollbarWidth);
      else
        aResult->SizeTo(scrollbarWidth, scrollbarWidth+1);
 
      *aIsOverridable = PR_FALSE;
      break;
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNativeThemeCocoa::WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                     nsIAtom* aAttribute, PRBool* aShouldRepaint)
{
  // Some widget types just never change state.
  switch (aWidgetType) {
    case NS_THEME_TOOLBOX:
    case NS_THEME_TOOLBAR:
    case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL: 
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_TOOLTIP:
    case NS_THEME_TAB_PANELS:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_DIALOG:
    case NS_THEME_MENUPOPUP:
      *aShouldRepaint = PR_FALSE;
      return NS_OK;
  }

  // XXXdwh Not sure what can really be done here.  Can at least guess for
  // specific widgets that they're highly unlikely to have certain states.
  // For example, a toolbar doesn't care about any states.
  if (!aAttribute) {
    // Hover/focus/active changed.  Always repaint.
    *aShouldRepaint = PR_TRUE;
  } else {
    // Check the attribute to see if it's relevant.  
    // disabled, checked, dlgtype, default, etc.
    *aShouldRepaint = PR_FALSE;
    if (aAttribute == nsWidgetAtoms::disabled ||
        aAttribute == nsWidgetAtoms::checked ||
        aAttribute == nsWidgetAtoms::selected ||
        aAttribute == nsWidgetAtoms::mozmenuactive ||
        aAttribute == nsWidgetAtoms::sortdirection)
      *aShouldRepaint = PR_TRUE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNativeThemeCocoa::ThemeChanged()
{
  // This is unimplemented because we don't care if gecko changes its theme
  // and Mac OS X doesn't have themes.
  return NS_OK;
}


PRBool 
nsNativeThemeCocoa::ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame,
                                      PRUint8 aWidgetType)
{
#ifndef MOZ_MACBROWSER
  // Only support HTML widgets in Camino builds
  if (aFrame && aFrame->GetContent()->IsNodeOfType(nsINode::eHTML))
    return PR_FALSE;
#endif

  if (aPresContext && !aPresContext->PresShell()->IsThemeSupportEnabled())
    return PR_FALSE;

  PRBool retVal = PR_FALSE;

  switch (aWidgetType) {
    case NS_THEME_DIALOG:
    case NS_THEME_WINDOW:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_MENUITEM:
    case NS_THEME_TOOLTIP:
    
    case NS_THEME_CHECKBOX:
    case NS_THEME_CHECKBOX_SMALL:
    case NS_THEME_CHECKBOX_CONTAINER:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_SMALL:
    case NS_THEME_RADIO_CONTAINER:
    case NS_THEME_BUTTON:
    case NS_THEME_BUTTON_SMALL:
    case NS_THEME_BUTTON_BEVEL:
    case NS_THEME_SPINNER:
    case NS_THEME_TOOLBAR:
    case NS_THEME_STATUSBAR:
    case NS_THEME_TEXTFIELD:
    //case NS_THEME_TOOLBOX:
    //case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TOOLBAR_SEPARATOR:
    
    case NS_THEME_TAB_PANELS:
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
    
    case NS_THEME_TREEVIEW_TWISTY:
    case NS_THEME_TREEVIEW_TWISTY_OPEN:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TREEVIEW_HEADER:
    case NS_THEME_TREEVIEW_HEADER_CELL:
    case NS_THEME_TREEVIEW_HEADER_SORTARROW:
    case NS_THEME_TREEVIEW_TREEITEM:
    case NS_THEME_TREEVIEW_LINE:

    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:

    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
      retVal = PR_TRUE;
      break;

    case NS_THEME_LISTBOX:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_DROPDOWN_TEXT:
      // Support listboxes and dropdowns regardless of styling,
      // since non-themed ones look totally wrong.
      return PR_TRUE;
  }

  return retVal ? !IsWidgetStyled(aPresContext, aFrame, aWidgetType) : PR_FALSE;
}


PRBool
nsNativeThemeCocoa::WidgetIsContainer(PRUint8 aWidgetType)
{
  // flesh this out at some point
  switch (aWidgetType) {
   case NS_THEME_DROPDOWN_BUTTON:
   case NS_THEME_RADIO:
   case NS_THEME_CHECKBOX:
   case NS_THEME_PROGRESSBAR:
    return PR_FALSE;
    break;
  }
  return PR_TRUE;
}


PRBool
nsNativeThemeCocoa::ThemeDrawsFocusForWidget(nsPresContext* aPresContext, nsIFrame* aFrame, PRUint8 aWidgetType)
{
  if (aWidgetType == NS_THEME_DROPDOWN ||
      aWidgetType == NS_THEME_BUTTON ||
      aWidgetType == NS_THEME_BUTTON_SMALL)
    return PR_TRUE;
  
  return PR_FALSE;
}
