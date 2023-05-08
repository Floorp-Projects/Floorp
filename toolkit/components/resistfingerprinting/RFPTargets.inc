/*
 * Once allocated, a string or value cannot ever be changed or re-used
 */

ITEM_VALUE(TouchEvents, 0x0001)
ITEM_VALUE(PointerEvents, 0x0002)
ITEM_VALUE(KeyboardEvents, 0x0003)

ITEM_VALUE(ScreenOrientation, 0x0005)

// SpeechSynthesis part of the Web Speech API
ITEM_VALUE(SpeechSynthesis, 0x0006)

// `prefers-color-scheme` CSS media feature
ITEM_VALUE(CSSPrefersColorScheme, 0x0007)

// `prefers-reduced-motion` CSS media feature
ITEM_VALUE(CSSPrefersReducedMotion, 0x0008)

// `prefers-contrast` CSS media feature
ITEM_VALUE(CSSPrefersContrast, 0x0009)
// Add random noises to image data extracted from canvas.
ITEM_VALUE(CanvasRandomization, 0x000A)

// Guard the canvas image data behind a user prompt
ITEM_VALUE(CanvasImageExtractionPrompt, 0x000B)

/**
 * Reserving 0x000C and 0x000D for
 *   1. CanvasExtractionFromThirdPartiesIsBlocked
 *   2. CanvasExtractionBeforeUSerInputIsBlocked
 */

/*
 * Certain classes (Document, at time of writing) have a cached value for
 * ShouldResistFingerprinting. By adding a Target check into their ShouldRFP
 * function, we are potentially changing the return value of what was
 * previously the cached value. This is intentional in most places, but not
 * in any place that is asking if e.g. restyling needs to occur. Use this enum
 * value for that in conjunction with a check in the affected class.
 */
ITEM_VALUE(IgnoreTargetAndReturnCachedValue, 0xFFFD)

/*
 * In certain cases, we precompute the vaqlue of ShouldRFP for e.g. a Document.
 * (This saves us more computation and casting later.)  This document will still
 * need to check whether an individual target is allowed, but the initial
 * question of "Does this document have any RFP applied to it ever?" can still
 * be precomputed. This enum value will always be included in RFPLite, so when a
 * document asks if they might have RFP enabled, it will return true. (Putting
 * this value in the overrides pref is undefined behavior and may do anything.)
 */
ITEM_VALUE(IsAlwaysEnabledForPrecompute, 0xFFFE)

/*
 * This value is the default argument value, to allow all callsites to ShouldRFP
 * continue working. We will eventually remove the default argument, and comment
 * out this enum value.
 */
ITEM_VALUE(Unknown, 0xFFFF)
