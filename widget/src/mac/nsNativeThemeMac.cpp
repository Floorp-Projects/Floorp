/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 *   Mike Pinkerton (pinkerton@netscape.com)
 *
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <Gestalt.h>
#include "nsNativeThemeMac.h"
#include "nsRenderingContextMac.h"
#include "nsDeviceContextMac.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsTransform2D.h"
#include "nsThemeConstants.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIEventStateManager.h"
#include "nsINameSpaceManager.h"
#include "nsIPresContext.h"
#include "nsILookAndFeel.h"
#include "nsRegionPool.h"
#include "nsGfxUtils.h"

//
// Return true if we are on Mac OS X, caching the result after the first call
// Yes, this needs to go somehwere better.
//
static PRBool OnMacOSX()
{
  static PRBool gInitVer = PR_FALSE;
  static PRBool gOnMacOSX = PR_FALSE;
  if(! gInitVer) {
    long version;
    OSErr err = ::Gestalt(gestaltSystemVersion, &version);
    gOnMacOSX = (err == noErr && version >= 0x00001000);
    gInitVer = PR_TRUE;
  }
  return gOnMacOSX;
}


static void 
ConvertGeckoToNativeRect(const nsRect& aSrc, Rect& aDst) 
{
  aDst.top = aSrc.y;
  aDst.bottom = aSrc.y + aSrc.height;
  aDst.left = aSrc.x;
  aDst.right = aSrc.x + aSrc.width;
}


static void
GetPrimaryPresShell(nsIFrame* aFrame, nsIPresShell** aResult)
{
  *aResult = nsnull;

  if (!aFrame)
    return;
 
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  content->GetDocument(*getter_AddRefs(doc));
  if (doc)
    doc->GetShellAt(0, aResult); // Addref happens here.
}


static PRInt32
GetContentState(nsIFrame* aFrame)
{
  if (!aFrame)
    return 0;

  nsCOMPtr<nsIPresShell> shell;
  GetPrimaryPresShell(aFrame, getter_AddRefs(shell));
  if (!shell)
    return 0;

  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));
  nsCOMPtr<nsIEventStateManager> esm;
  context->GetEventStateManager(getter_AddRefs(esm));
  PRInt32 flags = 0;
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  esm->GetContentState(content, flags);
  return flags;
}


//
// GetAttribute
//
// Gets the given attribute from the given frame's content node
// and returns PR_TRUE if the attribute was found and PR_FALSE if 
// it wasn't
//
static PRBool
GetAttribute(nsIFrame* aFrame, nsIAtom* inAttribute, nsCString& outValue)
{
  if (!aFrame)
    return PR_FALSE;
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  nsAutoString attr;
  nsresult res = content->GetAttr(kNameSpaceID_None, inAttribute, attr);
  outValue = NS_LossyConvertUCS2toASCII(attr).get();

  return ( res != NS_CONTENT_ATTR_NO_VALUE &&
           !(res != NS_CONTENT_ATTR_NOT_THERE && attr.IsEmpty()));
}


static PRBool
HasAttrValue(nsIContent* aContent, nsIAtom* aAtom, const char* aStr)
{
  nsAutoString attr;
  aContent->GetAttr(kNameSpaceID_None, aAtom, attr);
  return attr.EqualsIgnoreCase(aStr);
}


static PRInt32
CheckIntAttr(nsIFrame* aFrame, nsIAtom* aAtom)
{
  nsCAutoString value;
  if ( GetAttribute(aFrame, aAtom, value) )
    return atoi(value.get());
  else
    return 0;
}


static PRBool
CheckBooleanAttr(nsIFrame* aFrame, nsIAtom* aAtom)
{
  nsCAutoString value;
  GetAttribute(aFrame, aAtom, value);
  if ( GetAttribute(aFrame, aAtom, value) )
    return strcmp(value.get(), "true") == 0; // This handles the XUL case.
  else
    return PR_TRUE;                          // handles the HTML case where no val is true
}


//
// DoNothing
//
// An eraseProc for drawing theme buttons so that we don't erase over anything
// that might be drawn before us in the background layer. Does absolutely
// nothing.
//
static pascal void
DoNothing(const Rect *bounds, UInt32 eraseData, SInt16 depth, Boolean isColorDev)
{
  // be gentle, erase nothing.
}


#ifdef XP_MAC
#pragma mark -
#endif

         
NS_IMPL_ISUPPORTS1(nsNativeThemeMac, nsITheme)

nsNativeThemeMac::nsNativeThemeMac()
  : mEraseProc(nsnull)
{
  NS_INIT_ISUPPORTS();

  mEraseProc = NewThemeEraseUPP(DoNothing);

  mCheckedAtom = do_GetAtom("checked");
  mDisabledAtom = do_GetAtom("disabled");
  mSelectedAtom = do_GetAtom("selected");
  mDefaultAtom = do_GetAtom("default");
  mValueAtom = do_GetAtom("value");
  mModeAtom = do_GetAtom("mode");
  mOrientAtom = do_GetAtom("orient");
  mCurPosAtom = do_GetAtom("curpos");
  mMaxPosAtom = do_GetAtom("maxpos");
  mScrollbarAtom = do_GetAtom("scrollbar");
  mClassAtom = do_GetAtom("class");
  mSortDirectionAtom = do_GetAtom("sortDirection");
  mInputAtom = do_GetAtom("input");
  mInputCheckedAtom = do_GetAtom("_moz-input-checked");
}

nsNativeThemeMac::~nsNativeThemeMac()
{
  if ( mEraseProc )
    ::DisposeThemeEraseUPP(mEraseProc);
}



PRBool
nsNativeThemeMac::IsDisabled(nsIFrame* aFrame)
{
  return CheckBooleanAttr(aFrame, mDisabledAtom);
}


PRBool
nsNativeThemeMac::IsDefaultButton(nsIFrame* aFrame)
{
  return CheckBooleanAttr(aFrame, mDefaultAtom);
}

  
PRBool
nsNativeThemeMac::IsChecked(nsIFrame* aFrame, PRBool aIsHTML)
{
  return CheckBooleanAttr(aFrame, aIsHTML ? mInputCheckedAtom : mCheckedAtom);
}


PRBool
nsNativeThemeMac::IsSelected(nsIFrame* aFrame)
{
  return CheckBooleanAttr(aFrame, mSelectedAtom);
}


PRBool
nsNativeThemeMac::IsSortedColumn(nsIFrame* aFrame)
{
  // if the "sortDirection" attribute is set, we're the sorted column
  nsCAutoString mode;
  if ( GetAttribute(aFrame, mSortDirectionAtom, mode) )
    return !mode.IsEmpty();
  
  return PR_FALSE;
}


PRBool
nsNativeThemeMac::IsSortReversed(nsIFrame* aFrame)
{
  nsCAutoString mode;
  if ( GetAttribute(aFrame, mSortDirectionAtom, mode) )
    return mode.Equals("descending");
  
  return PR_FALSE;
}


PRBool
nsNativeThemeMac::DoTabsPointUp(nsIFrame* aFrame)
{
  nsCAutoString mode;
  if ( GetAttribute(aFrame, mClassAtom, mode) )
    return mode.Find("tab-bottom") != kNotFound;
  
  return PR_FALSE;
}


PRBool
nsNativeThemeMac::IsIndeterminate(nsIFrame* aFrame)
{
  nsCAutoString mode;
  if ( GetAttribute(aFrame, mModeAtom, mode) )
    return strcmp(mode.get(), "undetermined") == 0;
  
  return PR_FALSE;
}


//
// GetScrollbarParent
//
// Starting at the given frame, walk up the chain until we find the
// top-level scrollbar, computing offsets as we go. Callers will
// would subtract this offset from the scrollbar's coordinates
// to draw in the current frame's coord system.
//
nsIFrame*
nsNativeThemeMac::GetScrollbarParent(nsIFrame* inButton, nsPoint* outOffset)
{
  outOffset->MoveTo(0,0);
  if ( !inButton )
    return nsnull;
  
  PRBool found = PR_FALSE;
  nsIFrame* currFrame = inButton;
  do {
    // grab the content node of this frame, check if its tag is
    // |scrollbar|. If not, keep going up the chain.
    nsCOMPtr<nsIContent> content;
    currFrame->GetContent(getter_AddRefs(content));
    NS_ASSERTION(content, "Couldn't get content from frame, are we in a scrollbar?");
    if ( content ) {
      nsCOMPtr<nsIAtom> tag;
      content->GetTag(*getter_AddRefs(tag));
      if ( tag == mScrollbarAtom )
        found = PR_TRUE;
      else {
        // drat, add to our offset and check the parent
        nsPoint offsetFromParent;
        currFrame->GetOrigin(offsetFromParent);
        *outOffset += offsetFromParent;
        
        currFrame->GetParent(&currFrame);
      }
    }
    else {
      // hrm, no content, we're probably not in a scrollbar. just bail
      currFrame = nsnull;
      found = PR_TRUE;
    }
  } while ( !found && currFrame );
    
  return currFrame;
}


//
// GetScrollbarParentLocalRect
//
// Given a child of a scrollbar, returns the parent scrollbar frame as
// well as the rect of the parent scrollbar offset into the coordinate
// system of the given child. A caller can turn around and pass this
// rect directly to the AppearanceManager for drawing the entire scrollbar.
//
nsIFrame*
nsNativeThemeMac::GetScrollbarParentLocalRect ( nsIFrame* inButton, nsTransform2D* inMatrix, Rect* outAdjustedRect )
{
  ::SetRect(outAdjustedRect, 0, 0, 0, 0);
  
  nsPoint offset;
  nsIFrame* scrollbar = GetScrollbarParent(inButton, &offset);
  if ( scrollbar ) {
    nsRect scrollbarRect;
    scrollbar->GetRect(scrollbarRect);
    nsRect localScrollRect(-offset.x, -offset.y, scrollbarRect.width, scrollbarRect.height);

//printf("offset is (%ld, %ld)\n", offset.x, offset.y);
//printf("pre-xform (%ld, %ld) w=%ld h=%ld\n", localScrollRect.x, localScrollRect.y, localScrollRect.width,
//                localScrollRect.height);
    // now that we have it in gecko coords, transform it to coords the OS can use
    inMatrix->TransformCoord(&localScrollRect.x, &localScrollRect.y, 
                               &localScrollRect.width, &localScrollRect.height);
    ConvertGeckoToNativeRect(localScrollRect, *outAdjustedRect);
  }

  return scrollbar;
}


#ifdef XP_MAC
#pragma mark -
#endif


void
nsNativeThemeMac::DrawCheckboxRadio ( ThemeButtonKind inKind, const Rect& inBoxRect, PRBool inChecked,
                                       PRBool inDisabled, PRInt32 inState )
{
  ThemeButtonDrawInfo info;
  if ( inDisabled )
    info.state = kThemeStateInactive;
  else
    info.state = ((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER)) ?
                     kThemeStatePressed : kThemeStateActive;
  info.value = inChecked ? kThemeButtonOn : kThemeButtonOff;
  info.adornment = (inState & NS_EVENT_STATE_FOCUS) ? kThemeAdornmentFocus : kThemeAdornmentNone;
  
  ::DrawThemeButton ( &inBoxRect, inKind, &info, nsnull, nsnull, nsnull, 0L );
}


void
nsNativeThemeMac::DrawCheckbox ( const Rect& inBoxRect, PRBool inChecked, PRBool inDisabled, PRInt32 inState )
{
  DrawCheckboxRadio(kThemeCheckBox, inBoxRect, inChecked, inDisabled, inState);
}


void
nsNativeThemeMac::DrawRadio ( const Rect& inBoxRect, PRBool inChecked, PRBool inDisabled, PRInt32 inState )
{
  DrawCheckboxRadio(kThemeRadioButton, inBoxRect, inChecked, inDisabled, inState);
}


void
nsNativeThemeMac::DrawButton ( ThemeButtonKind inKind, const Rect& inBoxRect, PRBool inIsDefault, 
                                  PRBool inDisabled, ThemeButtonValue inValue, ThemeButtonAdornment inAdornment,
                                  PRInt32 inState )
{
  ThemeButtonDrawInfo info;
  if ( inDisabled )
    info.state = kThemeStateUnavailableInactive;
  else
    info.state = ((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER)) ? 
                    kThemeStatePressed : kThemeStateActive;
  info.value = inValue;
  info.adornment = inAdornment;
  if ( inState & NS_EVENT_STATE_FOCUS )    
    info.adornment = kThemeAdornmentFocus;
  if ( inIsDefault )
    info.adornment |= kThemeAdornmentDefault;

  ::DrawThemeButton ( &inBoxRect, inKind, &info, nsnull, mEraseProc, nsnull, 0L );
}


void
nsNativeThemeMac::DrawToolbar ( const Rect& inBoxRect )
{
#if 0
  const PRInt32 kThemeBrushToolbarBackground = 52;    // from 3.4.1 headers
  ::SetThemeBackground(kThemeBrushToolbarBackground, 24, true);
  ::EraseRect(&inBoxRect);
  ::SetThemeBackground(kThemeBrushWhite, 24, true);
printf("told to draw at %ld %ld w %ld h %ld\n", inBoxRect.left, inBoxRect.top, inBoxRect.right-inBoxRect.left,
        inBoxRect.bottom - inBoxRect.top);
#endif
  ThemeDrawState drawState = kThemeStateActive;
  ::DrawThemeWindowHeader(&inBoxRect, drawState);
}


void
nsNativeThemeMac::DrawEditText ( const Rect& inBoxRect, PRBool inIsDisabled )
{
#if TARGET_CARBON
  Pattern whitePat;
  ::BackColor(whiteColor);
  ::BackPat(GetQDGlobalsWhite(&whitePat));
  ::EraseRect(&inBoxRect);
  
  ThemeDrawState drawState = inIsDisabled ? kThemeStateActive : kThemeStateDisabled;
  ::DrawThemeEditTextFrame(&inBoxRect, drawState);
#endif
}


void
nsNativeThemeMac::DrawListBox ( const Rect& inBoxRect, PRBool inIsDisabled )
{
#if TARGET_CARBON
  Pattern whitePat;
  ::BackColor(whiteColor);
  ::BackPat(GetQDGlobalsWhite(&whitePat));
  ::EraseRect(&inBoxRect);
  
  ThemeDrawState drawState = inIsDisabled ? kThemeStateActive : kThemeStateDisabled;
  ::DrawThemeListBoxFrame(&inBoxRect, drawState);
#endif
}


void
nsNativeThemeMac::DrawProgress ( const Rect& inBoxRect, PRBool inIsDisabled, PRBool inIsIndeterminate, 
                                  PRBool inIsHorizontal, PRInt32 inValue )
{
  ThemeTrackDrawInfo info;
  static SInt32 sPhase = 0;
  
  info.kind = inIsIndeterminate ? kThemeMediumIndeterminateBar: kThemeMediumProgressBar;
  info.bounds = inBoxRect;
  info.min = 0;
  info.max = 100;
  info.value = inValue;
  info.attributes = inIsHorizontal ? kThemeTrackHorizontal : 0L;
  info.enableState = inIsDisabled ? kThemeTrackDisabled : kThemeTrackActive;
  info.trackInfo.progress.phase = sPhase++;       // animate for the next time we're called
  
  ::DrawThemeTrack(&info, nsnull, nsnull, 0L);
}


void
nsNativeThemeMac::DrawTabPanel ( const Rect& inBoxRect, PRBool inIsDisabled )
{
  ThemeDrawState drawState = inIsDisabled ? kThemeStateActive : kThemeStateDisabled;
  ::DrawThemeTabPane(&inBoxRect, drawState);
}


void
nsNativeThemeMac::DrawSeparator ( const Rect& inBoxRect, PRBool inIsDisabled )
{
  ThemeDrawState drawState = inIsDisabled ? kThemeStateActive : kThemeStateDisabled;
  ::DrawThemeSeparator(&inBoxRect, drawState);
}


void
nsNativeThemeMac::DrawTab ( const Rect& inBoxRect, PRBool inIsDisabled, PRBool inIsFrontmost, 
                              PRBool inIsHorizontal, PRBool inTabBottom, PRInt32 inState )
{
  ThemeTabStyle style = 0L;
  if ( inIsFrontmost ) {
    if ( inIsDisabled ) 
      style = kThemeTabFrontInactive;
    else
      style = kThemeTabFront;
  }
  else {
    if ( inIsDisabled )
      style = kThemeTabNonFrontInactive;
    else if ( (inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER) )
      style = kThemeTabNonFrontPressed;
    else
      style = kThemeTabNonFront;  
  }

  ThemeTabDirection direction = inTabBottom ? kThemeTabSouth : kThemeTabNorth; // don't yet support vertical tabs
  ::DrawThemeTab(&inBoxRect, style, direction, nsnull, 0L);
}


//
// DrawFullScrollbar
//
// Draw everything in one fell swoop. Unfortunately, the AM doesn't give
// us the ability to draw individual components. There is a routine
// called |DrawThemeScrollBarArrows|, but it's a no-op in Aqua.
//
void
nsNativeThemeMac::DrawFullScrollbar ( const Rect& inSbarRect, PRInt32 inWidgetHit, PRInt32 inLineHeight,
                                       PRBool inIsDisabled, PRInt32 inMax, PRInt32 inValue, PRInt32 inState )
{
  ThemeTrackDrawInfo info;

  // the scrollbar is horizontal if the width is greater than the height. Too bad the API
  // doesn't tell us which is which.
  PRBool isHorizontal = 
          (inSbarRect.right - inSbarRect.left) > (inSbarRect.bottom - inSbarRect.top);
   
  // compute the number of lines in our view. It's probably safe to assume that
  // the height of the scrollbar is the height of the scrollable view
  PRInt32 viewSize = isHorizontal ? (inSbarRect.right - inSbarRect.left) : (inSbarRect.bottom - inSbarRect.top);
  viewSize /= inLineHeight;

  // Figure out if something should be drawn depressed
//printf("-- widget drawn is %ld\n", inWidgetHit);
  ThemeTrackPressState pressState = 0L;  
  if ( (inState & NS_EVENT_STATE_ACTIVE && inState & NS_EVENT_STATE_HOVER) ) {
//printf("something is :hover:active!\n");
    switch ( inWidgetHit ) {
      case NS_THEME_SCROLLBAR_BUTTON_UP:
        pressState = kThemeTopOutsideArrowPressed;
        break;    
      case NS_THEME_SCROLLBAR_BUTTON_DOWN:
        pressState = kThemeBottomOutsideArrowPressed;
        break;    
    }
  }

//XXX can we draw inactive if we can determine if we're not in the topmost window?
//XXX this is true for all controls, but scrollbars are the ones you notice the most

  info.kind = kThemeMediumScrollBar;
  info.bounds = inSbarRect;
  info.min = 0;
  info.max = inMax;
  info.value = inValue;
  info.attributes = isHorizontal ? kThemeTrackHorizontal : 0L;
  info.attributes |= kThemeTrackShowThumb;
  info.enableState = kThemeTrackActive;
  info.trackInfo.scrollbar.viewsize = viewSize;
  info.trackInfo.scrollbar.pressState = pressState;
  
  ::DrawThemeTrack(&info, nsnull, nsnull, 0L);

#ifdef DEBUG_PINK
  // some debug info for helping diagnose problems
  printf("--- BEGIN scrollbar debug info\n");
  printf("-- widget drawn is %ld\n", inWidgetHit);
  printf("bounds (%ld, %ld), (%ld, %ld)\n",inSbarRect.left, inSbarRect.top, inSbarRect.right, inSbarRect.bottom );
  if ( isHorizontal )
    printf("horizontal\n");
  else
    printf("vertical\n");
  printf("viewSize %ld\n", viewSize);
  printf("max %ld\n", inMax);
  printf("value %ld\n", inValue);
  printf("pressState %x\n", pressState);
  printf("--- END scrollbar debug info\n");
#endif
}


NS_IMETHODIMP
nsNativeThemeMac::DrawWidgetBackground(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                        PRUint8 aWidgetType, const nsRect& aRect, const nsRect& aClipRect)
{
  // setup to draw into the correct port
  nsDrawingSurface surf;
  aContext->GetDrawingSurface(&surf);
  nsDrawingSurfaceMac* macSurface = (nsDrawingSurfaceMac*)surf;
  CGrafPtr port = nsnull;
  NS_ASSERTION(macSurface,"no surface!!!\n");
  if ( macSurface )
    macSurface->GetGrafPtr(&port);
  else
    return NS_ERROR_FAILURE;      // we won't get far w/out something to draw into
  StPortSetter temp(port);

  // save off the old clip rgn for later restoration. however, we're currently
  // not using the cliprect because aqua likes to draw shadows and stuff outside
  // the bounds we give it, and clipping to the exact rect looks horrible.
  StRegionFromPool oldClip;
  ::GetClip(oldClip);

  // transform rect coordinates to correct coord system
  nsTransform2D* transformMatrix;
  aContext->GetCurrentTransform(transformMatrix);
  nsRect transRect(aRect), transClipRect(aClipRect);
  Rect macRect, clipRect;
  transformMatrix->TransformCoord(&transRect.x, &transRect.y, &transRect.width, &transRect.height);
  ConvertGeckoToNativeRect(transRect, macRect);
#ifdef CLIP_DRAWING
  transformMatrix->TransformCoord(&transClipRect.x, &transClipRect.y, &transClipRect.width, &transClipRect.height);
  ConvertGeckoToNativeRect(transClipRect, clipRect);
  ::ClipRect(&clipRect);
#endif
  
  PRBool isHTML = PR_FALSE;
  // for some widgets, the parent determines the appropriate state. grab the parent instead.
  if ( aWidgetType == NS_THEME_CHECKBOX || aWidgetType == NS_THEME_RADIO ) {
    nsCOMPtr<nsIContent> content;
    aFrame->GetContent(getter_AddRefs(content));
    if (content->IsContentOfType(nsIContent::eXUL))
      aFrame->GetParent(&aFrame);
    else {
      nsCOMPtr<nsIAtom> tag;
      content->GetTag(*getter_AddRefs(tag));
      if (tag == mInputAtom)
        isHTML = PR_TRUE;
    }
  }
  
  PRInt32 eventState = GetContentState(aFrame);

  switch ( aWidgetType ) {
  
    case NS_THEME_DIALOG:
      ::SetThemeBackground(kThemeBrushDialogBackgroundActive, 24, true);
      ::EraseRect(&macRect);
      ::SetThemeBackground(kThemeBrushWhite, 24, true);
      break;
      
    case NS_THEME_MENU:
    printf("!!! draw menu bg\n");
      ::SetThemeBackground(kThemeBrushDialogBackgroundActive, 24, true);
      ::EraseRect(&macRect);
      ::SetThemeBackground(kThemeBrushWhite, 24, true);
      break;

    case NS_THEME_TOOLTIP:
    {
      RGBColor yellow = {65535,65535,45000};
      ::RGBBackColor(&yellow);
      ::EraseRect(&macRect);
      ::SetThemeBackground(kThemeBrushWhite, 24, true);
      break;
    }

    case NS_THEME_CHECKBOX:
      DrawCheckbox ( macRect, IsChecked(aFrame, isHTML), IsDisabled(aFrame), eventState );
      break;    
    case NS_THEME_RADIO:
      DrawRadio ( macRect, IsSelected(aFrame), IsDisabled(aFrame), eventState );
      break;
    case NS_THEME_BUTTON:
      DrawButton ( kThemePushButton, macRect, IsDefaultButton(aFrame), IsDisabled(aFrame), 
                    kThemeButtonOn, kThemeAdornmentNone, eventState );
      break;      
    case NS_THEME_TOOLBAR_BUTTON:
      DrawButton ( kThemePushButton, macRect, IsDefaultButton(aFrame), IsDisabled(aFrame),
                    kThemeButtonOn, kThemeAdornmentNone, eventState );
      break;
    case NS_THEME_TOOLBAR_SEPARATOR:
      DrawSeparator ( macRect, IsDisabled(aFrame) );
      break;
      
    case NS_THEME_TOOLBAR:
    case NS_THEME_TOOLBOX:
    case NS_THEME_STATUSBAR:
      DrawToolbar ( macRect );
      break;
      
    case NS_THEME_DROPDOWN:
      DrawButton ( kThemePopupButton, macRect, IsDefaultButton(aFrame), IsDisabled(aFrame), 
                    kThemeButtonOn, kThemeAdornmentNone, eventState );
      break;
    case NS_THEME_DROPDOWN_BUTTON:
      // do nothing, this is covered by the DROPDOWN case
      break;

    case NS_THEME_TEXTFIELD:
      DrawEditText ( macRect, IsDisabled(aFrame) );
      break;
      
    case NS_THEME_PROGRESSBAR:
      DrawProgress ( macRect, IsDisabled(aFrame), IsIndeterminate(aFrame), PR_TRUE, CheckIntAttr(aFrame, mValueAtom) );
      break;
    case NS_THEME_PROGRESSBAR_VERTICAL:
      DrawProgress ( macRect, IsDisabled(aFrame), IsIndeterminate(aFrame), PR_FALSE, CheckIntAttr(aFrame, mValueAtom) );
      break;
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
      // do nothing, covered by the progress bar cases above
      break;

    case NS_THEME_TREEVIEW_TWISTY:
      DrawButton ( kThemeDisclosureButton, macRect, PR_FALSE, IsDisabled(aFrame), 
                    kThemeDisclosureRight, kThemeAdornmentNone, eventState );
      break;
    case NS_THEME_TREEVIEW_TWISTY_OPEN:
      DrawButton ( kThemeDisclosureButton, macRect, PR_FALSE, IsDisabled(aFrame), 
                    kThemeDisclosureDown, kThemeAdornmentNone, eventState );
      break;
    case NS_THEME_TREEVIEW_HEADER_CELL:
      DrawButton ( kThemeListHeaderButton, macRect, PR_FALSE, IsDisabled(aFrame), 
                    IsSortedColumn(aFrame) ? kThemeButtonOn : kThemeButtonOff,
                    IsSortReversed(aFrame) ? kThemeAdornmentHeaderButtonSortUp : kThemeAdornmentNone,
                    eventState );      
      break;
    case NS_THEME_TREEVIEW_TREEITEM:
    case NS_THEME_TREEVIEW:
      ::SetThemeBackground(kThemeBrushWhite, 24, true);
      ::EraseRect ( &macRect );
      break;
    case NS_THEME_TREEVIEW_HEADER:
      // do nothing, taken care of by individual header cells
    case NS_THEME_TREEVIEW_HEADER_SORTARROW:
      // do nothing, taken care of by treeview header
    case NS_THEME_TREEVIEW_LINE:
      // do nothing, these lines don't exist on macos
      break;
       
#if 0
    case NS_THEME_SCROLLBAR:
      break;
#endif

    case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:
    case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL: 
      // do nothing, these don't exist in aqua
      break;

    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    {
      const PRInt32 kLineHeight = 16;           // should get this from the view
      
      // draw the thumb and the scrollbar track. In order to do that, we
      // need to get the rect of the parent scrollbar (all we have now
      // is the rect of the thumb) in coordinates relative to the current coord
      // system. GetScrollbarParentLocalRect() will do all that for us.
      Rect macScrollbarRect;
      nsIFrame* scrollbar = GetScrollbarParentLocalRect(aFrame, transformMatrix, &macScrollbarRect);
      if ( scrollbar ) {
        // covert the scrollbar's maxpos to lines. That becomes the number of
        // clicks it takes to scroll to the bottom of the document, which is what
        // apperance wants for the max value of the scrollbar. Ensure that
        // |maxPos| is at least 1. If there really is nothing to scroll, Gecko
        // will hide the scrollbar.
        PRInt32 maxPos = CheckIntAttr(scrollbar, mMaxPosAtom) / kLineHeight;
        if ( !maxPos )
          maxPos = 1;
        PRInt32 curPos = CheckIntAttr(scrollbar, mCurPosAtom) / kLineHeight;
        
        DrawFullScrollbar ( macScrollbarRect, aWidgetType, kLineHeight, IsDisabled(aFrame),
                              maxPos, curPos, eventState);
      }
      break;
    }
    
    case NS_THEME_LISTBOX:
      DrawListBox(macRect, IsDisabled(aFrame));
      break;
    
    case NS_THEME_TAB:
      DrawTab(macRect, IsDisabled(aFrame), IsSelected(aFrame), PR_TRUE, DoTabsPointUp(aFrame), eventState);
      break;      
    case NS_THEME_TAB_PANELS:
      DrawTabPanel(macRect, IsDisabled(aFrame));
      break;
  }

  ::SetClip(oldClip);
  
  return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#endif


NS_IMETHODIMP
nsNativeThemeMac::GetWidgetBorder(nsIDeviceContext* aContext, 
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsMargin* aResult)
{
  aResult->SizeTo(0,0,0,0);
      
  // XXX we should probably cache some of these metrics
  
#if TARGET_CARBON
  switch ( aWidgetType ) {
  
    case NS_THEME_BUTTON:
      if ( OnMacOSX() )
        aResult->SizeTo(kAquaPushButtonEndcaps, kAquaPushButtonTopBottom, 
                            kAquaPushButtonEndcaps, kAquaPushButtonTopBottom);
      else
        aResult->SizeTo(5,2,5,2);     // 5px for AGA
      break;

    case NS_THEME_TOOLBAR_BUTTON:
      //aResult->SizeTo(5,5,5,5);    // 5px around the button in aqua
      break;

    case NS_THEME_DROPDOWN:
      if ( OnMacOSX() )
        aResult->SizeTo(kAquaDropdownLeftEndcap, kAquaPushButtonTopBottom, 
                          kAquaDropwdonRightEndcap, kAquaPushButtonTopBottom);
      else
        aResult->SizeTo(3,0,3,0);     // 3px for AGA
      break;
    
    case NS_THEME_TEXTFIELD:
    {
      SInt32 shadow = 0, frameOutset = 0;
      ::GetThemeMetric(kThemeMetricEditTextWhitespace, &shadow);
      ::GetThemeMetric(kThemeMetricEditTextFrameOutset, &frameOutset);
      aResult->SizeTo(shadow + frameOutset, shadow + frameOutset, shadow + frameOutset,
                        shadow + frameOutset);
      break;
    }

    case NS_THEME_LISTBOX:
    {
      SInt32 frameOutset = 0;
      ::GetThemeMetric(kThemeMetricListBoxFrameOutset, &frameOutset);
      aResult->SizeTo(frameOutset, frameOutset, frameOutset, frameOutset);
      break;
    }
      
  }
#endif
  
  return NS_OK;
}


NS_IMETHODIMP
nsNativeThemeMac::GetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                       PRUint8 aWidgetType, nsSize* aResult, PRBool* aIsOverridable)
{
  // XXX we should probably cache some of these metrics
  aResult->SizeTo(0,0);
  *aIsOverridable = PR_TRUE;
  
#if TARGET_CARBON
  switch ( aWidgetType ) {
  
    case NS_THEME_BUTTON:
    {
      SInt32 buttonHeight = 0;
      ::GetThemeMetric(kThemeMetricPushButtonHeight, &buttonHeight);
      aResult->SizeTo(kAquaPushButtonEndcaps*2, buttonHeight);
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
      ::GetThemeMetric(kThemeMetricCheckBoxWidth, &radioWidth);
      ::GetThemeMetric(kThemeMetricCheckBoxHeight, &radioHeight);
      aResult->SizeTo(radioWidth, radioHeight);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_DROPDOWN:
    {
      SInt32 popupHeight = 0;
      ::GetThemeMetric(kThemeMetricPopupButtonHeight, &popupHeight);
      aResult->SizeTo(0, popupHeight);
      break;
    }
    
    case NS_THEME_DROPDOWN_BUTTON:
      // the drawing for this is done by the dropdown, so just make this
      // zero sized.
      aResult->SizeTo(0,0);
      break;
      
    case NS_THEME_TEXTFIELD:
    {
      // at minimum, we should be tall enough for 9pt text.
      SInt32 shadow = 0, frameOutset = 0;
      ::GetThemeMetric(kThemeMetricEditTextWhitespace, &shadow);
      ::GetThemeMetric(kThemeMetricEditTextFrameOutset, &frameOutset);
      aResult->SizeTo(0, (shadow + frameOutset) * 2 + 9);      
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
      break;
    }

  }
#endif

  return NS_OK;
}


NS_IMETHODIMP
nsNativeThemeMac::WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                     nsIAtom* aAttribute, PRBool* aShouldRepaint)
{
  // Some widget types just never change state.
  switch ( aWidgetType ) {
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
    case NS_THEME_MENU:
      *aShouldRepaint = PR_FALSE;
      return NS_OK;
  }

  // XXXdwh Not sure what can really be done here.  Can at least guess for
  // specific widgets that they're highly unlikely to have certain states.
  // For example, a toolbar doesn't care about any states.
  if (!aAttribute) {
    // Hover/focus/active changed.  Always repaint.
    *aShouldRepaint = PR_TRUE;
  }
  else {
    // Check the attribute to see if it's relevant.  
    // disabled, checked, dlgtype, default, etc.
    *aShouldRepaint = PR_FALSE;
    if (aAttribute == mDisabledAtom || aAttribute == mCheckedAtom ||
        aAttribute == mSelectedAtom)
      *aShouldRepaint = PR_TRUE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNativeThemeMac::ThemeChanged()
{
  // what do we do here?
  return NS_OK;
}


PRBool 
nsNativeThemeMac::ThemeSupportsWidget(nsIPresContext* aPresContext,
                                      PRUint8 aWidgetType)
{
  // XXX We can go even further and call the API to ask if support exists.
  PRBool retVal = PR_FALSE;
  
  switch ( aWidgetType ) {
    case NS_THEME_DIALOG:
    case NS_THEME_WINDOW:
    case NS_THEME_MENU:
    case NS_THEME_TOOLTIP:
    
    case NS_THEME_CHECKBOX:
    case NS_THEME_CHECKBOX_CONTAINER:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_CONTAINER:
    case NS_THEME_BUTTON:
    case NS_THEME_TOOLBAR:
    case NS_THEME_STATUSBAR:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_DROPDOWN_TEXT:
    case NS_THEME_TEXTFIELD:
    //case NS_THEME_TOOLBOX:
    //case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TOOLBAR_SEPARATOR:
    
    case NS_THEME_LISTBOX:
    
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
      // for now, only use on osx since i haven't yet verified on os9
      if ( OnMacOSX() )
        retVal = PR_TRUE;
      break;
  
  }
  
  return retVal;
}


PRBool
nsNativeThemeMac::WidgetIsContainer(PRUint8 aWidgetType)
{
  // XXXdwh At some point flesh all of this out.
  switch ( aWidgetType ) {
   case NS_THEME_DROPDOWN_BUTTON:
   case NS_THEME_RADIO:
   case NS_THEME_CHECKBOX:
   case NS_THEME_PROGRESSBAR:
    return PR_FALSE;
    break;
  }
  return PR_TRUE;
}
