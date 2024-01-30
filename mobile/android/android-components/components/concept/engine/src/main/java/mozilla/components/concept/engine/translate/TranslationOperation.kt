/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * The operation the translations engine is performing.
 */
enum class TranslationOperation {
    /**
     * The page should be translated.
     */
    TRANSLATE,

    /**
     * A translated page should be restored.
     */
    RESTORE,

    /**
     * The list of languages that the translation engine should fetch. This includes
     * the languages supported for translating both "to" and "from" with their BCP-47 language tag
     * and localized name.
     */
    FETCH_LANGUAGES,

    /**
     * The page related settings the translation engine should fetch.
     */
    FETCH_PAGE_SETTINGS,
}
