/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.session.Session
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class ToolbarPopupWindowTest {

    @Test
    fun getUrlForClipboard() {
        val customTabSession: Session = mockk()
        every { customTabSession.url } returns "https://mozilla.org"

        // Custom tab
        assertEquals(
            "https://mozilla.org",
            ToolbarPopupWindow.getUrlForClipboard(mockk(), customTabSession)
        )

        // Regular tab
        val regularTab = createTab(url = "http://firefox.com")
        var store =
            BrowserStore(BrowserState(tabs = listOf(regularTab), selectedTabId = regularTab.id))
        assertEquals(regularTab.content.url, ToolbarPopupWindow.getUrlForClipboard(store))

        // Reader Tab
        val readerTab = createTab(
            url = "moz-extension://1234",
            readerState = ReaderState(active = true, activeUrl = "https://blog.mozilla.org/123")
        )
        store = BrowserStore(BrowserState(tabs = listOf(readerTab), selectedTabId = readerTab.id))
        assertEquals(readerTab.readerState.activeUrl, ToolbarPopupWindow.getUrlForClipboard(store))
    }
}
