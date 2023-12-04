/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintDialog_h_
#define nsPrintDialog_h_

#include "nsIPrintDialogService.h"
#include "nsCOMPtr.h"
#include "nsCocoaUtils.h"

#import <Cocoa/Cocoa.h>

class nsIPrintSettings;
class nsIStringBundle;

class nsPrintDialogServiceX final : public nsIPrintDialogService {
  virtual ~nsPrintDialogServiceX();

 public:
  nsPrintDialogServiceX();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTDIALOGSERVICE
};

@interface PrintPanelAccessoryView : NSView {
  nsIPrintSettings* mSettings;
  nsIStringBundle* mPrintBundle;
  NSButton* mPrintSelectionOnlyCheckbox;
  NSButton* mShrinkToFitCheckbox;
  NSButton* mPrintBGColorsCheckbox;
  NSButton* mPrintBGImagesCheckbox;
  NSPopUpButton* mHeaderLeftList;
  NSPopUpButton* mHeaderCenterList;
  NSPopUpButton* mHeaderRightList;
  NSPopUpButton* mFooterLeftList;
  NSPopUpButton* mFooterCenterList;
  NSPopUpButton* mFooterRightList;
}

- (id)initWithSettings:(nsIPrintSettings*)aSettings
         haveSelection:(bool)aHaveSelection;

- (void)exportSettings;

@end

@interface PrintPanelAccessoryController
    : NSViewController <NSPrintPanelAccessorizing>

- (id)initWithSettings:(nsIPrintSettings*)aSettings
         haveSelection:(bool)aHaveSelection;

- (void)exportSettings;

@end

NS_DEFINE_STATIC_IID_ACCESSOR(nsPrintDialogServiceX, NS_IPRINTDIALOGSERVICE_IID)

#endif  // nsPrintDialog_h_
