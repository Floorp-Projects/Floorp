/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.prompt

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.browser.engine.gecko.ext.convertToChoices
import mozilla.components.concept.engine.prompt.PromptRequest
import org.mozilla.geckoview.GeckoSession.PromptDelegate.BasePrompt
import org.mozilla.geckoview.GeckoSession.PromptDelegate.ChoicePrompt
import org.mozilla.geckoview.GeckoSession.PromptDelegate.PromptInstanceDelegate

/**
 * Implementation of [PromptInstanceDelegate] used to update a
 * prompt request when onPromptUpdate is invoked.
 *
 * @param geckoSession [GeckoEngineSession] used to notify the engine observer
 * with the onPromptUpdate callback.
 * @param previousPrompt [PromptRequest] to be updated.
 */
internal class ChoicePromptDelegate(
    private val geckoSession: GeckoEngineSession,
    private var previousPrompt: PromptRequest,
) : PromptInstanceDelegate {

    override fun onPromptDismiss(prompt: BasePrompt) {
        geckoSession.notifyObservers {
            onPromptDismissed(previousPrompt)
        }
    }

    override fun onPromptUpdate(prompt: BasePrompt) {
        if (prompt is ChoicePrompt) {
            val promptRequest = updatePromptChoices(prompt)
            if (promptRequest != null) {
                geckoSession.notifyObservers {
                    this.onPromptUpdate(previousPrompt.uid, promptRequest)
                }
                previousPrompt = promptRequest
            }
        }
    }

    /**
     * Use the received prompt to create the updated [PromptRequest]
     * @param updatedPrompt The [ChoicePrompt] with the updated choices.
     */
    private fun updatePromptChoices(updatedPrompt: ChoicePrompt): PromptRequest? {
        return when (previousPrompt) {
            is PromptRequest.MenuChoice -> {
                (previousPrompt as PromptRequest.MenuChoice)
                    .copy(choices = convertToChoices(updatedPrompt.choices))
            }
            is PromptRequest.SingleChoice -> {
                (previousPrompt as PromptRequest.SingleChoice)
                    .copy(choices = convertToChoices(updatedPrompt.choices))
            }
            is PromptRequest.MultipleChoice -> {
                (previousPrompt as PromptRequest.MultipleChoice)
                    .copy(choices = convertToChoices(updatedPrompt.choices))
            }
            else -> null
        }
    }
}
