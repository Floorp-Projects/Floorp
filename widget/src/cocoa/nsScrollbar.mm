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

#include "nsScrollbar.h"
#include "nsIDeviceContext.h"
#if TARGET_CARBON || (UNIVERSAL_INTERFACES_VERSION >= 0x0330)
#include <ControlDefinitions.h>
#endif

NS_IMPL_ISUPPORTS_INHERITED1(nsScrollbar, nsChildView, nsIScrollbar);


ControlActionUPP nsScrollbar::sControlActionProc = nsnull;


nsScrollbar::nsScrollbar()
	:	Inherited()
	, mValue(0)
	,	mMaxValue(0)
	,	mVisibleImageSize(0)
	,	mLineIncrement(0)
	,	mMouseDownInScroll(PR_FALSE)
	,	mClickedPartCode(0)
{
	WIDGET_SET_CLASSNAME("nsScrollbar");
}


nsScrollbar::~nsScrollbar()
{
}



NSView*
nsScrollbar::CreateCocoaView ( )
{
  // Cocoa sets the orientation of a scrollbar at creation time by looking
  // at its frame and taking the longer side to be the orientation. Since
  // chances are good at this point gecko just wants us to be 1x1, use
  // the flag at creation to force the desired orientation.
  NSRect orientation;
  orientation.origin.x = orientation.origin.y = 0;
  if ( IsVertical() ) {
    orientation.size.width = 20;
    orientation.size.height = 100;
  }
  else {
    orientation.size.width = 100;
    orientation.size.height = 20;
  }
  return [[[ScrollbarView alloc] initWithFrame:orientation] autorelease];
}


GrafPtr
nsScrollbar::GetQuickDrawPort ( )
{
  // pray we're always a child of a NSQuickDrawView
  NSQuickDrawView* parent = (NSQuickDrawView*)[mView superview];
  return [parent qdPort];
}



/**-------------------------------------------------------------------------------
 * ScrollActionProc Callback for TrackControl
 * @update	jrm 99/01/11
 * @param ctrl - The Control being tracked
 * @param part - Part of the control (arrow, thumb, gutter) being hit
 */
pascal 
void nsScrollbar::ScrollActionProc(ControlHandle ctrl, ControlPartCode part)
{
	nsScrollbar* me = (nsScrollbar*)(::GetControlReference(ctrl));
	NS_ASSERTION(nsnull != me, "NULL nsScrollbar");
	if (nsnull != me)
		me->DoScrollAction(part);
}

/**-------------------------------------------------------------------------------
 * ScrollActionProc Callback for TrackControl
 * @update	jrm 99/01/11
 * @param part - Part of the control (arrow, thumb, gutter) being hit
 */
void nsScrollbar::DoScrollAction(ControlPartCode part)
{
	PRUint32 pos;
	PRUint32 incr;
	PRUint32 visibleImageSize;
	PRInt32 scrollBarMessage = 0;
	GetPosition(pos);
	GetLineIncrement(incr);
	GetThumbSize(visibleImageSize);
	switch(part)
	{
		case kControlUpButtonPart:
		{
			scrollBarMessage = NS_SCROLLBAR_LINE_PREV;
			SetPosition(pos - incr);
			break;
		}
		case kControlDownButtonPart:
			scrollBarMessage = NS_SCROLLBAR_LINE_NEXT;
			SetPosition(pos + incr);
			break;
		case kControlPageUpPart:
			scrollBarMessage = NS_SCROLLBAR_PAGE_PREV;
			SetPosition(pos - visibleImageSize);
			break;
		case kControlPageDownPart:
			scrollBarMessage = NS_SCROLLBAR_PAGE_NEXT;
			SetPosition(pos + visibleImageSize);
			break;
		case kControlIndicatorPart:
			scrollBarMessage = NS_SCROLLBAR_POS;
			SetPosition([mView intValue]);
			break;
	}
	EndDraw();
	
	// send event to scroll the parent
	nsScrollbarEvent scrollBarEvent;
	scrollBarEvent.eventStructType = NS_GUI_EVENT;
	scrollBarEvent.widget = this;
	scrollBarEvent.message = scrollBarMessage;
	GetPosition(pos);
	scrollBarEvent.position = pos;
	Inherited::DispatchWindowEvent(scrollBarEvent);

	// update the area of the parent uncovered by the scrolling
	nsIWidget* parent = GetParent();
	parent->Update();
	NS_RELEASE(parent);

	// update this scrollbar
	Invalidate(PR_FALSE);
	Update();

	StartDraw();
}

/**-------------------------------------------------------------------------------
 * DispatchMouseEvent handle an event for this scrollbar
 * @update  dc 08/31/98
 * @Param aEvent -- The mouse event to respond to for this button
 * @return -- True if the event was handled, PR_FALSE if we did not handle it.
 */ 
PRBool nsScrollbar::DispatchMouseEvent(nsMouseEvent &aEvent)
{
  return PR_TRUE;
}


//
// SetMaxRange
//
// Set the maximum range of a scroll bar. This should be set to the
// full scrollable area minus the visible area.
//
NS_METHOD nsScrollbar::SetMaxRange(PRUint32 aEndRange)
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
NS_METHOD nsScrollbar::GetMaxRange(PRUint32& aMaxRange)
{
	aMaxRange = mMaxValue;
	return NS_OK;
}


//
// SetPosition
//
// Set the current position of the slider and redraw the scrollbar
//
NS_METHOD nsScrollbar::SetPosition(PRUint32 aPos)
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
NS_METHOD nsScrollbar::GetPosition(PRUint32& aPos)
{
	aPos = mValue;
	return NS_OK;
}


//
// SetThumbSize
//
// Change the knob proportion to be the ratio of the size of the visible image (given in |aSize|)
// to the total area (visible + max). Recall that the max size is the total minus the
// visible area.
//
NS_METHOD nsScrollbar::SetThumbSize(PRUint32 aSize)
{
  mVisibleImageSize = ((int)aSize) > 0 ? aSize : 1;
  
  PRInt32 fullVisibleArea = mVisibleImageSize + mMaxValue;
  [mView setFloatValue:[mView floatValue] knobProportion:(mVisibleImageSize / (float)fullVisibleArea)];
  
  return NS_OK;
}


//
// GetThumbSize
//
// Get the height of the visible view area.
//
NS_METHOD nsScrollbar::GetThumbSize(PRUint32& aSize)
{
	aSize = mVisibleImageSize;
	return NS_OK;
}


//
// SetLineIncrement
//
// Set the line increment of the scroll bar
//
NS_METHOD nsScrollbar::SetLineIncrement(PRUint32 aLineIncrement)
{
	mLineIncrement	= (((int)aLineIncrement) > 0 ? aLineIncrement : 1);
	return NS_OK;
}


//
// GetLineIncrement
//
// Get the line increment of the scroll bar
//
NS_METHOD nsScrollbar::GetLineIncrement(PRUint32& aLineIncrement)
{
	aLineIncrement = mLineIncrement;
	return NS_OK;
}

/**-------------------------------------------------------------------------------
 *	See documentation in nsScrollbar.h
 *	@update	dc 012/10/98
 */
NS_METHOD nsScrollbar::SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
								PRUint32 aPosition, PRUint32 aLineIncrement)
{
	SetLineIncrement(aLineIncrement);
	SetPosition(aPosition);
	mVisibleImageSize = aThumbSize; // needed by SetMaxRange
	SetMaxRange(aMaxRange);
	SetThumbSize(aThumbSize); // Needs to know the maximum value when calling Mac toolbox.

	return NS_OK;
}


//
// Show
//
// Hide or show the scrollbar
//
NS_IMETHODIMP
nsScrollbar::Show(PRBool bState)
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
nsScrollbar::Enable(PRBool bState)
{
  [mView setEnabled:(bState ? YES : NO)];
  return NS_OK;
}


NS_IMETHODIMP
nsScrollbar::IsEnabled(PRBool *aState)
{
  if (aState)
   *aState = [mView isEnabled] ? PR_TRUE : PR_FALSE;
  return NS_OK;
}


#pragma mark -

@implementation ScrollbarView

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

- (void)mouseDown:(NSEvent *)theEvent
{
  printf("mouse down in scrollbar\n");
  [super mouseDown:theEvent];
}


@end
