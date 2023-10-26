/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

/**
 * Value type that represents the state of a translation within a [SessionState].
 *
 * @property isTranslated The page is currently translated.
 * @property isTranslateProcessing The page is currently attempting a translation.
 * @property isRestoreProcessing The page is currently attempting a restoration.
 */
data class TranslationsState(
    val isTranslated: Boolean = false,
    val isTranslateProcessing: Boolean = false,
    val isRestoreProcessing: Boolean = false,
)
