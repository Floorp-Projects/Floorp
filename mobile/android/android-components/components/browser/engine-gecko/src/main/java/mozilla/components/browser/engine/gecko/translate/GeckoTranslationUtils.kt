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
     * @param throwable The engine throwable that occurred during translating. Ordinarily should be
     * a [TranslationsException].
     */
    fun fromGeckoErrorToTranslationError(throwable: Throwable): TranslationError =
        if (throwable is TranslationsException) {
            when ((throwable).code) {
                TranslationsException.ERROR_UNKNOWN ->
                    TranslationError.UnknownError(throwable)

                TranslationsException.ERROR_ENGINE_NOT_SUPPORTED ->
                    TranslationError.EngineNotSupportedError(throwable)

                TranslationsException.ERROR_COULD_NOT_TRANSLATE ->
                    TranslationError.CouldNotTranslateError(throwable)

                TranslationsException.ERROR_COULD_NOT_RESTORE ->
                    TranslationError.CouldNotRestoreError(throwable)

                TranslationsException.ERROR_COULD_NOT_LOAD_LANGUAGES ->
                    TranslationError.CouldNotLoadLanguagesError(throwable)

                TranslationsException.ERROR_LANGUAGE_NOT_SUPPORTED ->
                    TranslationError.LanguageNotSupportedError(throwable)

                TranslationsException.ERROR_MODEL_COULD_NOT_RETRIEVE ->
                    TranslationError.ModelCouldNotRetrieveError(throwable)

                TranslationsException.ERROR_MODEL_COULD_NOT_DELETE ->
                    TranslationError.ModelCouldNotDeleteError(throwable)

                TranslationsException.ERROR_MODEL_COULD_NOT_DOWNLOAD ->
                    TranslationError.ModelCouldNotDownloadError(throwable)

                TranslationsException.ERROR_MODEL_LANGUAGE_REQUIRED ->
                    TranslationError.ModelLanguageRequiredError(throwable)

                TranslationsException.ERROR_MODEL_DOWNLOAD_REQUIRED ->
                    TranslationError.ModelDownloadRequiredError(throwable)

                else -> TranslationError.UnknownError(throwable)
            }
        } else {
            TranslationError.UnknownError(throwable)
        }
}
