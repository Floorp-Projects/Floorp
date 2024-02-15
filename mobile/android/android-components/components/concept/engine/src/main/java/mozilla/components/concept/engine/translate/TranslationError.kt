/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * The types of translation errors that can occur. Has features for determining telemetry error
 * names and determining if an error needs to be displayed.
 *
 * @param errorName The translation error name. The expected use is for telemetry.
 * @param displayError Signal to determine if we need to specifically display an error for
 * this given issue. (Some errors should only silently report telemetry or simply revert to the
 * prior UI state.)
 * @param cause The original throwable before it was converted into this error state.
 */
sealed class TranslationError(
    val errorName: String,
    val displayError: Boolean,
    override val cause: Throwable?,
) : Throwable(cause = cause) {

    /**
     * Default error for unexpected issues.
     *
     * @param cause The original throwable that lead us to the unknown error state.
     */
    class UnknownError(override val cause: Throwable) :
        TranslationError(errorName = "unknown", displayError = false, cause = cause)

    /**
     * Default error for unexpected null value received on a non-null translations call.
     */
    class UnexpectedNull :
        TranslationError(errorName = "unexpected-null", displayError = false, cause = null)

    /**
     * Default error when a translation session coordinator is not available.
     */
    class MissingSessionCoordinator :
        TranslationError(errorName = "missing-session-coordinator", displayError = false, cause = null)

    /**
     * Translations engine does not work on the device architecture.
     *
     * @param cause The original throwable before it was converted into this error state.
     */
    class EngineNotSupportedError(override val cause: Throwable?) :
        TranslationError(errorName = "engine-not-supported", displayError = false, cause = cause)

    /**
     * Could not determine if the translations engine works on the device architecture.
     *
     * @param cause The original [Throwable] before it was converted into this error state.
     */
    class UnknownEngineSupportError(override val cause: Throwable?) :
        TranslationError(errorName = "unknown-engine-support", displayError = false, cause = cause)

    /**
     * Generic could not compete a translation error.
     *
     * @param cause The original throwable before it was converted into this error state.
     */
    class CouldNotTranslateError(override val cause: Throwable?) :
        TranslationError(errorName = "could-not-translate", displayError = true, cause = cause)

    /**
     * Generic could not restore the page after a translation error.
     *
     * @param cause The original throwable before it was converted into this error state.
     */
    class CouldNotRestoreError(override val cause: Throwable?) :
        TranslationError(errorName = "could-not-restore", displayError = false, cause = cause)

    /**
     * Could not determine the translation download size between a given "to" and "from" language
     * translation pair.
     *
     * @param cause The original [Throwable] before it was converted into this error state.
     */
    class CouldNotDetermineDownloadSizeError(override val cause: Throwable?) :
        TranslationError(
            errorName = "could-not-determine-translation-download-size",
            displayError = false,
            cause = cause,
        )

    /**
     * Could not load language options error.
     *
     * @param cause The original throwable before it was converted into this error state.
     */
    class CouldNotLoadLanguagesError(override val cause: Throwable?) :
        TranslationError(errorName = "could-not-load-languages", displayError = true, cause = cause)

    /**
     * Could not load page settings error.
     *
     * @param cause The original throwable before it was converted into this error state.
     */
    class CouldNotLoadPageSettingsError(override val cause: Throwable?) :
        TranslationError(errorName = "could-not-load-settings", displayError = false, cause = cause)

    /**
     * Could not load never translate sites error.
     *
     * @param cause The original throwable before it was converted into this error state.
     */
    class CouldNotLoadNeverTranslateSites(override val cause: Throwable?) :
        TranslationError(errorName = "could-not-load-never-translate-sites", displayError = false, cause = cause)

    /**
     * The language is not supported for translation.
     *
     * @param cause The original throwable before it was converted into this error state.
     */
    class LanguageNotSupportedError(override val cause: Throwable?) :
        TranslationError(errorName = "language-not-supported", displayError = true, cause = cause)

    /**
     * Could not retrieve information on the language model.
     *
     * @param cause The original throwable before it was converted into this error state.
     */
    class ModelCouldNotRetrieveError(override val cause: Throwable?) :
        TranslationError(
            errorName = "model-could-not-retrieve",
            displayError = false,
            cause = cause,
        )

    /**
     * Could not delete the language model.
     *
     * @param cause The original throwable before it was converted into this error state.
     */
    class ModelCouldNotDeleteError(override val cause: Throwable?) :
        TranslationError(errorName = "model-could-not-delete", displayError = false, cause = cause)

    /**
     * Could not download the language model.
     *
     * @param cause The original throwable before it was converted into this error state.
     */
    class ModelCouldNotDownloadError(override val cause: Throwable?) :
        TranslationError(
            errorName = "model-could-not-download",
            displayError = false,
            cause = cause,
        )

    /**
     * A language is required for language scoped requests.
     *
     * @param cause The original throwable before it was converted into this error state.
     */
    class ModelLanguageRequiredError(override val cause: Throwable?) :
        TranslationError(errorName = "model-language-required", displayError = false, cause = cause)

    /**
     * A download is required and the translate request specified do not download.
     *
     * @param cause The original throwable before it was converted into this error state.
     */
    class ModelDownloadRequiredError(override val cause: Throwable?) :
        TranslationError(errorName = "model-download-required", displayError = false, cause = cause)
}
