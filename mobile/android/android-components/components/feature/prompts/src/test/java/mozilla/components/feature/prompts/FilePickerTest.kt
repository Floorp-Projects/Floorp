/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.Manifest
import android.app.Activity.RESULT_CANCELED
import android.app.Activity.RESULT_OK
import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager.PERMISSION_DENIED
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.net.Uri
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.grantPermission
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class FilePickerTest {

    private val noopSingle: (Context, Uri) -> Unit = { _, _ -> }
    private val noopMulti: (Context, Array<Uri>) -> Unit = { _, _ -> }
    private val request = PromptRequest.File(
        mimeTypes = emptyArray(),
        onSingleFileSelected = noopSingle,
        onMultipleFilesSelected = noopMulti,
        onDismiss = {}
    )

    private lateinit var fragment: PromptContainer
    private lateinit var store: BrowserStore
    private lateinit var state: BrowserState
    private lateinit var filePicker: FilePicker

    @Before
    fun setup() {
        fragment = mock()
        state = mock()
        store = mock()
        whenever(store.state).thenReturn(state)
        filePicker = FilePicker(fragment, store) { }
    }

    @Test
    fun `FilePicker acts on a given (custom tab) session or the selected session`() {
        val customTabContent: ContentState = mock()
        whenever(customTabContent.promptRequest).thenReturn(request)
        val customTab = CustomTabSessionState("custom-tab", customTabContent, mock(), mock())

        whenever(state.customTabs).thenReturn(listOf(customTab))
        filePicker = FilePicker(fragment, store, customTab.id) { }
        filePicker.onActivityResult(R.id.mozac_feature_prompts_file_picker_activity_request_code, 0, null)
        verify(store).dispatch(ContentAction.ConsumePromptRequestAction(customTab.id))

        val selected = prepareSelectedSession(request)
        filePicker = FilePicker(fragment, store) { }
        filePicker.onActivityResult(R.id.mozac_feature_prompts_file_picker_activity_request_code, 0, null)
        verify(store).dispatch(ContentAction.ConsumePromptRequestAction(selected.id))
    }

    @Test
    fun `handleFilePickerRequest without the required permission will call onNeedToRequestPermissions`() {
        var onRequestPermissionWasCalled = false
        val context = ApplicationProvider.getApplicationContext<Context>()

        filePicker = FilePicker(fragment, store) {
            onRequestPermissionWasCalled = true
        }

        doReturn(context).`when`(fragment).context

        filePicker.handleFileRequest(request)

        assertTrue(onRequestPermissionWasCalled)
        verify(fragment, never()).startActivityForResult(Intent(), 1)
    }

    @Test
    fun `handleFilePickerRequest with the required permission will call startActivityForResult`() {
        val mockFragment: PromptContainer = mock()
        var onRequestPermissionWasCalled = false
        val context = ApplicationProvider.getApplicationContext<Context>()

        filePicker = FilePicker(mockFragment, store) {
            onRequestPermissionWasCalled = true
        }

        doReturn(context).`when`(mockFragment).context

        grantPermission(Manifest.permission.READ_EXTERNAL_STORAGE)

        filePicker.handleFileRequest(request)

        assertFalse(onRequestPermissionWasCalled)
        verify(mockFragment).startActivityForResult(any<Intent>(), anyInt())
    }

    @Test
    fun `onPermissionsGranted will forward call to filePickerRequest`() {
        val selected = prepareSelectedSession(request)
        stubContext()
        filePicker = spy(filePicker)
        filePicker.onPermissionsGranted()

        verify(filePicker).handleFileRequest(any(), eq(false))
        verify(store).dispatch(ContentAction.ConsumePromptRequestAction(selected.id))
    }

    @Test
    fun `onPermissionsDeny will call onDismiss and consume the file PromptRequest of the actual session`() {
        var onDismissWasCalled = false
        val filePickerRequest = request.copy {
            onDismissWasCalled = true
        }

        val selected = prepareSelectedSession(filePickerRequest)

        stubContext()

        filePicker.onPermissionsDenied()

        assertTrue(onDismissWasCalled)
        verify(store).dispatch(ContentAction.ConsumePromptRequestAction(selected.id))
    }

    @Test
    fun `onActivityResult with RESULT_OK and isMultipleFilesSelection false will consume PromptRequest of the actual session`() {
        var onSingleFileSelectionWasCalled = false

        val onSingleFileSelection: (Context, Uri) -> Unit = { _, _ ->
            onSingleFileSelectionWasCalled = true
        }

        val filePickerRequest = request.copy(onSingleFileSelected = onSingleFileSelection)

        val selected = prepareSelectedSession(filePickerRequest)
        val intent = Intent()

        intent.data = mock()

        stubContext()

        filePicker.onActivityResult(R.id.mozac_feature_prompts_file_picker_activity_request_code, RESULT_OK, intent)

        assertTrue(onSingleFileSelectionWasCalled)
        verify(store).dispatch(ContentAction.ConsumePromptRequestAction(selected.id))
    }

    @Test
    fun `onActivityResult with RESULT_OK and isMultipleFilesSelection true will consume PromptRequest of the actual session`() {
        var onMultipleFileSelectionWasCalled = false

        val onMultipleFileSelection: (Context, Array<Uri>) -> Unit = { _, _ ->
            onMultipleFileSelectionWasCalled = true
        }

        val filePickerRequest = request.copy(
            isMultipleFilesSelection = true,
            onMultipleFilesSelected = onMultipleFileSelection
        )

        val selected = prepareSelectedSession(filePickerRequest)
        val intent = Intent()

        intent.clipData = mock()
        val item = mock<ClipData.Item>()

        doReturn(mock<Uri>()).`when`(item).uri

        intent.clipData?.apply {
            doReturn(1).`when`(this).itemCount
            doReturn(item).`when`(this).getItemAt(0)
        }

        stubContext()

        filePicker.onActivityResult(R.id.mozac_feature_prompts_file_picker_activity_request_code, RESULT_OK, intent)

        assertTrue(onMultipleFileSelectionWasCalled)
        verify(store).dispatch(ContentAction.ConsumePromptRequestAction(selected.id))
    }

    @Test
    fun `onActivityResult with not RESULT_OK will consume PromptRequest of the actual session and call onDismiss `() {
        var onDismissWasCalled = false

        val filePickerRequest = request.copy(isMultipleFilesSelection = true) {
            onDismissWasCalled = true
        }

        val selected = prepareSelectedSession(filePickerRequest)
        val intent = Intent()

        filePicker.onActivityResult(R.id.mozac_feature_prompts_file_picker_activity_request_code, RESULT_CANCELED, intent)

        assertTrue(onDismissWasCalled)
        verify(store).dispatch(ContentAction.ConsumePromptRequestAction(selected.id))
    }

    @Test
    fun `onRequestPermissionsResult with FILE_PICKER_REQUEST and PERMISSION_GRANTED will call onPermissionsGranted`() {
        filePicker = spy(filePicker)
        filePicker.onPermissionsResult(emptyArray(), IntArray(1) { PERMISSION_GRANTED })

        verify(filePicker).onPermissionsGranted()
    }

    @Test
    fun `onRequestPermissionsResult with FILE_PICKER_REQUEST and PERMISSION_DENIED will call onPermissionsDeny`() {
        filePicker = spy(filePicker)
        filePicker.onPermissionsResult(emptyArray(), IntArray(1) { PERMISSION_DENIED })

        verify(filePicker).onPermissionsDenied()
    }

    private fun prepareSelectedSession(request: PromptRequest? = null): TabSessionState {
        val promptRequest: PromptRequest = request ?: mock()
        val content: ContentState = mock()
        whenever(content.promptRequest).thenReturn(promptRequest)

        val selected = TabSessionState("browser-tab", content, mock(), mock())
        whenever(state.selectedTabId).thenReturn(selected.id)
        whenever(state.tabs).thenReturn(listOf(selected))
        return selected
    }

    private fun stubContext() {
        val context = ApplicationProvider.getApplicationContext<Context>()
        doReturn(context).`when`(fragment).context
        filePicker = FilePicker(fragment, store) {}
    }
}
