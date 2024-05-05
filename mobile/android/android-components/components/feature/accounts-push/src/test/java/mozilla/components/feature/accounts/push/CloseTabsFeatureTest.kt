/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceType
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class CloseTabsFeatureTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val device123 = Device(
        id = "123",
        displayName = "Charcoal",
        deviceType = DeviceType.DESKTOP,
        isCurrentDevice = false,
        lastAccessTime = null,
        capabilities = listOf(DeviceCapability.CLOSE_TABS),
        subscriptionExpired = true,
        subscription = null,
    )

    @Test
    fun `GIVEN a notification to close multiple URLs WHEN all URLs are open in tabs THEN all tabs are closed and the callback is invoked`() {
        val urls = listOf(
            "https://mozilla.org",
            "https://getfirefox.com",
            "https://example.org",
            "https://getthunderbird.com",
        )
        val browserStore = BrowserStore(
            BrowserState(
                tabs = urls.map { createTab(it) },
            ),
        )
        val callback: (Device?, List<String>) -> Unit = mock()
        val feature = CloseTabsFeature(
            browserStore,
            accountManager = mock(),
            owner = mock(),
            onTabsClosed = callback,
        )

        feature.observer.onTabsClosed(device123, urls)

        browserStore.waitUntilIdle()

        assertTrue(browserStore.state.tabs.isEmpty())
        verify(callback).invoke(eq(device123), eq(urls))
    }

    @Test
    fun `GIVEN a notification to close a URL WHEN the URL is not open in a tab THEN the callback is not invoked`() {
        val browserStore = BrowserStore()
        val callback: (Device?, List<String>) -> Unit = mock()
        val feature = CloseTabsFeature(
            browserStore,
            accountManager = mock(),
            owner = mock(),
            onTabsClosed = callback,
        )

        feature.observer.onTabsClosed(device123, listOf("https://mozilla.org"))

        browserStore.waitUntilIdle()

        verify(callback, never()).invoke(any(), any())
    }

    @Test
    fun `GIVEN a notification to close duplicate URLs WHEN the duplicate URLs are open in tabs THEN the number of tabs closed matches the number of URLs and the callback is invoked`() {
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://mozilla.org", id = "1"),
                    createTab("https://mozilla.org", id = "2"),
                    createTab("https://getfirefox.com", id = "3"),
                    createTab("https://getfirefox.com", id = "4"),
                    createTab("https://getfirefox.com", id = "5"),
                    createTab("https://getthunderbird.com", id = "6"),
                    createTab("https://example.org", id = "7"),
                ),
            ),
        )
        val callback: (Device?, List<String>) -> Unit = mock()
        val feature = CloseTabsFeature(
            browserStore,
            accountManager = mock(),
            owner = mock(),
            onTabsClosed = callback,
        )

        feature.observer.onTabsClosed(
            device123,
            listOf(
                "https://mozilla.org",
                "https://getfirefox.com",
                "https://getfirefox.com",
                "https://example.org",
                "https://example.org",
            ),
        )

        browserStore.waitUntilIdle()

        assertEquals(listOf("2", "5", "6"), browserStore.state.tabs.map { it.id })
        verify(callback).invoke(
            eq(device123),
            eq(
                listOf(
                    "https://mozilla.org",
                    "https://getfirefox.com",
                    "https://getfirefox.com",
                    "https://example.org",
                ),
            ),
        )
    }
}
