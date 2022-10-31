/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar.internal

import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.RememberObserver
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import mozilla.components.compose.browser.awesomebar.AwesomeBarColors
import mozilla.components.compose.browser.awesomebar.AwesomeBarOrientation
import mozilla.components.concept.awesomebar.AwesomeBar
import java.util.SortedMap

@Composable
@Suppress("LongParameterList")
internal fun Suggestions(
    suggestions: SortedMap<AwesomeBar.SuggestionProviderGroup, List<AwesomeBar.Suggestion>>,
    colors: AwesomeBarColors,
    orientation: AwesomeBarOrientation,
    onSuggestionClicked: (AwesomeBar.SuggestionProviderGroup, AwesomeBar.Suggestion) -> Unit,
    onAutoComplete: (AwesomeBar.SuggestionProviderGroup, AwesomeBar.Suggestion) -> Unit,
    onScroll: () -> Unit,
) {
    val state = rememberLazyListState()

    ScrollHandler(state, onScroll)

    LazyColumn(
        state = state,
        modifier = Modifier.testTag("mozac.awesomebar.suggestions"),
    ) {
        suggestions.forEach { (group, suggestions) ->
            if (suggestions.isNotEmpty()) {
                group.title?.let { title ->
                    item(group.id) {
                        SuggestionGroup(title, colors)
                    }
                }
            }

            items(
                items = suggestions.take(group.limit),
                key = { suggestion -> "${group.id}:${suggestion.provider.id}:${suggestion.id}" },
            ) { suggestion ->
                Suggestion(
                    suggestion,
                    colors,
                    orientation,
                    onSuggestionClicked = { onSuggestionClicked(group, suggestion) },
                    onAutoComplete = { onAutoComplete(group, suggestion) },
                )
            }
        }
    }
}

/**
 * An effect for handling scrolls in a [LazyColumn]. Will invoke [onScroll] at the beginning
 * of a scroll gesture.
 */
@Composable
private fun ScrollHandler(
    state: LazyListState,
    onScroll: () -> Unit,
) {
    val scrollInProgress = state.isScrollInProgress
    remember(scrollInProgress) {
        ScrollHandlerImpl(scrollInProgress, onScroll)
    }
}

/**
 * [RememberObserver] implementation that will make sure that [onScroll] get called only once as
 * long as [scrollInProgress] doesn't change.
 */
private class ScrollHandlerImpl(
    private val scrollInProgress: Boolean,
    private val onScroll: () -> Unit,
) : RememberObserver {
    override fun onAbandoned() = Unit
    override fun onForgotten() = Unit

    override fun onRemembered() {
        if (scrollInProgress) {
            onScroll()
        }
    }
}
