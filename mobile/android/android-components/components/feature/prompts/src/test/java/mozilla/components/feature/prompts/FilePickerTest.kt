/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.Manifest
import android.app.Activity
import android.app.Activity.RESULT_CANCELED
import android.app.Activity.RESULT_OK
import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager.PERMISSION_DENIED
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.net.Uri
import androidx.fragment.app.Fragment
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.grantPermission
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
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

    private lateinit var mockFragment: Fragment
    private lateinit var mockSessionManager: SessionManager
    private lateinit var filePicker: FilePicker

    @Before
    fun setup() {
        mockFragment = mock()
        mockSessionManager = spy(SessionManager(mock()))
        filePicker = FilePicker(mockFragment, mockSessionManager) { }
    }

    @Test
    fun `FilePicker acts on a given session or the selected session`() {
        val session = Session("custom-tab")
        `when`(mockSessionManager.findSessionById(session.id)).thenReturn(session)

        filePicker = FilePicker(mockFragment, mockSessionManager, session.id) { }
        filePicker.onActivityResult(FilePicker.FILE_PICKER_ACTIVITY_REQUEST_CODE, 0, null)

        verify(mockSessionManager).findSessionById(session.id)

        val selected = Session("browser-tab")
        `when`(mockSessionManager.selectedSession).thenReturn(selected)

        filePicker = FilePicker(mockFragment, mockSessionManager) { }
        filePicker.onActivityResult(FilePicker.FILE_PICKER_ACTIVITY_REQUEST_CODE, 0, null)

        verify(mockSessionManager).selectedSession
    }

    @Test
    fun `startActivityForResult must delegate its calls either to an activity or a fragment`() {
        val mockActivity: Activity = mock()
        val mockFragment: Fragment = mock()
        val intent = Intent()
        val code = 1

        filePicker = FilePicker(mockActivity, mockSessionManager) { }

        filePicker.startActivityForResult(intent, code)
        verify(mockActivity).startActivityForResult(intent, code)

        filePicker = FilePicker(mockFragment, mockSessionManager) { }

        filePicker.startActivityForResult(intent, code)
        verify(mockFragment).startActivityForResult(intent, code)
    }

    @Test
    fun `handleFilePickerRequest without the required permission will call onNeedToRequestPermissions`() {
        var onRequestPermissionWasCalled = false
        val context = ApplicationProvider.getApplicationContext<Context>()

        filePicker = FilePicker(mockFragment, mockSessionManager) {
            onRequestPermissionWasCalled = true
        }

        doReturn(context).`when`(mockFragment).requireContext()

        filePicker.handleFileRequest(request)

        assertTrue(onRequestPermissionWasCalled)
        verify(mockFragment, never()).startActivityForResult(Intent(), 1)
    }

    @Test
    fun `handleFilePickerRequest with the required permission will call startActivityForResult`() {
        val mockFragment: Fragment = mock()
        var onRequestPermissionWasCalled = false
        val context = ApplicationProvider.getApplicationContext<Context>()

        filePicker = FilePicker(mockFragment, mockSessionManager) {
            onRequestPermissionWasCalled = true
        }

        doReturn(context).`when`(mockFragment).requireContext()

        grantPermission(Manifest.permission.READ_EXTERNAL_STORAGE)

        filePicker.handleFileRequest(request)

        assertFalse(onRequestPermissionWasCalled)
        verify(mockFragment).startActivityForResult(any<Intent>(), anyInt())
    }

    @Test
    fun `onPermissionsGranted will forward its call to filePickerRequest`() {
        val session = getSelectedSession()

        session.promptRequest = Consumable.from(request)

        stubContext()

        filePicker = spy(filePicker)

        filePicker.onPermissionsGranted()

        verify(mockSessionManager).selectedSession
        verify(filePicker).handleFileRequest(any(), eq(false))
        assertFalse(session.promptRequest.isConsumed())
    }

    @Test
    fun `onPermissionsDeny will call onDismiss and consume the file PromptRequest of the actual session`() {
        var onDismissWasCalled = false
        val filePickerRequest = request.copy {
            onDismissWasCalled = true
        }

        val session = getSelectedSession()

        stubContext()

        session.promptRequest = Consumable.from(filePickerRequest)

        filePicker.onPermissionsDenied()

        verify(mockSessionManager).selectedSession
        assertTrue(onDismissWasCalled)
        assertTrue(session.promptRequest.isConsumed())
    }

    @Test
    fun `onActivityResult with RESULT_OK and isMultipleFilesSelection false will consume PromptRequest of the actual session`() {

        var onSingleFileSelectionWasCalled = false

        val onSingleFileSelection: (Context, Uri) -> Unit = { _, _ ->
            onSingleFileSelectionWasCalled = true
        }

        val filePickerRequest = request.copy(onSingleFileSelected = onSingleFileSelection)

        val session = getSelectedSession()
        val intent = Intent()

        intent.data = mock()
        session.promptRequest = Consumable.from(filePickerRequest)

        stubContext()

        filePicker.onActivityResult(PromptFeature.FILE_PICKER_ACTIVITY_REQUEST_CODE, RESULT_OK, intent)

        verify(mockSessionManager).selectedSession
        assertTrue(onSingleFileSelectionWasCalled)
        assertTrue(session.promptRequest.isConsumed())
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

        val session = getSelectedSession()
        val intent = Intent()

        intent.clipData = mock()
        val item = mock<ClipData.Item>()

        doReturn(mock<Uri>()).`when`(item).uri

        intent.clipData?.apply {
            doReturn(1).`when`(this).itemCount
            doReturn(item).`when`(this).getItemAt(0)
        }

        session.promptRequest = Consumable.from(filePickerRequest)

        stubContext()

        filePicker.onActivityResult(PromptFeature.FILE_PICKER_ACTIVITY_REQUEST_CODE, RESULT_OK, intent)

        verify(mockSessionManager).selectedSession
        assertTrue(onMultipleFileSelectionWasCalled)
        assertTrue(session.promptRequest.isConsumed())
    }

    @Test
    fun `onActivityResult with not RESULT_OK will consume PromptRequest of the actual session and call onDismiss `() {

        var onDismissWasCalled = false

        val filePickerRequest = request.copy(isMultipleFilesSelection = true) {
            onDismissWasCalled = true
        }

        val session = getSelectedSession()
        val intent = Intent()

        session.promptRequest = Consumable.from(filePickerRequest)

        filePicker.onActivityResult(PromptFeature.FILE_PICKER_ACTIVITY_REQUEST_CODE, RESULT_CANCELED, intent)

        verify(mockSessionManager).selectedSession
        assertTrue(onDismissWasCalled)
        assertTrue(session.promptRequest.isConsumed())
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

    private fun getSelectedSession() = Session("").also {
        mockSessionManager.add(it, selected = true)
    }

    private fun stubContext() {
        val context = ApplicationProvider.getApplicationContext<Context>()

        doReturn(context).`when`(mockFragment).requireContext()

        filePicker = FilePicker(mockFragment, mockSessionManager) {}
    }
}
