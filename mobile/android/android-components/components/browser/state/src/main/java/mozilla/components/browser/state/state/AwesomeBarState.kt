/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.awesomebar.AwesomeBar

/**
 * State for interactions with the [AwesomeBar].
 *
 * @property visibilityState The suggestions and groups that are currently displayed in the [AwesomeBar].
 * @property clickedSuggestion The [AwesomeBar.Suggestion] that the user clicked. This is `null` if the user is still
 * interacting with the [AwesomeBar], or entered a search term or URL instead of clicking on a suggestion.
 */
data class AwesomeBarState(
    val visibilityState: AwesomeBar.VisibilityState = AwesomeBar.VisibilityState(),
    val clickedSuggestion: AwesomeBar.Suggestion? = null,
)
