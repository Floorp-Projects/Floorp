/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Names should not be re-used.

ITEM_VALUE(TouchEvents,                 1u << 0)
ITEM_VALUE(PointerEvents,               1u << 1)
ITEM_VALUE(KeyboardEvents,              1u << 2)
ITEM_VALUE(ScreenOrientation,           1u << 3)
// SpeechSynthesis part of the Web Speech API
ITEM_VALUE(SpeechSynthesis,             1u << 4)
// `prefers-color-scheme` CSS media feature
ITEM_VALUE(CSSPrefersColorScheme,       1u << 5)
// `prefers-reduced-motion` CSS media feature
ITEM_VALUE(CSSPrefersReducedMotion,     1u << 6)
// `prefers-contrast` CSS media feature
ITEM_VALUE(CSSPrefersContrast,          1u << 7)
// Add random noises to image data extracted from canvas.
ITEM_VALUE(CanvasRandomization,         1u << 8)
// Canvas targets: For unusual combinations of these, see comments
// in IsImageExtractionAllowed
ITEM_VALUE(CanvasImageExtractionPrompt, 1u << 9)
ITEM_VALUE(CanvasExtractionFromThirdPartiesIsBlocked, 1u << 10)
ITEM_VALUE(CanvasExtractionBeforeUserInputIsBlocked, 1u << 11)
// Various "client identification" values of the navigator object
ITEM_VALUE(NavigatorAppName,            1u << 12)
ITEM_VALUE(NavigatorAppVersion,         1u << 13)
ITEM_VALUE(NavigatorBuildID,            1u << 14)
ITEM_VALUE(NavigatorHWConcurrency,      1u << 15)
ITEM_VALUE(NavigatorOscpu,              1u << 16)
ITEM_VALUE(NavigatorPlatform,           1u << 17)
ITEM_VALUE(NavigatorUserAgent,          1u << 18)
// Audio/VideoStreamTrack labels
ITEM_VALUE(StreamTrackLabel,            1u << 19)
ITEM_VALUE(StreamVideoFacingMode,       1u << 20)

// !!! Don't forget to update kDefaultFingerintingProtections in nsRFPService.cpp
// if necessary.

/*
 * In certain cases, we precompute the value of ShouldRFP for e.g. a Document.
 * (This saves us more computation and casting later.)  This document will still
 * need to check whether an individual target is allowed, but the initial
 * question of "Does this document have any RFP applied to it ever?" can still
 * be precomputed. This enum value will always be included in RFPLite, so when a
 * document asks if they might have RFP enabled, it will return true. (Putting
 * this value in the overrides pref is undefined behavior and may do anything.)
 */
ITEM_VALUE(IsAlwaysEnabledForPrecompute, 0)

/*
 * This value is the default argument value, to allow all callsites to ShouldRFP
 * continue working. We will eventually remove the default argument, and then also
 * remove this enum value.
 */
ITEM_VALUE(Unknown, 0xffffffff)
