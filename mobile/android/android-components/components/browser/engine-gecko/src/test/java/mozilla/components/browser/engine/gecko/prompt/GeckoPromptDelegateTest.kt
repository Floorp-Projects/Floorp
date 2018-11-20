/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.prompt

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.support.test.mock
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
}