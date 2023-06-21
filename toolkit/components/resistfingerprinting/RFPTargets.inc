/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Names should not be re-used.

ITEM_VALUE(TouchEvents,                 1llu << 0)
ITEM_VALUE(PointerEvents,               1llu << 1)
ITEM_VALUE(KeyboardEvents,              1llu << 2)
ITEM_VALUE(ScreenOrientation,           1llu << 3)
// SpeechSynthesis part of the Web Speech API
ITEM_VALUE(SpeechSynthesis,             1llu << 4)
// `prefers-color-scheme` CSS media feature
ITEM_VALUE(CSSPrefersColorScheme,       1llu << 5)
// `prefers-reduced-motion` CSS media feature
ITEM_VALUE(CSSPrefersReducedMotion,     1llu << 6)
// `prefers-contrast` CSS media feature
ITEM_VALUE(CSSPrefersContrast,          1llu << 7)
// Add random noises to image data extracted from canvas.
ITEM_VALUE(CanvasRandomization,         1llu << 8)
// Canvas targets: For unusual combinations of these, see comments
// in IsImageExtractionAllowed
ITEM_VALUE(CanvasImageExtractionPrompt, 1llu << 9)
ITEM_VALUE(CanvasExtractionFromThirdPartiesIsBlocked, 1llu << 10)
ITEM_VALUE(CanvasExtractionBeforeUserInputIsBlocked, 1llu << 11)
// Various "client identification" values of the navigator object
ITEM_VALUE(NavigatorAppName,            1llu << 12)
ITEM_VALUE(NavigatorAppVersion,         1llu << 13)
ITEM_VALUE(NavigatorBuildID,            1llu << 14)
ITEM_VALUE(NavigatorHWConcurrency,      1llu << 15)
ITEM_VALUE(NavigatorOscpu,              1llu << 16)
ITEM_VALUE(NavigatorPlatform,           1llu << 17)
ITEM_VALUE(NavigatorUserAgent,          1llu << 18)
// Audio/VideoStreamTrack labels
ITEM_VALUE(StreamTrackLabel,            1llu << 19)
ITEM_VALUE(StreamVideoFacingMode,       1llu << 20)
ITEM_VALUE(JSDateTimeUTC,               1llu << 21)
ITEM_VALUE(JSMathFdlibm,                1llu << 22)
ITEM_VALUE(Gamepad,                     1llu << 23)
ITEM_VALUE(HttpUserAgent,               1llu << 24)
ITEM_VALUE(WindowOuterSize,             1llu << 25)
ITEM_VALUE(WindowScreenXY,              1llu << 26)
ITEM_VALUE(WindowInnerScreenXY,         1llu << 27)
ITEM_VALUE(ScreenPixelDepth,            1llu << 28)
ITEM_VALUE(ScreenRect,                  1llu << 29)
ITEM_VALUE(ScreenAvailRect,             1llu << 30)
// HTMLVideoElement
// mozParsedFrames, mozDecodedFrames, mozPresentedFrames, mozPaintedFrames
ITEM_VALUE(VideoElementMozFrames,       1llu << 31)
// mozFrameDelay
ITEM_VALUE(VideoElementMozFrameDelay,   1llu << 32)
// getVideoPlaybackQuality()
ITEM_VALUE(VideoElementPlaybackQuality, 1llu << 33)
// See also Reduce Timer Precision (RTP) Caller Type
ITEM_VALUE(ReduceTimerPrecision,        1llu << 34)
// Hide keyboard and pointer WidgetEvents
ITEM_VALUE(WidgetEvents,                1llu << 35)
ITEM_VALUE(MediaDevices,                1llu << 36)
ITEM_VALUE(MediaCapabilities,           1llu << 37)
ITEM_VALUE(AudioSampleRate,             1llu << 38)
ITEM_VALUE(NavigatorConnection,         1llu << 39)
ITEM_VALUE(WindowDevicePixelRatio,      1llu << 40)
ITEM_VALUE(MouseEventScreenPoint,       1llu << 41)

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
ITEM_VALUE(Unknown, 0xffffffff'ffffffff)
