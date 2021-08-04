/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import mozilla.components.concept.awesomebar.AwesomeBar

/**
 * An awesome bar displaying suggestions from the list of provided [AwesomeBar.SuggestionProvider]s.
 */
@Composable
fun AwesomeBar(
    text: String,
    providers: List<AwesomeBar.SuggestionProvider>,
    onSuggestionClicked: (AwesomeBar.Suggestion) -> Unit,
    onAutoComplete: (AwesomeBar.Suggestion) -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(Color.White)
    ) {
        val fetcher = remember { SuggestionFetcher(providers) }

        LaunchedEffect(text) {
            fetcher.fetch(text)
        }

        Suggestions(
            fetcher.state.value,
            onSuggestionClicked,
            onAutoComplete
        )
    }
}
