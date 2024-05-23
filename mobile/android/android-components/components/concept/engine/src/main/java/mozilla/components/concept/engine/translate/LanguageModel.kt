/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

import org.jetbrains.annotations.VisibleForTesting

/**
 * The language model container for representing language model state to the user.
 *
 * Please note, a single LanguageModel is usually comprised of
 * an aggregation of multiple machine learning models on the translations engine level. The engine
 * has already handled this abstraction.
 *
 * @property language The specified language the language model set can process.
 * @property status The download status of the language models,
 * which can be not downloaded, download processing, or downloaded.
 * @property size The size of the total model download(s).
 */
data class LanguageModel(
    val language: Language? = null,
    val status: ModelState = ModelState.NOT_DOWNLOADED,
    val size: Long? = null,
) {
    companion object {
        /**
         * Convenience method to make the updated language model state based on an [ModelManagementOptions]
         * operation.
         *
         * @param currentLanguageModels The current list and state of language models.
         * @param options The operation that is requested to change the language model(s).
         * @param newStatus What the new state should be based on the change.
         * @return The new state of the language models based on the information.
         */
        fun determineNewLanguageModelState(
            currentLanguageModels: List<LanguageModel>?,
            options: ModelManagementOptions,
            newStatus: ModelState,
        ): List<LanguageModel>? =
            when (options.operationLevel) {
                OperationLevel.LANGUAGE -> {
                    currentLanguageModels?.map { model ->
                        if ((model.language?.code == options.languageToManage) &&
                            checkIfOperable(currentStatus = model.status, newStatus = newStatus)
                        ) {
                            model.copy(status = newStatus)
                        } else {
                            model.copy()
                        }
                    }
                }

                OperationLevel.CACHE -> {
                    // Cache isn't tracked on the models here, only specific full language models
                    // are tracked, so no state change. This operation is clearing individual model
                    // files not a part of a complete language model package.
                    currentLanguageModels
                }

                OperationLevel.ALL -> {
                    currentLanguageModels?.map { model ->
                        if (checkIfOperable(currentStatus = model.status, newStatus = newStatus)) {
                            model.copy(status = newStatus)
                        } else {
                            model.copy()
                        }
                    }
                }
            }

        /**
         * Helper method to determine if changing from one proposed status to another is possible or
         * if it will result in no operation on the engine side.
         *
         * @param currentStatus The current status of the language model.
         * @param newStatus The proposed status the state should move to.
         * @return Will return true if the status change will result in a change. Will return false if the
         * engine is expected to have a no op operation.
         */
        @VisibleForTesting
        fun checkIfOperable(currentStatus: ModelState, newStatus: ModelState) = when (currentStatus) {
            ModelState.NOT_DOWNLOADED -> newStatus != ModelState.DELETION_IN_PROGRESS
            ModelState.DOWNLOAD_IN_PROGRESS -> true
            ModelState.DELETION_IN_PROGRESS -> true
            ModelState.DOWNLOADED -> newStatus != ModelState.DOWNLOAD_IN_PROGRESS
        }
    }
}
