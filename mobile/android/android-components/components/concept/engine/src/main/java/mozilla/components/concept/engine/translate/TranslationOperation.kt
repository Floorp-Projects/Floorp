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
    FETCH_SUPPORTED_LANGUAGES,

    /**
     * The list of available language machine learning translation models the translation engine should fetch.
     */
    FETCH_LANGUAGE_MODELS,

    /**
     * The page related settings the translation engine should fetch.
     */
    FETCH_PAGE_SETTINGS,

    /**
     * Fetch the translations offer setting.
     * Note: this request is also encompassed in [FETCH_PAGE_SETTINGS], but intended for checking
     * fetching for global settings or when only this setting is needed.
     */
    FETCH_OFFER_SETTING,

    /**
     * Fetch the user preference on whether to offer, always translate, or never translate for
     * all supported language settings.
     */
    FETCH_AUTOMATIC_LANGUAGE_SETTINGS,

    /**
     * The list of never translate sites the translation engine should fetch.
     */
    FETCH_NEVER_TRANSLATE_SITES,
}
