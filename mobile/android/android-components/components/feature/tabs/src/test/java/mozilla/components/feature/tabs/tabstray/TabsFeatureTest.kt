/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.tabs.TabsUseCases
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
        val interactor: TabsTrayInteractor = mock()
        val selectTabUseCase: TabsUseCases.SelectTabUseCase = mock()
        val removeTabUseCase: TabsUseCases.RemoveTabUseCase = mock()
        val tabsFeature = spy(
            TabsFeature(
                mock(),
                store,
                selectTabUseCase,
                removeTabUseCase,
                mock(),
                mock()
            )
        )

        assertNotEquals(tabsFeature.interactor, interactor)
        assertNotEquals(tabsFeature.presenter, presenter)

        tabsFeature.interactor = interactor
        tabsFeature.presenter = presenter

        assertEquals(tabsFeature.interactor, interactor)
        assertEquals(tabsFeature.presenter, presenter)
    }

    @Test
    fun start() {
        val store = BrowserStore()
        val presenter: TabsTrayPresenter = mock()
        val interactor: TabsTrayInteractor = mock()
        val selectTabUseCase: TabsUseCases.SelectTabUseCase = mock()
        val removeTabUseCase: TabsUseCases.RemoveTabUseCase = mock()
        val tabsFeature = spy(
            TabsFeature(
                mock(),
                store,
                selectTabUseCase,
                removeTabUseCase,
                mock(),
                mock()
            )
        )

        tabsFeature.presenter = presenter
        tabsFeature.interactor = interactor

        tabsFeature.start()

        verify(presenter).start()
        verify(interactor).start()
    }

    @Test
    fun stop() {
        val store = BrowserStore()
        val presenter: TabsTrayPresenter = mock()
        val interactor: TabsTrayInteractor = mock()
        val selectTabUseCase: TabsUseCases.SelectTabUseCase = mock()
        val removeTabUseCase: TabsUseCases.RemoveTabUseCase = mock()
        val tabsFeature = spy(
            TabsFeature(
                mock(),
                store,
                selectTabUseCase,
                removeTabUseCase,
                mock(),
                mock()
            )
        )

        tabsFeature.presenter = presenter
        tabsFeature.interactor = interactor

        tabsFeature.stop()

        verify(presenter).stop()
        verify(interactor).stop()
    }

    @Test
    fun filterTabs() {
        val store = BrowserStore()
        val presenter: TabsTrayPresenter = mock()
        val tabsTray: TabsTray = mock()
        val interactor: TabsTrayInteractor = mock()
        val selectTabUseCase: TabsUseCases.SelectTabUseCase = mock()
        val removeTabUseCase: TabsUseCases.RemoveTabUseCase = mock()
        val tabsFeature = spy(
            TabsFeature(
                tabsTray,
                store,
                selectTabUseCase,
                removeTabUseCase,
                mock(),
                mock()
            )
        )

        tabsFeature.presenter = presenter
        tabsFeature.interactor = interactor

        val filter: (TabSessionState) -> Boolean = { true }

        tabsFeature.filterTabs(filter)

        verify(presenter).tabsFilter = filter
        verify(tabsTray).updateTabs(Tabs(emptyList(), -1))
    }

    @Test
    fun `filterTabs uses default filter if a new one is not provided`() {
        val store = BrowserStore()
        val filter: (TabSessionState) -> Boolean = { false }
        val selectTabUseCase: TabsUseCases.SelectTabUseCase = mock()
        val removeTabUseCase: TabsUseCases.RemoveTabUseCase = mock()
        val tabsFeature = spy(
            TabsFeature(
                mock(),
                store,
                selectTabUseCase,
                removeTabUseCase,
                defaultTabsFilter = filter,
                closeTabsTray = mock()
            )
        )
        val presenter: TabsTrayPresenter = mock()
        val interactor: TabsTrayInteractor = mock()

        tabsFeature.presenter = presenter
        tabsFeature.interactor = interactor

        tabsFeature.filterTabs()

        verify(presenter).tabsFilter = filter
    }
}
