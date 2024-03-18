/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * The container for referring to the different page settings.
 *
 * See [TranslationPageSettings] for the corresponding data model
 */
enum class TranslationPageSettingOperation {
    /**
     * The system should offer a translation on a page.
     */
    UPDATE_ALWAYS_OFFER_POPUP,

    /**
     * The page's always translate language setting.
     */
    UPDATE_ALWAYS_TRANSLATE_LANGUAGE,

    /**
     * The page's never translate language setting.
     */
    UPDATE_NEVER_TRANSLATE_LANGUAGE,

    /**
     *  The page's never translate site setting.
     */
    UPDATE_NEVER_TRANSLATE_SITE,
}
