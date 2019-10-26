/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

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
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.SystemAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
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
import mozilla.components.feature.prompts.FilePicker.Companion.FILE_PICKER_ACTIVITY_REQUEST_CODE
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.security.InvalidParameterException
import java.util.Date

@RunWith(AndroidJUnit4::class)
class PromptFeatureTest {

    private val testDispatcher = TestCoroutineDispatcher()

    private lateinit var store: BrowserStore
    private lateinit var fragmentManager: FragmentManager

    private val tabId = "test-tab"
    private fun tab(): TabSessionState? {
        return store.state.tabs.find { it.id == tabId }
    }

    @Before
    @ExperimentalCoroutinesApi
    fun setUp() {
        Dispatchers.setMain(testDispatcher)
        store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = tabId)
            ),
            customTabs = listOf(
                createCustomTab("https://www.mozilla.org", id = "custom-tab")
            ),
            selectedTabId = tabId
        ))
        fragmentManager = mockFragmentManager()
    }

    @After
    @ExperimentalCoroutinesApi
    fun tearDown() {
        Dispatchers.resetMain()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `PromptFeature acts on the selected session by default`() {
        val feature = spy(PromptFeature(fragment = mock(), store = store, fragmentManager = fragmentManager) { })
        feature.start()

        val promptRequest = SingleChoice(arrayOf()) {}
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        testDispatcher.advanceUntilIdle()
        verify(feature).onPromptRequested(store.state.tabs.first())
    }

    @Test
    fun `PromptFeature acts on a given custom tab session`() {
        val feature = spy(
            PromptFeature(
                fragment = mock(),
                store = store,
                customTabId = "custom-tab",
                fragmentManager = fragmentManager) { }
        )
        feature.start()

        val promptRequest = SingleChoice(arrayOf()) {}
        store.dispatch(ContentAction.UpdatePromptRequestAction("custom-tab", promptRequest)).joinBlocking()
        testDispatcher.advanceUntilIdle()
        verify(feature).onPromptRequested(store.state.customTabs.first())
    }

    @Test
    fun `New promptRequests for selected session will cause fragment transaction`() {
        val feature = PromptFeature(fragment = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()

        val singleChoiceRequest = SingleChoice(arrayOf()) {}
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, singleChoiceRequest)).joinBlocking()
        verify(fragmentManager).beginTransaction()
    }

    @Test
    fun `New promptRequests for selected session will not cause fragment transaction if feature is stopped`() {
        val feature = PromptFeature(fragment = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()
        feature.stop()

        val singleChoiceRequest = SingleChoice(arrayOf()) {}
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, singleChoiceRequest)).joinBlocking()
        verify(fragmentManager, never()).beginTransaction()
    }

    @Test
    fun `Feature will re-attach to already existing fragment`() {
        val fragment: ChoiceDialogFragment = mock()
        doReturn(tabId).`when`(fragment).sessionId
        doReturn(fragment).`when`(fragmentManager).findFragmentByTag(FRAGMENT_TAG)

        val singleChoiceRequest = SingleChoice(arrayOf()) {}
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, singleChoiceRequest)).joinBlocking()

        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()
        verify(fragment).feature = feature
    }

    @Test
    fun `Existing fragment will be removed if session has no prompt request`() {
        val fragment: ChoiceDialogFragment = mock()
        doReturn(tabId).`when`(fragment).sessionId
        doReturn(fragment).`when`(fragmentManager).findFragmentByTag(FRAGMENT_TAG)

        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(any())

        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()

        verify(fragment, never()).feature = feature
        verify(fragmentManager).beginTransaction()
        verify(transaction).remove(fragment)
    }

    @Test
    fun `Existing fragment will be removed if session does not exist anymore`() {
        val fragment: ChoiceDialogFragment = mock()
        doReturn("invalid-tab").`when`(fragment).sessionId
        doReturn(fragment).`when`(fragmentManager).findFragmentByTag(FRAGMENT_TAG)

        val singleChoiceRequest = SingleChoice(arrayOf()) {}
        store.dispatch(ContentAction.UpdatePromptRequestAction("invalid-tab", singleChoiceRequest)).joinBlocking()

        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(any())

        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()

        verify(fragment, never()).feature = feature
        verify(fragmentManager).beginTransaction()
        verify(transaction).remove(fragment)
    }

    @Test
    fun `Calling onCancel will consume promptRequest`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }

        val singleChoiceRequest = SingleChoice(arrayOf()) {}
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, singleChoiceRequest)).joinBlocking()

        assertEquals(singleChoiceRequest, tab()?.content?.promptRequest)
        feature.onCancel(tabId)

        processActions()
        assertNull(tab()?.content?.promptRequest)
    }

    @Test
    fun `Selecting an item in a single choice dialog will consume promptRequest`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()

        val singleChoiceRequest = SingleChoice(arrayOf()) {}
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, singleChoiceRequest)).joinBlocking()

        assertEquals(singleChoiceRequest, tab()?.content?.promptRequest)
        feature.onConfirm(tabId, mock<Choice>())

        processActions()
        assertNull(tab()?.content?.promptRequest)
    }

    @Test
    fun `Selecting an item in a menu choice dialog will consume promptRequest`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()

        val menuChoiceRequest = MenuChoice(arrayOf()) {}
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, menuChoiceRequest)).joinBlocking()

        assertEquals(menuChoiceRequest, tab()?.content?.promptRequest)
        feature.onConfirm(tabId, mock<Choice>())

        processActions()
        assertNull(tab()?.content?.promptRequest)
    }

    @Test
    fun `Selecting items on multiple choice dialog will consume promptRequest`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()

        val multipleChoiceRequest = MultipleChoice(arrayOf()) {}
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, multipleChoiceRequest)).joinBlocking()

        assertEquals(multipleChoiceRequest, tab()?.content?.promptRequest)
        feature.onConfirm(tabId, arrayOf<Choice>())

        processActions()
        assertNull(tab()?.content?.promptRequest)
    }

    @Test
    fun `onNoMoreDialogsChecked will consume promptRequest`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }

        var onShowNoMoreAlertsWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = Alert("title", "message", false, { onDismissWasCalled = true }) {
            onShowNoMoreAlertsWasCalled = true
        }

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onConfirm(tabId, false)
        processActions()
        assertNull(tab()?.content?.promptRequest)
        assertTrue(onShowNoMoreAlertsWasCalled)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onCancel(tabId)
        processActions()
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onCancel with an alert request will consume promptRequest and call onDismiss`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        var onDismissWasCalled = false
        val promptRequest = Alert("title", "message", false, { onDismissWasCalled = true }) {}

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onCancel(tabId)
        processActions()
        assertTrue(onDismissWasCalled)
        assertNull(tab()?.content?.promptRequest)
    }

    @Test
    fun `onConfirmTextPrompt will consume promptRequest`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
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

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onConfirm(tabId, false to "")
        processActions()
        assertNull(tab()?.content?.promptRequest)
        assertTrue(onConfirmWasCalled)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onCancel(tabId)
        processActions()
        assertNull(tab()?.content?.promptRequest)
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onCancel with an TextPrompt request will consume promptRequest and call onDismiss`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        var onDismissWasCalled = false

        val promptRequest = TextPrompt(
            "title",
            "message",
            "value",
            false,
            { onDismissWasCalled = true }) { _, _ -> }

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onCancel(tabId)
        processActions()
        assertNull(tab()?.content?.promptRequest)
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
            val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
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

            feature.start()
            store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

            val now = Date()
            feature.onConfirm(tabId, now)
            processActions()

            assertNull(tab()?.content?.promptRequest)
            assertEquals(now, selectedDate)
            store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

            feature.onClear(tabId)
            assertTrue(onClearWasCalled)
        }
    }

    @Test(expected = InvalidParameterException::class)
    fun `calling handleDialogsRequest with invalid type will throw an exception`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.handleDialogsRequest(mock<PromptRequest.File>(), mock())
    }

    @Suppress("Deprecation")
    @Test(expected = IllegalStateException::class)
    fun `Initializing a PromptFeature without giving an activity or fragment reference will throw an exception`() {
        PromptFeature(null, null, store, null, fragmentManager) { }
    }

    @Test
    fun `onActivityResult with RESULT_OK and isMultipleFilesSelection false will consume PromptRequest`() {
        var onSingleFileSelectionWasCalled = false

        val onSingleFileSelection: (Context, Uri) -> Unit = { _, _ ->
            onSingleFileSelectionWasCalled = true
        }

        val filePickerRequest = PromptRequest.File(emptyArray(), false, onSingleFileSelection, { _, _ -> }) { }

        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        val intent = Intent()

        intent.data = mock()
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, filePickerRequest)).joinBlocking()

        feature.onActivityResult(FILE_PICKER_ACTIVITY_REQUEST_CODE, RESULT_OK, intent)
        processActions()
        assertTrue(onSingleFileSelectionWasCalled)
        assertNull(tab()?.content?.promptRequest)
    }

    @Test
    fun `onActivityResult with RESULT_OK and isMultipleFilesSelection true will consume PromptRequest of the actual session`() {
        var onMultipleFileSelectionWasCalled = false

        val onMultipleFileSelection: (Context, Array<Uri>) -> Unit = { _, _ ->
            onMultipleFileSelectionWasCalled = true
        }

        val filePickerRequest =
            PromptRequest.File(emptyArray(), true, { _, _ -> }, onMultipleFileSelection) {}

        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        val intent = Intent()

        intent.clipData = mock()
        val item = mock<ClipData.Item>()

        doReturn(mock<Uri>()).`when`(item).uri

        intent.clipData?.apply {
            doReturn(1).`when`(this).itemCount
            doReturn(item).`when`(this).getItemAt(0)
        }

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, filePickerRequest)).joinBlocking()

        feature.onActivityResult(FILE_PICKER_ACTIVITY_REQUEST_CODE, RESULT_OK, intent)
        processActions()
        assertTrue(onMultipleFileSelectionWasCalled)
        assertNull(tab()?.content?.promptRequest)
    }

    @Test
    fun `onActivityResult with RESULT_CANCELED will consume PromptRequest call onDismiss `() {
        var onDismissWasCalled = false

        val filePickerRequest =
            PromptRequest.File(emptyArray(), true, { _, _ -> }, { _, _ -> }) {
                onDismissWasCalled = true
            }

        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        val intent = Intent()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, filePickerRequest)).joinBlocking()

        feature.onActivityResult(FILE_PICKER_ACTIVITY_REQUEST_CODE, RESULT_CANCELED, intent)
        processActions()
        assertTrue(onDismissWasCalled)
        assertNull(tab()?.content?.promptRequest)
    }

    @Test
    fun `Calling onConfirmAuthentication will consume promptRequest`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }

        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = Authentication(
            title = "title",
            message = "message",
            userName = "username",
            password = "password",
            method = HOST,
            level = NONE,
            onlyShowPassword = false,
            previousFailed = false,
            isCrossOrigin = false,
            onConfirm = { _, _ -> onConfirmWasCalled = true },
            onDismiss = { onDismissWasCalled = true }
        )

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onConfirm(tabId, "" to "")
        processActions()
        assertNull(tab()?.content?.promptRequest)
        assertTrue(onConfirmWasCalled)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onCancel(tabId)
        processActions()
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onCancel on a authentication request will consume promptRequest and call onDismiss`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        var onDismissWasCalled = false

        val promptRequest = Authentication(
            title = "title",
            message = "message",
            userName = "username",
            password = "password",
            method = HOST,
            level = NONE,
            onlyShowPassword = false,
            previousFailed = false,
            isCrossOrigin = false,
            onConfirm = { _, _ -> },
            onDismiss = { onDismissWasCalled = true }
        )

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onCancel(tabId)
        processActions()
        assertNull(tab()?.content?.promptRequest)
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onConfirm on a color request will consume promptRequest`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }

        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = Color(
            "#e66465",
            {
                onConfirmWasCalled = true
            }) {
            onDismissWasCalled = true
        }

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onConfirm(tabId, "#f6b73c")
        processActions()
        assertNull(tab()?.content?.promptRequest)
        assertTrue(onConfirmWasCalled)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onCancel(tabId)
        processActions()
        assertNull(tab()?.content?.promptRequest)
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onConfirm on a popup request will consume promptRequest`() {
        val fragment: Fragment = mock()
        val context: Context = mock()
        whenever(context.getString(R.string.mozac_feature_prompts_popup_dialog_title)).thenReturn("")
        whenever(context.getString(R.string.mozac_feature_prompts_allow)).thenReturn("")
        whenever(context.getString(R.string.mozac_feature_prompts_deny)).thenReturn("")
        whenever(fragment.requireContext()).thenReturn(context)

        val feature = PromptFeature(fragment = fragment, store = store, fragmentManager = fragmentManager) { }
        var onConfirmWasCalled = false

        val promptRequest = PromptRequest.Popup(
            "http://www.popuptest.com/",
            { onConfirmWasCalled = true }
        ) {}

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onConfirm(tabId, null)
        processActions()
        assertNull(tab()?.content?.promptRequest)
        assertTrue(onConfirmWasCalled)
    }

    @Test
    fun `Calling onCancel on a popup request will consume promptRequest`() {
        val fragment: Fragment = mock()
        val context: Context = mock()
        whenever(context.getString(R.string.mozac_feature_prompts_popup_dialog_title)).thenReturn("")
        whenever(context.getString(R.string.mozac_feature_prompts_allow)).thenReturn("")
        whenever(context.getString(R.string.mozac_feature_prompts_deny)).thenReturn("")
        whenever(fragment.requireContext()).thenReturn(context)

        val feature = PromptFeature(fragment = fragment, store = store, fragmentManager = fragmentManager) { }
        var onCancelWasCalled = false

        val promptRequest = PromptRequest.Popup("http://www.popuptest.com/", { }) {
            onCancelWasCalled = true
        }

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onCancel(tabId)
        processActions()
        assertNull(tab()?.content?.promptRequest)
        assertTrue(onCancelWasCalled)
    }

    @Test
    fun `Calling onConfirm on a confirm request will consume promptRequest`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
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

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onConfirm(tabId, true to MultiButtonDialogFragment.ButtonType.POSITIVE)
        processActions()
        assertNull(tab()?.content?.promptRequest)
        assertTrue(onPositiveButtonWasCalled)

        feature.promptAbuserDetector.resetJSAlertAbuseState()
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onConfirm(tabId, true to MultiButtonDialogFragment.ButtonType.NEGATIVE)
        processActions()
        assertNull(tab()?.content?.promptRequest)
        assertTrue(onNegativeButtonWasCalled)

        feature.promptAbuserDetector.resetJSAlertAbuseState()
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onConfirm(tabId, true to MultiButtonDialogFragment.ButtonType.NEUTRAL)
        processActions()
        assertNull(tab()?.content?.promptRequest)
        assertTrue(onNeutralButtonWasCalled)
    }

    @Test
    fun `Calling onCancel on a confirm request will consume promptRequest`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
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

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onCancel(tabId)
        processActions()
        assertNull(tab()?.content?.promptRequest)
        assertTrue(onCancelWasCalled)
    }

    @Test
    fun `When dialogs are being abused prompts are not allowed`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        var onDismissWasCalled: Boolean
        val onDismiss = { onDismissWasCalled = true }
        val alertRequest = Alert("", "", false, onDismiss, {})
        val textRequest = TextPrompt("", "", "", false, onDismiss) { _, _ -> }
        val confirmRequest =
            PromptRequest.Confirm("", "", false, "+", "-", "", {}, {}, {}, onDismiss)

        val promptRequests = arrayOf<PromptRequest>(alertRequest, textRequest, confirmRequest)

        feature.start()
        feature.promptAbuserDetector.userWantsMoreDialogs(false)

        promptRequests.forEach { request ->
            onDismissWasCalled = false
            store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, request)).joinBlocking()
            verify(fragmentManager, never()).beginTransaction()
            assertTrue(onDismissWasCalled)
        }
    }

    @Test
    fun `When dialogs are being abused but the page is refreshed prompts are allowed`() {
        val feature = PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        var onDismissWasCalled = false
        val onDismiss = { onDismissWasCalled = true }
        val alertRequest = Alert("", "", false, onDismiss, {})

        feature.start()
        feature.promptAbuserDetector.userWantsMoreDialogs(false)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, alertRequest)).joinBlocking()

        verify(fragmentManager, never()).beginTransaction()
        assertTrue(onDismissWasCalled)

        // Simulate reloading page
        store.dispatch(ContentAction.UpdateLoadingStateAction(tabId, true)).joinBlocking()
        store.dispatch(ContentAction.UpdateLoadingStateAction(tabId, false)).joinBlocking()
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, alertRequest)).joinBlocking()

        assertTrue(feature.promptAbuserDetector.shouldShowMoreDialogs)
        verify(fragmentManager).beginTransaction()
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()
        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(any())
        return fragmentManager
    }

    private fun processActions() {
        // Dispatching a random unrelated action to block on it in order to wait for all other
        // dispatched actions to have completed: https://github.com/mozilla-mobile/android-components/issues/4529
        store.dispatch(SystemAction.LowMemoryAction).joinBlocking()
    }
}
