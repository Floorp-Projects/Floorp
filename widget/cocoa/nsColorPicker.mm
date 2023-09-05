/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsColorPicker.h"
#include "nsCocoaUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla;

static unsigned int HexStrToInt(NSString* str) {
  unsigned int result = 0;

  for (unsigned int i = 0; i < [str length]; ++i) {
    char c = [str characterAtIndex:i];
    result *= 16;
    if (c >= '0' && c <= '9') {
      result += c - '0';
    } else if (c >= 'A' && c <= 'F') {
      result += 10 + (c - 'A');
    } else {
      result += 10 + (c - 'a');
    }
  }

  return result;
}

@interface NSColorPanelWrapper : NSObject <NSWindowDelegate> {
  NSColorPanel* mColorPanel;
  nsColorPicker* mColorPicker;
}
- (id)initWithPicker:(nsColorPicker*)aPicker;
- (void)open:(NSColor*)aInitialColor title:(NSString*)aTitle;
- (void)colorChanged:(NSColorPanel*)aPanel;
- (void)windowWillClose:(NSNotification*)aNotification;
- (void)close;
@end

@implementation NSColorPanelWrapper
- (id)initWithPicker:(nsColorPicker*)aPicker {
  mColorPicker = aPicker;
  mColorPanel = [NSColorPanel sharedColorPanel];

  self = [super init];
  return self;
}

- (void)open:(NSColor*)aInitialColor title:(NSString*)aTitle {
  [mColorPanel setTarget:self];
  [mColorPanel setAction:@selector(colorChanged:)];
  [mColorPanel setDelegate:self];
  [mColorPanel setTitle:aTitle];
  [mColorPanel setColor:aInitialColor];
  [mColorPanel setFrameOrigin:[NSEvent mouseLocation]];
  [mColorPanel makeKeyAndOrderFront:nil];
}

- (void)colorChanged:(NSColorPanel*)aPanel {
  if (!mColorPicker) {
    return;
  }
  mColorPicker->Update([mColorPanel color]);
}

- (void)windowWillClose:(NSNotification*)aNotification {
  if (!mColorPicker) {
    return;
  }
  mColorPicker->Done();
}

- (void)close {
  [mColorPanel setTarget:nil];
  [mColorPanel setAction:nil];
  [mColorPanel setDelegate:nil];

  mColorPanel = nil;
  mColorPicker = nullptr;
}
@end

NS_IMPL_ISUPPORTS(nsColorPicker, nsIColorPicker)

nsColorPicker::~nsColorPicker() {
  if (mColorPanelWrapper) {
    [mColorPanelWrapper close];
    [mColorPanelWrapper release];
    mColorPanelWrapper = nullptr;
  }
}

// TODO(bug 1805397): Implement default colors
NS_IMETHODIMP
nsColorPicker::Init(mozIDOMWindowProxy* aParent, const nsAString& aTitle,
                    const nsAString& aInitialColor,
                    const nsTArray<nsString>& aDefaultColors) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Color pickers can only be opened from main thread currently");
  mTitle = aTitle;
  mColor = aInitialColor;
  mColorPanelWrapper = [[NSColorPanelWrapper alloc] initWithPicker:this];
  return NS_OK;
}

/* static */ NSColor* nsColorPicker::GetNSColorFromHexString(
    const nsAString& aColor) {
  NSString* str = nsCocoaUtils::ToNSString(aColor);

  double red = HexStrToInt([str substringWithRange:NSMakeRange(1, 2)]) / 255.0;
  double green =
      HexStrToInt([str substringWithRange:NSMakeRange(3, 2)]) / 255.0;
  double blue = HexStrToInt([str substringWithRange:NSMakeRange(5, 2)]) / 255.0;

  return [NSColor colorWithDeviceRed:red green:green blue:blue alpha:1.0];
}

/* static */ void nsColorPicker::GetHexStringFromNSColor(NSColor* aColor,
                                                         nsAString& aResult) {
  CGFloat redFloat, greenFloat, blueFloat;

  NSColor* color = aColor;
  @try {
    [color getRed:&redFloat green:&greenFloat blue:&blueFloat alpha:nil];
  } @catch (NSException* e) {
    color = [color colorUsingColorSpace:[NSColorSpace genericRGBColorSpace]];
    [color getRed:&redFloat green:&greenFloat blue:&blueFloat alpha:nil];
  }

  nsCocoaUtils::GetStringForNSString(
      [NSString stringWithFormat:@"#%02x%02x%02x", (int)(redFloat * 255),
                                 (int)(greenFloat * 255),
                                 (int)(blueFloat * 255)],
      aResult);
}

NS_IMETHODIMP
nsColorPicker::Open(nsIColorPickerShownCallback* aCallback) {
  MOZ_ASSERT(aCallback);
  mCallback = aCallback;

  [mColorPanelWrapper open:GetNSColorFromHexString(mColor)
                     title:nsCocoaUtils::ToNSString(mTitle)];

  NS_ADDREF_THIS();

  return NS_OK;
}

void nsColorPicker::Update(NSColor* aColor) {
  GetHexStringFromNSColor(aColor, mColor);
  mCallback->Update(mColor);
}

void nsColorPicker::Done() {
  [mColorPanelWrapper close];
  [mColorPanelWrapper release];
  mColorPanelWrapper = nullptr;
  mCallback->Done(u""_ns);
  mCallback = nullptr;
  NS_RELEASE_THIS();
}
