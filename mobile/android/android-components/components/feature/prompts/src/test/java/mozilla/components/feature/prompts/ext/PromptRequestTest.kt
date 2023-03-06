/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.ext

import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest.Confirm
import mozilla.components.concept.engine.prompt.PromptRequest.Popup
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.TextPrompt
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import kotlin.reflect.KClass

class PromptRequestTest {
    @Test
    fun `GIVEN only a subset of prompts should be shown in fullscreen WHEN checking which are not THEN return the expected result`() {
        val expected = listOf<KClass<out PromptRequest>>(
            Alert::class,
            TextPrompt::class,
            Confirm::class,
            Popup::class,
        )

        assertEquals(expected, PROMPTS_TO_EXIT_FULLSCREEN_FOR)
    }

    @Test
    fun `GIVEN a prompt which should not be shown in fullscreen WHEN trying to execute code such prompts THEN execute the code`() {
        var invocations = 0
        val alert: Alert = mock()
        val text: TextPrompt = mock()
        val confirm: Confirm = mock()
        val popup: Popup = mock()
        val windowedPrompts = listOf(alert, text, confirm, popup)

        windowedPrompts.forEachIndexed { index, prompt ->
            prompt.executeIfWindowedPrompt { invocations++ }
            assertEquals(index + 1, invocations)
        }
    }

    @Test
    fun `GIVEN a prompt which should be shown in fullscreen WHEN trying to execute code for windowed prompts THEN don't do anything`() {
        var invocations = 0
        val choice: SingleChoice = mock()

        choice.executeIfWindowedPrompt { invocations++ }

        assertEquals(0, invocations)
    }
}
