/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import com.google.android.material.tabs.TabLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import org.junit.Test

class TabLayoutObserverTest {
    private val interactor = mockk<TabsTrayInteractor>(relaxed = true)

    @Test
    fun `WHEN tab is selected THEN notify the interactor`() {
        val observer = TabLayoutObserver(interactor)
        val tab = mockk<TabLayout.Tab>()
        every { tab.position } returns 1

        observer.onTabSelected(tab)

        verify { interactor.setCurrentTrayPosition(1) }
    }
}
