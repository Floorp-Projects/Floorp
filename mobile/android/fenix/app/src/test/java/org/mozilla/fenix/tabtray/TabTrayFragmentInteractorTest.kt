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
    fun onShareSelectedTabsClicked() {
        val tab = Tab("1234", "mozilla.org")
        val tab2 = Tab("5678", "pocket.com")
        val selectedTabs = setOf(tab, tab2)
        interactor.onShareSelectedTabsClicked(selectedTabs)
        verify { controller.handleShareSelectedTabsClicked(selectedTabs) }
    }

    @Test
    fun onBookmarkSelectedTabs() {
        val tab = Tab("1234", "mozilla.org")
        val tab2 = Tab("5678", "pocket.com")
        val selectedTabs = setOf(tab, tab2)
        interactor.onBookmarkSelectedTabs(selectedTabs)
        verify { controller.handleBookmarkSelectedTabs(selectedTabs) }
    }

    @Test
    fun onDeleteSelectedTabs() {
        val tab = Tab("1234", "mozilla.org")
        val tab2 = Tab("5678", "pocket.com")
        val selectedTabs = setOf(tab, tab2)
        interactor.onDeleteSelectedTabs(selectedTabs)
        verify { controller.handleDeleteSelectedTabs(selectedTabs) }
    }

    @Test
    fun onNewTabTapped() {
        interactor.onNewTabTapped(private = true)
        verify { controller.handleNewTabTapped(true) }

        interactor.onNewTabTapped(private = false)
        verify { controller.handleNewTabTapped(false) }
    }

    @Test
    fun onTabSettingsClicked() {
        interactor.onTabSettingsClicked()

        verify {
            controller.handleTabSettingsClicked()
        }
    }

    @Test
    fun onTabTrayDismissed() {
        interactor.onTabTrayDismissed()
        verify { controller.handleTabTrayDismissed() }
    }

    @Test
    fun onShareTabsClicked() {
        interactor.onShareTabsOfTypeClicked(private = true)
        verify { controller.handleShareTabsOfTypeClicked(true) }

        interactor.onShareTabsOfTypeClicked(private = false)
        verify { controller.handleShareTabsOfTypeClicked(false) }
    }

    @Test
    fun onSaveToCollectionClicked() {
        val tab = Tab("1234", "mozilla.org")
        interactor.onSaveToCollectionClicked(setOf(tab))
        verify { controller.handleSaveToCollectionClicked(setOf(tab)) }
    }

    @Test
    fun onCloseAllTabsClicked() {
        interactor.onCloseAllTabsClicked(private = false)
        verify { controller.handleCloseAllTabsClicked(false) }

        interactor.onCloseAllTabsClicked(private = true)
        verify { controller.handleCloseAllTabsClicked(true) }
    }

    @Test
    fun onSyncedTabClicked() {
        interactor.onSyncedTabClicked(mockk(relaxed = true))
        verify { controller.handleSyncedTabClicked(any()) }
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

    @Test
    fun onSetUpAutoCloseTabsClicked() {
        interactor.onSetUpAutoCloseTabsClicked()
        verify { controller.handleSetUpAutoCloseTabsClicked() }
    }
}
