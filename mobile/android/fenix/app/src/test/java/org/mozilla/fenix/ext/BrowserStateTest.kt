/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class BrowserStateTest {

    @Test
    fun `WHEN there is a selected tab THEN asRecentTabs returns the selected tab as a list`() {
        val tab = createTab(
            url = "https://www.mozilla.org",
            id = "1"
        )
        val tabs = listOf(tab)
        val state = BrowserState(
            tabs = tabs,
            selectedTabId = tab.id
        )
        val recentTabs = state.asRecentTabs()

        assertEquals(tabs, recentTabs)
    }

    @Test
    fun `WHEN there is no selected tab THEN asRecentTabs returns an empty list`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://www.mozilla.org",
                    id = "1"
                )
            )
        )
        val recentTabs = state.asRecentTabs()

        assertEquals(0, recentTabs.size)
    }

    @Test
    fun `WHEN the selected tab is private THEN asRecentTabs returns an empty list`() {
        val tab = createTab(
            url = "https://www.mozilla.org",
            id = "1",
            private = true
        )
        val tabs = listOf(tab)
        val state = BrowserState(
            tabs = tabs,
            selectedTabId = tab.id
        )
        val recentTabs = state.asRecentTabs()

        assertEquals(0, recentTabs.size)
    }
}
