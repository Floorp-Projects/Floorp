/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
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
        val sessionManager = SessionManager(engine = mock())
        val presenter: TabsTrayPresenter = mock()
        val interactor: TabsTrayInteractor = mock()
        val useCases = TabsUseCases(sessionManager)
        val tabsFeature = spy(TabsFeature(mock(), sessionManager, useCases, mock()))

        assertNotEquals(tabsFeature.interactor, interactor)
        assertNotEquals(tabsFeature.presenter, presenter)

        tabsFeature.interactor = interactor
        tabsFeature.presenter = presenter

        assertEquals(tabsFeature.interactor, interactor)
        assertEquals(tabsFeature.presenter, presenter)
    }

    @Test
    fun start() {
        val sessionManager = SessionManager(engine = mock())
        val presenter: TabsTrayPresenter = mock()
        val interactor: TabsTrayInteractor = mock()
        val useCases = TabsUseCases(sessionManager)
        val tabsFeature = spy(TabsFeature(mock(), sessionManager, useCases, mock()))

        tabsFeature.presenter = presenter
        tabsFeature.interactor = interactor

        tabsFeature.start()

        verify(presenter).start()
        verify(interactor).start()
    }

    @Test
    fun stop() {
        val sessionManager = SessionManager(engine = mock())
        val presenter: TabsTrayPresenter = mock()
        val interactor: TabsTrayInteractor = mock()
        val useCases = TabsUseCases(sessionManager)
        val tabsFeature = spy(TabsFeature(mock(), sessionManager, useCases, mock()))

        tabsFeature.presenter = presenter
        tabsFeature.interactor = interactor

        tabsFeature.stop()

        verify(presenter).stop()
        verify(interactor).stop()
    }

    @Test
    fun filterTabs() {
        val sessionManager = SessionManager(engine = mock())
        val presenter: TabsTrayPresenter = mock()
        val interactor: TabsTrayInteractor = mock()
        val useCases = TabsUseCases(sessionManager)
        val tabsFeature = spy(TabsFeature(mock(), sessionManager, useCases, mock()))

        tabsFeature.presenter = presenter
        tabsFeature.interactor = interactor

        val filter: (Session) -> Boolean = { true }

        tabsFeature.filterTabs(filter)

        verify(presenter).sessionsFilter = filter
        verify(presenter).calculateDiffAndUpdateTabsTray()
    }
}
