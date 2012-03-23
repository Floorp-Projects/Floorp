/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCharsetSource_h_
#define nsCharsetSource_h_

// note: the value order defines the priority; higher numbers take priority
#define kCharsetUninitialized           0
#define kCharsetFromWeakDocTypeDefault  1
#define kCharsetFromUserDefault         2
#define kCharsetFromDocTypeDefault      3 // This and up confident for XHR
#define kCharsetFromCache               4
#define kCharsetFromParentFrame         5
#define kCharsetFromAutoDetection       6
#define kCharsetFromHintPrevDoc         7
#define kCharsetFromMetaPrescan         8 // this one and smaller: HTML5 Tentative
#define kCharsetFromMetaTag             9 // this one and greater: HTML5 Confident
#define kCharsetFromIrreversibleAutoDetection 10
#define kCharsetFromByteOrderMark      11
#define kCharsetFromChannel            12
#define kCharsetFromOtherComponent     13
// Levels below here will be forced onto childframes too
#define kCharsetFromParentForced       14
#define kCharsetFromUserForced         15
#define kCharsetFromPreviousLoading    16

#endif /* nsCharsetSource_h_ */
