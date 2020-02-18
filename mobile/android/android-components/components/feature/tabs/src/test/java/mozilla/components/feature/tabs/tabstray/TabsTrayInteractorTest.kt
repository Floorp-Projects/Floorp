/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.mock
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

@RunWith(AndroidJUnit4::class)
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

        val tab = Tab("1", "https://www.mozilla.org")
        interactor.onTabSelected(tab)

        verify(selectTabUseCase).invoke(tab.id)
    }

    @Test
    fun `interactor will call use case when tab is closed`() {
        val removeTabUseCase: TabsUseCases.RemoveTabUseCase = mock()
        val interactor = TabsTrayInteractor(mock(), mock(), removeTabUseCase, mock())

        val tab = Tab("1", "https://www.mozilla.org")
        interactor.onTabClosed(tab)

        verify(removeTabUseCase).invoke(tab.id)
    }
}
