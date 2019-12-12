/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCharsetSource_h_
#define nsCharsetSource_h_

// note: the value order defines the priority; higher numbers take priority
enum {
  kCharsetUninitialized,
  kCharsetFromFallback,
  kCharsetFromTopLevelDomain,
  kCharsetFromFileURLGuess,
  kCharsetFromDocTypeDefault,  // This and up confident for XHR
  kCharsetFromCache,
  kCharsetFromParentFrame,
  kCharsetFromAutoDetection,
  kCharsetFromMetaPrescan,  // this one and smaller: HTML5 Tentative
  kCharsetFromMetaTag,      // this one and greater: HTML5 Confident
  kCharsetFromIrreversibleAutoDetection,
  kCharsetFromChannel,
  kCharsetFromOtherComponent,
  kCharsetFromParentForced,  // propagates to child frames
  kCharsetFromUserForced,    // propagates to child frames
  kCharsetFromUserForcedAutoDetection,
  kCharsetFromByteOrderMark,
  kCharsetFromUtf8OnlyMime,  // For JSON, WebVTT and such
  kCharsetFromBuiltIn,       // resource: URLs
};

#endif /* nsCharsetSource_h_ */
