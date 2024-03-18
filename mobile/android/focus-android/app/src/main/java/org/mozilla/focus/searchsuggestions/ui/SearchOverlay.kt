/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.searchsuggestions.ui

import androidx.appcompat.content.res.AppCompatResources
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.runtime.livedata.observeAsState
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.input.nestedscroll.NestedScrollConnection
import androidx.compose.ui.input.nestedscroll.NestedScrollSource
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.core.graphics.drawable.toBitmap
import kotlinx.coroutines.DelicateCoroutinesApi
import mozilla.components.compose.browser.awesomebar.AwesomeBar
import mozilla.components.compose.browser.awesomebar.AwesomeBarDefaults
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import org.mozilla.focus.R
import org.mozilla.focus.components
import org.mozilla.focus.searchsuggestions.SearchSuggestionsViewModel
import org.mozilla.focus.searchsuggestions.State
import org.mozilla.focus.topsites.TopSitesOverlay
import org.mozilla.focus.ui.theme.focusColors

@OptIn(DelicateCoroutinesApi::class)
@Composable
fun SearchOverlay(
    viewModel: SearchSuggestionsViewModel,
    defaultSearchEngineName: String,
    onListScrolled: () -> Unit,
) {
    val state = viewModel.state.observeAsState()
    val query = viewModel.searchQuery.observeAsState()

    when (state.value) {
        is State.Disabled,
        is State.NoSuggestionsAPI,
        -> {
            if (query.value.isNullOrEmpty()) {
                TopSitesOverlay(modifier = Modifier.background(focusColors.surface))
            }
        }
        is State.ReadyForSuggestions -> {
            if (query.value.isNullOrEmpty()) {
                TopSitesOverlay(modifier = Modifier.background(focusColors.surface))
            } else {
                SearchSuggestions(
                    text = query.value ?: "",
                    onSuggestionClicked = { suggestion ->
                        viewModel.selectSearchSuggestion(
                            suggestion.title!!,
                            defaultSearchEngineName,
                        )
                    },
                    onAutoComplete = { suggestion ->
                        val editSuggestion = suggestion.editSuggestion ?: return@SearchSuggestions
                        viewModel.setAutocompleteSuggestion(editSuggestion)
                    },
                    onListScrolled = onListScrolled,
                )
            }
        }
        else -> {
            // no-op
        }
    }
}

@Composable
private fun SearchSuggestions(
    text: String,
    onSuggestionClicked: (AwesomeBar.Suggestion) -> Unit,
    onAutoComplete: (AwesomeBar.Suggestion) -> Unit,
    onListScrolled: () -> Unit,
) {
    val context = LocalContext.current
    val components = components

    val icon = AppCompatResources.getDrawable(context, R.drawable.mozac_ic_search_24)?.toBitmap()
    val provider = remember(context) {
        SearchSuggestionProvider(
            context,
            components.store,
            components.searchUseCases.newPrivateTabSearch,
            components.client,
            mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
            private = true,
            showDescription = false,
            icon = icon,
        )
    }

    val nestedScrollConnection = remember {
        object : NestedScrollConnection {
            override fun onPreScroll(available: Offset, source: NestedScrollSource): Offset {
                onListScrolled.invoke()
                return Offset.Zero
            }
        }
    }

    Column(
        modifier = Modifier.nestedScroll(nestedScrollConnection),
    ) {
        AwesomeBar(
            text = text,
            colors = AwesomeBarDefaults.colors(
                background = focusColors.surface,
            ),
            providers = listOf(provider),
            onSuggestionClicked = onSuggestionClicked,
            onAutoComplete = onAutoComplete,
        )
    }
}
