/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser.browser

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.navigation.NavController
import mozilla.components.browser.state.helper.Target
import mozilla.components.compose.browser.awesomebar.AwesomeBar
import mozilla.components.compose.browser.toolbar.BrowserToolbar
import mozilla.components.compose.engine.WebContent
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.lib.state.ext.composableStore
import org.mozilla.samples.compose.browser.components
import java.util.UUID

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
                AwesomeBar(url, providers = listOf(
                    FakeProvider()
                ))
            }
        }
    }
}

// For testing purposes - to be replaced with actual providers later
@Suppress("MagicNumber")
private class FakeProvider : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()
    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        val suggestions = mutableListOf<AwesomeBar.Suggestion>()

        repeat(10) {
            suggestions.add(
                AwesomeBar.Suggestion(
                this,
                    title = text + UUID.randomUUID().toString()
                )
            )
        }

        return suggestions
    }
}
