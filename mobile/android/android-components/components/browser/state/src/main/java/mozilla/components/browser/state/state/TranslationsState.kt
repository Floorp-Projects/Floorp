/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.translate.TranslationDownloadSize
import mozilla.components.concept.engine.translate.TranslationEngineState
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationPageSettings

/**
 * Value type that represents the state of a translation within a [SessionState].
 *
 * @property isExpectedTranslate Expect the user to be interested in translating the page.
 * @property isOfferTranslate Offer translating the page to the user.
 * @property translationEngineState The state and expectations of the translation engine for the
 * page.
 * @property isTranslated The page is currently translated.
 * @property isTranslateProcessing The page is currently attempting a translation.
 * @property isRestoreProcessing The page is currently attempting a restoration.
 * @property translationDownloadSize The download size for the given to/from translation pair. The
 * translation engine requires the pair's ML models to be present on the device to complete a
 * translation.
 * @property pageSettings The translation engine settings that relate to the current page.
 * @property translationError Type of error that occurred when acquiring resources, translating, or
 * restoring a translation.
 * @property settingsError Type of error that occurred when acquiring resources or setting preferences.
 */
data class TranslationsState(
    val isExpectedTranslate: Boolean = false,
    val isOfferTranslate: Boolean = false,
    val translationEngineState: TranslationEngineState? = null,
    val isTranslated: Boolean = false,
    val isTranslateProcessing: Boolean = false,
    val isRestoreProcessing: Boolean = false,
    val translationDownloadSize: TranslationDownloadSize? = null,
    val pageSettings: TranslationPageSettings? = null,
    val translationError: TranslationError? = null,
    val settingsError: TranslationError? = null,
)
