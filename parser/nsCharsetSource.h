/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCharsetSource_h_
#define nsCharsetSource_h_

// note: the value order defines the priority; higher numbers take priority
enum nsCharsetSource {
  kCharsetUninitialized,
  kCharsetFromFallback,
  kCharsetFromDocTypeDefault,  // This and up confident for XHR
  // Start subdividing source for telemetry purposes
  kCharsetFromInitialAutoDetectionASCII,
  kCharsetFromInitialAutoDetectionWouldHaveBeenUTF8,
  kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8Generic,
  kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8Content,
  kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8DependedOnTLD,
  // End subdividing source for telemetry purposes
  kCharsetFromParentFrame,  // Same-origin parent takes precedence over detector
                            // to avoid breaking tests. (Also, the HTML spec
                            // says so.)
  kCharsetFromXmlDeclaration,
  kCharsetFromMetaTag,
  kCharsetFromChannel,
  kCharsetFromOtherComponent,
  kCharsetFromInitialUserForcedAutoDetection,
  // Start subdividing source for telemetry purposes
  // Deliberately no Final version of ASCII
  kCharsetFromFinalAutoDetectionWouldHaveBeenUTF8InitialWasASCII,
  kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8Generic,
  kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8GenericInitialWasASCII,
  kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8Content,
  kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8ContentInitialWasASCII,
  kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8DependedOnTLD,
  kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8DependedOnTLDInitialWasASCII,
  kCharsetFromFinalAutoDetectionFile,
  // End subdividing source for telemetry purposes
  kCharsetFromFinalUserForcedAutoDetection,
  kCharsetFromXmlDeclarationUtf16,  // This one is overridden by
                                    // kCharsetFromChannel
  kCharsetFromByteOrderMark,
  kCharsetFromUtf8OnlyMime,  // For JSON, WebVTT and such
  kCharsetFromBuiltIn,       // resource: URLs
};

#endif /* nsCharsetSource_h_ */
