/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.tabstray

import androidx.compose.foundation.Image
import androidx.compose.material.IconButton
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.material.contentColorFor
import androidx.compose.material.primarySurface
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.sp
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.browser.tabstray.R
import mozilla.components.lib.state.ext.observeAsComposableState

private const val MAX_VISIBLE_TABS = 99
private const val SO_MANY_TABS_OPEN = "∞"

/**
 * A button showing the count of tabs in the [store] using the provided [tabsFilter].
 *
 * @param store The store to observe.
 * @param onClicked Gets invoked when the user clicks the button.
 * @param tabsFilter Used for filtering the list of tabs.
 */
@Composable
fun TabCounterButton(
    store: BrowserStore,
    onClicked: () -> Unit,
    tabsFilter: (TabSessionState) -> Boolean = { true },
) {
    IconButton(
        onClick = onClicked,
    ) {
        val backgroundColor = MaterialTheme.colors.primarySurface
        val foregroundColor = contentColorFor(backgroundColor)
        val tabs = store.observeAsComposableState { state -> state.tabs.filter(tabsFilter) }
        val count = tabs.value?.size ?: 0

        Image(
            painter = painterResource(R.drawable.mozac_tabcounter_background),
            contentDescription = createContentDescription(count),
            colorFilter = ColorFilter.tint(foregroundColor),
        )

        Text(
            createButtonText(count),
            fontSize = 12.sp,
            color = foregroundColor,
        )
    }
}

private fun createButtonText(count: Int): String {
    return if (count > MAX_VISIBLE_TABS) {
        SO_MANY_TABS_OPEN
    } else {
        count.toString()
    }
}

@Composable
private fun createContentDescription(count: Int): String {
    return if (count == 1) {
        stringResource(R.string.mozac_tab_counter_open_tab_tray_single)
    } else {
        String.format(
            stringResource(R.string.mozac_tab_counter_open_tab_tray_plural),
            count.toString(),
        )
    }
}
