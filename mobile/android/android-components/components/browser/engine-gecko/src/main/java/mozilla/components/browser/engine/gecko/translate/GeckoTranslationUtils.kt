/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.browser.engine.gecko.translate

import mozilla.components.concept.engine.translate.TranslationError
import org.mozilla.geckoview.TranslationsController.TranslationsException

/**
 * Utility file for translations functions related to the Gecko implementation.
 */
object GeckoTranslationUtils {

    /**
     * Convenience method for mapping a [TranslationsException] to the Android Components defined
     * error type of [TranslationError].
     *
     * Throwable is the engine throwable that occurred during translating. Ordinarily should be
     * a [TranslationsException].
     */
    fun Throwable.intoTranslationError(): TranslationError {
        return if (this is TranslationsException) {
            when ((this).code) {
                TranslationsException.ERROR_UNKNOWN ->
                    TranslationError.UnknownError(this)

                TranslationsException.ERROR_ENGINE_NOT_SUPPORTED ->
                    TranslationError.EngineNotSupportedError(this)

                TranslationsException.ERROR_COULD_NOT_TRANSLATE ->
                    TranslationError.CouldNotTranslateError(this)

                TranslationsException.ERROR_COULD_NOT_RESTORE ->
                    TranslationError.CouldNotRestoreError(this)

                TranslationsException.ERROR_COULD_NOT_LOAD_LANGUAGES ->
                    TranslationError.CouldNotLoadLanguagesError(this)

                TranslationsException.ERROR_LANGUAGE_NOT_SUPPORTED ->
                    TranslationError.LanguageNotSupportedError(this)

                TranslationsException.ERROR_MODEL_COULD_NOT_RETRIEVE ->
                    TranslationError.ModelCouldNotRetrieveError(this)

                TranslationsException.ERROR_MODEL_COULD_NOT_DELETE ->
                    TranslationError.ModelCouldNotDeleteError(this)

                TranslationsException.ERROR_MODEL_COULD_NOT_DOWNLOAD ->
                    TranslationError.ModelCouldNotDownloadError(this)

                TranslationsException.ERROR_MODEL_LANGUAGE_REQUIRED ->
                    TranslationError.ModelLanguageRequiredError(this)

                TranslationsException.ERROR_MODEL_DOWNLOAD_REQUIRED ->
                    TranslationError.ModelDownloadRequiredError(this)

                else -> TranslationError.UnknownError(this)
            }
        } else {
            TranslationError.UnknownError(this)
        }
    }
}
