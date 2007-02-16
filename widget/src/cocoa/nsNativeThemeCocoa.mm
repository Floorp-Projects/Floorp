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

extern "C" {
  CG_EXTERN void CGContextSetCTM(CGContextRef, CGAffineTransform);
}

#define HITHEME_ORIENTATION kHIThemeOrientationNormal

NS_IMPL_ISUPPORTS1(nsNativeThemeCocoa, nsITheme)

static PRBool sInitializedBorders = PR_FALSE;

nsNativeThemeCocoa::nsNativeThemeCocoa()
{
  if (!sInitializedBorders) {
    sInitializedBorders = PR_TRUE;
    sTextfieldBorderSize.left = sTextfieldBorderSize.top = 2;
    sTextfieldBorderSize.right = sTextfieldBorderSize.bottom = 2;
    sTextfieldBGTransparent = PR_FALSE;
    sListboxBGTransparent = PR_TRUE;
    sTextfieldDisabledBGColorID = nsILookAndFeel::eColor__moz_field;
  }
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
    bdi.state = kThemeStateInactive;
  else if ((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER))
    bdi.state = kThemeStatePressed;
  else
    bdi.state = kThemeStateActive;

  bdi.value = inChecked ? kThemeButtonOn : kThemeButtonOff;
  bdi.adornment = (inState & NS_EVENT_STATE_FOCUS) ? kThemeAdornmentFocus : kThemeAdornmentNone;

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
    bdi.state = kThemeStateUnavailableInactive;
  else if ((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER))
    bdi.state = kThemeStatePressed;
  else
    bdi.state = (inKind == kThemeArrowButton) ? kThemeStateInactive : kThemeStateActive;

  if (inState & NS_EVENT_STATE_FOCUS)
    bdi.adornment |= kThemeAdornmentFocus;

  if (inIsDefault && !inDisabled)
    bdi.adornment |= kThemeAdornmentDefault;

  HIThemeDrawButton(&inBoxRect, &bdi, cgContext, HITHEME_ORIENTATION, NULL);
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
    bdi.state = kThemeStateUnavailableInactive;

  HIThemeDrawButton(&inBoxRect, &bdi, cgContext, HITHEME_ORIENTATION, NULL);
}

void
nsNativeThemeCocoa::DrawFrame(CGContextRef cgContext, HIThemeFrameKind inKind,
                              const HIRect& inBoxRect, PRBool inIsDisabled, PRInt32 inState)
{
  HIThemeFrameDrawInfo fdi;
  fdi.version = 0;
  fdi.kind = inKind;
  fdi.state = inIsDisabled ? (ThemeDrawState) kThemeStateDisabled : (ThemeDrawState) kThemeStateActive;
  fdi.isFocused = (inState & NS_EVENT_STATE_FOCUS) != 0;

  HIThemeDrawFrame(&inBoxRect, &fdi, cgContext, HITHEME_ORIENTATION);
}

void
nsNativeThemeCocoa::DrawProgress(CGContextRef cgContext,
                                 const HIRect& inBoxRect, PRBool inIsDisabled, PRBool inIsIndeterminate, 
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
  tdi.enableState = inIsDisabled ? kThemeTrackDisabled : kThemeTrackActive;
  tdi.trackInfo.progress.phase = sPhase++; // animate for the next time we're called

  HIThemeDrawTrack(&tdi, NULL, cgContext, HITHEME_ORIENTATION);
}


void
nsNativeThemeCocoa::DrawTabPanel(CGContextRef cgContext, const HIRect& inBoxRect, PRBool inIsDisabled)
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
                              PRBool inIsVertical, PRInt32 inCurrentValue,
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
      tdi.style = kThemeTabFrontInactive;
    else
      tdi.style = kThemeTabFront;
  } else {
    if (inIsDisabled)
      tdi.style = kThemeTabNonFrontInactive;
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
  if (thebesSurface->GetType() != gfxASurface::SurfaceTypeQuartz2) {
    fprintf (stderr, "Expected surface of type Quartz2, got %d\n",
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
    CGContextConcatCTM(cgContext,
                       CGAffineTransformMake(mat.xx, mat.xy,
                                             mat.yx, mat.yy,
                                             floor(mat.x0 + ROUND_CONST_FLOAT),
                                             floor(mat.y0 + ROUND_CONST_FLOAT)));
  }

#if 0
  if (1 /*aWidgetType == NS_THEME_TEXTFIELD*/) {
    fprintf(stderr, "Native theme drawing widget %d [%p] dis:%d in rect [%d %d %d %d]\n",
            aWidgetType, aFrame, IsDisabled(aFrame), aRect.x, aRect.y, aRect.width, aRect.height);
    fprintf (stderr, "Native theme xform[0]: [%f %f %f %f %f %f]\n",
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
  CGRect bounds = CGContextGetClipBoundingBox (cgContext);
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

    case NS_THEME_RADIO:
      DrawCheckboxRadio(cgContext, kThemeRadioButton, macRect, IsSelected(aFrame), IsDisabled(aFrame), eventState);
      break;

    case NS_THEME_CHECKBOX_SMALL:
      if (aRect.height == 15) {
      	// draw at 14x16, see comment in GetMinimumWidgetSize
        // XXX this should probably happen earlier, before transform; this is very fragile
        macRect.size.height += 1.0;
      }
      DrawCheckboxRadio(cgContext, kThemeSmallCheckBox, macRect, IsChecked(aFrame), IsDisabled(aFrame), eventState);
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
      DrawButton(cgContext, kThemePushButton, macRect,
                 IsDefaultButton(aFrame), IsDisabled(aFrame),
                 kThemeButtonOn, kThemeAdornmentNone, eventState);
      break;

    case NS_THEME_BUTTON_SMALL:
      DrawButton(cgContext, kThemePushButtonSmall, macRect,
                 IsDefaultButton(aFrame), IsDisabled(aFrame),
                 kThemeButtonOn, kThemeAdornmentNone, eventState);
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
      HIThemeSeparatorDrawInfo sdi = { 0, IsDisabled(aFrame) ? (ThemeDrawState) kThemeStateDisabled : (ThemeDrawState) kThemeStateActive };
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
      DrawProgress(cgContext, macRect, IsDisabled(aFrame),
                   IsIndeterminateProgress(aFrame), PR_TRUE, GetProgressValue(aFrame));
      break;

    case NS_THEME_PROGRESSBAR_VERTICAL:
      DrawProgress(cgContext, macRect, IsDisabled(aFrame),
                   IsIndeterminateProgress(aFrame), PR_FALSE, GetProgressValue(aFrame));
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

      DrawScale(cgContext, macRect, IsDisabled(aFrame), eventState,
                (aWidgetType == NS_THEME_SCALE_VERTICAL),
                curpos, minpos, maxpos);
    }
      break;

    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:
      // do nothing, drawn by scale
      break;

    case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:
    case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL: 
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
      // Scrollbars are now native on mac, via nsNativeScrollbarFrame.
      // So, this should never be called.
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
      DrawTabPanel(cgContext, macRect, IsDisabled(aFrame));
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
      aResult->SizeTo(kAquaPushButtonEndcaps, kAquaPushButtonTopBottom, 
                      kAquaPushButtonEndcaps, kAquaPushButtonTopBottom);
      break;

    case NS_THEME_BUTTON_SMALL:
      aResult->SizeTo(kAquaSmallPushButtonEndcaps, kAquaPushButtonTopBottom,
                      kAquaSmallPushButtonEndcaps, kAquaPushButtonTopBottom);
      break;

    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
      aResult->SizeTo(kAquaDropdownLeftEndcap, kAquaPushButtonTopBottom, 
                      kAquaDropwdonRightEndcap, kAquaPushButtonTopBottom);
      break;
    
    case NS_THEME_TEXTFIELD:
      aResult->SizeTo(2, 2, 2, 2);
      break;

    case NS_THEME_LISTBOX: {
      SInt32 frameOutset = 0;
      ::GetThemeMetric(kThemeMetricListBoxFrameOutset, &frameOutset);
      aResult->SizeTo(frameOutset, frameOutset, frameOutset, frameOutset);
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
      SInt32 buttonHeight = 0;
      ::GetThemeMetric(kThemeMetricPushButtonHeight, &buttonHeight);
      aResult->SizeTo(kAquaPushButtonEndcaps * 2, buttonHeight);
      break;
    }
      
    case NS_THEME_BUTTON_SMALL:
    {
      SInt32 buttonHeight = 0;
      ::GetThemeMetric(kThemeMetricSmallPushButtonHeight, &buttonHeight);
      aResult->SizeTo(kAquaSmallPushButtonEndcaps * 2, buttonHeight);
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
    
    case NS_THEME_RADIO:
    {
      SInt32 radioHeight = 0, radioWidth = 0;
      ::GetThemeMetric(kThemeMetricRadioButtonWidth, &radioWidth);
      ::GetThemeMetric(kThemeMetricRadioButtonHeight, &radioHeight);
      aResult->SizeTo(radioWidth, radioHeight);
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
      
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:
    case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    {
      // yeah, i know i'm cheating a little here, but i figure that it
      // really doesn't matter if the scrollbar is vertical or horizontal
      // and the width metric is a really good metric for every piece
      // of the scrollbar.
      SInt32 scrollbarWidth = 0;
      ::GetThemeMetric(kThemeMetricScrollBarWidth, &scrollbarWidth);
      aResult->SizeTo(scrollbarWidth, scrollbarWidth);
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
    case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:
    case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL:
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
