/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import mozilla.components.browser.session.Session
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.tabs.TabsUseCases
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class TabsTrayInteractorTest {
    @Test
    fun `interactor registers and unregisters from tabstray`() {
        val tabsTray: TabsTray = mock()
        val interactor = TabsTrayInteractor(tabsTray, mock(), mock(), mock())

        interactor.start()
        verify(tabsTray).register(interactor)
        verifyNoMoreInteractions(tabsTray)

        interactor.stop()
        verify(tabsTray).unregister(interactor)
        verifyNoMoreInteractions(tabsTray)
    }

    @Test
    fun `interactor will call use case when tab is selected`() {
        val selectTabUseCase: TabsUseCases.SelectTabUseCase = mock()
        val interactor = TabsTrayInteractor(mock(), selectTabUseCase, mock(), mock())

        val session = Session("https://www.mozilla.org")
        interactor.onTabSelected(session)

        verify(selectTabUseCase).invoke(session)
    }

    @Test
    fun `interactor will call use case when tab is closed`() {
        val removeTabUseCase: TabsUseCases.RemoveTabUseCase = mock()
        val interactor = TabsTrayInteractor(mock(), mock(), removeTabUseCase, mock())

        val session = Session("https://www.mozilla.org")
        interactor.onTabClosed(session)

        verify(removeTabUseCase).invoke(session)
    }
}
