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

#include "nsReadableUtils.h"
#include "nsWidgetAtoms.h"
#include "nsWatchTask.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMElement.h"
#include "nsIScrollbarMediator.h"

NS_IMPL_ISUPPORTS_INHERITED1(nsNativeScrollbar, nsChildView, nsINativeScrollbar)

inline void BoundsCheck(PRInt32 low, PRUint32& value, PRUint32 high)
{
  if ((PRInt32) value < low)
    value = low;
  if (value > high)
    value = high;
}

nsNativeScrollbar::nsNativeScrollbar()
  : nsChildView()
  , mContent(nsnull)
  , mMediator(nsnull)
  , mScrollbar(nsnull)
  , mValue(0)
  , mMaxValue(0)
  , mVisibleImageSize(0)
  , mLineIncrement(0)
{
  WIDGET_SET_CLASSNAME("nsNativeScrollbar");
}


nsNativeScrollbar::~nsNativeScrollbar()
{
}


//
// CreateCocoaView
//
// Create a NativeScrollbarView for insertion into the cocoa view hierarchy.
// Cocoa sets the orientation of a scrollbar at creation time by looking
// at its frame and taking the longer side to be the orientation. Since
// chances are good at this point gecko just wants us to be 1x1, assume
// we're going to be vertical. If later when we get a content node assigned
// we find we're horizontal, we can update then.
//
NSView*
nsNativeScrollbar::CreateCocoaView ( )
{
  NSRect orientation;
  orientation.origin.x = orientation.origin.y = 0;
  orientation.size.width = 20;
  orientation.size.height = 100;

  return [[[NativeScrollbarView alloc] initWithFrame:orientation geckoChild:this] autorelease];
}


GrafPtr
nsNativeScrollbar::GetQuickDrawPort ( )
{
  // pray we're always a child of a NSQuickDrawView
  if ( [mParentView isKindOfClass: [ChildView class]] ) {
    NSQuickDrawView* parent = NS_STATIC_CAST(NSQuickDrawView*, mParentView);
    return [parent qdPort];
  }
  
  return nsnull;
}


//
// DoScroll
//
// Called from the action proc of the scrollbar, adjust the control's
// value as well as the value in the content node which communicates
// to gecko that the document is scrolling.
// 
void
nsNativeScrollbar::DoScroll(NSScrollerPart inPart)
{
  PRUint32 oldPos, newPos;
  PRUint32 incr;
  PRUint32 visibleImageSize;
  GetPosition(&oldPos);
  GetLineIncrement(&incr);
  GetViewSize(&visibleImageSize);
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
      newPos = oldPos - (mLineIncrement ? mLineIncrement : 1);
      if ( mMediator ) {
        BoundsCheck(0, newPos, mMaxValue);
        mMediator->ScrollbarButtonPressed(mScrollbar, oldPos, newPos);
      } else {
        UpdateContentPosition(newPos);
      }
      break;
    
    case NSScrollerIncrementLine:					// scroll down/right
      newPos = oldPos + (mLineIncrement ? mLineIncrement : 1);
      if ( mMediator ) {
        BoundsCheck(0, newPos, mMaxValue);
        mMediator->ScrollbarButtonPressed(mScrollbar, oldPos, newPos);
      } else {
        UpdateContentPosition(newPos); 
      }
      break;
  
    //
    // For page up/down, scroll by the page height and update the attributes
    // on the content node (as above). If we have a mediator, we're in an
    // outliner so tell it directly that the position has changed. Note that
    // outliner takes signed values, so we have to convert our unsigned to 
    // signed values first.
    //

    case NSScrollerDecrementPage:				// scroll up a page
      newPos = oldPos - visibleImageSize;
      UpdateContentPosition(newPos);
      if ( mMediator ) {
        PRInt32 op = oldPos, np = mValue;
        if ( np < 0 )
          np = 0;
        mMediator->PositionChanged(mScrollbar, op, np);
      }
      break;
    
    case NSScrollerIncrementPage:			// scroll down a page
      newPos = oldPos + visibleImageSize;
      UpdateContentPosition(newPos);
      if ( mMediator ) {
        PRInt32 op = oldPos, np = mValue;
        if ( np < 0 )
          np = 0;
        mMediator->PositionChanged(mScrollbar, op, np);
      }
      break;
    
    //
    // The scroller handles changing the value on the thumb for
    // us, so read it, convert it back to the range gecko is expecting,
    // and tell the content.
    //
    case NSScrollerKnob:
    case NSScrollerKnobSlot:
      newPos = (int) ([mView floatValue] * mMaxValue);
      UpdateContentPosition(newPos);
      if ( mMediator ) {
        PRInt32 op = oldPos, np = mValue;
        if ( np < 0 )
          np = 0;
        mMediator->PositionChanged(mScrollbar, op, np);
      }
      break;
    
    default:
      ; // do nothing
  }
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
  
  // guarantee |inNewPos| is in the range of [0, mMaxValue] so it's correctly unsigned
  BoundsCheck(0, inNewPos, mMaxValue);
    
  // convert the int to a string
  nsAutoString buffer;
  buffer.AppendInt(inNewPos);
  
  mContent->SetAttr(kNameSpaceID_None, nsWidgetAtoms::curpos, buffer, PR_TRUE);
  SetPosition(inNewPos);
}



//
// DispatchMouseEvent
//
// We don't need to do much here, cocoa will handle tracking the mouse for us. Returning
// true means that the event is handled.
//
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
    // Update the current value based on the new range. We need to recompute the
    // float value in case we had to set the value to 0 because gecko cheated
    // and set the position before it set the max value.
    PRInt32 fullVisibleArea = mVisibleImageSize + mMaxValue;
    [mView setFloatValue:(mValue / (float)mMaxValue) knobProportion:(mVisibleImageSize / (float)fullVisibleArea)];
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
// Set the current position of the slider and redraw the scrollbar. We have
// to convert between the integer values gecko uses (0,mMaxValue) to the float
// values that cocoa uses (0,1).
//
NS_IMETHODIMP
nsNativeScrollbar::SetPosition(PRUint32 aPos)
{
  if ((PRInt32)aPos < 0)
    aPos = 0;

  // while we _should_ be ensuring that we don't set our value higher
  // than our max value, the gfx scrollview code plays fast and loose
  // with the rules while going back/forward and adjusts the value to the
  // previous value long before it sets the max. As a result, we would
  // lose the given value (since max would most likely be 0). The only
  // way around that is to relax our restrictions a little bit. (bug 135191)
  //   mValue = ((PRInt32)aPos) > mMaxValue ? mMaxValue : ((int)aPos);
  mValue = aPos;
  if ( mMaxValue )
    [mView setFloatValue:(mValue / (float)mMaxValue)];
  else
    [mView setFloatValue:0.0];

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
  
  // Update the current value based on the new range. We need to recompute the
  // float value in case we had to set the value to 0 because gecko cheated
  // and set the position before it set the max value.
  PRInt32 fullVisibleArea = mVisibleImageSize + mMaxValue;
  [mView setFloatValue:(mValue / (float)mMaxValue) knobProportion:(mVisibleImageSize / (float)fullVisibleArea)];
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
nsNativeScrollbar::SetContent(nsIContent* inContent, nsISupports* inScrollbar, 
                              nsIScrollbarMediator* inMediator)
{
  mContent = inContent;
  mMediator = inMediator;
  mScrollbar = inScrollbar;
  
  if ( mContent ) {
    // we may have to re-create the scrollbar view as horizontal. Check the
    // 'orient' attribute and rebuild the view with all the settings
    // present in the current view
    nsAutoString orient;
    mContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::orient, orient);
    if ( orient.Equals(NS_LITERAL_STRING("horizontal")) )
      RecreateHorizontalScrollbar();
  }
  
  return NS_OK;
}


//
// RecreateHorizontalScrollbar
//
// Replace the vertical scroller we created earlier with a horizontal scroller
// of the same dimensions and values
//
void
nsNativeScrollbar::RecreateHorizontalScrollbar()
{
  // set framerect so that cocoa thinks it's a horizontal scroller
  NSRect orientation;
  orientation.origin.x = orientation.origin.y = 0;
  orientation.size.width = 100;
  orientation.size.height = 16;
  
  // save off the old values and get rid of the previous view. Hiding
  // it removes it from the parent hierarchy.
  NSRect oldBounds = [mView bounds];
  float oldValue = [mView floatValue];
  float oldProportion = [mView knobProportion];
  mVisible = PR_TRUE;														// ensure that hide does the work
  Show(PR_FALSE);
  [mView release];
  
  // create the new horizontal scroller, init it, hook it up to the
  // view hierarchy and reset the values.
  mView = [[NativeScrollbarView alloc] initWithFrame:orientation geckoChild:this];
  [mView setNativeWindow: [mParentView getNativeWindow]];
  [mView setFrame:oldBounds];
  [mView setFloatValue:oldValue knobProportion:oldProportion];
  Show(PR_TRUE);
  Enable(PR_TRUE);
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

//
// -initWithFrame:geckoChild
// Designated Initializer
//
// Init our superclass and make the connection to the gecko nsIWidget we're
// mirroring
//
- (id)initWithFrame:(NSRect)frameRect geckoChild:(nsNativeScrollbar*)inChild
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


- (BOOL)isFlipped
{
  return YES;
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

