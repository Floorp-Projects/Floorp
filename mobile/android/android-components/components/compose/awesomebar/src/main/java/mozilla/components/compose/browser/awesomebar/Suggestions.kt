/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar

import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.runtime.Composable
import mozilla.components.concept.awesomebar.AwesomeBar

@Composable
internal fun Suggestions(
    suggestions: List<AwesomeBar.Suggestion>,
    onSuggestionClicked: (AwesomeBar.Suggestion) -> Unit
) {
    LazyColumn {
        items(suggestions) { suggestion ->
            Suggestion(suggestion, onSuggestionClicked)
        }
    }
}
