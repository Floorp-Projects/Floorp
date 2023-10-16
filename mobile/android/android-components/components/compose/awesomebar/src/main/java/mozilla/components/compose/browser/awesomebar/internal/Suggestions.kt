/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar.internal

import android.os.Parcelable
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.RememberObserver
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.parcelize.Parcelize
import mozilla.components.compose.browser.awesomebar.AwesomeBarColors
import mozilla.components.compose.browser.awesomebar.AwesomeBarOrientation
import mozilla.components.concept.awesomebar.AwesomeBar

@Composable
@Suppress("LongParameterList")
internal fun Suggestions(
    suggestions: Map<AwesomeBar.SuggestionProviderGroup, List<AwesomeBar.Suggestion>>,
    colors: AwesomeBarColors,
    orientation: AwesomeBarOrientation,
    onSuggestionClicked: (AwesomeBar.SuggestionProviderGroup, AwesomeBar.Suggestion) -> Unit,
    onAutoComplete: (AwesomeBar.SuggestionProviderGroup, AwesomeBar.Suggestion) -> Unit,
    onVisibilityStateUpdated: (AwesomeBar.VisibilityState) -> Unit,
    onScroll: () -> Unit,
) {
    val state = rememberLazyListState()

    ScrollHandler(state, onScroll)

    LazyColumn(
        state = state,
        modifier = Modifier.testTag("mozac.awesomebar.suggestions"),
    ) {
        suggestions.forEach { (group, suggestions) ->
            val title = group.title
            if (suggestions.isNotEmpty() && !title.isNullOrEmpty()) {
                item(ItemKey.SuggestionGroup(group.id)) {
                    SuggestionGroup(title, colors)
                }
            }

            items(
                items = suggestions.take(group.limit),
                key = { suggestion -> ItemKey.Suggestion(group.id, suggestion.provider.id, suggestion.id) },
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

    val currentSuggestions by rememberUpdatedState(suggestions)
    val currentOnVisibilityStateUpdated by rememberUpdatedState(onVisibilityStateUpdated)

    LaunchedEffect(Unit) {
        // This effect is launched on the initial composition, and cancelled when `Suggestions` leaves the Composition.
        // The flow below emits new values when either the latest list of suggestions changes, or the visibility or
        // position of any suggestion in that list changes.
        snapshotFlow { currentSuggestions }
            .combine(snapshotFlow { state.layoutInfo.visibleItemsInfo }) { suggestions, visibleItemsInfo ->
                VisibleItems(
                    suggestions = suggestions,
                    visibleItemKeys = visibleItemsInfo.map { it.key as ItemKey },
                )
            }
            .distinctUntilChangedBy { it.visibleItemKeys }
            .collect { currentOnVisibilityStateUpdated(it.toVisibilityState()) }
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

/**
 * A stable, unique key for an item in the [Suggestions] list.
 */
internal sealed interface ItemKey {
    @Parcelize
    data class SuggestionGroup(val id: String) : ItemKey, Parcelable

    @Parcelize
    data class Suggestion(
        val groupId: String,
        val providerId: String,
        val suggestionId: String,
    ) : ItemKey, Parcelable
}

/**
 * A snapshot of all the fetched suggestions to show in the [Suggestions] list, and the keys of the visible items
 * in that list, ordered top to bottom. The intersection of the two is the current [AwesomeBar.VisibilityState].
 */
internal data class VisibleItems(
    val suggestions: Map<AwesomeBar.SuggestionProviderGroup, List<AwesomeBar.Suggestion>>,
    val visibleItemKeys: List<ItemKey>,
) {
    fun toVisibilityState(): AwesomeBar.VisibilityState =
        if (visibleItemKeys.isEmpty()) {
            AwesomeBar.VisibilityState()
        } else {
            AwesomeBar.VisibilityState(
                // `suggestions` is insertion-ordered, and `toMap()` preserves that order, so the groups in
                // `visibleProviderGroups` are in the same order as they're shown in the awesomebar.
                visibleProviderGroups = suggestions.mapNotNull { (group, suggestions) ->
                    val visibleSuggestions = suggestions.filter { suggestion ->
                        val suggestionItemKey = ItemKey.Suggestion(
                            groupId = group.id,
                            providerId = suggestion.provider.id,
                            suggestionId = suggestion.id,
                        )
                        visibleItemKeys.contains(suggestionItemKey)
                    }
                    if (visibleSuggestions.isNotEmpty()) {
                        group to visibleSuggestions
                    } else {
                        null
                    }
                }.toMap(),
            )
        }
}
