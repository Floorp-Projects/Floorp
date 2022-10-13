/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.browser.engine.gecko.prompt

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.support.test.mock
import mozilla.components.test.ReflectionUtils
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoSession

@RunWith(AndroidJUnit4::class)
class ChoicePromptDelegateTest {

    @Test
    fun `WHEN onPromptUpdate is called from GeckoView THEN notifyObservers is invoked with onPromptUpdate`() {
        val mockSession = GeckoEngineSession(mock())
        var isOnPromptUpdateCalled = false
        var isOnConfirmCalled = false
        var isOnDismissCalled = false
        var observedPrompt: PromptRequest? = null
        var observedUID: String? = null
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptUpdate(
                    previousPromptRequestUid: String,
                    promptRequest: PromptRequest,
                ) {
                    observedPrompt = promptRequest
                    observedUID = previousPromptRequestUid
                    isOnPromptUpdateCalled = true
                }
            },
        )
        val prompt = PromptRequest.SingleChoice(
            arrayOf(),
            { isOnConfirmCalled = true },
            { isOnDismissCalled = true },
        )
        val delegate = ChoicePromptDelegate(mockSession, prompt)
        val updatedPrompt = mock<GeckoSession.PromptDelegate.ChoicePrompt>()
        ReflectionUtils.setField(updatedPrompt, "choices", arrayOf<GeckoChoice>())

        delegate.onPromptUpdate(updatedPrompt)

        assertTrue(isOnPromptUpdateCalled)
        assertEquals(prompt.uid, observedUID)
        // Verify if the onConfirm and onDismiss callbacks were changed
        (observedPrompt as PromptRequest.SingleChoice).onConfirm(mock())
        (observedPrompt as PromptRequest.SingleChoice).onDismiss()
        assertTrue(isOnDismissCalled)
        assertTrue(isOnConfirmCalled)
    }

    @Test
    fun `WHEN onPromptDismiss is called from GeckoView THEN notifyObservers is invoked with onPromptDismissed`() {
        val mockSession = GeckoEngineSession(mock())
        var isOnDismissCalled = false
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptDismissed(promptRequest: PromptRequest) {
                    super.onPromptDismissed(promptRequest)
                    isOnDismissCalled = true
                }
            },
        )
        val basePrompt: GeckoSession.PromptDelegate.ChoicePrompt = mock()
        val prompt: PromptRequest = mock()
        val delegate = ChoicePromptDelegate(mockSession, prompt)

        delegate.onPromptDismiss(basePrompt)

        assertTrue(isOnDismissCalled)
    }
}
