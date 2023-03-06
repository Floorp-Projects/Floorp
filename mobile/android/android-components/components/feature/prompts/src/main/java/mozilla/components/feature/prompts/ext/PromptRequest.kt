/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.ext

import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest.Confirm
import mozilla.components.concept.engine.prompt.PromptRequest.Popup
import mozilla.components.concept.engine.prompt.PromptRequest.TextPrompt
import kotlin.reflect.KClass

/**
 * List of all prompts who are not to be shown in fullscreen.
 */
@PublishedApi
internal val PROMPTS_TO_EXIT_FULLSCREEN_FOR = listOf<KClass<out PromptRequest>>(
    Alert::class,
    TextPrompt::class,
    Confirm::class,
    Popup::class,
)

/**
 * Convenience method for executing code if the current [PromptRequest] is one that
 * should not be shown in fullscreen tabs.
 */
internal inline fun <reified T> T.executeIfWindowedPrompt(
    block: () -> Unit,
) where T : PromptRequest {
    PROMPTS_TO_EXIT_FULLSCREEN_FOR
        .firstOrNull {
            this::class == it
        }?.let { block.invoke() }
}
