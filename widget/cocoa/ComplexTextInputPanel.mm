/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 * Modified by Josh Aas of Mozilla Corporation.
 */

#import "ComplexTextInputPanel.h"

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include "mozilla/Preferences.h"
#include "nsChildView.h"
#include "nsCocoaFeatures.h"

using namespace mozilla;

extern "C" OSStatus TSMProcessRawKeyEvent(EventRef anEvent);

#define kInputWindowHeight 20

@interface ComplexTextInputPanelImpl : NSPanel {
  NSTextView *mInputTextView;
}

+ (ComplexTextInputPanelImpl*)sharedComplexTextInputPanelImpl;

- (NSTextInputContext*)inputContext;
- (void)interpretKeyEvent:(NSEvent*)event string:(NSString**)string;
- (void)cancelComposition;
- (BOOL)inComposition;

// This places the text input panel fully onscreen and below the lower left
// corner of the focused plugin.
- (void)adjustTo:(NSPoint)point;

@end

@implementation ComplexTextInputPanelImpl

+ (ComplexTextInputPanelImpl*)sharedComplexTextInputPanelImpl
{
  static ComplexTextInputPanelImpl *sComplexTextInputPanelImpl;
  if (!sComplexTextInputPanelImpl)
    sComplexTextInputPanelImpl = [[ComplexTextInputPanelImpl alloc] init];
  return sComplexTextInputPanelImpl;
}

- (id)init
{
  // In the original Apple code the style mask is given by a function which is not open source.
  // What could possibly be worth hiding in that function, I do not know.
  // Courtesy of gdb: stylemask: 011000011111, 0x61f
  self = [super initWithContentRect:NSZeroRect styleMask:0x61f backing:NSBackingStoreBuffered defer:YES];
  if (!self)
    return nil;

  // Set the frame size.
  NSRect visibleFrame = [[NSScreen mainScreen] visibleFrame];
  NSRect frame = NSMakeRect(visibleFrame.origin.x, visibleFrame.origin.y, visibleFrame.size.width, kInputWindowHeight);

  [self setFrame:frame display:NO];

  mInputTextView = [[NSTextView alloc] initWithFrame:[self.contentView frame]];        
  mInputTextView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable | NSViewMaxXMargin | NSViewMinXMargin | NSViewMaxYMargin | NSViewMinYMargin;

  NSScrollView* scrollView = [[NSScrollView alloc] initWithFrame:[self.contentView frame]];
  scrollView.documentView = mInputTextView;
  self.contentView = scrollView;
  [scrollView release];

  [self setFloatingPanel:YES];

  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(keyboardInputSourceChanged:)
                                               name:NSTextInputContextKeyboardSelectionDidChangeNotification
                                             object:nil];

  return self;
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  
  [mInputTextView release];
  
  [super dealloc];
}

- (void)keyboardInputSourceChanged:(NSNotification *)notification
{
  static int8_t sDoCancel = -1;
  if (!sDoCancel || ![self inComposition]) {
    return;
  }
  if (sDoCancel < 0) {
    bool cancelComposition = false;
    static const char* kPrefName =
      "ui.plugin.cancel_composition_at_input_source_changed";
    nsresult rv = Preferences::GetBool(kPrefName, &cancelComposition);
    NS_ENSURE_SUCCESS(rv, );
    sDoCancel = cancelComposition ? 1 : 0;
  }
  if (sDoCancel) {
    [self cancelComposition];
  }
}

- (void)interpretKeyEvent:(NSEvent*)event string:(NSString**)string
{
  *string = nil;

  if (![[mInputTextView inputContext] handleEvent:event]) {
    return;
  }

  if ([mInputTextView hasMarkedText]) {
    // Don't show the input method window for dead keys
    if ([[event characters] length] > 0) {
      [self orderFront:nil];
    }
    return;
  } else {
    [self orderOut:nil];

    NSString *text = [[mInputTextView textStorage] string];
    if ([text length] > 0) {
      *string = [[text copy] autorelease];
    }
  }

  [mInputTextView setString:@""];
}

- (NSTextInputContext*)inputContext
{
  return [mInputTextView inputContext];
}

- (void)cancelComposition
{
  [mInputTextView setString:@""];
  [self orderOut:nil];
}

- (BOOL)inComposition
{
  return [mInputTextView hasMarkedText];
}

- (void)adjustTo:(NSPoint)point
{
  NSRect selfRect = [self frame];
  NSRect rect = NSMakeRect(point.x,
                           point.y - selfRect.size.height,
                           500,
                           selfRect.size.height);

  // Adjust to screen.
  NSRect screenRect = [[NSScreen mainScreen] visibleFrame];
  if (rect.origin.x < screenRect.origin.x) {
    rect.origin.x = screenRect.origin.x;
  }
  if (rect.origin.y < screenRect.origin.y) {
    rect.origin.y = screenRect.origin.y;
  }
  CGFloat xMostOfScreen = screenRect.origin.x + screenRect.size.width;
  CGFloat yMostOfScreen = screenRect.origin.y + screenRect.size.height;
  CGFloat xMost = rect.origin.x + rect.size.width;
  CGFloat yMost = rect.origin.y + rect.size.height;
  if (xMostOfScreen < xMost) {
    rect.origin.x -= xMost - xMostOfScreen;
  }
  if (yMostOfScreen < yMost) {
    rect.origin.y -= yMost - yMostOfScreen;
  }

  [self setFrame:rect display:[self isVisible]];
}

@end

class ComplexTextInputPanelPrivate : public ComplexTextInputPanel
{
public:
  ComplexTextInputPanelPrivate();

  virtual void InterpretKeyEvent(void* aEvent, nsAString& aOutText);
  virtual bool IsInComposition();
  virtual void PlacePanel(int32_t x, int32_t y);
  virtual void* GetInputContext() { return [mPanel inputContext]; }
  virtual void CancelComposition() { [mPanel cancelComposition]; }

private:
  ~ComplexTextInputPanelPrivate();
  ComplexTextInputPanelImpl* mPanel;
};

ComplexTextInputPanelPrivate::ComplexTextInputPanelPrivate()
{
  mPanel = [[ComplexTextInputPanelImpl alloc] init];
}

ComplexTextInputPanelPrivate::~ComplexTextInputPanelPrivate()
{
  [mPanel release];
}

ComplexTextInputPanel*
ComplexTextInputPanel::GetSharedComplexTextInputPanel()
{
  static ComplexTextInputPanelPrivate *sComplexTextInputPanelPrivate;
  if (!sComplexTextInputPanelPrivate) {
    sComplexTextInputPanelPrivate = new ComplexTextInputPanelPrivate();
  }
  return sComplexTextInputPanelPrivate;
}

void
ComplexTextInputPanelPrivate::InterpretKeyEvent(void* aEvent, nsAString& aOutText)
{
  NSString* textString = nil;
  [mPanel interpretKeyEvent:(NSEvent*)aEvent string:&textString];

  if (textString) {
    nsCocoaUtils::GetStringForNSString(textString, aOutText);
  }
}

bool
ComplexTextInputPanelPrivate::IsInComposition()
{
  return !![mPanel inComposition];
}

void
ComplexTextInputPanelPrivate::PlacePanel(int32_t x, int32_t y)
{
  [mPanel adjustTo:NSMakePoint(x, y)];
}
