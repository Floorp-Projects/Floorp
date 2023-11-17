/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

private var unsupportedError = "Translations support is not available in this engine."

/**
 * Entry point for interacting with runtime translation options.
 */
interface TranslationsRuntime {

    /**
     * Checks if the translations engine is supported or not. The engine only
     * supports certain architectures.
     *
     * An example use case is checking if translations options should ever be displayed.
     *
     * @param onSuccess Callback invoked when successful with the compatibility status of running
     * translations.
     * @param onError Callback invoked if an issue occurred when determining status.
     */
    fun isTranslationsEngineSupported(
        onSuccess: (Boolean) -> Unit,
        onError: (Throwable) -> Unit,
    ): Unit = onError(UnsupportedOperationException(unsupportedError))

    /**
     * Queries what language models are downloaded and will return the download size
     * for the given language pair or else return an error.
     *
     * An example use case is checking how large of a download will occur for a given
     * specifc translation.
     *
     * @param fromLanguage The language the translations engine will use to translate from.
     * @param toLanguage The language the translations engine will use to translate to.
     * @param onSuccess Callback invoked if the pair download size was fetched successfully. With
     * the size in bytes that will be required to complete for the download. Zero bytes indicates
     * no download is required.
     * @param onError Callback invoked if an issue occurred when checking sizes.
     */
    fun getTranslationsPairDownloadSize(
        fromLanguage: String,
        toLanguage: String,
        onSuccess: (Long) -> Unit,
        onError: (Throwable) -> Unit,
    ): Unit = onError(UnsupportedOperationException(unsupportedError))

    /**
     * Aggregates the states of complete models downloaded. Note, this function does not aggregate
     * the cache or state of incomplete models downloaded.
     *
     * An example use case is listing the current install states of the language models.
     *
     * @param onSuccess Callback invoked if the states were correctly aggregated as a list.
     * @param onError Callback invoked if an issue occurred when aggregating model state.
     */
    fun getTranslationsModelDownloadStates(
        onSuccess: (List<LanguageModel>) -> Unit,
        onError: (Throwable) -> Unit,
    ): Unit = onError(UnsupportedOperationException(unsupportedError))

    /**
     * Fetches a list of to and from languages supported by the translations engine.
     *
     * An example use case is is for populating translation options.
     *
     * @param onSuccess Callback invoked if the list of to and from languages was retrieved.
     * @param onError Callback invoked if an issue occurred.
     */
    fun getSupportedTranslationLanguages(
        onSuccess: (TranslationSupport) -> Unit,
        onError: (Throwable) -> Unit,
    ): Unit = onError(UnsupportedOperationException(unsupportedError))

    /**
     * Use to download and delete complete model sets for a given language. Can bulk update all
     * models, a given language set, or the cache or incomplete models (models that are not a part
     * of a complete language set).
     *
     * An example use case is for managing deleting and installing model sets.
     *
     * @param options The options for the operation.
     * @param onSuccess Callback invoked if the operation completed successfully.
     * @param onError Callback invoked if an issue occurred.
     */
    fun manageTranslationsLanguageModel(
        options: ModelManagementOptions,
        onSuccess: () -> Unit,
        onError: (Throwable) -> Unit,
    ): Unit = onError(UnsupportedOperationException(unsupportedError))

    /**
     * Retrieves the user preferred languages using the app language(s), web requested language(s),
     * and OS language(s).
     *
     * An example use case is presenting translate "to language" options for the user. Note, the
     * user's predicted first choice is also available via the state of the translation.
     *
     * @param onSuccess Callback invoked if the operation completed successfully with a list of user
     * preferred languages.
     * @param onError Callback invoked if an issue occurred.
     */
    fun getUserPreferredLanguages(
        onSuccess: (List<String>) -> Unit,
        onError: (Throwable) -> Unit,
    ): Unit = onError(UnsupportedOperationException(unsupportedError))
}
