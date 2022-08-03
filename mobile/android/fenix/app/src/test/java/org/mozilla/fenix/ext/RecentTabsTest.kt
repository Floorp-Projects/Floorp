/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.state.state.TabSessionState
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.home.recenttabs.RecentTab

class RecentTabsTest {
    @Test
    fun `Test filtering out tab`() {
        val filteredId = "id"
        val mockSessionState: TabSessionState = mockk()
        every { mockSessionState.id } returns filteredId
        val tab = RecentTab.Tab(mockSessionState)
        val searchGroup = RecentTab.SearchGroup(
            tabId = filteredId,
            searchTerm = "",
            url = "",
            thumbnail = null,
            count = 0
        )

        val recentTabs = listOf(tab, searchGroup)
        val result = recentTabs.filterOutTab(tab)

        assertEquals(listOf(searchGroup), result)
    }
}
