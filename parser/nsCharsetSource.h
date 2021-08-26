/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCharsetSource_h_
#define nsCharsetSource_h_

// note: the value order defines the priority; higher numbers take priority
enum {
  kCharsetUninitialized,
  kCharsetFromFallback,
  kCharsetFromDocTypeDefault,  // This and up confident for XHR
  // Start subdividing source for telementry purposes
  kCharsetFromInitialAutoDetectionASCII,
  kCharsetFromInitialAutoDetectionWouldHaveBeenUTF8,
  kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8Generic,
  kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8Content,
  kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8DependedOnTLD,
  // Deliberately no Final version of ASCII
  kCharsetFromFinalAutoDetectionWouldHaveBeenUTF8,
  kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8Generic,
  kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8Content,
  kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8DependedOnTLD,
  kCharsetFromFinalAutoDetectionFile,
  // End subdividing source for telementry purposes
  kCharsetFromParentFrame,  // Same-origin parent takes precedence over detector
                            // to avoid breaking tests. (Also, the HTML spec
                            // says so.)
  kCharsetFromXmlDeclaration,
  kCharsetFromMetaPrescan,  // this one and smaller: HTML5 Tentative
  kCharsetFromMetaTag,      // this one and greater: HTML5 Confident
  kCharsetFromChannel,
  kCharsetFromOtherComponent,
  kCharsetFromPendingUserForcedAutoDetection,  // Marker value to be upgraded
                                               // later
  kCharsetFromInitialUserForcedAutoDetection,
  kCharsetFromFinalUserForcedAutoDetection,
  kCharsetFromXmlDeclarationUtf16,        // This one is overridden by
                                          // kCharsetFromChannel
  kCharsetFromIrreversibleAutoDetection,  // This one is overridden by
                                          // kCharsetFromChannel
  kCharsetFromByteOrderMark,
  kCharsetFromUtf8OnlyMime,  // For JSON, WebVTT and such
  kCharsetFromBuiltIn,       // resource: URLs
};

#endif /* nsCharsetSource_h_ */
