/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.prompt

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.prompt.PromptRequest
import org.mozilla.geckoview.GeckoSession

internal class PromptInstanceDismissDelegate(
    private val geckoSession: GeckoEngineSession,
    private val promptRequest: PromptRequest,
) : GeckoSession.PromptDelegate.PromptInstanceDelegate {

    override fun onPromptDismiss(prompt: GeckoSession.PromptDelegate.BasePrompt) {
        geckoSession.notifyObservers {
            onPromptDismissed(promptRequest)
        }
    }
}
