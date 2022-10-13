/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.compose

import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import mozilla.components.browser.icons.BrowserIcons

/**
 * The scope of a [BrowserIcons.Loader] block.
 */
interface IconLoaderScope {
    val state: MutableState<IconLoaderState>
}

/**
 * Renders the inner [content] block once an icon was loaded.
 */
@Composable
fun IconLoaderScope.WithIcon(
    content: @Composable (icon: IconLoaderState.Icon) -> Unit,
) {
    WithInternalState {
        val state = state.value
        if (state is IconLoaderState.Icon) {
            content(state)
        }
    }
}

/**
 * Renders the inner [content] block until an icon was loaded.
 */
@Composable
fun IconLoaderScope.Placeholder(
    content: @Composable () -> Unit,
) {
    WithInternalState {
        val state = state.value
        if (state is IconLoaderState.Loading) {
            content()
        }
    }
}

@Composable
private fun IconLoaderScope.WithInternalState(
    content: @Composable InternalIconLoaderScope.() -> Unit,
) {
    val internalScope = this as InternalIconLoaderScope
    internalScope.content()
}

internal class InternalIconLoaderScope(
    override val state: MutableState<IconLoaderState> = mutableStateOf(IconLoaderState.Loading),
) : IconLoaderScope
