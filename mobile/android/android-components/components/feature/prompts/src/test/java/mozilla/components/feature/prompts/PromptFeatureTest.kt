/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.support.v4.app.FragmentManager
import android.support.v4.app.FragmentTransaction
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
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
        promptFeature = PromptFeature(mockSessionManager, mockFragmentManager)
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

        promptFeature = PromptFeature(mockSessionManager, mockFragmentManager)

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

        val feature = PromptFeature(mockSessionManager, fragmentManager)

        feature.start()
        verify(fragmentManager).beginTransaction()
        verify(transaction).remove(fragment)
    }

    fun `Already existing fragment will be removed if session does not exist anymore`() {
        val sessionManager = SessionManager(mock())
        val fragment: ChoiceDialogFragment = mock()
        doReturn(UUID.randomUUID().toString()).`when`(fragment).sessionId

        val transaction: FragmentTransaction = mock()
        val fragmentManager: FragmentManager = mock()

        doReturn(fragment).`when`(fragmentManager).findFragmentByTag(any())
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(fragment)

        val feature = PromptFeature(sessionManager, fragmentManager)

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

        promptFeature.onSingleChoiceSelect(session.id, mock())

        assertTrue(session.promptRequest.isConsumed())
    }

    @Test
    fun `Unknown session will no consume pending promptRequest`() {
        val session = getSelectedSession()
        val singleChoiceRequest = SingleChoice(arrayOf()) {}

        promptFeature.start()

        session.promptRequest = Consumable.from(singleChoiceRequest)

        promptFeature.onSingleChoiceSelect("unknown_session", mock())

        assertFalse(session.promptRequest.isConsumed())

        promptFeature.onMultipleChoiceSelect("unknown_session", arrayOf())

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

        promptFeature.onSingleChoiceSelect(session.id, mock())

        assertTrue(session.promptRequest.isConsumed())
    }

    @Test
    fun `Selecting items o multiple choice dialog will consume promptRequest`() {
        val session = getSelectedSession()
        val multipleChoiceRequest = MultipleChoice(arrayOf()) {}

        promptFeature.start()

        session.promptRequest = Consumable.from(multipleChoiceRequest)

        promptFeature.onMultipleChoiceSelect(session.id, arrayOf())

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

        promptFeature.onShouldMoreDialogsChecked(session.id, false)

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

        promptFeature.onShouldMoreDialogsChecked("unknown_session_id", false)

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
}