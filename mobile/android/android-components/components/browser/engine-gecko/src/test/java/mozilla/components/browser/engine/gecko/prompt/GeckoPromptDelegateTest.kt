/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.prompt

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_MULTIPLE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_SINGLE
import org.robolectric.RobolectricTestRunner

typealias GeckoChoice = GeckoSession.PromptDelegate.Choice

@RunWith(RobolectricTestRunner::class)
class GeckoPromptDelegateTest {

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_SINGLE must provide a SingleChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var promptRequestSingleChoice: PromptRequest = MultipleChoice(arrayOf()) {}
        var confirmWasCalled = false

        val callback = object : GeckoSession.PromptDelegate.ChoiceCallback {
            override fun confirm(id: String?) {}

            override fun confirm(items: Array<out GeckoSession.PromptDelegate.Choice>?) {}

            override fun dismiss() {}

            override fun getCheckboxValue() = false

            override fun setCheckboxValue(value: Boolean) {}

            override fun hasCheckbox() = false

            override fun getCheckboxMessage() = ""

            override fun confirm(ids: Array<out String>?) {}

            override fun confirm(item: GeckoChoice?) {
                confirmWasCalled = true
            }
        }

        val gecko = GeckoPromptDelegate(mockSession)
        val geckoChoices = arrayOf<GeckoChoice>()

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                promptRequestSingleChoice = promptRequest
            }
        })

        gecko.onChoicePrompt(null, null, null, CHOICE_TYPE_SINGLE, geckoChoices, callback)

        assertTrue(promptRequestSingleChoice is SingleChoice)

        (promptRequestSingleChoice as SingleChoice).onSelect(mock())
        assertTrue(confirmWasCalled)
    }

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_MULTIPLE must provide a MultipleChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var promptRequestSingleChoice: PromptRequest = SingleChoice(arrayOf()) {}
        var confirmWasCalled = false

        val callback = object : GeckoSession.PromptDelegate.ChoiceCallback {
            override fun confirm(id: String?) {}

            override fun confirm(items: Array<out GeckoSession.PromptDelegate.Choice>?) {}

            override fun dismiss() {}

            override fun getCheckboxValue() = false

            override fun setCheckboxValue(value: Boolean) {}

            override fun hasCheckbox() = false

            override fun getCheckboxMessage() = ""

            override fun confirm(ids: Array<out String>?) {
                confirmWasCalled = true
            }

            override fun confirm(item: GeckoChoice?) {}
        }

        val gecko = GeckoPromptDelegate(mockSession)
        val geckoChoices = arrayOf<GeckoChoice>()

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                promptRequestSingleChoice = promptRequest
            }
        })

        gecko.onChoicePrompt(null, null, null, CHOICE_TYPE_MULTIPLE, geckoChoices, callback)

        assertTrue(promptRequestSingleChoice is MultipleChoice)

        (promptRequestSingleChoice as MultipleChoice).onSelect(arrayOf())
        assertTrue(confirmWasCalled)
    }

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_MENU must provide a MenuChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var promptRequestSingleChoice: PromptRequest = PromptRequest.MenuChoice(arrayOf()) {}
        var confirmWasCalled = false

        val callback = object : GeckoSession.PromptDelegate.ChoiceCallback {
            override fun confirm(id: String?) = Unit
            override fun confirm(items: Array<out GeckoSession.PromptDelegate.Choice>?) = Unit
            override fun dismiss() = Unit
            override fun getCheckboxValue() = false
            override fun setCheckboxValue(value: Boolean) = Unit
            override fun hasCheckbox() = false
            override fun getCheckboxMessage() = ""
            override fun confirm(ids: Array<out String>?) = Unit

            override fun confirm(item: GeckoChoice?) {
                confirmWasCalled = true
            }
        }

        val gecko = GeckoPromptDelegate(mockSession)
        val geckoChoices = arrayOf<GeckoChoice>()

        mockSession.register(
                object : EngineSession.Observer {
                    override fun onPromptRequest(promptRequest: PromptRequest) {
                        promptRequestSingleChoice = promptRequest
                    }
                })

        gecko.onChoicePrompt(
                null, null, null,
                GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_MENU, geckoChoices, callback
        )

        assertTrue(promptRequestSingleChoice is PromptRequest.MenuChoice)

        (promptRequestSingleChoice as PromptRequest.MenuChoice).onSelect(mock())
        assertTrue(confirmWasCalled)
    }

    @Test
    fun `onAlert must provide an alert PromptRequest`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var alertRequest: PromptRequest? = null
        var dismissWasCalled = false
        var setCheckboxValueWasCalled = false

        val callback = object : GeckoSession.PromptDelegate.AlertCallback {

            override fun setCheckboxValue(value: Boolean) {
                setCheckboxValueWasCalled = true
            }

            override fun dismiss() {
                dismissWasCalled = true
            }

            override fun getCheckboxValue(): Boolean = false
            override fun hasCheckbox(): Boolean = false
            override fun getCheckboxMessage(): String = ""
        }

        val promptDelegate = GeckoPromptDelegate(mockSession)

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                alertRequest = promptRequest
            }
        })

        promptDelegate.onAlert(null, "title", "message", callback)

        assertTrue(alertRequest is PromptRequest.Alert)

        (alertRequest as Alert).onDismiss()
        assertTrue(dismissWasCalled)

        (alertRequest as Alert).onShouldShowNoMoreDialogs(true)
        assertTrue(setCheckboxValueWasCalled)

        assertEquals((alertRequest as Alert).title, "title")
        assertEquals((alertRequest as Alert).message, "message")
    }

    @Test
    fun `hitting functions not yet implemented`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        val gecko = GeckoPromptDelegate(mockSession)
        gecko.onButtonPrompt(null, "", "", null, null)
        gecko.onDateTimePrompt(null, "", 0, null, null, null, null)
        gecko.onFilePrompt(null, "", 0, null, null)
        gecko.onColorPrompt(null, "", "", null)
        gecko.onAuthPrompt(null, "", "", null, null)
        gecko.onTextPrompt(null, "", "", null, null)
        gecko.onPopupRequest(null, "")
    }

    @Test
    fun `toIdsArray must convert an list of choices to array of id strings`() {
        val choices = arrayOf(Choice(id = "0", label = ""), Choice(id = "1", label = ""))
        val ids = choices.toIdsArray()
        ids.forEachIndexed { index, item ->
            assertEquals("$index", item)
        }
    }
}