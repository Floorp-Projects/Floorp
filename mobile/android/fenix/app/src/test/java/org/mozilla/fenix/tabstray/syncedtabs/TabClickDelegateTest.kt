/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.syncedtabs

import io.mockk.Called
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.storage.sync.Tab
import org.junit.Test
import org.mozilla.fenix.tabstray.NavigationInteractor

class TabClickDelegateTest {

    private val interactor = mockk<NavigationInteractor>(relaxed = true)
    private val tab = mockk<Tab>()

    @Test
    fun `WHEN tab is clicked THEN invoke the interactor`() {
        val delegate = TabClickDelegate(interactor)

        delegate.onTabClicked(tab)

        verify { interactor.onSyncedTabClicked(tab) }
    }

    @Test
    fun `WHEN refresh is invoked THEN do nothing`() {
        val delegate = TabClickDelegate(interactor)

        delegate.onRefresh()

        verify { interactor wasNot Called }
    }
}
