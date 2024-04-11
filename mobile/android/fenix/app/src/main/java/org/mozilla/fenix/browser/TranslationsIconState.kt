/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import mozilla.components.concept.engine.translate.Language

/**
 * Translations action button state.
 *
 * @property isVisible Whether or not the translation action button is visible.
 * @property isTranslated If the web site is translated,
 * @property fromSelectedLanguage Initial "from" language based on the translation state and page state.
 * @property toSelectedLanguage Initial "to" language based on the translation state and page state.
 */
data class TranslationsIconState(
    val isVisible: Boolean,
    val isTranslated: Boolean,
    val fromSelectedLanguage: Language? = null,
    val toSelectedLanguage: Language? = null,
)
