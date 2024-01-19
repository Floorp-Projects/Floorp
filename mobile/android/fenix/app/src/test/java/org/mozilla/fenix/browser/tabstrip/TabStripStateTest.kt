package org.mozilla.fenix.browser.tabstrip

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import org.junit.Assert.assertEquals
import org.junit.Test

class TabStripStateTest {

    @Test
    fun `WHEN browser state tabs is empty THEN tabs strip state tabs is empty`() {
        val browserState = BrowserState(tabs = emptyList())
        val actual = browserState.toTabStripState(isSelectDisabled = false, isPrivateMode = false)

        val expected = TabStripState(tabs = emptyList())

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN private mode is off THEN tabs strip state tabs should include only non private tabs`() {
        val browserState = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://example.com",
                    title = "Example 1",
                    private = false,
                    id = "1",
                ),
                createTab(
                    url = "https://example2.com",
                    title = "Example 2",
                    private = true,
                    id = "2",
                ),
                createTab(
                    url = "https://example3.com",
                    title = "Example 3",
                    private = false,
                    id = "3",
                ),
            ),
        )
        val actual = browserState.toTabStripState(isSelectDisabled = false, isPrivateMode = false)

        val expected = TabStripState(
            tabs = listOf(
                TabStripItem(
                    id = "1",
                    title = "Example 1",
                    url = "https://example.com",
                    isSelected = false,
                    isPrivate = false,
                ),
                TabStripItem(
                    id = "3",
                    title = "Example 3",
                    url = "https://example3.com",
                    isSelected = false,
                    isPrivate = false,
                ),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN private mode is on THEN tabs strip state tabs should include only private tabs`() {
        val browserState = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://example.com",
                    title = "Example",
                    private = false,
                    id = "1",
                ),
                createTab(
                    url = "https://example2.com",
                    title = "Private Example",
                    private = true,
                    id = "2",
                ),
                createTab(
                    url = "https://example3.com",
                    title = "Example 3",
                    private = true,
                    id = "3",
                ),
            ),
        )
        val actual = browserState.toTabStripState(isSelectDisabled = false, isPrivateMode = true)

        val expected = TabStripState(
            tabs = listOf(
                TabStripItem(
                    id = "2",
                    title = "Private Example",
                    url = "https://example2.com",
                    isSelected = false,
                    isPrivate = true,
                ),
                TabStripItem(
                    id = "3",
                    title = "Example 3",
                    url = "https://example3.com",
                    isSelected = false,
                    isPrivate = true,
                ),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN isSelectDisabled is false THEN tabs strip state tabs should have a selected tab`() {
        val browserState = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://example.com",
                    title = "Example 1",
                    private = false,
                    id = "1",
                ),
                createTab(
                    url = "https://example2.com",
                    title = "Example 2",
                    private = false,
                    id = "2",
                ),
            ),
            selectedTabId = "2",
        )
        val actual = browserState.toTabStripState(isSelectDisabled = false, isPrivateMode = false)

        val expected = TabStripState(
            tabs = listOf(
                TabStripItem(
                    id = "1",
                    title = "Example 1",
                    url = "https://example.com",
                    isSelected = false,
                    isPrivate = false,
                ),
                TabStripItem(
                    id = "2",
                    title = "Example 2",
                    url = "https://example2.com",
                    isSelected = true,
                    isPrivate = false,
                ),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN isSelectDisabled is true THEN tabs strip state tabs should not have a selected tab`() {
        val browserState = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://example.com",
                    title = "Example 1",
                    private = false,
                    id = "1",
                ),
                createTab(
                    url = "https://example2.com",
                    title = "Example 2",
                    private = false,
                    id = "2",
                ),
            ),
            selectedTabId = "2",
        )
        val actual = browserState.toTabStripState(isSelectDisabled = true, isPrivateMode = false)

        val expected = TabStripState(
            tabs = listOf(
                TabStripItem(
                    id = "1",
                    title = "Example 1",
                    url = "https://example.com",
                    isSelected = false,
                    isPrivate = false,
                ),
                TabStripItem(
                    id = "2",
                    title = "Example 2",
                    url = "https://example2.com",
                    isSelected = false,
                    isPrivate = false,
                ),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN a tab does not have a title THEN tabs strip should display the url`() {
        val browserState = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://example.com",
                    title = "Example 1",
                    private = false,
                    id = "1",
                ),
                createTab(
                    url = "https://example2.com",
                    title = "",
                    private = false,
                    id = "2",
                ),
            ),
            selectedTabId = "2",
        )
        val actual = browserState.toTabStripState(isSelectDisabled = false, isPrivateMode = false)

        val expected = TabStripState(
            tabs = listOf(
                TabStripItem(
                    id = "1",
                    title = "Example 1",
                    url = "https://example.com",
                    isSelected = false,
                    isPrivate = false,
                ),
                TabStripItem(
                    id = "2",
                    title = "https://example2.com",
                    url = "https://example2.com",
                    isSelected = true,
                    isPrivate = false,
                ),
            ),
        )

        assertEquals(expected, actual)
    }
}
