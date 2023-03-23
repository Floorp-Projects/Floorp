/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.content.Context
import io.mockk.mockk
import io.mockk.spyk
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

class CustomTabToolbarMenuTest {

    private lateinit var firefoxCustomTab: CustomTabSessionState
    private lateinit var store: BrowserStore
    private lateinit var customTabToolbarMenu: CustomTabToolbarMenu
    private lateinit var context: Context

    @Before
    fun setUp() {
        context = mockk(relaxed = true)

        firefoxCustomTab = createCustomTab(url = "https://firefox.com", id = "123")

        store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(url = "https://wikipedia.com", id = "1"),
                ),
                customTabs = listOf(
                    firefoxCustomTab,
                    createCustomTab(url = "https://mozilla.com", id = "456"),
                ),
            ),
        )

        customTabToolbarMenu = spyk(
            CustomTabToolbarMenu(
                context = context,
                store = store,
                sessionId = firefoxCustomTab.id,
                shouldReverseItems = false,
                isSandboxCustomTab = false,
                onItemTapped = { },
            ),
        )
    }

    @Test
    fun `custom tab toolbar menu uses the proper custom tab session`() {
        assertEquals(firefoxCustomTab.id, customTabToolbarMenu.session?.id)
        assertEquals("https://firefox.com", customTabToolbarMenu.session?.content?.url)
    }
}
