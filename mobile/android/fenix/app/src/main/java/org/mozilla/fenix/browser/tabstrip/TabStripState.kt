/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.tabstrip

import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.state.BrowserState

/**
 * The ui state of the tabs strip.
 *
 * @property tabs The list of [TabStripItem].
 */
data class TabStripState(
    val tabs: List<TabStripItem>,
) {
    companion object {
        val initial = TabStripState(tabs = emptyList())
    }
}

/**
 * The ui state of a tab.
 *
 * @property id The id of the tab.
 * @property title The title of the tab.
 * @property url The url of the tab.
 * @property isPrivate Whether or not the tab is private.
 * @property isSelected Whether or not the tab is selected.
 */
data class TabStripItem(
    val id: String,
    val title: String,
    val url: String,
    val isPrivate: Boolean,
    val isSelected: Boolean,
)

/**
 * Converts [BrowserState] to [TabStripState] that contains the information needed to render the
 * tabs strip.
 *
 * @param isSelectDisabled When true, the tabs will show as selected.
 * @param isPrivateMode Whether or not the browser is in private mode.
 */
internal fun BrowserState.toTabStripState(
    isSelectDisabled: Boolean,
    isPrivateMode: Boolean,
): TabStripState {
    return TabStripState(
        tabs = getNormalOrPrivateTabs(isPrivateMode)
            .map {
                TabStripItem(
                    id = it.id,
                    title = it.content.title.ifBlank { it.content.url },
                    url = it.content.url,
                    isPrivate = it.content.private,
                    isSelected = !isSelectDisabled && it.id == selectedTabId,
                )
            },
    )
}
