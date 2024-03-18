/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.storage.HistoryMetadataKey
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.ext.DEFAULT_ACTIVE_DAYS
import java.util.concurrent.TimeUnit

class TabSessionStateKtTest {

    private val maxTime = TimeUnit.DAYS.toMillis(DEFAULT_ACTIVE_DAYS)
    private var inactiveTimestamp = 0L

    @Before
    fun setup() {
        // Subtracting an extra 10 seconds in case the test runner is loopy.
        inactiveTimestamp = System.currentTimeMillis() - maxTime - 10_000
    }

    @Test
    fun `WHEN tab was recently accessed THEN isActive is true`() {
        val tab = createTab(
            url = "https://mozilla.org",
            lastAccess = System.currentTimeMillis(),
            createdAt = 0,
        )
        assertTrue(tab.isNormalTabActive(maxTime))
    }

    @Test
    fun `WHEN tab was recently created THEN isActive is true`() {
        val tab = createTab(
            url = "https://mozilla.org",
            lastAccess = 0,
            createdAt = System.currentTimeMillis(),
        )
        assertTrue(tab.isNormalTabActive(maxTime))
    }

    @Test
    fun `WHEN tab either was not created or accessed recently THEN isActive is true`() {
        val tab = createTab(
            url = "https://mozilla.org",
            lastAccess = 0,
            createdAt = inactiveTimestamp,
        )
        assertFalse(tab.isNormalTabActive(maxTime))

        val tab2 = createTab(
            url = "https://mozilla.org",
            lastAccess = inactiveTimestamp,
            createdAt = 0,
        )
        assertFalse(tab2.isNormalTabActive(maxTime))
    }

    @Test
    fun `WHEN tab has not been accessed or recently created THEN isActive is false`() {
        val tab = createTab(
            url = "https://mozilla.org",
            lastAccess = inactiveTimestamp,
            createdAt = inactiveTimestamp,
        )
        assertFalse(tab.isNormalTabActive(maxTime))
    }

    @Test
    fun `WHEN normal tab is recently used THEN return true`() {
        val tab = createTab(
            url = "https://mozilla.org",
            lastAccess = System.currentTimeMillis(),
            createdAt = System.currentTimeMillis(),
            private = false,
        )
        val test = tab.isNormalTabActive(maxTime)
        assertTrue(test)
    }

    @Test
    fun `WHEN tabs are private THEN always false`() {
        val tab = createTab(
            url = "https://mozilla.org",
            lastAccess = System.currentTimeMillis(),
            createdAt = System.currentTimeMillis(),
            private = true,
        )
        assertFalse(tab.isNormalTabActive(maxTime))
    }

    @Test
    fun `WHEN inactive tabs are private THEN always false`() {
        val tab = createTab(
            url = "https://mozilla.org",
            lastAccess = inactiveTimestamp,
            createdAt = inactiveTimestamp,
            private = true,
        )
        assertFalse(tab.isNormalTabActive(maxTime))
    }

    @Test
    fun `WHEN tab has a search term or metadata THEN return true `() {
        val tab = createTab(
            url = "https://mozilla.org",
            createdAt = System.currentTimeMillis(),
            historyMetadata = HistoryMetadataKey("https://getpockjet.com", "cats"),
        )
        val tab2 = createTab(
            url = "https://mozilla.org",
            createdAt = System.currentTimeMillis(),
            searchTerms = "dogs",
        )
        val tab3 = createTab(
            url = "https://mozilla.org",
            createdAt = inactiveTimestamp,
            searchTerms = "dogs",
        )
        assertTrue(tab.isNormalTabActiveWithSearchTerm(maxTime))
        assertTrue(tab2.isNormalTabActiveWithSearchTerm(maxTime))
        assertFalse(tab3.isNormalTabActiveWithSearchTerm(maxTime))
    }
}
