/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsNativeScrollbar.h"
#include "nsIDeviceContext.h"
#if TARGET_CARBON || (UNIVERSAL_INTERFACES_VERSION >= 0x0330)
#include <ControlDefinitions.h>
#endif

#include "nsWidgetAtoms.h"
#include "nsWatchTask.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMElement.h"
#include "nsIScrollbarMediator.h"

NS_IMPL_ISUPPORTS_INHERITED1(nsNativeScrollbar, nsChildView, nsINativeScrollbar);


#if 0
//
// StControlActionProcOwner
//
// A class that wraps a control action proc so that it is disposed of
// correctly when the shared library shuts down
//
class StNativeControlActionProcOwner {
public:
  
  StNativeControlActionProcOwner ( )
  {
    sControlActionProc = NewControlActionUPP(nsNativeScrollbar::ScrollActionProc);
    NS_ASSERTION(sControlActionProc, "Couldn't create live scrolling action proc");
  }
  ~StNativeControlActionProcOwner ( )
  {
    if ( sControlActionProc )
      DisposeControlActionUPP(sControlActionProc);
  }

  ControlActionUPP ActionProc() { return sControlActionProc; }
  
private:
  ControlActionUPP sControlActionProc;  
};


static ControlActionUPP ScrollbarActionProc ( );

static ControlActionUPP 
ScrollbarActionProc ( )
{
  static StNativeControlActionProcOwner sActionProcOwner;
  return sActionProcOwner.ActionProc();
}
#endif


nsNativeScrollbar::nsNativeScrollbar()
  : nsChildView()
  , mContent(nsnull)
  , mMediator(nsnull)
  , mValue(0)
  , mMaxValue(0)
  , mVisibleImageSize(0)
  , mLineIncrement(0)
  , mMouseDownInScroll(PR_FALSE)
  , mClickedPartCode(0)
{
  WIDGET_SET_CLASSNAME("nsNativeScrollbar");
}


nsNativeScrollbar::~nsNativeScrollbar()
{
}


NSView*
nsNativeScrollbar::CreateCocoaView ( )
{
  // Cocoa sets the orientation of a scrollbar at creation time by looking
  // at its frame and taking the longer side to be the orientation. Since
  // chances are good at this point gecko just wants us to be 1x1, use
  // the flag at creation to force the desired orientation.
  NSRect orientation;
  orientation.origin.x = orientation.origin.y = 0;
  if ( 1 ) {
    orientation.size.width = 20;
    orientation.size.height = 100;
  }
  else {
    orientation.size.width = 100;
    orientation.size.height = 20;
  }
  return [[[NativeScrollbarView alloc] initWithFrame:orientation] autorelease];

}

GrafPtr
nsNativeScrollbar::GetQuickDrawPort ( )
{
  // pray we're always a child of a NSQuickDrawView
  NSQuickDrawView* parent = (NSQuickDrawView*)[mView superview];
  return [parent qdPort];
}


//
// ScrollActionProc
//
// Called from the OS toolbox while the scrollbar is being tracked.
//
pascal void
nsNativeScrollbar::ScrollActionProc(ControlHandle ctrl, ControlPartCode part)
{
  nsNativeScrollbar* self = (nsNativeScrollbar*)(::GetControlReference(ctrl));
  NS_ASSERTION(self, "NULL nsNativeScrollbar");
  if ( self )
    self->DoScrollAction(part);
}


//
// DoScrollAction
//
// Called from the action proc of the scrollbar, adjust the control's
// value as well as the value in the content node which communicates
// to gecko that the document is scrolling.
// 
void
nsNativeScrollbar::DoScrollAction(ControlPartCode part)
{
return;

#if 0
  PRUint32 oldPos, newPos;
  PRUint32 incr;
  PRUint32 visibleImageSize;
  PRInt32 scrollBarMessage = 0;
  GetPosition(&oldPos);
  GetLineIncrement(&incr);
  GetViewSize(&visibleImageSize);
  switch(part)
  {
    //
    // For the up/down buttons, scroll up or down by the line height and 
    // update the attributes on the content node (the scroll frame listens
    // for these attributes and will scroll accordingly). However,
    // if we have a mediator, we're in an outliner and we have to scroll by
    // lines. Outliner ignores the params to ScrollbarButtonPressed() except
    // to check if one is greater than the other to indicate direction.
    //
    
    case kControlUpButtonPart:
      if ( mMediator )
        mMediator->ScrollbarButtonPressed(1,0);
      else {
        newPos = oldPos - incr;
        UpdateContentPosition(newPos);
      }
      break;
         
    case kControlDownButtonPart:
      if ( mMediator )
        mMediator->ScrollbarButtonPressed(0,1);
      else {
        newPos = oldPos + incr;
        UpdateContentPosition(newPos); 
      }
      break;
    
    //
    // For page up/down and dragging the thumb, scroll by the page height
    // (or directly report the value of the scrollbar) and update the attributes
    // on the content node (as above). If we have a mediator, we're in an
    // outliner so tell it directly that the position has changed. Note that
    // outliner takes signed values, so we have to convert our unsigned to 
    // signed values first.
    //
    
    case kControlPageUpPart:
      newPos = oldPos - visibleImageSize;
      UpdateContentPosition(newPos);
      if ( mMediator ) {
        PRInt32 op = oldPos, np = mValue;
        if ( np < 0 )
          np = 0;
        mMediator->PositionChanged(op, np);
      }
      break;
      
    case kControlPageDownPart:
      newPos = oldPos + visibleImageSize;
      UpdateContentPosition(newPos);
      if ( mMediator ) {
        PRInt32 op = oldPos, np = mValue;
        if ( np < 0 )
          np = 0;
        mMediator->PositionChanged(op, np);
      }
      break;
      
    case kControlIndicatorPart:
      newPos = ::GetControl32BitValue(GetControl());
      UpdateContentPosition(newPos);
      if ( mMediator ) {
        PRInt32 op = oldPos, np = mValue;
        if ( np < 0 )
          np = 0;
        mMediator->PositionChanged(op, np);
      }
      break;
  }
  EndDraw();
    
	// update the area of the parent uncovered by the scrolling. Since
	// we may be in a tight loop, we need to manually validate the area
	// we just updated so the update rect doesn't continue to get bigger
	// and bigger the more we scroll.
	nsCOMPtr<nsIWidget> parent ( dont_AddRef(GetParent()) );
	parent->Update();
	parent->Validate();

  StartDraw();
#endif
}


//
// UpdateContentPosition
//
// Tell the content node that the scrollbar has changed value and
// then update the scrollbar's position
//
void
nsNativeScrollbar::UpdateContentPosition(PRUint32 inNewPos)
{
  if ( inNewPos == mValue || !mContent )   // break any possible recursion
    return;
  
  // convert the int to a string
  char buffer[20];
  sprintf(buffer, "%d", inNewPos);
  
  mContent->SetAttr(kNameSpaceID_None, nsWidgetAtoms::curpos, NS_ConvertASCIItoUCS2(buffer), PR_TRUE);
  SetPosition(inNewPos);
}


/**-------------------------------------------------------------------------------
 * DispatchMouseEvent handle an event for this scrollbar
 * @update  dc 08/31/98
 * @Param aEvent -- The mouse event to respond to for this button
 * @return -- True if the event was handled, PR_FALSE if we did not handle it.
 */ 
PRBool
nsNativeScrollbar::DispatchMouseEvent(nsMouseEvent &aEvent)
{
  return PR_TRUE;
}


//
// SetMaxRange
//
// Set the maximum range of a scroll bar. This should be set to the
// full scrollable area minus the visible area.
//
NS_IMETHODIMP
nsNativeScrollbar::SetMaxRange(PRUint32 aEndRange)
{
  mMaxValue = ((int)aEndRange) > 0 ? aEndRange : 10;
  if ( GetControl() ) {
    PRInt32 fullVisibleArea = mVisibleImageSize + mMaxValue;
    [mView setFloatValue:[mView floatValue] knobProportion:(mVisibleImageSize / (float)fullVisibleArea)];
  }
  return NS_OK;
}


//
// GetMaxRange
//
// Get the maximum range of a scroll bar
//
NS_IMETHODIMP
nsNativeScrollbar::GetMaxRange(PRUint32* aMaxRange)
{
  *aMaxRange = mMaxValue;
  return NS_OK;
}


//
// SetPosition
//
// Set the current position of the slider and redraw the scrollbar
//
NS_IMETHODIMP
nsNativeScrollbar::SetPosition(PRUint32 aPos)
{
  if ((PRInt32)aPos < 0)
    aPos = 0;

  mValue = ((PRInt32)aPos) > mMaxValue ? mMaxValue : ((int)aPos);
  [mView setFloatValue:(aPos / (float)mMaxValue)];

  return NS_OK;
}


//
// GetPosition
//
// Get the current position of the slider
//
NS_IMETHODIMP
nsNativeScrollbar::GetPosition(PRUint32* aPos)
{
  *aPos = mValue;
  return NS_OK;
}


//
// SetViewSize
//
// Change the knob proportion to be the ratio of the size of the visible image (given in |aSize|)
// to the total area (visible + max). Recall that the max size is the total minus the
// visible area.
//
NS_IMETHODIMP
nsNativeScrollbar::SetViewSize(PRUint32 aSize)
{
  mVisibleImageSize = ((int)aSize) > 0 ? aSize : 1;
  
  PRInt32 fullVisibleArea = mVisibleImageSize + mMaxValue;
  [mView setFloatValue:[mView floatValue] knobProportion:(mVisibleImageSize / (float)fullVisibleArea)];
  
  return NS_OK;
}


//
// GetViewSize
//
// Get the height of the visible view area.
//
NS_IMETHODIMP
nsNativeScrollbar::GetViewSize(PRUint32* aSize)
{
  *aSize = mVisibleImageSize;
  return NS_OK;
}


//
// SetLineIncrement
//
// Set the line increment of the scroll bar
//
NS_IMETHODIMP
nsNativeScrollbar::SetLineIncrement(PRUint32 aLineIncrement)
{
  mLineIncrement  = (((int)aLineIncrement) > 0 ? aLineIncrement : 1);
  return NS_OK;
}


//
// GetLineIncrement
//
// Get the line increment of the scroll bar
//
NS_IMETHODIMP
nsNativeScrollbar::GetLineIncrement(PRUint32* aLineIncrement)
{
  *aLineIncrement = mLineIncrement;
  return NS_OK;
}


//
// GetNarrowSize
//
// Ask the appearance manager for the dimensions of the narrow axis
// of the scrollbar. We cheat and assume the width of a vertical scrollbar
// is the same as the height of a horizontal scrollbar. *shrug*. Shoot me.
//
NS_IMETHODIMP
nsNativeScrollbar::GetNarrowSize(PRInt32* outSize)
{
  if ( *outSize )
    return NS_ERROR_FAILURE;
  SInt32 width = 0;
  ::GetThemeMetric(kThemeMetricScrollBarWidth, &width);
  *outSize = width;
  return NS_OK;
}


//
// SetContent
//
// Hook up this native scrollbar to the rest of gecko. We care about
// the content so we can set attributes on it to affect the scrollview. We
// care about the mediator for <outliner> so we can do row-based scrolling.
//
NS_IMETHODIMP
nsNativeScrollbar::SetContent(nsIContent* inContent, nsIScrollbarMediator* inMediator)
{
  mContent = inContent;
  mMediator = inMediator;
  return NS_OK;
}


//
// Show
//
// Hide or show the scrollbar
//
NS_IMETHODIMP
nsNativeScrollbar::Show(PRBool bState)
{
  // the only way to get the scrollbar view to not draw is to remove it
  // from the view hierarchy. cache the parent view so that we can
  // hook it up later if we're told to show.
  if ( mVisible && !bState ) {
    mParentView = [mView superview];
    [mView removeFromSuperview];
  }
  else if ( !mVisible && bState ) {
    if ( mParentView )
      [mParentView addSubview:mView];
  }

  mVisible = bState;
  return NS_OK;
}


//
// Enable
//
// Enable/disable this scrollbar
//
NS_IMETHODIMP
nsNativeScrollbar::Enable(PRBool bState)
{
  [mView setEnabled:(bState ? YES : NO)];
  return NS_OK;
}


NS_IMETHODIMP
nsNativeScrollbar::IsEnabled(PRBool *aState)
{
  if (aState)
   *aState = [mView isEnabled] ? PR_TRUE : PR_FALSE;
  return NS_OK;
}


#pragma mark -


@implementation NativeScrollbarView

- (NSWindow*) getNativeWindow
{
  NSWindow* currWin = [self window];
  if (currWin)
     return currWin;
  else
     return mWindow;
}

- (void) setNativeWindow: (NSWindow*)aWindow
{
  mWindow = aWindow;
}

- (void)trackKnob:(NSEvent *)theEvent
{
  printf("tracking knob\n");
  [super trackKnob:theEvent];
  printf("done tracking knob\n");
}

- (void)trackScrollButtons:(NSEvent *)theEvent
{
  printf("tracking buttons");
  [super trackScrollButtons:theEvent];
  printf("done tracking buttons");
}


- (BOOL)isFlipped
{
  return YES;
}


@end

