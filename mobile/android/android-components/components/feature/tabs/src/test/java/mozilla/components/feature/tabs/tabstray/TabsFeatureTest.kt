/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class TabsFeatureTest {

    @Test
    fun `asserting getters`() {
        val store = BrowserStore()
        val presenter: TabsTrayPresenter = mock()
        val tabsFeature = spy(
            TabsFeature(
                mock(),
                store,
            ) { true },
        )

        assertNotEquals(tabsFeature.presenter, presenter)

        tabsFeature.presenter = presenter

        assertEquals(tabsFeature.presenter, presenter)
    }

    @Test
    fun start() {
        val store = BrowserStore()
        val presenter: TabsTrayPresenter = mock()
        val tabsFeature = spy(
            TabsFeature(
                mock(),
                store,
            ) { true },
        )

        tabsFeature.presenter = presenter

        tabsFeature.start()

        verify(presenter).start()
    }

    @Test
    fun stop() {
        val store = BrowserStore()
        val presenter: TabsTrayPresenter = mock()
        val tabsFeature = spy(
            TabsFeature(
                mock(),
                store,
            ) { true },
        )

        tabsFeature.presenter = presenter

        tabsFeature.stop()

        verify(presenter).stop()
    }

    @Test
    fun filterTabs() {
        val store = BrowserStore()
        val presenter: TabsTrayPresenter = mock()
        val tabsTray: TabsTray = mock()
        val tabsFeature = spy(
            TabsFeature(
                tabsTray,
                store,
            ) { true },
        )

        tabsFeature.presenter = presenter

        val filter: (TabSessionState) -> Boolean = { true }

        tabsFeature.filterTabs(filter)

        verify(presenter).tabsFilter = filter
        verify(tabsTray).updateTabs(emptyList(), null, null)
    }

    @Test
    fun `filterTabs uses default filter if a new one is not provided`() {
        val store = BrowserStore()
        val filter: (TabSessionState) -> Boolean = { false }
        val tabsFeature = spy(
            TabsFeature(
                mock(),
                store,
                defaultTabsFilter = filter,
            ),
        )
        val presenter: TabsTrayPresenter = mock()

        tabsFeature.presenter = presenter

        tabsFeature.filterTabs()

        verify(presenter).tabsFilter = filter
    }
}
