/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.browser.engine.gecko.prompt

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.support.test.mock
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.Autocomplete
import org.mozilla.geckoview.GeckoSession

@RunWith(AndroidJUnit4::class)
class PromptInstanceDismissDelegateTest {

    @Test
    fun `GIVEN delegate with promptRequest WHEN onPromptDismiss called from geckoview THEN notifyObservers the prompt is dismissed`() {
        val mockSession = GeckoEngineSession(mock())
        var onDismissWasCalled = false
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptDismissed(promptRequest: PromptRequest) {
                    super.onPromptDismissed(promptRequest)
                    onDismissWasCalled = true
                }
            },
        )
        val basePrompt: GeckoSession.PromptDelegate.AutocompleteRequest<Autocomplete.LoginSaveOption> = mock()
        val prompt: PromptRequest = mock()
        val delegate = PromptInstanceDismissDelegate(mockSession, prompt)

        delegate.onPromptDismiss(basePrompt)

        assertTrue(onDismissWasCalled)
    }
}
