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
  return [[[ScrollbarView alloc] initWithFrame:orientation geckoChild:this] autorelease];
}


GrafPtr
nsScrollbar::GetQuickDrawPort ( )
{
  // pray we're always a child of a NSQuickDrawView
  NSQuickDrawView* parent = (NSQuickDrawView*)[mView superview];
  return [parent qdPort];
}

//
// DoScroll
//
// Called from the action proc of the scrollbar, adjust the control's
// value as well as the value in the content node which communicates
// to gecko that the document is scrolling.
// 
void
nsScrollbar::DoScroll(NSScrollerPart inPart)
{
	PRInt32 scrollBarMessage = 0;
  PRUint32 oldPos, newPos;
  PRUint32 incr;
  GetPosition(oldPos);
  GetLineIncrement(incr);
  switch ( inPart ) {

    //
    // For the up/down buttons, scroll up or down by the line height and 
    // update the attributes on the content node (the scroll frame listens
    // for these attributes and will scroll accordingly). However,
    // if we have a mediator, we're in an outliner and we have to scroll by
    // lines. Outliner ignores the params to ScrollbarButtonPressed() except
    // to check if one is greater than the other to indicate direction.
    //

    case NSScrollerDecrementLine: 				// scroll up/left
			scrollBarMessage = NS_SCROLLBAR_LINE_PREV;
      newPos = oldPos - incr;
      SetPosition(newPos);
      break;
    
    case NSScrollerIncrementLine:					// scroll down/right
			scrollBarMessage = NS_SCROLLBAR_LINE_NEXT;
      newPos = oldPos + incr;
      SetPosition(newPos); 
      break;
  
    case NSScrollerDecrementPage:				// scroll up a page
			scrollBarMessage = NS_SCROLLBAR_PAGE_PREV;
      newPos = oldPos - mVisibleImageSize;
      SetPosition(newPos);
      break;
    
    case NSScrollerIncrementPage:			// scroll down a page
			scrollBarMessage = NS_SCROLLBAR_PAGE_NEXT;
      newPos = oldPos + mVisibleImageSize;
      SetPosition(newPos);
      break;
    
    //
    // The scroller handles changing the value on the thumb for
    // us, so read it, convert it back to the range gecko is expecting,
    // and tell the content.
    //
    case NSScrollerKnob:
    case NSScrollerKnobSlot:
			scrollBarMessage = NS_SCROLLBAR_POS;
      newPos = (int) ([mView floatValue] * (mMaxValue-mVisibleImageSize));
      SetPosition(newPos);
      break;
    
    default:
      newPos = oldPos; // do nothing
  }
  
	// send event to scroll the parent
  nsScrollbarEvent scrollBarEvent;
  scrollBarEvent.eventStructType = NS_GUI_EVENT;
  scrollBarEvent.widget = this;
  scrollBarEvent.message = scrollBarMessage;
  GetPosition(newPos);
  scrollBarEvent.position = newPos;
  Inherited::DispatchWindowEvent(scrollBarEvent);
  
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
  if ( GetControl() )
    [mView setFloatValue:[mView floatValue] knobProportion:(mVisibleImageSize / (float)mMaxValue)];
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

  mValue = aPos > mMaxValue ? mMaxValue : aPos;
  [mView setFloatValue:(mValue / (float)(mMaxValue-mVisibleImageSize))];

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
  
  [mView setFloatValue:(mValue / (float)(mMaxValue-mVisibleImageSize))
          knobProportion:(mVisibleImageSize / (float)mMaxValue)];
  
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

  mVisibleImageSize = ((PRInt32)aThumbSize) > 0 ? aThumbSize : 1;
  mMaxValue = ((PRInt32)aMaxRange) > 0 ? aMaxRange : 10;
  mValue = aPosition > mMaxValue ? mMaxValue : aPosition;

  [mView setFloatValue:(mValue / (float)(mMaxValue-mVisibleImageSize))
          knobProportion:(mVisibleImageSize / (float)mMaxValue )];

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

//
// -initWithFrame:geckoChild
// Designated Initializer
//
// Init our superclass and make the connection to the gecko nsIWidget we're
// mirroring
//
- (id)initWithFrame:(NSRect)frameRect geckoChild:(nsScrollbar*)inChild
{
  [super initWithFrame:frameRect];

  NS_ASSERTION(inChild, "Need to provide a tether between this and a nsChildView class");
  mGeckoChild = inChild;
  
  // make ourselves the target of the scroll and set the action message
  [self setTarget:self];
  [self setAction:@selector(scroll:)];

  return self;
}


//
// -initWithFrame
//
// overridden parent class initializer
//
- (id)initWithFrame:(NSRect)frameRect
{
  NS_WARNING("You're calling the wrong initializer. You really want -initWithFrame:geckoChild");
  return [self initWithFrame:frameRect geckoChild:nsnull];
}

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

//
// -widget
//
// return our gecko child view widget. Note this does not AddRef.
//
- (nsIWidget*) widget
{
  return NS_STATIC_CAST(nsIWidget*, mGeckoChild);
}

- (BOOL)isFlipped
{
  return YES;
}


//
// -mouseMoved
//
// our parent view will try to forward this message down to us. The
// default behavior for NSResponder is to forward it up the chain. Can you
// say "infinite recursion"? I thought so. Just stub out the action to
// break the cycle of madness.
//
- (void)mouseMoved:(NSEvent*)theEvent
{
  // do nothing
}

//
// -scroll
//
// the action message we've set up to be called when the scrollbar needs
// to adjust its value. Feed back into the owning widget to process
// how much to scroll and adjust the correct attributes.
//
- (IBAction)scroll:(NSScroller*)sender
{
  if ( mGeckoChild )
    mGeckoChild->DoScroll([sender hitPart]);
}


@end

