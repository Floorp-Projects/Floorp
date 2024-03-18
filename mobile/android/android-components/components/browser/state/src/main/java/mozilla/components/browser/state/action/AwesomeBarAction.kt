/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.concept.awesomebar.AwesomeBar

/**
 * [BrowserAction]s related to interactions with the [AwesomeBar].
 */
sealed class AwesomeBarAction : BrowserAction() {
    /**
     * Indicates that the suggestions displayed in the [AwesomeBar] have changed.
     */
    data class VisibilityStateUpdated(val visibilityState: AwesomeBar.VisibilityState) : AwesomeBarAction()

    /**
     * Indicates that the user clicked a [suggestion] in the [AwesomeBar].
     */
    data class SuggestionClicked(val suggestion: AwesomeBar.Suggestion) : AwesomeBarAction()

    /**
     * Indicates that the user has finished engaging with the [AwesomeBar].
     *
     * An [abandoned] engagement means that the user dismissed the [AwesomeBar] without either clicking on a
     * suggestion, or entering a search term or URL.
     */
    data class EngagementFinished(val abandoned: Boolean) : AwesomeBarAction()
}
