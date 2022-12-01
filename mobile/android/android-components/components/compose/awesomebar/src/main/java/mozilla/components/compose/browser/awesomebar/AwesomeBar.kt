/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import mozilla.components.compose.browser.awesomebar.internal.SuggestionFetcher
import mozilla.components.compose.browser.awesomebar.internal.Suggestions
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.base.profiler.Profiler

/**
 * An awesome bar displaying suggestions from the list of provided [AwesomeBar.SuggestionProvider]s.
 *
 * @param text The text entered by the user and for which the AwesomeBar should show suggestions for.
 * @param colors The color scheme the AwesomeBar will use for the UI.
 * @param providers The list of suggestion providers to query whenever the [text] changes.
 * @param orientation Whether the AwesomeBar is oriented to the top or the bottom of the screen.
 * @param onSuggestionClicked Gets invoked whenever the user clicks on a suggestion in the AwesomeBar.
 * @param onAutoComplete Gets invoked when the user clicks on the "autocomplete" icon of a suggestion.
 * @param onScroll Gets invoked at the beginning of the user performing a scroll gesture.
 */
@Composable
fun AwesomeBar(
    text: String,
    colors: AwesomeBarColors = AwesomeBarDefaults.colors(),
    providers: List<AwesomeBar.SuggestionProvider>,
    orientation: AwesomeBarOrientation = AwesomeBarOrientation.TOP,
    onSuggestionClicked: (AwesomeBar.Suggestion) -> Unit,
    onAutoComplete: (AwesomeBar.Suggestion) -> Unit,
    onScroll: () -> Unit = {},
    profiler: Profiler? = null,
) {
    val groups = remember(providers) {
        providers
            .groupBy { it.groupTitle() }
            .map {
                AwesomeBar.SuggestionProviderGroup(
                    providers = it.value,
                    title = it.key,
                )
            }
    }

    AwesomeBar(
        text = text,
        colors = colors,
        groups = groups,
        orientation = orientation,
        onSuggestionClicked = { _, suggestion -> onSuggestionClicked(suggestion) },
        onAutoComplete = { _, suggestion -> onAutoComplete(suggestion) },
        onScroll = onScroll,
        profiler = profiler,
    )
}

/**
 * An awesome bar displaying suggestions in groups from the list of provided [AwesomeBar.SuggestionProviderGroup]s.
 *
 * @param text The text entered by the user and for which the AwesomeBar should show suggestions for.
 * @param colors The color scheme the AwesomeBar will use for the UI.
 * @param groups The list of groups of suggestion providers to query whenever the [text] changes.
 * @param orientation Whether the AwesomeBar is oriented to the top or the bottom of the screen.
 * @param onSuggestionClicked Gets invoked whenever the user clicks on a suggestion in the AwesomeBar.
 * @param onAutoComplete Gets invoked when the user clicks on the "autocomplete" icon of a suggestion.
 * @param onScroll Gets invoked at the beginning of the user performing a scroll gesture.
 */
@Composable
fun AwesomeBar(
    text: String,
    colors: AwesomeBarColors = AwesomeBarDefaults.colors(),
    groups: List<AwesomeBar.SuggestionProviderGroup>,
    orientation: AwesomeBarOrientation = AwesomeBarOrientation.TOP,
    onSuggestionClicked: (AwesomeBar.SuggestionProviderGroup, AwesomeBar.Suggestion) -> Unit,
    onAutoComplete: (AwesomeBar.SuggestionProviderGroup, AwesomeBar.Suggestion) -> Unit,
    onScroll: () -> Unit = {},
    profiler: Profiler? = null,
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .testTag("mozac.awesomebar")
            .background(colors.background),
    ) {
        val fetcher = remember(groups) { SuggestionFetcher(groups, profiler) }
        val suggestions = derivedStateOf { fetcher.state.value }.value.toSortedMap(compareBy { it.title })

        LaunchedEffect(text, fetcher) {
            fetcher.fetch(text)
        }

        Suggestions(
            suggestions,
            colors,
            orientation,
            onSuggestionClicked,
            onAutoComplete,
            onScroll,
        )
    }
}
