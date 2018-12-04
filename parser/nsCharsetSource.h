/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCharsetSource_h_
#define nsCharsetSource_h_

// note: the value order defines the priority; higher numbers take priority
#define kCharsetUninitialized 0
#define kCharsetFromFallback 1
#define kCharsetFromTopLevelDomain 2
#define kCharsetFromFileURLGuess 3
#define kCharsetFromDocTypeDefault 4  // This and up confident for XHR
#define kCharsetFromCache 5
#define kCharsetFromParentFrame 6
#define kCharsetFromAutoDetection 7
#define kCharsetFromHintPrevDoc 8
#define kCharsetFromMetaPrescan 9  // this one and smaller: HTML5 Tentative
#define kCharsetFromMetaTag 10     // this one and greater: HTML5 Confident
#define kCharsetFromIrreversibleAutoDetection 11
#define kCharsetFromChannel 12
#define kCharsetFromOtherComponent 13
#define kCharsetFromParentForced 14  // propagates to child frames
#define kCharsetFromUserForced 15    // propagates to child frames
#define kCharsetFromByteOrderMark 16
#define kCharsetFromUtf8OnlyMime 17  // For JSON, WebVTT and such
#define kCharsetFromBuiltIn 18       // resource: URLs

#endif /* nsCharsetSource_h_ */
