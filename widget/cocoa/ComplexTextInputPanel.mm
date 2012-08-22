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

#include "mozilla/Preferences.h"

using namespace mozilla;

#define kInputWindowHeight 20

@implementation ComplexTextInputPanel

+ (ComplexTextInputPanel*)sharedComplexTextInputPanel
{
  static ComplexTextInputPanel *sComplexTextInputPanel;
  if (!sComplexTextInputPanel)
    sComplexTextInputPanel = [[ComplexTextInputPanel alloc] init];
  return sComplexTextInputPanel;
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

- (BOOL)interpretKeyEvent:(NSEvent*)event string:(NSString**)string
{
  BOOL hadMarkedText = [mInputTextView hasMarkedText];

  *string = nil;

  if (![[mInputTextView inputContext] handleEvent:event])
    return NO;

  if ([mInputTextView hasMarkedText]) {
    // Don't show the input method window for dead keys
    if ([[event characters] length] > 0)
      [self orderFront:nil];

    return YES;
  } else {
    [self orderOut:nil];

    NSString *text = [[mInputTextView textStorage] string];
    if ([text length] > 0)
      *string = [[text copy] autorelease];
  }

  [mInputTextView setString:@""];
  return hadMarkedText;
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

@end
