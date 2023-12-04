/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSettingsX_h_
#define nsPrintSettingsX_h_

#include "nsPrintSettingsImpl.h"
#import <Cocoa/Cocoa.h>

// clang-format off
#define NS_PRINTSETTINGSX_IID                        \
  {                                                  \
    0x0DF2FDBD, 0x906D, 0x4726, {                    \
      0x9E, 0x4D, 0xCF, 0xE0, 0x87, 0x8D, 0x70, 0x7C \
    }                                                \
  }
// clang-format on

class nsPrintSettingsX : public nsPrintSettings {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PRINTSETTINGSX_IID)
  NS_DECL_ISUPPORTS_INHERITED

  nsPrintSettingsX();
  explicit nsPrintSettingsX(const PrintSettingsInitializer& aSettings);

  nsresult Init() { return NS_OK; }

  void SetDestination(uint16_t aDestination) { mDestination = aDestination; }
  void GetDestination(uint16_t* aDestination) { *aDestination = mDestination; }

  void SetDisposition(const nsString& aDisposition) {
    mDisposition = aDisposition;
  }
  void GetDisposition(nsString& aDisposition) { aDisposition = mDisposition; }

  // Get a Cocoa NSPrintInfo that is configured with our current settings.
  // This follows Create semantics, so the caller is responsible to release
  // the returned object when no longer required.
  //
  // Pass true for aWithScaling to have the print scaling factor included in
  // the returned printInfo. Normally we pass false, as scaling is handled
  // by Gecko and we don't want the Cocoa print system to impose scaling again
  // on the output, but if we're retrieving the info in order to populate the
  // system print UI, then we do want to know about it.
  NSPrintInfo* CreateOrCopyPrintInfo(bool aWithScaling = false);

  // Update our internal settings to reflect the properties of the given
  // NSPrintInfo.
  //
  // If aAdoptPrintInfo is set, the given NSPrintInfo will be retained and
  // returned by subsequent CreateOrCopyPrintInfo calls, which is required
  // for custom settings from the OS print dialog to be passed through to
  // print jobs. However, this means that subsequent changes to print settings
  // via the generic nsPrintSettings methods will NOT be reflected in the
  // resulting NSPrintInfo.
  void SetFromPrintInfo(NSPrintInfo* aPrintInfo, bool aAdoptPrintInfo);

 protected:
  virtual ~nsPrintSettingsX() {
    if (mSystemPrintInfo) {
      [mSystemPrintInfo release];
    }
  };

  nsPrintSettingsX& operator=(const nsPrintSettingsX& rhs);

  nsresult _Clone(nsIPrintSettings** _retval) override;
  nsresult _Assign(nsIPrintSettings* aPS) override;

  int GetCocoaUnit(int16_t aGeckoUnit);

  double PaperSizeFromCocoaPoints(double aPointsValue) {
    return aPointsValue *
           (mPaperSizeUnit == kPaperSizeInches ? 1.0 / 72.0 : 25.4 / 72.0);
  }

  double CocoaPointsFromPaperSize(double aSizeUnitValue) {
    return aSizeUnitValue *
           (mPaperSizeUnit == kPaperSizeInches ? 72.0 : 72.0 / 25.4);
  }

  // Needed to correctly track the various job dispositions (spool, preview,
  // save to file) that the user can choose via the system print dialog.
  // Unfortunately it seems to be necessary to set both the Cocoa "job
  // disposition" and the PrintManager "destination type" in order for all the
  // various workflows such as "Save to Web Receipts" to work.
  nsString mDisposition;
  uint16_t mDestination;

  // If the user has used the system print UI, we retain a reference to its
  // printInfo because it may contain settings that we don't know how to handle
  // and that will be lost if we round-trip through nsPrintSettings fields.
  // We'll use this printInfo if asked to run a print job.
  //
  // This "wrapped" printInfo is NOT serialized or copied when printSettings
  // objects are passed around; it is used only by the settings object to which
  // it was originally passed.
  NSPrintInfo* mSystemPrintInfo = nullptr;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPrintSettingsX, NS_PRINTSETTINGSX_IID)

#endif  // nsPrintSettingsX_h_
