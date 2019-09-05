/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.app.Activity
import android.app.Activity.RESULT_CANCELED
import android.app.Activity.RESULT_OK
import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Level.NONE
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Method.HOST
import mozilla.components.concept.engine.prompt.PromptRequest.Color
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.TextPrompt
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.security.InvalidParameterException
import java.util.Date
import java.util.UUID

@RunWith(AndroidJUnit4::class)
class PromptFeatureTest {

    private lateinit var mockSessionManager: SessionManager
    private lateinit var mockFragmentManager: FragmentManager
    private lateinit var promptFeature: PromptFeature

    @Before
    fun setup() {
        val engine = mock<Engine>()
        mockFragmentManager = mockFragmentManager()

        mockSessionManager = spy(SessionManager(engine))
        promptFeature = PromptFeature(mock<Fragment>(), mockSessionManager, null, mockFragmentManager) { }
    }

    @After
    fun tearDown() {
        promptFeature.promptAbuserDetector.resetJSAlertAbuseState()
    }

    @Test
    fun `PromptFeatures act on a given session or the selected session`() {
        val session = Session("custom-tab")
        `when`(mockSessionManager.findSessionById(session.id)).thenReturn(session)

        promptFeature = PromptFeature(mock<Fragment>(), mockSessionManager, session.id, mockFragmentManager) { }
        promptFeature.start()

        verify(mockSessionManager).findSessionById(session.id)

        val selected = Session("browser-tab")
        `when`(mockSessionManager.selectedSession).thenReturn(selected)

        promptFeature = PromptFeature(mock<Fragment>(), mockSessionManager, null, mockFragmentManager) { }
        promptFeature.start()

        verify(mockSessionManager).selectedSession
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

        promptFeature = PromptFeature(mock<Activity>(), mockSessionManager, null, mockFragmentManager) { }

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

        val feature = PromptFeature(mock<Fragment>(), mockSessionManager, null, fragmentManager) { }

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

        val feature = PromptFeature(mock<Activity>(), mockSessionManager, null, fragmentManager) { }

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
    fun `selecting a time will consume promptRequest`() {
        val timeSelectionTypes = listOf(
            PromptRequest.TimeSelection.Type.DATE,
            PromptRequest.TimeSelection.Type.DATE_AND_TIME,
            PromptRequest.TimeSelection.Type.TIME,
            PromptRequest.TimeSelection.Type.MONTH
        )

        timeSelectionTypes.forEach { type ->
            val session = getSelectedSession()
            var onClearWasCalled = false
            var selectedDate: Date? = null
            val promptRequest = PromptRequest.TimeSelection(
                "title", Date(0),
                null,
                null,
                type,
                { date -> selectedDate = date }) {
                onClearWasCalled = true
            }

            promptFeature.start()
            session.promptRequest = Consumable.from(promptRequest)

            val now = Date()
            promptFeature.onConfirm(session.id, now)

            assertTrue(session.promptRequest.isConsumed())
            assertEquals(now, selectedDate)
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
        promptFeature = PromptFeature(null, null, mockSessionManager, null, mockFragmentManager) { }
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

    @Test
    fun `When dialogs are been abused prompts must not be allow to be displayed`() {
        val session = getSelectedSession()
        var onDismissWasCalled: Boolean
        val onDismiss = { onDismissWasCalled = true }
        val mockAlertRequest = Alert("", "", false, onDismiss, {})
        val mockTextRequest = TextPrompt("", "", "", false, onDismiss) { _, _ -> }
        val mockConfirmRequest =
            PromptRequest.Confirm("", "", false, "", "", "", {}, {}, {}, onDismiss)

        val promptRequest = arrayOf(mockAlertRequest, mockTextRequest, mockConfirmRequest)

        stubContext()
        promptFeature.start()
        promptFeature.promptAbuserDetector.userWantsMoreDialogs(false)

        promptRequest.forEach { _ ->
            onDismissWasCalled = false
            session.promptRequest = Consumable.from(mockAlertRequest)

            verify(mockFragmentManager, never()).beginTransaction()
            assertTrue(onDismissWasCalled)
        }
    }

    @Test
    fun `When dialogs are been abused but the page is refreshed prompts must be allow to be displayed`() {
        val session = getSelectedSession()
        var onDismissWasCalled = false
        val onDismiss = { onDismissWasCalled = true }
        val mockAlertRequest = Alert("", "", false, onDismiss, {})

        stubContext()
        promptFeature.start()
        promptFeature.promptAbuserDetector.userWantsMoreDialogs(false)

        session.promptRequest = Consumable.from(mockAlertRequest)

        verify(mockFragmentManager, never()).beginTransaction()
        assertTrue(onDismissWasCalled)

        session.notifyObservers {
            onLoadingStateChanged(session, false)
        }

        session.promptRequest = Consumable.from(mockAlertRequest)

        verify(mockFragmentManager).beginTransaction()
        assertTrue(promptFeature.promptAbuserDetector.shouldShowMoreDialogs)
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

        doReturn(testContext).`when`(mockFragment).requireContext()

        promptFeature = PromptFeature(mockFragment, mockSessionManager, null, mockFragmentManager) {}
    }
}
