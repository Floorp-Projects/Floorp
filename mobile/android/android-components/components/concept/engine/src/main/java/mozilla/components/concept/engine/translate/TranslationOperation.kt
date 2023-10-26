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
}
