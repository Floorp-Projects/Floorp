/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.LanguageModel
import mozilla.components.concept.engine.translate.ModelManagementOptions
import mozilla.components.concept.engine.translate.ModelOperation
import mozilla.components.concept.engine.translate.ModelState
import mozilla.components.concept.engine.translate.OperationLevel
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LanguageModelTest {

    private val mockLanguageModels = listOf(
        LanguageModel(language = Language(code = "es"), status = ModelState.NOT_DOWNLOADED, size = 111),
        LanguageModel(language = Language(code = "de"), status = ModelState.NOT_DOWNLOADED, size = 122),
        LanguageModel(language = Language(code = "fr"), status = ModelState.DOWNLOADED, size = 133),
        LanguageModel(language = Language(code = "en"), status = ModelState.DOWNLOADED, size = 144),
    )

    @Test
    fun `GIVEN a language level model operation THEN update the state of that language only`() {
        val options = ModelManagementOptions(
            languageToManage = "es",
            operation = ModelOperation.DOWNLOAD,
            operationLevel = OperationLevel.LANGUAGE,
        )

        // Simulated process state before syncing with the engine
        val processState = if (options.operation == ModelOperation.DOWNLOAD) ModelState.DOWNLOAD_IN_PROGRESS else ModelState.DELETION_IN_PROGRESS

        val newModelState = LanguageModel.determineNewLanguageModelState(
            currentLanguageModels = mockLanguageModels,
            options = options,
            newStatus = processState,
        )

        val expectedModelState = listOf(
            LanguageModel(language = Language(code = "es"), status = ModelState.DOWNLOAD_IN_PROGRESS, size = 111),
            mockLanguageModels[1],
            mockLanguageModels[2],
            mockLanguageModels[3],
        )

        assertEquals(expectedModelState, newModelState)
    }

    @Test
    fun `GIVEN a language level model operation THEN do not update that language if it is a noop`() {
        val options = ModelManagementOptions(
            languageToManage = "en",
            operation = ModelOperation.DOWNLOAD,
            operationLevel = OperationLevel.LANGUAGE,
        )

        // Simulated process state before syncing with the engine
        val processState = if (options.operation == ModelOperation.DOWNLOAD) ModelState.DOWNLOAD_IN_PROGRESS else ModelState.DELETION_IN_PROGRESS

        val newModelState = LanguageModel.determineNewLanguageModelState(
            currentLanguageModels = mockLanguageModels,
            options = options,
            newStatus = processState,
        )

        // Expect no state change, since downloading a downloaded model does not make sense.
        assertEquals(mockLanguageModels, newModelState)
    }

    @Test
    fun `GIVEN an all model operation THEN update models that aren't already in that state`() {
        val options = ModelManagementOptions(
            languageToManage = null,
            operation = ModelOperation.DOWNLOAD,
            operationLevel = OperationLevel.ALL,
        )

        // Simulated process state before syncing with the engine
        val processState = if (options.operation == ModelOperation.DOWNLOAD) ModelState.DOWNLOAD_IN_PROGRESS else ModelState.DELETION_IN_PROGRESS

        val newModelState = LanguageModel.determineNewLanguageModelState(
            currentLanguageModels = mockLanguageModels,
            options = options,
            newStatus = processState,
        )

        val expectedModelState = listOf(
            LanguageModel(language = Language(code = "es"), status = ModelState.DOWNLOAD_IN_PROGRESS, size = 111),
            LanguageModel(language = Language(code = "de"), status = ModelState.DOWNLOAD_IN_PROGRESS, size = 122),
            LanguageModel(language = Language(code = "fr"), status = ModelState.DOWNLOADED, size = 133),
            LanguageModel(language = Language(code = "en"), status = ModelState.DOWNLOADED, size = 144),
        )
        assertEquals(expectedModelState, newModelState)
    }

    @Test
    fun `GIVEN a cached model operation THEN the state shouldn't change`() {
        val options = ModelManagementOptions(
            languageToManage = null,
            operation = ModelOperation.DOWNLOAD,
            operationLevel = OperationLevel.CACHE,
        )

        // Simulated process state before syncing with the engine
        val processState = if (options.operation == ModelOperation.DOWNLOAD) ModelState.DOWNLOAD_IN_PROGRESS else ModelState.DELETION_IN_PROGRESS

        val newModelState = LanguageModel.determineNewLanguageModelState(
            currentLanguageModels = mockLanguageModels,
            options = options,
            newStatus = processState,
        )

        assertEquals(mockLanguageModels, newModelState)
    }

    @Test
    fun `GIVEN various state changes THEN the state should only be operable under certain conditions`() {
        // If a model is already downloaded, then we shouldn't switch to download in progress.
        assertFalse(LanguageModel.checkIfOperable(currentStatus = ModelState.DOWNLOADED, newStatus = ModelState.DOWNLOAD_IN_PROGRESS))

        // If a model is already not installed, then we shouldn't switch to deletion in progress.
        assertFalse(LanguageModel.checkIfOperable(currentStatus = ModelState.NOT_DOWNLOADED, newStatus = ModelState.DELETION_IN_PROGRESS))

        // If a model is already downloaded, then we can switch to deletion in progress.
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DOWNLOADED, newStatus = ModelState.DELETION_IN_PROGRESS))

        // If a model is already not installed, then we can switch to download in progress.
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.NOT_DOWNLOADED, newStatus = ModelState.DOWNLOAD_IN_PROGRESS))

        // In process states should always be operable.
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DOWNLOAD_IN_PROGRESS, newStatus = ModelState.DELETION_IN_PROGRESS))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DOWNLOAD_IN_PROGRESS, newStatus = ModelState.DOWNLOADED))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DOWNLOAD_IN_PROGRESS, newStatus = ModelState.NOT_DOWNLOADED))

        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DELETION_IN_PROGRESS, newStatus = ModelState.DOWNLOAD_IN_PROGRESS))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DELETION_IN_PROGRESS, newStatus = ModelState.DOWNLOADED))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DELETION_IN_PROGRESS, newStatus = ModelState.NOT_DOWNLOADED))
    }
}
