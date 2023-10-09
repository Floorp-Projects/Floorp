/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test

class UpdateProductUrlStateActionTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN shopping product action is dispatched THEN isProductUrl of the relevant tab should reflect that`() {
        val tab1 = TabSessionState(
            id = "tab1",
            content = ContentState(
                url = "https://mozilla.org",
                private = false,
                isProductUrl = false,
            ),
        )
        val tab2 = TabSessionState(
            id = "tab2",
            content = ContentState(
                url = "https://www.amazon.com/product/123",
                private = false,
                isProductUrl = false,
            ),
        )
        val browserState = BrowserState(tabs = listOf(tab1, tab2))

        val browserStore = BrowserStore(initialState = browserState)

        browserStore.dispatch(
            ContentAction.UpdateProductUrlStateAction(tabId = "tab2", isProductUrl = true),
        ).joinBlocking()

        val actual = browserStore.state.findTab("tab2")!!.content.isProductUrl

        assertTrue(actual)
    }

    @Test
    fun `WHEN shopping product action is dispatched THEN private tab should not be affected`() {
        val tab1 = TabSessionState(
            id = "tab1",
            content = ContentState(
                url = "https://www.amazon.com/product/123",
                private = true,
                isProductUrl = false,
            ),
        )
        val browserState = BrowserState(tabs = listOf(tab1))

        val browserStore = BrowserStore(initialState = browserState)

        browserStore.dispatch(
            ContentAction.UpdateProductUrlStateAction(tabId = "tab1", isProductUrl = true),
        ).joinBlocking()

        val actual = browserStore.state.findTab("tab1")!!.content.isProductUrl

        assertFalse(actual)
    }
}
