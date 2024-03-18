/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import io.mockk.verify
import org.junit.Test
import org.mozilla.fenix.tabstray.TabsTrayInteractor

class SelectionMenuIntegrationTest {

    private val interactor = mockk<TabsTrayInteractor>(relaxed = true)

    @Test
    fun `WHEN bookmark item is clicked THEN invoke interactor and close tray`() {
        val menu = SelectionMenuIntegration(mockk(), interactor)

        menu.handleMenuClicked(SelectionMenu.Item.BookmarkTabs)

        verify { interactor.onBookmarkSelectedTabsClicked() }
    }

    @Test
    fun `WHEN delete tabs item is clicked THEN invoke interactor and close tray`() {
        val menu = SelectionMenuIntegration(mockk(), interactor)

        menu.handleMenuClicked(SelectionMenu.Item.DeleteTabs)

        verify { interactor.onDeleteSelectedTabsClicked() }
    }
}
