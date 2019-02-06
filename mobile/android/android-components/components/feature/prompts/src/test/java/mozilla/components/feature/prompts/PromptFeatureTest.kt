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
import android.content.Intent.ACTION_GET_CONTENT
import android.content.Intent.EXTRA_ALLOW_MULTIPLE
import android.content.Intent.EXTRA_MIME_TYPES
import android.content.pm.PackageManager.PERMISSION_DENIED
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.net.Uri
import android.support.v4.app.Fragment
import android.support.v4.app.FragmentManager
import android.support.v4.app.FragmentTransaction
import androidx.test.core.app.ApplicationProvider
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.TextPrompt
import mozilla.components.concept.engine.prompt.PromptRequest.Color
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Level.NONE
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Method.HOST
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.grantPermission
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import java.security.InvalidParameterException
import java.util.Date
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
class PromptFeatureTest {

    private lateinit var mockSessionManager: SessionManager
    private lateinit var mockFragmentManager: FragmentManager
    private lateinit var promptFeature: PromptFeature

    @Before
    fun setup() {
        val engine = Mockito.mock(Engine::class.java)
        mockFragmentManager = mockFragmentManager()

        mockSessionManager = Mockito.spy(SessionManager(engine))
        promptFeature = PromptFeature(null, mock(), mockSessionManager, mockFragmentManager) { }
    }

    @Test
    fun `New promptRequests for selected session will cause fragment transaction`() {

        val session = getSelectedSession()
        val singleChoiceRequest = SingleChoice(arrayOf()) {}

        promptFeature.start()

        session.promptRequest = Consumable.from(singleChoiceRequest)

        verify(mockFragmentManager).beginTransaction()
    }

    @Test
    fun `New promptRequests for selected session will not cause fragment transaction if feature is stopped`() {

        val session = getSelectedSession()
        val singleChoiceRequest = MultipleChoice(arrayOf()) {}

        promptFeature.start()
        promptFeature.stop()

        session.promptRequest = Consumable.from(singleChoiceRequest)

        verify(mockFragmentManager, never()).beginTransaction()
    }

    @Test
    fun `Feature will re-attach to already existing fragment`() {

        val session = getSelectedSession().apply {
            promptRequest = Consumable.from(MultipleChoice(arrayOf()) {})
        }

        val fragment: ChoiceDialogFragment = mock()
        doReturn(session.id).`when`(fragment).sessionId

        mockFragmentManager = mock()
        doReturn(fragment).`when`(mockFragmentManager).findFragmentByTag(any())

        promptFeature = PromptFeature(mock(), null, mockSessionManager, mockFragmentManager) { }

        promptFeature.start()
        verify(fragment).feature = promptFeature
    }

    @Test
    fun `Already existing fragment will be removed if session has no prompt request set anymore`() {
        val session = getSelectedSession()

        val fragment: ChoiceDialogFragment = mock()

        doReturn(session.id).`when`(fragment).sessionId

        val transaction: FragmentTransaction = mock()
        val fragmentManager: FragmentManager = mock()

        doReturn(fragment).`when`(fragmentManager).findFragmentByTag(any())
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(fragment)

        val feature = PromptFeature(null, mock(), mockSessionManager, fragmentManager) { }

        feature.start()
        verify(fragmentManager).beginTransaction()
        verify(transaction).remove(fragment)
    }

    @Test
    fun `Already existing fragment will be removed if session does not exist anymore`() {
        val fragment: ChoiceDialogFragment = mock()
        doReturn(UUID.randomUUID().toString()).`when`(fragment).sessionId

        val transaction: FragmentTransaction = mock()
        val fragmentManager: FragmentManager = mock()

        doReturn(fragment).`when`(fragmentManager).findFragmentByTag(any())
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(fragment)

        val feature = PromptFeature(mock(), mock(), mockSessionManager, fragmentManager) { }

        feature.start()
        verify(fragmentManager).beginTransaction()
        verify(transaction).remove(fragment)
    }

    @Test
    fun `Calling onCancel will consume promptRequest`() {
        val session = getSelectedSession().apply {
            promptRequest = Consumable.from(MultipleChoice(arrayOf()) {})
        }

        assertFalse(session.promptRequest.isConsumed())
        promptFeature.onCancel(session.id)
        assertTrue(session.promptRequest.isConsumed())
    }

    @Test
    fun `Selecting a item a single choice dialog will consume promptRequest`() {
        val session = getSelectedSession()
        val singleChoiceRequest = SingleChoice(arrayOf()) {}

        promptFeature.start()

        session.promptRequest = Consumable.from(singleChoiceRequest)

        promptFeature.onConfirm(session.id, mock<Choice>())

        assertTrue(session.promptRequest.isConsumed())
    }

    @Test
    fun `Unknown session will no consume pending promptRequest`() {
        val session = getSelectedSession()
        val singleChoiceRequest = SingleChoice(arrayOf()) {}

        promptFeature.start()

        session.promptRequest = Consumable.from(singleChoiceRequest)

        promptFeature.onConfirm("unknown_session", mock())

        assertFalse(session.promptRequest.isConsumed())

        promptFeature.onConfirm("unknown_session", arrayOf<Choice>())

        assertFalse(session.promptRequest.isConsumed())

        promptFeature.onCancel("unknown_session")

        assertFalse(session.promptRequest.isConsumed())
    }

    @Test
    fun `Selecting a item a menu choice dialog will consume promptRequest`() {
        val session = getSelectedSession()
        val menuChoiceRequest = MenuChoice(arrayOf()) {}

        promptFeature.start()

        session.promptRequest = Consumable.from(menuChoiceRequest)

        promptFeature.onConfirm(session.id, mock<Choice>())

        assertTrue(session.promptRequest.isConsumed())
    }

    @Test
    fun `Selecting items on multiple choice dialog will consume promptRequest`() {
        val session = getSelectedSession()
        val multipleChoiceRequest = MultipleChoice(arrayOf()) {}

        promptFeature.start()

        session.promptRequest = Consumable.from(multipleChoiceRequest)

        promptFeature.onConfirm(session.id, arrayOf<Choice>())

        assertTrue(session.promptRequest.isConsumed())
    }

    @Test
    fun `onNoMoreDialogsChecked will consume promptRequest`() {
        val session = getSelectedSession()

        var onShowNoMoreAlertsWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = Alert("title", "message", false, { onDismissWasCalled = true }) {
            onShowNoMoreAlertsWasCalled = true
        }

        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onConfirm(session.id, false)

        assertTrue(session.promptRequest.isConsumed())
        assertTrue(onShowNoMoreAlertsWasCalled)

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onCancel(session.id)
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onNoMoreDialogsChecked or onCancel with unknown sessionId will not consume promptRequest`() {
        val session = getSelectedSession()

        var onShowNoMoreAlertsWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = Alert("title", "message", false, { onDismissWasCalled = true }) {
            onShowNoMoreAlertsWasCalled = true
        }

        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onConfirm("unknown_session_id", false)

        assertFalse(session.promptRequest.isConsumed())
        assertFalse(onShowNoMoreAlertsWasCalled)

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onCancel("unknown_session_id")
        assertFalse(onDismissWasCalled)
    }

    @Test
    fun `Calling onCancel with an alert request will consume promptRequest and call onDismiss`() {
        val session = getSelectedSession()

        var onDismissWasCalled = false

        val promptRequest = Alert("title", "message", false, { onDismissWasCalled = true }) {}

        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onCancel(session.id)

        assertTrue(session.promptRequest.isConsumed())
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onConfirmTextPrompt with unknown sessionId will not consume promptRequest`() {
        val session = getSelectedSession()

        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = TextPrompt(
            "title",
            "message",
            "input",
            false,
            { onDismissWasCalled = true }) { _, _ ->
            onConfirmWasCalled = true
        }
        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onConfirm("unknown_session_id", false to "")

        assertFalse(session.promptRequest.isConsumed())
        assertFalse(onConfirmWasCalled)

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onCancel("unknown_session_id")
        assertFalse(onDismissWasCalled)
    }

    @Test
    fun `onConfirmTextPrompt will consume promptRequest`() {
        val session = getSelectedSession()

        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = TextPrompt(
            "title",
            "message",
            "input",
            false,
            { onDismissWasCalled = true }) { _, _ ->
            onConfirmWasCalled = true
        }

        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onConfirm(session.id, false to "")

        assertTrue(session.promptRequest.isConsumed())
        assertTrue(onConfirmWasCalled)

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onCancel(session.id)
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onCancel with an TextPrompt request will consume promptRequest and call onDismiss`() {
        val session = getSelectedSession()

        var onDismissWasCalled = false

        val promptRequest = TextPrompt(
            "title",
            "message",
            "value",
            false,
            { onDismissWasCalled = true }) { _, _ -> }

        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onCancel(session.id)

        assertTrue(session.promptRequest.isConsumed())
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Selecting a time or calling onClear with unknown sessionId will not consume promptRequest`() {

        val timeSelectionTypes = listOf(
            PromptRequest.TimeSelection.Type.DATE,
            PromptRequest.TimeSelection.Type.DATE_AND_TIME,
            PromptRequest.TimeSelection.Type.TIME
        )

        timeSelectionTypes.forEach { timeType ->

            val session = getSelectedSession()
            var onClearWasCalled = false
            var selectedDate: Date? = null

            val promptRequest = PromptRequest.TimeSelection(
                "title", Date(),
                null,
                null,
                timeType,
                { date -> selectedDate = date }) {
                onClearWasCalled = true
            }

            promptFeature.start()

            session.promptRequest = Consumable.from(promptRequest)

            promptFeature.onConfirm("unknown_sessionId", Date())

            assertFalse(session.promptRequest.isConsumed())
            assertNull(selectedDate)
            session.promptRequest = Consumable.from(promptRequest)

            promptFeature.onClear("unknown_sessionId")
            assertFalse(onClearWasCalled)
        }
    }

    @Test
    fun `selecting a time will consume promptRequest`() {
        val timeSelectionTypes = listOf(
            PromptRequest.TimeSelection.Type.DATE,
            PromptRequest.TimeSelection.Type.DATE_AND_TIME,
            PromptRequest.TimeSelection.Type.TIME
        )

        timeSelectionTypes.forEach { _ ->

            val session = getSelectedSession()
            var onClearWasCalled = false
            var selectedDate: Date? = null
            val promptRequest = PromptRequest.TimeSelection(
                "title", Date(),
                null,
                null,
                PromptRequest.TimeSelection.Type.DATE,
                { date -> selectedDate = date }) {
                onClearWasCalled = true
            }

            promptFeature.start()
            session.promptRequest = Consumable.from(promptRequest)

            val date = Date()
            promptFeature.onConfirm(session.id, Date())

            assertTrue(session.promptRequest.isConsumed())
            assertEquals(selectedDate, date)
            session.promptRequest = Consumable.from(promptRequest)

            promptFeature.onClear(session.id)
            assertTrue(onClearWasCalled)
        }
    }

    @Test(expected = InvalidParameterException::class)
    fun `calling handleDialogsRequest with not a promptRequest type will throw an exception`() {
        promptFeature.handleDialogsRequest(mock<PromptRequest.File>(), mock())
    }

    @Test(expected = IllegalStateException::class)
    fun `Initializing a PromptFeature without giving an activity or fragment reference will throw an exception`() {
        promptFeature = PromptFeature(null, null, mockSessionManager, mockFragmentManager) { }
    }

    @Test
    fun `startActivityForResult must delegate its calls either to an activity or a fragment`() {
        val mockActivity: Activity = mock()
        val mockFragment: Fragment = mock()
        val intent = Intent()
        val code = 1

        promptFeature = PromptFeature(mockActivity, null, mockSessionManager, mockFragmentManager) { }

        promptFeature.startActivityForResult(intent, code)
        verify(mockActivity).startActivityForResult(intent, code)

        promptFeature = PromptFeature(null, mockFragment, mockSessionManager, mockFragmentManager) { }

        promptFeature.startActivityForResult(intent, code)
        verify(mockFragment).startActivityForResult(intent, code)
    }

    @Test
    fun `handleFilePickerRequest without the required permission will call onNeedToRequestPermissions`() {
        val mockFragment: Fragment = mock()
        val intent = Intent()
        val code = 1
        var onRequestPermissionWasCalled = false
        val filePickerRequest = PromptRequest.File(emptyArray(), false, { _, _ -> }, { _, _ -> }) {}
        val context = ApplicationProvider.getApplicationContext<Context>()

        promptFeature = PromptFeature(null, mockFragment, mockSessionManager, mockFragmentManager) {
            onRequestPermissionWasCalled = true
        }

        doReturn(context).`when`(mockFragment).requireContext()

        promptFeature.handleFilePickerRequest(filePickerRequest)

        assertTrue(onRequestPermissionWasCalled)
        verify(mockFragment, never()).startActivityForResult(intent, code)
    }

    @Test
    fun `handleFilePickerRequest with the required permission will call startActivityForResult`() {
        val mockFragment: Fragment = mock()
        var onRequestPermissionWasCalled = false
        val filePickerRequest = PromptRequest.File(emptyArray(), false, { _, _ -> }, { _, _ -> }) {}
        val context = ApplicationProvider.getApplicationContext<Context>()

        promptFeature = PromptFeature(null, mockFragment, mockSessionManager, mockFragmentManager) {
            onRequestPermissionWasCalled = true
        }

        doReturn(context).`when`(mockFragment).requireContext()

        grantPermission(Manifest.permission.READ_EXTERNAL_STORAGE)

        promptFeature.handleFilePickerRequest(filePickerRequest)

        assertFalse(onRequestPermissionWasCalled)
        verify(mockFragment).startActivityForResult(any<Intent>(), anyInt())
    }

    @Test
    fun `buildFileChooserIntent with allowMultipleFiles false and empty mimeTypes will create an intent without EXTRA_ALLOW_MULTIPLE and EXTRA_MIME_TYPES`() {

        val intent = promptFeature.buildFileChooserIntent(false, emptyArray())

        with(intent) {

            assertEquals(action, ACTION_GET_CONTENT)

            val mimeType = extras!!.get(EXTRA_MIME_TYPES)
            assertNull(mimeType)

            val allowMultipleFiles = extras!!.getBoolean(EXTRA_ALLOW_MULTIPLE)
            assertFalse(allowMultipleFiles)
        }
    }

    @Test
    fun `buildFileChooserIntent with allowMultipleFiles true and not empty mimeTypes will create an intent with EXTRA_ALLOW_MULTIPLE and EXTRA_MIME_TYPES`() {

        promptFeature = PromptFeature(null, mock(), mockSessionManager, mockFragmentManager) { }

        val intent = promptFeature.buildFileChooserIntent(true, arrayOf("image/jpeg"))

        with(intent) {

            assertEquals(action, ACTION_GET_CONTENT)

            val mimeTypes = extras!!.get(EXTRA_MIME_TYPES) as Array<*>
            assertEquals(mimeTypes.first(), "image/jpeg")

            val allowMultipleFiles = extras!!.getBoolean(EXTRA_ALLOW_MULTIPLE)
            assertTrue(allowMultipleFiles)
        }
    }

    @Test
    fun `onPermissionsGranted will forward its call to onPromptRequested`() {
        val filePickerRequest = PromptRequest.File(emptyArray(), false, { _, _ -> }, { _, _ -> }) {}

        val session = getSelectedSession()

        session.promptRequest = Consumable.from(filePickerRequest)

        stubContext()

        promptFeature = spy(promptFeature)

        promptFeature.onPermissionsGranted()

        verify(mockSessionManager).selectedSession
        verify(promptFeature).onPromptRequested(any(), any<PromptRequest.File>())
        assertFalse(session.promptRequest.isConsumed())
    }

    @Test
    fun `onPermissionsDeny will call onDismiss and consume the file PromptRequest of the actual session`() {

        var onDismissWasCalled = false
        val filePickerRequest = PromptRequest.File(emptyArray(), false, { _, _ -> }, { _, _ -> }) {
            onDismissWasCalled = true
        }

        val session = getSelectedSession()

        stubContext()

        session.promptRequest = Consumable.from(filePickerRequest)

        promptFeature.onPermissionsDenied()

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

        val filePickerRequest =
            PromptRequest.File(emptyArray(), false, onSingleFileSelection, { _, _ -> }) {
            }

        val session = getSelectedSession()
        val intent = Intent()

        intent.data = mock()
        session.promptRequest = Consumable.from(filePickerRequest)

        stubContext()

        promptFeature.onActivityResult(PromptFeature.FILE_PICKER_ACTIVITY_REQUEST_CODE, RESULT_OK, intent)

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

        val filePickerRequest =
            PromptRequest.File(emptyArray(), true, { _, _ -> }, onMultipleFileSelection) {}

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

        promptFeature.onActivityResult(PromptFeature.FILE_PICKER_ACTIVITY_REQUEST_CODE, RESULT_OK, intent)

        verify(mockSessionManager).selectedSession
        assertTrue(onMultipleFileSelectionWasCalled)
        assertTrue(session.promptRequest.isConsumed())
    }

    @Test
    fun `onActivityResult with not RESULT_OK will consume PromptRequest of the actual session and call onDismiss `() {

        var onDismissWasCalled = false

        val filePickerRequest =
            PromptRequest.File(emptyArray(), true, { _, _ -> }, { _, _ -> }) {
                onDismissWasCalled = true
            }

        val session = getSelectedSession()
        val intent = Intent()

        session.promptRequest = Consumable.from(filePickerRequest)

        promptFeature.onActivityResult(PromptFeature.FILE_PICKER_ACTIVITY_REQUEST_CODE, RESULT_CANCELED, intent)

        verify(mockSessionManager).selectedSession
        assertTrue(onDismissWasCalled)
        assertTrue(session.promptRequest.isConsumed())
    }

    @Test
    fun `onRequestPermissionsResult with FILE_PICKER_REQUEST and PERMISSION_GRANTED will call onPermissionsGranted`() {

        promptFeature = spy(promptFeature)

        promptFeature.onPermissionsResult(emptyArray(), IntArray(1) { PERMISSION_GRANTED })

        verify(promptFeature).onPermissionsGranted()
    }

    @Test
    fun `onRequestPermissionsResult with FILE_PICKER_REQUEST and PERMISSION_DENIED will call onPermissionsDeny`() {

        promptFeature = spy(promptFeature)

        promptFeature.onPermissionsResult(emptyArray(), IntArray(1) { PERMISSION_DENIED })

        verify(promptFeature).onPermissionsDenied()
    }

    @Test
    fun `Calling onConfirmAuthentication will consume promptRequest`() {
        val session = getSelectedSession()

        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = Authentication(
            "title",
            "message",
            "username",
            "password",
            HOST,
            NONE,
            false,
            false,
            false,
            { _, _ -> onConfirmWasCalled = true }) {
            onDismissWasCalled = true
        }

        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onConfirm(session.id, "" to "")

        assertTrue(session.promptRequest.isConsumed())
        assertTrue(onConfirmWasCalled)

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onCancel(session.id)
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onConfirmAuthentication with unknown sessionId will not consume promptRequest`() {
        val session = getSelectedSession()

        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = Authentication(
            "title",
            "message",
            "username",
            "password",
            HOST,
            NONE,
            false,
            false,
            false,
            { _, _ -> onConfirmWasCalled = true }) {
            onDismissWasCalled = true
        }

        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onConfirm("unknown_session_id", "" to "")

        assertFalse(session.promptRequest.isConsumed())
        assertFalse(onConfirmWasCalled)

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onCancel("unknown_session_id")
        assertFalse(onDismissWasCalled)
    }

    @Test
    fun `Calling onCancel with an Authentication request will consume promptRequest and call onDismiss`() {
        val session = getSelectedSession()
        var onDismissWasCalled = false

        val promptRequest = Authentication(
            "title",
            "message",
            "username",
            "password",
            HOST,
            NONE,
            false,
            false,
            false,
            { _, _ -> }) {
            onDismissWasCalled = true
        }

        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onCancel(session.id)

        assertTrue(session.promptRequest.isConsumed())
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onConfirm with a Color request will consume promptRequest`() {
        val session = getSelectedSession()

        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = Color(
            "#e66465",
            {
                onConfirmWasCalled = true
            }) {
            onDismissWasCalled = true
        }

        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onConfirm(session.id, "#f6b73c")

        assertTrue(session.promptRequest.isConsumed())
        assertTrue(onConfirmWasCalled)

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onCancel(session.id)
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `calling onConfirm for a Popup request will consume promptRequest`() {
        val session = getSelectedSession()
        var onConfirmWasCalled = false

        val promptRequest = PromptRequest.Popup(
            "http://www.popuptest.com/",
            { onConfirmWasCalled = true }
        ) {}

        stubContext()

        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onConfirm(session.id)

        assertTrue(session.promptRequest.isConsumed())
        assertTrue(onConfirmWasCalled)
    }

    @Test
    fun `calling onCancel with a Popup request will consume promptRequest`() {
        val session = getSelectedSession()
        var onCancelWasCalled = false

        val promptRequest = PromptRequest.Popup("http://www.popuptest.com/", { }) {
            onCancelWasCalled = true
        }

        stubContext()

        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onCancel(session.id)

        assertTrue(session.promptRequest.isConsumed())
        assertTrue(onCancelWasCalled)

        session.promptRequest = Consumable.from(promptRequest)
    }

    @Test
    fun `calling onConfirm for a Confirm request will consume promptRequest`() {
        val session = getSelectedSession()
        var onPositiveButtonWasCalled = false
        var onNegativeButtonWasCalled = false
        var onNeutralButtonWasCalled = false

        val onConfirmPositiveButton: (Boolean) -> Unit = {
            onPositiveButtonWasCalled = true
        }

        val onConfirmNegativeButton: (Boolean) -> Unit = {
            onNegativeButtonWasCalled = true
        }

        val onConfirmNeutralButton: (Boolean) -> Unit = {
            onNeutralButtonWasCalled = true
        }

        val promptRequest = PromptRequest.Confirm(
            "title",
            "message",
            false,
            "positive",
            "negative",
            "neutral",
            onConfirmPositiveButton,
            onConfirmNegativeButton,
            onConfirmNeutralButton
        ) {}

        stubContext()

        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onConfirm(session.id, true to MultiButtonDialogFragment.ButtonType.POSITIVE)

        assertTrue(session.promptRequest.isConsumed())
        assertTrue(onPositiveButtonWasCalled)

        session.promptRequest = Consumable.from(promptRequest)
        promptFeature.onConfirm(session.id, true to MultiButtonDialogFragment.ButtonType.NEGATIVE)

        assertTrue(session.promptRequest.isConsumed())
        assertTrue(onNegativeButtonWasCalled)

        session.promptRequest = Consumable.from(promptRequest)
        promptFeature.onConfirm(session.id, true to MultiButtonDialogFragment.ButtonType.NEUTRAL)

        assertTrue(session.promptRequest.isConsumed())
        assertTrue(onNeutralButtonWasCalled)
    }

    @Test
    fun `calling onCancel with a Confirm request will consume promptRequest`() {
        val session = getSelectedSession()
        var onCancelWasCalled = false

        val onConfirm: (Boolean) -> Unit = { }

        val onDismiss: () -> Unit = {
            onCancelWasCalled = true
        }

        val promptRequest = PromptRequest.Confirm(
            "title",
            "message",
            false,
            "positive",
            "negative",
            "neutral",
            onConfirm,
            onConfirm,
            onConfirm,
            onDismiss
        )

        stubContext()

        promptFeature.start()

        session.promptRequest = Consumable.from(promptRequest)

        promptFeature.onCancel(session.id)

        assertTrue(session.promptRequest.isConsumed())
        assertTrue(onCancelWasCalled)

        session.promptRequest = Consumable.from(promptRequest)
    }

    private fun getSelectedSession(): Session {
        val session = Session("")
        mockSessionManager.add(session)
        mockSessionManager.select(session)
        return session
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()
        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        return fragmentManager
    }

    private fun stubContext() {
        val mockFragment: Fragment = mock()
        val context = ApplicationProvider.getApplicationContext<Context>()

        doReturn(context).`when`(mockFragment).requireContext()

        promptFeature = PromptFeature(null, mockFragment, mockSessionManager, mockFragmentManager) {}
    }
}