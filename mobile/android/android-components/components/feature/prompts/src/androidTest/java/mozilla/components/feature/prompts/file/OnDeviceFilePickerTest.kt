/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.file

import android.content.ClipData
import android.content.Context
import android.content.Intent
import androidx.core.net.toUri
import androidx.test.core.app.ApplicationProvider
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.feature.prompts.PromptContainer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class OnDeviceFilePickerTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private val badUri1 = "file:///data/user/0/${context.packageName}/any_directory/file.text".toUri()
    private val badUri2 = "file:///data/data/${context.packageName}/any_directory/file.text".toUri()
    private val goodUri = "file:///data/directory/${context.packageName}/any_directory/file.text".toUri()

    @Test
    fun removeUrisUnderPrivateAppDir() {
        val uris = arrayOf(badUri1, badUri2, goodUri)

        val filterUris = uris.removeUrisUnderPrivateAppDir(context)

        assertEquals(1, filterUris.size)
        assertTrue(filterUris.contains(goodUri))
    }

    @Test
    fun unsafeUrisWillNotBeSelected() {
        val promptContainer = PromptContainer.TestPromptContainer(context)

        val filePicker = FilePicker(promptContainer, BrowserStore()) { }
        var onDismissWasExecuted = false
        var onSingleFileSelectedWasExecuted = false
        var onMultipleFilesSelectedWasExecuted = false

        val filePickerRequest = PromptRequest.File(
            arrayOf(""),
            isMultipleFilesSelection = true,
            onDismiss = { onDismissWasExecuted = true },
            onSingleFileSelected = { _, _ -> onSingleFileSelectedWasExecuted = true },
            onMultipleFilesSelected = { _, _ -> onMultipleFilesSelectedWasExecuted = true },
        )

        val intent = Intent()
        intent.clipData = ClipData("", arrayOf(), ClipData.Item(badUri1))
        filePicker.handleFilePickerIntentResult(intent, filePickerRequest)

        assertTrue(onDismissWasExecuted)
        assertFalse(onSingleFileSelectedWasExecuted)
        assertFalse(onMultipleFilesSelectedWasExecuted)
    }

    @Test
    fun safeUrisWillBeSelected() {
        val promptContainer = PromptContainer.TestPromptContainer(context)
        val filePicker = FilePicker(promptContainer, BrowserStore()) { }
        var urisWereSelected = false
        var onDismissWasExecuted = false
        var onSingleFileSelectedWasExecuted = false

        val filePickerRequest = PromptRequest.File(
            arrayOf(""),
            isMultipleFilesSelection = true,
            onDismiss = { onDismissWasExecuted = true },
            onSingleFileSelected = { _, _ -> onSingleFileSelectedWasExecuted = true },
            onMultipleFilesSelected = { _, uris -> urisWereSelected = uris.isNotEmpty() },
        )

        val intent = Intent()
        intent.clipData = ClipData("", arrayOf(), ClipData.Item(goodUri))
        filePicker.handleFilePickerIntentResult(intent, filePickerRequest)

        assertTrue(urisWereSelected)
        assertFalse(onDismissWasExecuted)
        assertFalse(onSingleFileSelectedWasExecuted)
    }

    @Test
    fun unsafeUriWillNotBeSelected() {
        val promptContainer = PromptContainer.TestPromptContainer(context)
        val filePicker = FilePicker(promptContainer, BrowserStore()) { }
        var onDismissWasExecuted = false
        var onSingleFileSelectedWasExecuted = false
        var onMultipleFilesSelectedWasExecuted = false

        val filePickerRequest = PromptRequest.File(
            arrayOf(""),
            onDismiss = { onDismissWasExecuted = true },
            onSingleFileSelected = { _, _ -> onSingleFileSelectedWasExecuted = true },
            onMultipleFilesSelected = { _, _ -> onMultipleFilesSelectedWasExecuted = true },
        )

        val intent = Intent()
        intent.data = badUri1
        filePicker.handleFilePickerIntentResult(intent, filePickerRequest)

        assertTrue(onDismissWasExecuted)
        assertFalse(onSingleFileSelectedWasExecuted)
        assertFalse(onMultipleFilesSelectedWasExecuted)
    }

    @Test
    fun safeUriWillBeSelected() {
        val promptContainer = PromptContainer.TestPromptContainer(context)
        val filePicker = FilePicker(promptContainer, BrowserStore()) { }
        var onDismissWasExecuted = false
        var onSingleFileSelectedWasExecuted = false
        var onMultipleFilesSelectedWasExecuted = false

        val filePickerRequest = PromptRequest.File(
            arrayOf(""),
            onDismiss = { onDismissWasExecuted = true },
            onSingleFileSelected = { _, _ -> onSingleFileSelectedWasExecuted = true },
            onMultipleFilesSelected = { _, _ -> onMultipleFilesSelectedWasExecuted = true },
        )

        val intent = Intent()
        intent.data = goodUri
        filePicker.handleFilePickerIntentResult(intent, filePickerRequest)

        assertTrue(onSingleFileSelectedWasExecuted)
        assertFalse(onMultipleFilesSelectedWasExecuted)
        assertFalse(onDismissWasExecuted)
    }
}
