/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser.browser

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalContext
import androidx.navigation.NavController
import mozilla.components.browser.state.helper.Target
import mozilla.components.compose.browser.awesomebar.AwesomeBar
import mozilla.components.compose.browser.toolbar.BrowserToolbar
import mozilla.components.compose.engine.WebContent
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.awesomebar.provider.ClipboardSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchActionProvider
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SessionSuggestionProvider
import mozilla.components.lib.state.ext.composableStore
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.samples.compose.browser.components

/**
 * The main browser screen.
 */
@Composable
fun BrowserScreen(navController: NavController) {
    val target = Target.SelectedTab

    val store = composableStore<BrowserScreenState, BrowserScreenAction> { restoredState ->
        BrowserScreenStore(restoredState ?: BrowserScreenState())
    }

    val editState = store.observeAsComposableState { state -> state.editMode }
    val editUrl = store.observeAsComposableState { state -> state.editText }
    val loadUrl = components().sessionUseCases.loadUrl

    Column {
        BrowserToolbar(
            components().store,
            target,
            editMode = editState.value!!,
            onDisplayMenuClicked = { navController.navigate("settings") },
            onTextCommit = { text ->
                store.dispatch(BrowserScreenAction.ToggleEditMode(false))
                loadUrl(text)
            },
            onTextEdit = { text -> store.dispatch(BrowserScreenAction.UpdateEditText(text)) },
            onDisplayToolbarClick = {
                store.dispatch(BrowserScreenAction.ToggleEditMode(true))
            },
            editText = editUrl.value,
            hint = "Search or enter address"
        )

        Box {
            WebContent(
                components().engine,
                components().store,
                Target.SelectedTab
            )

            val url = editUrl.value
            if (editState.value == true && url != null) {
                Suggestions(
                    url,
                    onSuggestionClicked = { suggestion ->
                        store.dispatch(BrowserScreenAction.ToggleEditMode(false))
                        suggestion.onSuggestionClicked?.invoke()
                    }
                )
            }
        }
    }
}

@Composable
private fun Suggestions(
    url: String,
    onSuggestionClicked: (AwesomeBar.Suggestion) -> Unit
) {
    val context = LocalContext.current
    val components = components()

    val sessionSuggestionProvider = remember(context) {
        SessionSuggestionProvider(
            context.resources,
            components.store,
            components.tabsUseCases.selectTab
        )
    }

    val searchActionProvider = remember {
        SearchActionProvider(components.store, components.searchUseCases.defaultSearch)
    }

    val searchSuggestionProvider = remember(context) {
        SearchSuggestionProvider(
            context,
            components.store,
            components.searchUseCases.defaultSearch,
            components.client,
            mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
            engine = components.engine,
            filterExactMatch = true
        )
    }

    val clipboardSuggestionProvider = remember(context) {
        ClipboardSuggestionProvider(
            context,
            components.sessionUseCases.loadUrl
        )
    }

    AwesomeBar(
        url,
        providers = listOf(
            sessionSuggestionProvider,
            searchActionProvider,
            searchSuggestionProvider,
            clipboardSuggestionProvider
        ),
        onSuggestionClicked = { suggestion -> onSuggestionClicked(suggestion) }
    )
}
