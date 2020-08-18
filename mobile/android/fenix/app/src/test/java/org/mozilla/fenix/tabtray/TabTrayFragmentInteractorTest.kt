/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.tabstray.Tab
import org.junit.Test

class TabTrayFragmentInteractorTest {
    private val controller = mockk<TabTrayController>(relaxed = true)
    private val interactor = TabTrayFragmentInteractor(controller)

    @Test
    fun onNewTabTapped() {
        interactor.onNewTabTapped(private = true)
        verify { controller.onNewTabTapped(true) }

        interactor.onNewTabTapped(private = false)
        verify { controller.onNewTabTapped(false) }
    }

    @Test
    fun onTabTrayDismissed() {
        interactor.onTabTrayDismissed()
        verify { controller.onTabTrayDismissed() }
    }

    @Test
    fun onShareTabsClicked() {
        interactor.onShareTabsClicked(private = true)
        verify { controller.onShareTabsClicked(true) }

        interactor.onShareTabsClicked(private = false)
        verify { controller.onShareTabsClicked(false) }
    }

    @Test
    fun onSaveToCollectionClicked() {
        val tab = Tab("1234", "mozilla.org")
        interactor.onSaveToCollectionClicked(setOf(tab))
        verify { controller.onSaveToCollectionClicked(setOf(tab)) }
    }

    @Test
    fun onCloseAllTabsClicked() {
        interactor.onCloseAllTabsClicked(private = false)
        verify { controller.onCloseAllTabsClicked(false) }

        interactor.onCloseAllTabsClicked(private = true)
        verify { controller.onCloseAllTabsClicked(true) }
    }

    @Test
    fun onSyncedTabClicked() {
        interactor.onSyncedTabClicked(mockk(relaxed = true))
        verify { controller.onSyncedTabClicked(any()) }
    }

    @Test
    fun onBackPressed() {
        interactor.onBackPressed()
        verify { controller.handleBackPressed() }
    }

    @Test
    fun onModeRequested() {
        interactor.onModeRequested()
        verify { controller.onModeRequested() }
    }

    @Test
    fun onOpenTab() {
        val tab = Tab("1234", "mozilla.org")
        interactor.onOpenTab(tab)
        verify { controller.handleOpenTab(tab) }
    }

    @Test
    fun onAddSelectedTab() {
        val tab = Tab("1234", "mozilla.org")
        interactor.onAddSelectedTab(tab)
        verify { controller.handleAddSelectedTab(tab) }
    }

    @Test
    fun onRemoveSelectedTab() {
        val tab = Tab("1234", "mozilla.org")
        interactor.onRemoveSelectedTab(tab)
        verify { controller.handleRemoveSelectedTab(tab) }
    }

    @Test
    fun onEnterMultiselect() {
        interactor.onEnterMultiselect()
        verify { controller.handleEnterMultiselect() }
    }
}
