/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.prompt

/**
 * Value type that represents a request for showing a native dialog for prompt web content.
 *
 */
sealed class PromptRequest {
    /**
     * Value type that represents a request for a single choice prompt.
     * @property choices All the possible options.
     * @property onSelect A callback indicating which option was selected.
     */
    data class SingleChoice(val choices: Array<Choice>, val onSelect: (Choice) -> Unit) : PromptRequest()

    /**
     * Value type that represents a request for a multiple choice prompt.
     * @property choices All the possible options.
     * @property onSelect A callback indicating witch options has been selected.
     */
    data class MultipleChoice(val choices: Array<Choice>, val onSelect: (Array<Choice>) -> Unit) : PromptRequest()

    /**
     * Value type that represents a request for a menu choice prompt.
     * @property choices All the possible options.
     * @property onSelect A callback indicating which option was selected.
     */
    data class MenuChoice(val choices: Array<Choice>, val onSelect: (Choice) -> Unit) : PromptRequest()
}
