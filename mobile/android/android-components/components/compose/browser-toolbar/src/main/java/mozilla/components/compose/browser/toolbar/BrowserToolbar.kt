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
    editText: String? = null
) {
    val selectedTab: SessionState? by target.observeAsComposableStateFrom(
        store = store,
        observe = { tab -> tab?.content?.url }
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
            onUrlEdit = { text -> onTextEdit(text) }
        )
    } else {
        BrowserDisplayToolbar(
            url = selectedTab?.content?.url ?: hint,
            onUrlClicked = {
                onDisplayToolbarClick()
            },
            onMenuClicked = { onDisplayMenuClicked() }
        )
    }
}
