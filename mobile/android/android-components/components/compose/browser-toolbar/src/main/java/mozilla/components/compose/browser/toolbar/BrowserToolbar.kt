/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import mozilla.components.browser.state.helper.Target
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore

/**
 * A customizable toolbar for browsers.
 *
 * The toolbar can switch between two modes: display and edit. The display mode displays the current
 * URL and controls for navigation. In edit mode the current URL can be edited. Those two modes are
 * implemented by the [BrowserDisplayToolbar] and [BrowserEditToolbar] composables.
 *
 * @param store The store to observe the [target] from.
 * @param target The target tab to observe.
 * @param onDisplayMenuClicked Function to get executed when the user clicks on the menu button in
 * "display" mode.
 * @param onTextEdit Function to get executed whenever the user edits the text in the toolbar in
 * "edit" mode.
 * @param onTextCommit Function to get executed when the user has finished editing the URL and wants
 * to load the entered text.
 * @param onDisplayToolbarClick Function to get executed when the user clicks on the URL in "display"
 * mode.
 * @param hint Text displayed in the toolbar when there's no URL to display (no tab or empty URL)
 * @param editMode Whether the toolbar is in "edit" or "display" mode.
 * @param editText The text the user is editing in "edit" mode.
 * @param browserActions Additional browser actions to be displayed on the right side of the toolbar
 * (outside of the URL bounding box) in display mode. Also see:
 * [MDN docs](https://developer.mozilla.org/en-US/Add-ons/WebExtensions/user_interface/Browser_action)
 */
@Composable
@Suppress("LongParameterList")
fun BrowserToolbar(
    store: BrowserStore,
    target: Target,
    onDisplayMenuClicked: () -> Unit,
    onTextEdit: (String) -> Unit,
    onTextCommit: (String) -> Unit,
    onDisplayToolbarClick: () -> Unit,
    hint: String = "",
    editMode: Boolean = false,
    editText: String? = null,
    browserActions: @Composable () -> Unit = {},
) {
    val selectedTab: SessionState? by target.observeAsComposableStateFrom(
        store = store,
        observe = { tab -> tab?.content?.url },
    )

    val url = selectedTab?.content?.url ?: ""
    val input = when (editText) {
        null -> url
        else -> editText
    }

    if (editMode) {
        BrowserEditToolbar(
            url = input,
            onUrlCommitted = { text -> onTextCommit(text) },
            onUrlEdit = { text -> onTextEdit(text) },
        )
    } else {
        BrowserDisplayToolbar(
            url = selectedTab?.content?.url ?: hint,
            onUrlClicked = {
                onDisplayToolbarClick()
            },
            onMenuClicked = { onDisplayMenuClicked() },
            browserActions = browserActions,
        )
    }
}
